// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent>
namespace {
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
void copy(const QString &src, const QString &dst) {
    QFile in(src);
    QSaveFile out(dst);
    if (!in.exists())
        return;
    if (!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly))
        throw std::runtime_error("Sauvegarde de session impossible.");
    while (!in.atEnd()) {
        auto b = in.read(1024 * 1024);
        if ((b.isEmpty() && in.error() != QFileDevice::NoError) || out.write(b) != b.size())
            throw std::runtime_error("Sauvegarde interrompue.");
    }
    if (!out.commit())
        throw std::runtime_error("Validation de sauvegarde impossible.");
}
} // namespace
void Controller::initReliability() {
    updater_ = std::make_unique<t64::Updater>(dataPath_, this);
    updater_->beforeInstall = [this] {
        if (busy_ || backupWorker_.isRunning() || clockRunning_)
            throw std::runtime_error("Terminez l’import, la sauvegarde ou la partie avant la mise à jour.");
        if (trainingActive())
            finishTraining();
        writeSession();
        const QString dir = dataPath_ + "/backups/pre-update-" +
                            QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmsszzz");
        if (!QDir().mkpath(dir))
            throw std::runtime_error("Impossible de créer la sauvegarde avant mise à jour.");
        db_->backup(dir + "/library.sqlite");
        copy(sessionPath_, dir + "/session.json");
        QJsonObject prefs = QJsonObject::fromVariantMap(preferences());
        QSaveFile file(dir + "/preferences.json");
        const auto bytes = QJsonDocument(prefs).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            throw std::runtime_error("Impossible de sauvegarder les préférences.");
        return dir;
    };
    connect(&backupWorker_, &QFutureWatcher<QString>::finished, this, [this] {
        const auto error = backupWorker_.result();
        if (!error.isEmpty())
            emit message(error, true);
        else {
            settings_.setValue("backup/last", QDateTime::currentDateTimeUtc());
            emit preferencesChanged();
        }
    });
    backupTimer_.setInterval(30 * 60 * 1000);
    connect(&backupTimer_, &QTimer::timeout, this, [this] { automaticBackup(false); });
}
void Controller::startBackgroundServices() {
    backupTimer_.start();
    QTimer::singleShot(1200, this, [this] { automaticBackup(false); });
    updater_->startAutomaticCheck();
}
void Controller::automaticBackup(bool force) {
    if (busy_ || backupWorker_.isRunning() || trainingActive() || clockRunning_)
        return;
    if (!force &&
        (!settings_.value("autoBackup", true).toBool() ||
         (settings_.value("backup/last").toDateTime().isValid() &&
          settings_.value("backup/last").toDateTime().secsTo(QDateTime::currentDateTimeUtc()) < 6 * 3600)))
        return;
    try {
        writeSession();
        const QString root = dataPath_ + "/backups", source = db_->path(), session = sessionPath_;
        const QString folder =
            root + "/auto-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmsszzz");
        if (!QDir().mkpath(folder))
            throw std::runtime_error("Dossier de sauvegarde inaccessible.");
        copy(session, folder + "/session.json");
        backupWorker_.setFuture(QtConcurrent::run([source, root, folder] {
            try {
                t64::Database db(source);
                db.backup(folder + "/library.sqlite");
                auto dirs = QDir(root).entryList({"auto-*"}, QDir::Dirs | QDir::NoDotAndDotDot,
                                                 QDir::Name | QDir::Reversed);
                // Keep 14 completed automatic snapshots; never prune pre-update or migration copies.
                for (int i = 14; i < dirs.size(); ++i)
                    QDir(root + "/" + dirs[i]).removeRecursively();
                return QString();
            } catch (const std::exception &e) {
                QDir(folder).removeRecursively();
                return QString::fromUtf8(e.what());
            }
        }));
    } catch (const std::exception &e) {
        fail(e);
    }
}

