// SPDX-License-Identifier: AGPL-3.0-or-later
#include "cancellation.h"
#include "database.h"
#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <deque>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

namespace t64 {
namespace {
void require(bool ok, const QString &error) {
    if (!ok)
        throw std::runtime_error(error.toStdString());
}
void checkpoint(const RepertoireProgress &progress, qint64 count) {
    require(!progress || progress(count), "Traitement du répertoire annulé.");
}
void run(QSqlQuery &q) {
    if (!q.exec())
        throw std::runtime_error(q.lastError().text().toStdString());
}
QString str(const std::string &v) {
    return QString::fromStdString(v);
}
QString key(const QString &fen) {
    return str(positionKey(fen.toStdString()));
}
QString title(const QString &name) {
    const auto text = name.trimmed();
    require(!text.isEmpty() && text.size() <= 200, "Nom du répertoire : 1 à 200 caractères.");
    return text;
}
} // namespace

void Database::upgradeV6(int originalVersion) {
    if (originalVersion >= 6)
        return;
    if (originalVersion > 0)
        backup(path_ + ".before-v6-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
    sql("SAVEPOINT schema6");
    try {
        sql("CREATE TABLE repertoires(id INTEGER PRIMARY KEY,name TEXT NOT NULL,white INTEGER NOT NULL,"
            "variant INTEGER NOT NULL,root TEXT NOT NULL,root_comment TEXT NOT NULL)");
        sql("CREATE TABLE repertoire_positions(repertoire INTEGER NOT NULL REFERENCES repertoires(id) ON "
            "DELETE CASCADE,key TEXT NOT NULL,fen TEXT NOT NULL,white INTEGER NOT NULL,"
            "due TEXT NOT NULL DEFAULT (date('now','localtime')),interval INTEGER NOT NULL DEFAULT 0,"
            "reviews INTEGER NOT NULL DEFAULT 0,successes INTEGER NOT NULL DEFAULT 0,"
            "PRIMARY KEY(repertoire,key)) WITHOUT ROWID");
        sql("CREATE INDEX repertoire_due ON repertoire_positions(repertoire,due,key)");
        sql("CREATE TABLE repertoire_moves(repertoire INTEGER NOT NULL,key TEXT NOT NULL,uci TEXT NOT NULL,"
            "target TEXT NOT NULL,san TEXT NOT NULL,note TEXT NOT NULL,nags TEXT NOT NULL DEFAULT "
            "'',priority INTEGER NOT NULL,"
            "model INTEGER REFERENCES games(id) ON DELETE SET NULL,PRIMARY KEY(repertoire,key,uci),"
            "FOREIGN KEY(repertoire,key) REFERENCES repertoire_positions(repertoire,key) ON DELETE CASCADE,"
            "FOREIGN KEY(repertoire,target) REFERENCES repertoire_positions(repertoire,key) ON DELETE "
            "CASCADE) "
            "WITHOUT ROWID");
        sql("CREATE INDEX repertoire_targets ON repertoire_moves(repertoire,target)");
        sql("CREATE TABLE repertoire_reviews(id INTEGER PRIMARY KEY,repertoire INTEGER NOT NULL REFERENCES "
            "repertoires(id) ON DELETE CASCADE,key TEXT NOT NULL,success INTEGER NOT NULL,"
            "reviewed TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')))");
        sql("PRAGMA user_version=6");
        sql("RELEASE schema6");
    } catch (...) {
        sql("ROLLBACK TO schema6");
        sql("RELEASE schema6");
        throw;
    }
}

qint64 Database::createRepertoire(const QString &name, bool white, const Game &root,
                                  const RepertoireProgress &progress, qint64 model) {
    checkpoint(progress, 0);
    const auto validated = title(name);
    checkedBoard(root.nodes.at(0).fen, root.chess960);
    sql("SAVEPOINT create_repertoire");
    try {
        QSqlQuery q(db_);
        q.prepare("INSERT INTO repertoires(name,white,variant,root,root_comment) VALUES(?,?,?,?,?)");
        q.addBindValue(validated);
        q.addBindValue(white);
        q.addBindValue(root.chess960);
        q.addBindValue(str(root.nodes[0].fen));
        q.addBindValue(str(root.nodes[0].comment));
        run(q);
        const auto id = q.lastInsertId().toLongLong();
        addToRepertoire(id, root, model, progress);
        sql("RELEASE create_repertoire");
        return id;
    } catch (...) {
        sql("ROLLBACK TO create_repertoire");
        sql("RELEASE create_repertoire");
        throw;
    }
}

QVariantMap Database::repertoire(qint64 id) const {
    QSqlQuery q(db_);
    q.prepare("SELECT name,white,variant,root,root_comment FROM repertoires WHERE id=?");
    q.addBindValue(id);
    run(q);
    require(q.next(), "Répertoire introuvable.");
    return {{"id", id},           {"name", q.value(0)},       {"white", q.value(1)}, {"chess960", q.value(2)},
            {"root", q.value(3)}, {"rootComment", q.value(4)}};
}

QVariantList Database::repertoires() const {
    QSqlQuery q(db_);
    q.prepare("SELECT r.id,r.name,r.white,r.variant,COUNT(p.key),"
              "COALESCE(SUM(p.due<=date('now','localtime')),0),COALESCE(SUM(p.reviews),0),"
              "COALESCE(SUM(p.successes),0) FROM repertoires r LEFT JOIN repertoire_positions p ON "
              "p.repertoire=r.id AND p.white=r.white AND EXISTS(SELECT 1 FROM repertoire_moves m WHERE "
              "m.repertoire=p.repertoire AND m.key=p.key) GROUP BY r.id ORDER BY r.name,r.id");
    run(q);
    QVariantList rows;
    while (q.next())
        rows << QVariantMap{{"id", q.value(0)},       {"name", q.value(1)},      {"white", q.value(2)},
                            {"chess960", q.value(3)}, {"positions", q.value(4)}, {"due", q.value(5)},
                            {"reviews", q.value(6)},  {"successes", q.value(7)}};
    return rows;
}

void Database::renameRepertoire(qint64 id, const QString &name) {
    QSqlQuery q(db_);
    q.prepare("UPDATE repertoires SET name=? WHERE id=?");
    q.addBindValue(title(name));
    q.addBindValue(id);
    run(q);
}
void Database::deleteRepertoire(qint64 id) {
    QSqlQuery q(db_);
    q.prepare("DELETE FROM repertoires WHERE id=?");
    q.addBindValue(id);
    run(q);
}

void Database::addToRepertoire(qint64 id, const Game &game, qint64 model,
                               const RepertoireProgress &progress) {
    checkpoint(progress, 0);
    const auto rep = repertoire(id);
    require(rep["chess960"].toBool() == game.chess960, "Variante incompatible avec ce répertoire.");
    const auto rootKey = key(str(game.nodes.at(0).fen));
    QSqlQuery known(db_);
    known.prepare("SELECT 1 FROM repertoire_positions WHERE repertoire=? AND key=?");
    known.addBindValue(id);
    known.addBindValue(rootKey);
    run(known);
    require(rootKey == key(rep["root"].toString()) || known.next(),
            "L’étude doit commencer à la racine ou à une position déjà présente dans le répertoire.");
    known.finish();
    sql("SAVEPOINT repertoire_merge");
    try {
        // Only reachable nodes: deleted variations remain in the study's backing vector.
        std::vector<int> reachable{0};
        for (size_t i = 0; i < reachable.size(); ++i) {
            checkpoint(progress, i);
            const auto &node = game.nodes.at(reachable[i]);
            QSqlQuery q(db_);
            q.prepare("INSERT OR IGNORE INTO repertoire_positions(repertoire,key,fen,white) VALUES(?,?,?,?)");
            q.addBindValue(id);
            q.addBindValue(key(str(node.fen)));
            q.addBindValue(str(node.fen));
            q.addBindValue(checkedBoard(node.fen, game.chess960).sideToMove() == chess::Color::WHITE);
            run(q);
            require(node.children.size() <= 100000 - reachable.size(),
                    "Répertoire : étude limitée à 100 000 positions.");
            for (int child : node.children)
                reachable.push_back(child);
        }
        for (int parent : reachable) {
            checkpoint(progress, reachable.size());
            const auto position = key(str(game.nodes[parent].fen));
            for (int child : game.nodes[parent].children) {
                checkpoint(progress, reachable.size());
                const auto &node = game.nodes.at(child);
                QSqlQuery q(db_);
                q.prepare(
                    "INSERT INTO repertoire_moves(repertoire,key,uci,target,san,note,nags,priority,model) "
                    "VALUES(?,?,?,?,?,?,?,COALESCE((SELECT MAX(priority)+1 FROM repertoire_moves WHERE "
                    "repertoire=? AND key=?),0),?) ON CONFLICT(repertoire,key,uci) DO UPDATE SET "
                    "model=COALESCE(repertoire_moves.model,excluded.model),"
                    "nags=CASE WHEN repertoire_moves.nags='' THEN excluded.nags ELSE repertoire_moves.nags "
                    "END,"
                    "note=CASE WHEN "
                    "excluded.note='' OR instr(repertoire_moves.note,excluded.note)>0 THEN "
                    "repertoire_moves.note WHEN repertoire_moves.note='' THEN excluded.note ELSE "
                    "repertoire_moves.note||char(10)||excluded.note END");
                q.addBindValue(id);
                q.addBindValue(position);
                q.addBindValue(str(node.uci));
                q.addBindValue(key(str(node.fen)));
                q.addBindValue(str(node.san));
                q.addBindValue(str(node.comment));
                QStringList nags;
                for (int nag : node.nags)
                    nags << QString::number(nag);
                q.addBindValue(nags.isEmpty() ? QString("") : nags.join(' '));
                q.addBindValue(id);
                q.addBindValue(position);
                q.addBindValue(model ? QVariant(model) : QVariant());
                run(q);
            }
        }
        checkpoint(progress, reachable.size());
        sql("RELEASE repertoire_merge");
    } catch (...) {
        sql("ROLLBACK TO repertoire_merge");
        sql("RELEASE repertoire_merge");
        throw;
    }
}

int Database::importRepertoireFile(qint64 id, const QString &path, const RepertoireProgress &progress) {
    checkpoint(progress, 0);
    require(QFileInfo(path).isFile(), "Choisissez un fichier PGN régulier.");
    constexpr qint64 limit = 16 * 1024 * 1024;
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), file.errorString());
    require(file.size() <= limit, "Import de répertoire limité à 16 Mio par fichier.");
    const auto bytes = file.read(limit + 1);
    require(file.error() == QFileDevice::NoError, file.errorString());
    require(bytes.size() <= limit, "Import de répertoire limité à 16 Mio par fichier.");
    return importRepertoirePgn(id, bytes, progress);
}
// Le PGN déjà en mémoire : la couche application s'en sert pour les fichiers compressés,
// qu'elle déplie elle-même — la décompression vit dans t64content, pas ici.
int Database::importRepertoirePgn(qint64 id, const QByteArray &pgn, const RepertoireProgress &progress) {
    checkpoint(progress, 0);
    constexpr int cap = 200;
    std::istringstream input(pgn.toStdString());
    qint64 done = 0;
    int count = 0;
    sql("SAVEPOINT repertoire_file");
    try {
        scanPgnRecords(
            input,
            [&](std::string &&record, uint64_t) {
                auto games = parsePgn(record, 1, [&] { checkpoint(progress, done); });
                if (games.empty())
                    return;
                require(++count <= cap, "Import de répertoire limité à 200 parties par fichier.");
                qint64 processed = 0;
                addToRepertoire(id, games.front(), 0, [&](qint64 positions) {
                    processed = positions;
                    checkpoint(progress, done + positions);
                    return true;
                });
                done += processed;
            },
            [&](uint64_t) { checkpoint(progress, done); });
        require(count > 0, "Aucune partie à ajouter au répertoire.");
        checkpoint(progress, done);
        sql("RELEASE repertoire_file");
        return count;
    } catch (...) {
        sql("ROLLBACK TO repertoire_file");
        sql("RELEASE repertoire_file");
        throw;
    }
}

