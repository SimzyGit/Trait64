// SPDX-License-Identifier: AGPL-3.0-or-later
#include "database.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>
#include <QUuid>
#include <array>
#include <fstream>
#include <stdexcept>

namespace t64 {
namespace {
void require(bool ok, const QString &error) {
    if (!ok)
        throw std::runtime_error(error.toStdString());
}
void run(QSqlQuery &q) {
    if (!q.exec())
        throw std::runtime_error(q.lastError().text().toStdString());
}
QString lockPath(const QString &database) {
    return QFileInfo(database).canonicalFilePath() + ".import.lock";
}
struct Paused {};
} // namespace

void Database::upgradeV5(int originalVersion) {
    if (originalVersion >= 5)
        return;
    if (originalVersion > 0)
        backup(path_ + ".before-v5-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
    sql("SAVEPOINT schema5");
    try {
        sql("CREATE TABLE import_jobs(id INTEGER PRIMARY KEY,filename TEXT NOT NULL,bytes INTEGER NOT NULL,"
            "modified INTEGER NOT NULL,sha256 TEXT NOT NULL DEFAULT '',"
            "source_id INTEGER REFERENCES sources(id) ON DELETE SET NULL,"
            "status TEXT NOT NULL DEFAULT 'paused' CHECK(status IN('paused','running','failed','completed')),"
            "offset INTEGER NOT NULL DEFAULT 0,processed INTEGER NOT NULL DEFAULT 0,"
            "added INTEGER NOT NULL DEFAULT 0,error TEXT NOT NULL DEFAULT '',"
            "updated TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')))");
        sql("PRAGMA user_version=5");
        sql("RELEASE schema5");
    } catch (...) {
        sql("ROLLBACK TO schema5");
        sql("RELEASE schema5");
        throw;
    }
}

qint64 Database::createImportJob(const QString &path, const QVariantMap &source) {
    const QFileInfo file(path);
    require(file.isFile() && file.isReadable(), "PGN introuvable ou illisible.");
    const auto canonical = file.canonicalFilePath();
    require(canonical != QFileInfo(path_).canonicalFilePath(), "La bibliothèque n’est pas un PGN.");
    auto origin = source;
    if (origin.isEmpty())
        origin = {
            {"id", "local:" +
                       QString::fromLatin1(
                           QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha256).toHex())},
            {"title", file.completeBaseName()},
            {"source", QUrl::fromLocalFile(canonical).toString()},
            {"license", "Licence non renseignée (fichier local)"}};
    sql("SAVEPOINT create_import_job");
    try {
        const auto sourceId = registerSource(origin);
        QSqlQuery q(db_);
        q.prepare("INSERT INTO import_jobs(filename,bytes,modified,source_id) VALUES(?,?,?,?)");
        q.addBindValue(canonical);
        q.addBindValue(file.size());
        q.addBindValue(file.lastModified().toMSecsSinceEpoch());
        q.addBindValue(sourceId ? QVariant(sourceId) : QVariant());
        run(q);
        const auto id = q.lastInsertId().toLongLong();
        sql("RELEASE create_import_job");
        return id;
    } catch (...) {
        sql("ROLLBACK TO create_import_job");
        sql("RELEASE create_import_job");
        throw;
    }
}

QVariantMap Database::importJob(qint64 id) const {
    QSqlQuery q(db_);
    q.prepare("SELECT j.id,j.filename,j.bytes,j.modified,j.sha256,j.source_id,j.status,j.offset,j.processed,"
              "j.added,j.error,j.updated,COALESCE(s.title,'') FROM import_jobs j LEFT JOIN sources s ON "
              "s.id=j.source_id WHERE j.id=?");
    q.addBindValue(id);
    run(q);
    require(q.next(), "Import introuvable.");
    QVariantMap row;
    const QStringList keys{"id",     "file",      "bytes", "modified", "sha256",  "sourceId", "status",
                           "offset", "processed", "added", "error",    "updated", "title"};
    for (int i = 0; i < keys.size(); ++i)
        row[keys[i]] = q.value(i);
    return row;
}

QVariantList Database::importJobs() const {
    QSqlQuery q(db_);
    q.prepare("SELECT id FROM import_jobs ORDER BY (status='completed'),id DESC LIMIT 200");
    run(q);
    QVariantList rows;
    while (q.next())
        rows << importJob(q.value(0).toLongLong());
    return rows;
}

void Database::forgetImportJob(qint64 id) {
    QLockFile lock(lockPath(path_));
    lock.setStaleLockTime(0);
    require(lock.tryLock(), "Un import reprenable utilise cette bibliothèque.");
    QSqlQuery q(db_);
    q.prepare("DELETE FROM import_jobs WHERE id=?");
    q.addBindValue(id);
    run(q);
}

QVariantMap Database::runImportJob(qint64 id, const std::function<bool(const ImportProgress &)> &progress,
                                   const QString &replacement, int batchSize, int workers) {
    require(workers >= 1 && workers <= 8, "Nombre de lecteurs PGN invalide (1 à 8).");
    require(batchSize >= 1 && batchSize <= 1000, "Taille de lot invalide.");
    QLockFile lock(lockPath(path_));
    // Long imports must not steal another living process's lock after a timeout.
    lock.setStaleLockTime(0);
    require(lock.tryLock(), "Un import reprenable utilise déjà cette bibliothèque.");
    auto job = importJob(id);
    if (job["status"] == "completed")
        return job;
    ImportProgress p;
    p.job = id;
    p.total = job["bytes"].toLongLong();
    p.processed = p.committed = job["processed"].toLongLong();
    p.added = job["added"].toLongLong();
    p.committedBytes = job["offset"].toLongLong();
    bool transaction = false;
    auto notify = [&] {
        if (progress && !progress(p))
            throw Paused{};
    };
    auto setStatus = [&](const QString &status, const QString &error) {
        QSqlQuery q(db_);
        q.prepare("UPDATE import_jobs SET status=?,error=?,updated=strftime('%Y-%m-%dT%H:%M:%fZ','now') "
                  "WHERE id=?");
        q.addBindValue(status);
        q.addBindValue(error);
        q.addBindValue(id);
        run(q);
    };
    auto rollback = [&] {
        if (transaction) {
            db_.rollback();
            transaction = false;
        }
    };
    try {
        setStatus("running", "");
        notify();
        const QString path = replacement.isEmpty() ? job["file"].toString() : replacement;
        const QFileInfo initial(path);
        require(initial.isFile() && initial.size() == p.total,
                "Le PGN est absent ou sa taille a changé. Retrouvez le fichier original pour reprendre.");
        const auto modified = initial.lastModified().toMSecsSinceEpoch();
        auto unchanged = [&] {
            const QFileInfo now(path);
            require(now.isFile() && now.size() == p.total &&
                        now.lastModified().toMSecsSinceEpoch() == modified,
                    "Le PGN a changé pendant l’import. Le dernier lot n’a pas été conservé.");
        };
        std::ifstream input(QFile::encodeName(path).constData(), std::ios::binary);
        require(bool(input), "Impossible d’ouvrir le PGN.");
        QCryptographicHash hash(QCryptographicHash::Sha256);
        std::array<char, 1024 * 1024> buffer;
        while (input.read(buffer.data(), buffer.size()) || input.gcount()) {
            hash.addData(QByteArrayView(buffer.data(), input.gcount()));
            p.bytes += input.gcount();
            notify();
        }
        require(!input.bad() && p.bytes == p.total, "Erreur pendant la vérification du fichier PGN.");
        unchanged();
        const auto digest = QString::fromLatin1(hash.result().toHex());
        require(job["sha256"].toString().isEmpty() || job["sha256"] == digest,
                "L’empreinte SHA-256 du PGN a changé. Reprise refusée : utilisez le fichier original.");
        QSqlQuery verified(db_);
        verified.prepare("UPDATE import_jobs SET sha256=?,filename=?,modified=? WHERE id=?");
        verified.addBindValue(digest);
        verified.addBindValue(initial.canonicalFilePath());
        verified.addBindValue(modified);
        verified.addBindValue(id);
        run(verified);
        input.clear();
        input.seekg(p.committedBytes);
        require(bool(input), "Impossible de rejoindre le point de reprise.");
        p.bytes = p.committedBytes;
        p.verifying = false;
        const auto start = p.committedBytes;
        const auto sourceId = job["sourceId"].toLongLong();
        qint64 batchBefore = 0, batchAdded = p.added, nextOffset = start;
        auto gameCount = [&] {
            QSqlQuery count(db_);
            count.prepare("SELECT games FROM library_counts WHERE id=1");
            run(count);
            require(count.next(), "Compteur de bibliothèque absent.");
            return count.value(0).toLongLong();
        };
        auto begin = [&] {
            if (!transaction) {
                sql("BEGIN IMMEDIATE");
                transaction = true;
                batchBefore = gameCount();
                batchAdded = p.added;
            }
        };
        auto commit = [&](bool complete) {
            begin();
            unchanged();
            p.added = batchAdded + gameCount() - batchBefore;
            QSqlQuery q(db_);
            q.prepare("UPDATE import_jobs SET offset=?,processed=?,added=?,status=?,error='',"
                      "updated=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE id=?");
            q.addBindValue(complete ? p.total : nextOffset);
            q.addBindValue(p.processed);
            q.addBindValue(p.added);
            q.addBindValue(complete ? "completed" : "running");
            q.addBindValue(id);
            run(q);
            if (complete)
                recordImport(sourceId, initial.canonicalFilePath(), digest, int(p.processed), int(p.added));
            require(db_.commit(), db_.lastError().text());
            transaction = false;
            p.committed = p.processed;
            p.committedBytes = complete ? p.total : nextOffset;
        };
        notify();
        readPgnParallel(
            input, workers,
            [&](Game &&game, uint64_t offset) {
                notify();
                begin();
                linkSource(sourceId, save(game));
                ++p.processed;
                nextOffset = start + qint64(offset);
                p.bytes = std::max(p.bytes, nextOffset);
                if (p.processed - p.committed >= batchSize)
                    commit(false);
                notify();
            },
            [&](uint64_t offset) {
                p.bytes = start + qint64(offset);
                notify();
            });
        require(p.processed > 0, "Aucune partie trouvée dans le PGN.");
        notify();
        commit(true);
    } catch (const Paused &) {
        rollback();
        setStatus("paused", "");
    } catch (const std::exception &e) {
        rollback();
        setStatus("failed", QString::fromUtf8(e.what()));
    }
    return importJob(id);
}
} // namespace t64