QVariantList Controller::backups() const {
    struct Found {
        QString file, kind;
        QDateTime when;
    };
    std::vector<Found> found;
    const QString root = dataPath_ + "/backups";
    const QList<QPair<QString, QString>> kinds{
        {"auto-", "auto"}, {"pre-update-", "update"}, {"before-restore-", "restore"}};
    for (const auto &folder : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QFileInfo library(folder.absoluteFilePath() + "/library.sqlite");
        if (!library.isFile())
            continue;
        for (const auto &[prefix, kind] : kinds)
            if (folder.fileName().startsWith(prefix))
                found.push_back({library.absoluteFilePath(), kind, library.lastModified()});
    }
    // Les copies faites avant une migration de schéma, posées à côté de la bibliothèque.
    const QFileInfo current(db_->path());
    for (const auto &file : current.dir().entryInfoList({current.fileName() + ".before-*"}, QDir::Files))
        if (!file.fileName().endsWith(".tmp"))
            found.push_back({file.absoluteFilePath(), "migration", file.lastModified()});
    std::sort(found.begin(), found.end(), [](const Found &a, const Found &b) { return a.when > b.when; });
    QVariantList rows;
    for (const auto &item : found) {
        // Lue en lecture seule : ouvrir une ancienne sauvegarde avec la bibliothèque la
        // migrerait sur place, et elle ne serait plus ce qu'elle était.
        qint64 games = -1;
        const auto name = "backup-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", name);
            db.setDatabaseName(item.file);
            db.setConnectOptions("QSQLITE_OPEN_READONLY");
            if (db.open()) {
                QSqlQuery q(db);
                if (q.exec("SELECT COUNT(*) FROM games WHERE deleted=0") && q.next())
                    games = q.value(0).toLongLong();
            }
        }
        QSqlDatabase::removeDatabase(name);
        rows << QVariantMap{{"file", item.file},
                            {"kind", item.kind},
                            {"date", item.when.toLocalTime().toString("dd/MM/yyyy HH:mm")},
                            {"size", QFileInfo(item.file).size()},
                            {"games", games}};
    }
    return rows;
}

void Controller::restoreBackup(const QString &file) {
    try {
        require(!busy_ && !trainingActive() && !backupWorker_.isRunning() &&
                    !analysisQueue_.state()["running"].toBool() && !repertoireReadActive_,
                "Attendez la fin des traitements en cours avant de restaurer.");
        const auto known = backups();
        require(std::any_of(known.begin(), known.end(),
                            [&](const QVariant &row) { return row.toMap()["file"] == file; }),
                "Seules les sauvegardes de la liste peuvent être restaurées.");
        checkpoint();
        writeSession();
        const QString target = db_->path();
        const QString safety = dataPath_ + "/backups/before-restore-" +
                               QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmsszzz");
        require(QDir().mkpath(safety), "Dossier de sauvegarde inaccessible.");
        db_->backup(safety + "/library.sqlite");
        // La sauvegarde est copiée puis ouverte à côté : une ancienne version y est migrée
        // et vérifiée, sans toucher ni au fichier d'origine ni à la bibliothèque en place.
        const QString staged = target + ".restore.tmp";
        QFile::remove(staged);
        copy(file, staged);
        {
            t64::Database candidate(staged);
            require(candidate.integrity() == "ok", "Cette sauvegarde est endommagée : rien n’a été changé.");
        }
        db_.reset();
        for (const char *suffix : {"", "-wal", "-shm"})
            QFile::remove(target + suffix);
        if (!QFile::rename(staged, target)) {
            // Le remplacement a échoué à mi-chemin : la bibliothèque d'avant revient.
            copy(safety + "/library.sqlite", target);
            db_ = std::make_unique<t64::Database>(target);
            throw std::runtime_error("Restauration impossible : la bibliothèque d’avant est conservée.");
        }
        db_ = std::make_unique<t64::Database>(target);
        openBackup(QUrl::fromLocalFile(target).toString());
        emit message(
            tr("Sauvegarde restaurée. La bibliothèque remplacée est gardée dans la liste des sauvegardes."));
    } catch (const std::exception &e) {
        if (!db_)
            db_ = std::make_unique<t64::Database>(dataPath_ + "/library.sqlite");
        fail(e);
    }
}

QString Controller::userGuide() const {
    QFile file(translator_->english() ? ":/assets/guide/en.md" : ":/assets/guide/fr.md");
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}
bool Controller::firstLaunch() {
    if (settings_.value("guide/offered").toBool())
        return false;
    settings_.setValue("guide/offered", true);
    return true;
}