QVariantMap Database::repertoireOverview(qint64 id, const std::shared_ptr<std::atomic_bool> &canceled) const {
    sql("SAVEPOINT repertoire_view");
    try {
        QVariantMap result;
        {
            // Remove the progress handler before releasing or rolling back the transaction.
            QueryCancellation cancellation(db_, canceled);
            const auto list = repertoires();
            QVariantMap detail;
            for (const auto &value : list) {
                const auto row = value.toMap();
                if (row["id"].toLongLong() != id)
                    continue;
                detail = repertoire(id);
                for (auto it = row.begin(); it != row.end(); ++it)
                    detail[it.key()] = it.value();
                detail["gaps"] = repertoireGaps(id);
                detail["weaknesses"] = repertoireWeaknesses(id);
                detail["gapsLoaded"] = true;
                break;
            }
            require(!canceled || !canceled->load(), "Recherche annulée.");
            result = {{"list", list}, {"detail", detail}};
        }
        sql("RELEASE repertoire_view");
        return result;
    } catch (...) {
        sql("ROLLBACK TO repertoire_view");
        sql("RELEASE repertoire_view");
        throw;
    }
}

void Database::importRepertoire(qint64 id, const std::vector<Game> &games) {
    require(!games.empty(), "Aucune partie à ajouter au répertoire.");
    sql("SAVEPOINT repertoire_import");
    try {
        for (const auto &game : games)
            addToRepertoire(id, game);
        sql("RELEASE repertoire_import");
    } catch (...) {
        sql("ROLLBACK TO repertoire_import");
        sql("RELEASE repertoire_import");
        throw;
    }
}

