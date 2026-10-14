// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtConcurrent>
namespace {
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
         settings_.value("backup/last").toDateTime().isValid() &&
             settings_.value("backup/last").toDateTime().secsTo(QDateTime::currentDateTimeUtc()) < 6 * 3600))
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