QVariantList Database::repertoireMoves(qint64 id, const QString &fen) const {
    QSqlQuery q(db_);
    q.prepare(
        "SELECT m.uci,m.san,m.note,m.priority,COALESCE(m.model,0),p.fen,m.nags FROM repertoire_moves m JOIN "
        "repertoire_positions p ON p.repertoire=m.repertoire AND p.key=m.target WHERE m.repertoire=? "
        "AND m.key=? ORDER BY m.priority,m.uci");
    q.addBindValue(id);
    q.addBindValue(key(fen));
    run(q);
    QVariantList rows;
    while (q.next())
        rows << QVariantMap{{"uci", q.value(0)},      {"san", q.value(1)},   {"note", q.value(2)},
                            {"priority", q.value(3)}, {"model", q.value(4)}, {"targetFen", q.value(5)},
                            {"nags", q.value(6)}};
    return rows;
}

void Database::editRepertoireMove(qint64 id, const QString &fen, const QString &uci, const QString &note,
                                  bool principal) {
    require(note.size() <= 100000, "Note limitée à 100 000 caractères.");
    sql("SAVEPOINT edit_repertoire");
    try {
        QSqlQuery q(db_);
        q.prepare("UPDATE repertoire_moves SET note=? WHERE repertoire=? AND key=? AND uci=?");
        q.addBindValue(note.isNull() ? QString("") : note);
        q.addBindValue(id);
        q.addBindValue(key(fen));
        q.addBindValue(uci);
        run(q);
        require(q.numRowsAffected() == 1, "Coup absent du répertoire.");
        if (principal) {
            q.prepare("UPDATE repertoire_moves SET priority=CASE WHEN uci=? THEN 0 ELSE priority+1 END "
                      "WHERE repertoire=? AND key=?");
            q.addBindValue(uci);
            q.addBindValue(id);
            q.addBindValue(key(fen));
            run(q);
        }
        sql("RELEASE edit_repertoire");
    } catch (...) {
        sql("ROLLBACK TO edit_repertoire");
        sql("RELEASE edit_repertoire");
        throw;
    }
}

void Database::removeRepertoireMove(qint64 id, const QString &fen, const QString &uci) {
    const auto rep = repertoire(id);
    sql("SAVEPOINT remove_repertoire_move");
    try {
        QSqlQuery q(db_);
        q.prepare("DELETE FROM repertoire_moves WHERE repertoire=? AND key=? AND uci=?");
        q.addBindValue(id);
        q.addBindValue(key(fen));
        q.addBindValue(uci);
        run(q);
        // A position reached by a different move order must remain available and keep its reviews.
        q.prepare("WITH RECURSIVE reachable(key) AS (SELECT ? UNION SELECT m.target FROM repertoire_moves m "
                  "JOIN reachable r ON r.key=m.key WHERE m.repertoire=?) DELETE FROM repertoire_positions "
                  "WHERE repertoire=? AND key NOT IN(SELECT key FROM reachable)");
        q.addBindValue(key(rep["root"].toString()));
        q.addBindValue(id);
        q.addBindValue(id);
        run(q);
        sql("RELEASE remove_repertoire_move");
    } catch (...) {
        sql("ROLLBACK TO remove_repertoire_move");
        sql("RELEASE remove_repertoire_move");
        throw;
    }
}

Game Database::exportRepertoire(qint64 id, const RepertoireProgress &progress) const {
    checkpoint(progress, 0);
    sql("SAVEPOINT repertoire_export");
    try {
        const auto rep = repertoire(id);
        Game game(rep["root"].toString().toStdString(), rep["chess960"].toBool());
        game.tags["Event"] = rep["name"].toString().toStdString();
        game.tags["Trait64RepertoireSide"] = rep["white"].toBool() ? "White" : "Black";
        game.tags["Result"] = "*";
        game.tags["Trait64RepertoireId"] = std::to_string(id);
        game.nodes[0].comment = rep["rootComment"].toString().toStdString();
        std::deque<int> pending{0};
        std::set<std::string> expanded;
        while (!pending.empty()) {
            checkpoint(progress, game.nodes.size());
            const auto parent = pending.front();
            pending.pop_front();
            const auto position = game.nodes[parent].fen;
            if (!expanded.insert(positionKey(position)).second) {
                game.nodes[parent].comment +=
                    "\nTransposition : suite développée à la même position dans ce PGN.";
                continue;
            }
            for (const auto &value : repertoireMoves(id, str(position))) {
                checkpoint(progress, game.nodes.size());
                const auto move = value.toMap();
                const auto child = game.play(parent, move["uci"].toString().toStdString());
                game.nodes[child].comment = move["note"].toString().toStdString();
                for (const auto &nag : move["nags"].toString().split(' ', Qt::SkipEmptyParts))
                    game.nodes[child].nags.push_back(nag.toInt());
                pending.push_back(child);
            }
        }
        checkpoint(progress, game.nodes.size());
        sql("RELEASE repertoire_export");
        return game;
    } catch (...) {
        sql("ROLLBACK TO repertoire_export");
        sql("RELEASE repertoire_export");
        throw;
    }
}

QVariantMap Database::nextRepertoirePosition(qint64 id, bool includeFuture) const {
    const auto rep = repertoire(id);
    QSqlQuery q(db_);
    q.prepare("SELECT p.fen,p.due,p.reviews,p.successes,p.interval FROM repertoire_positions p JOIN "
              "repertoires r ON r.id=p.repertoire WHERE p.repertoire=? AND p.white=r.white AND "
              "EXISTS(SELECT 1 FROM repertoire_moves m WHERE m.repertoire=p.repertoire AND m.key=p.key) "
              "AND (? OR p.due<=date('now','localtime')) ORDER BY p.due,p.reviews,p.key LIMIT 1");
    q.addBindValue(id);
    q.addBindValue(includeFuture);
    run(q);
    if (!q.next())
        return {};
    auto card = QVariantMap{{"repertoire", id},        {"name", rep["name"]},   {"chess960", rep["chess960"]},
                            {"fen", q.value(0)},       {"due", q.value(1)},     {"reviews", q.value(2)},
                            {"successes", q.value(3)}, {"interval", q.value(4)}};
    card["answers"] = repertoireMoves(id, card["fen"].toString());
    return card;
}

void Database::reviewRepertoire(qint64 id, const QString &fen, bool success) {
    sql("SAVEPOINT repertoire_review");
    try {
        QSqlQuery q(db_);
        q.prepare("SELECT p.interval FROM repertoire_positions p JOIN repertoires r ON r.id=p.repertoire "
                  "WHERE p.repertoire=? AND p.key=? AND p.white=r.white AND EXISTS(SELECT 1 FROM "
                  "repertoire_moves m WHERE m.repertoire=p.repertoire AND m.key=p.key)");
        q.addBindValue(id);
        q.addBindValue(key(fen));
        run(q);
        require(q.next(), "Position d’entraînement introuvable.");
        const int previous = q.value(0).toInt();
        const int interval = success ? (previous == 0   ? 1
                                        : previous == 1 ? 3
                                                        : std::min(365, previous * 2))
                                     : 1;
        q.finish();
        q.prepare("UPDATE repertoire_positions SET due=?,interval=?,reviews=reviews+1,successes=successes+? "
                  "WHERE repertoire=? AND key=?");
        q.addBindValue(QDate::currentDate().addDays(interval).toString(Qt::ISODate));
        q.addBindValue(interval);
        q.addBindValue(success ? 1 : 0);
        q.addBindValue(id);
        q.addBindValue(key(fen));
        run(q);
        q.prepare("INSERT INTO repertoire_reviews(repertoire,key,success) VALUES(?,?,?)");
        q.addBindValue(id);
        q.addBindValue(key(fen));
        q.addBindValue(success);
        run(q);
        sql("RELEASE repertoire_review");
    } catch (...) {
        sql("ROLLBACK TO repertoire_review");
        sql("RELEASE repertoire_review");
        throw;
    }
}

QVariantList Database::repertoireWeaknesses(qint64 id) const {
    QSqlQuery q(db_);
    q.prepare("SELECT fen,reviews,successes,due,interval FROM repertoire_positions WHERE repertoire=? "
              "AND reviews>successes ORDER BY 1.0*successes/reviews,reviews DESC,key LIMIT 100");
    q.addBindValue(id);
    run(q);
    QVariantList rows;
    while (q.next())
        rows << QVariantMap{{"fen", q.value(0)},
                            {"reviews", q.value(1)},
                            {"successes", q.value(2)},
                            {"due", q.value(3)},
                            {"interval", q.value(4)}};
    return rows;
}

QVariantList Database::repertoireGaps(qint64 id) const {
    // Only opponent turns. Statistics describe the local reference corpus, not theoretical truth.
    // Repertoires keep their text key; the opening statistics are keyed by position hash.
    QSqlQuery positions(db_), stats(db_);
    positions.prepare(
        "SELECT p.key,p.fen FROM repertoire_positions p JOIN repertoires r ON r.id=p.repertoire "
        "WHERE p.repertoire=? AND p.white<>r.white");
    positions.addBindValue(id);
    run(positions);
    stats.prepare(
        "SELECT o.uci,o.san,o.count,o.white,o.draw,o.black FROM opening_stats o WHERE o.key=? AND "
        "o.count>0 AND NOT EXISTS(SELECT 1 FROM repertoire_moves m WHERE m.repertoire=? AND m.key=? "
        "AND m.uci=o.uci)");
    struct Gap {
        QString key, fen, uci, san;
        qint64 count, white, draw, black;
    };
    std::vector<Gap> gaps;
    while (positions.next()) {
        const auto key = positions.value(0).toString(), fen = positions.value(1).toString();
        stats.bindValue(0, hashKey(fen.toStdString()));
        stats.bindValue(1, id);
        stats.bindValue(2, key);
        run(stats);
        while (stats.next())
            gaps.push_back({key, fen, stats.value(0).toString(), stats.value(1).toString(),
                            stats.value(2).toLongLong(), stats.value(3).toLongLong(),
                            stats.value(4).toLongLong(), stats.value(5).toLongLong()});
    }
    std::sort(gaps.begin(), gaps.end(), [](const Gap &a, const Gap &b) {
        return std::tie(b.count, a.key, a.uci) < std::tie(a.count, b.key, b.uci);
    });
    QVariantList rows;
    for (size_t i = 0; i < gaps.size() && i < 100; ++i)
        rows << QVariantMap{{"fen", gaps[i].fen},     {"uci", gaps[i].uci},     {"san", gaps[i].san},
                            {"count", gaps[i].count}, {"white", gaps[i].white}, {"draw", gaps[i].draw},
                            {"black", gaps[i].black}};
    return rows;
}
} // namespace t64
