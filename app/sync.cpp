// SPDX-License-Identifier: AGPL-3.0-or-later
// Synchronisation entre appareils sans serveur : un paquet chiffré, « trait64-sync.t64z »,
// dans un dossier que l'utilisateur synchronise lui-même. Chaque passage fusionne d'abord
// ce qu'un autre appareil y a déposé (répertoires et études ajoutés sans doublon, profils,
// comptes et dispositions réunis par nom, réglages repris), puis réécrit le paquet avec le
// tout. Rien de la bibliothèque n'est jamais retiré par une synchronisation.
#include "controller.h"
#include "syncbundle.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUuid>
#include <QtConcurrent>

namespace {
const char *bundleName = "trait64-sync.t64z";
// Les réglages qui suivent l'utilisateur d'un appareil à l'autre ; les chemins (moteur,
// tables, livre) restent propres à chaque machine.
const QStringList syncedPreferences{
    "fontFamily",  "fontScale",        "uiScale",    "sidebarCompact", "highContrast",      "reduceMotion",
    "lightTheme",  "pieceStyle",       "boardLight", "boardDark",      "boardHighlight",    "backgroundColor",
    "accentColor", "coordinates",      "legalMoves", "lastMove",       "engineArrows",      "showHanging",
    "showAttacks", "assistedAnalysis", "sounds",     "soundVolume",    "notebookLayout",    "figurines",
    "language",    "multiPV",          "dailyGoal",  "dockLayout",     "analysisPanelRatio"};
QByteArray json(const QVariant &value) {
    return QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact);
}
QVariant parsed(const QByteArray &bytes) {
    return QJsonDocument::fromJson(bytes).toVariant();
}
// Deux listes réunies par une clé : l'élément local garde la main, l'autre comble.
QVariantList unite(const QVariantList &local, const QVariantList &remote,
                   const std::function<QString(const QVariantMap &)> &key) {
    QVariantList out = local;
    QSet<QString> known;
    for (const auto &row : local)
        known.insert(key(row.toMap()));
    for (const auto &row : remote)
        if (!known.contains(key(row.toMap())))
            out << row;
    return out;
}
} // namespace

QVariantMap Controller::syncState() const {
    return {{"folder", settings_.value("sync/folder").toString()},
            {"ready", !settings_.value("sync/folder").toString().isEmpty() &&
                          !settings_.value("sync/passphrase").toString().isEmpty()},
            {"last", settings_.value("sync/last", 0)},
            {"atStartup", settings_.value("sync/atStartup", false)},
            {"running", syncWorker_.isRunning()},
            {"status", syncStatus_}};
}

void Controller::configureSync(const QString &folderUrl, const QString &passphrase, bool atStartup) {
    const auto folder = localPath(folderUrl);
    if (!QFileInfo(folder).isDir()) {
        emit message(tr("Choisissez un dossier existant."), true);
        return;
    }
    if (passphrase.size() < 8) {
        emit message(tr("La phrase de passe doit compter au moins huit caractères."), true);
        return;
    }
    settings_.setValue("sync/folder", folder);
    // Gardée sur cet appareil seulement : c'est le dossier partagé qu'elle protège.
    settings_.setValue("sync/passphrase", passphrase);
    settings_.setValue("sync/atStartup", atStartup);
    if (settings_.value("sync/device").toString().isEmpty())
        settings_.setValue("sync/device", QUuid::createUuid().toString(QUuid::WithoutBraces));
    emit syncChanged();
}

void Controller::forgetSync() {
    for (const auto *key :
         {"sync/folder", "sync/passphrase", "sync/atStartup", "sync/lastApplied", "sync/last"})
        settings_.remove(key);
    syncStatus_.clear();
    emit syncChanged();
}

void Controller::syncNow() {
    if (syncWorker_.isRunning() || busy())
        return;
    const auto state = syncState();
    if (!state["ready"].toBool()) {
        emit message(tr("Réglez d’abord le dossier et la phrase de passe."), true);
        return;
    }
    try {
        checkpoint();
    } catch (const std::exception &e) {
        fail(e);
        return;
    }
    // Ce que l'on enverra de cet appareil, lu ici : les réglages ne se lisent pas d'un autre fil.
    QVariantMap local;
    for (const auto &key : syncedPreferences)
        if (settings_.contains(key))
            local[key] = settings_.value(key);
    QVariantList accounts;
    for (const auto &row : settings_.value("onlineAccounts").toList()) {
        auto account = row.toMap();
        account.remove("token"); // un jeton ne quitte jamais l'appareil
        account.remove("syncing");
        accounts << account;
    }
    local["onlineAccounts"] = accounts;
    local["profiles"] = settings_.value("profiles").toList();
    local["layouts"] = settings_.value("layouts").toList();
    const auto path = db_->path(), folder = state["folder"].toString(),
               passphrase = settings_.value("sync/passphrase").toString(),
               device = settings_.value("sync/device").toString();
    const qint64 lastApplied = settings_.value("sync/lastApplied", 0).toLongLong();
    syncStatus_ = tr("Synchronisation…");
    busy_ = true;
    emit busyChanged();
    emit syncChanged();
    if (!syncWorker_.property("connected").toBool()) {
        syncWorker_.setProperty("connected", true);
        connect(&syncWorker_, &QFutureWatcher<QVariantMap>::finished, this, [this] {
            const auto r = syncWorker_.result();
            busy_ = false;
            emit busyChanged();
            if (r.contains("error")) {
                syncStatus_ = r["error"].toString();
                emit message(tr(syncStatus_.toUtf8().constData()), true);
            } else {
                if (r["applied"].toBool()) {
                    settings_.setValue("sync/lastApplied", r["remoteStamp"]);
                    applySyncSettings(r["remote"].toMap());
                }
                settings_.setValue("sync/last", QDateTime::currentMSecsSinceEpoch());
                syncStatus_ =
                    r["applied"].toBool()
                        ? tr("Synchronisé : %1 répertoire(s) et %2 recueil(s) reçus de l’autre appareil.")
                              .arg(r["repertoires"].toInt())
                              .arg(r["studies"].toInt())
                        : tr("Synchronisé : rien de nouveau à recevoir ; le paquet de cet appareil est "
                             "déposé.");
            }
            refreshLibrary();
            refreshRepertoires();
            emit syncChanged();
        });
    }
    syncWorker_.setFuture(QtConcurrent::run([=]() -> QVariantMap {
        try {
            t64::Database db(path);
            const auto bundle = QDir(folder).filePath(bundleName);
            QVariantMap out{{"applied", false}};
            QVariantMap remoteSettings;
            if (QFileInfo::exists(bundle)) {
                QMap<QString, QByteArray> files;
                if (const auto error = t64::readSyncBundle(bundle, passphrase, &files); !error.isEmpty())
                    return {{"error", error}}; // jamais réécrire un paquet qu'on n'a pas su lire
                const auto manifest = parsed(files.value("manifest.json")).toMap();
                const qint64 stamp = manifest["stamp"].toLongLong();
                if (manifest["device"].toString() != device && stamp > lastApplied) {
                    int repertoires = 0, studies = 0;
                    for (const auto &row : parsed(files.value("repertoires.json")).toList()) {
                        const auto r = row.toMap();
                        qint64 id = 0;
                        for (const auto &mine : db.repertoires())
                            if (mine.toMap()["name"] == r["name"] &&
                                mine.toMap()["white"].toBool() == r["white"].toBool())
                                id = mine.toMap()["id"].toLongLong();
                        if (!id)
                            id = db.createRepertoire(r["name"].toString(), r["white"].toBool(), t64::Game());
                        db.importRepertoirePgn(id, files.value(r["file"].toString()));
                        db.setTags("repertoires", id, r["tags"].toStringList());
                        ++repertoires;
                    }
                    QTemporaryDir temp;
                    for (const auto &row : parsed(files.value("studies.json")).toList()) {
                        const auto st = row.toMap();
                        QFile pgn(temp.filePath("study.pgn"));
                        if (!pgn.open(QIODevice::WriteOnly) ||
                            pgn.write(files.value(st["file"].toString())) < 0)
                            continue;
                        pgn.close();
                        db.importPgnFile(pgn.fileName(), {},
                                         {{"id", st["slug"]},
                                          {"title", st["title"]},
                                          {"kind", st["kind"]},
                                          {"purpose", st["purpose"]},
                                          {"collection", st["title"]}});
                        for (const auto &mine : db.sources())
                            if (mine.toMap()["slug"] == st["slug"])
                                db.setTags("sources", mine.toMap()["id"].toLongLong(),
                                           mine.toMap()["tags"].toStringList() + st["tags"].toStringList());
                        ++studies;
                    }
                    remoteSettings = parsed(files.value("settings.json")).toMap();
                    out = {{"applied", true},
                           {"remote", remoteSettings},
                           {"remoteStamp", stamp},
                           {"repertoires", repertoires},
                           {"studies", studies}};
                }
            }
            // Le paquet réécrit : tout ce que cet appareil a maintenant, réunions comprises.
            QMap<QString, QByteArray> files;
            QVariantList repertoires, studies;
            int n = 0;
            for (const auto &row : db.repertoires()) {
                const auto r = row.toMap();
                const auto file = QString("repertoires/%1.pgn").arg(++n);
                files.insert(file,
                             QByteArray::fromStdString(db.exportRepertoire(r["id"].toLongLong()).pgn()));
                repertoires << QVariantMap{
                    {"file", file}, {"name", r["name"]}, {"white", r["white"]}, {"tags", r["tags"]}};
            }
            n = 0;
            for (const auto &row : db.sources()) {
                const auto st = row.toMap();
                if (st["kind"] != "study" && st["kind"] != "import")
                    continue;
                QBuffer buffer;
                buffer.open(QIODevice::WriteOnly);
                db.exportSource(buffer, st["id"].toLongLong());
                const auto file = QString("studies/%1.pgn").arg(++n);
                files.insert(file, buffer.data());
                studies << QVariantMap{
                    {"file", file},       {"slug", st["slug"]},       {"title", st["title"]},
                    {"kind", st["kind"]}, {"purpose", st["purpose"]}, {"tags", st["tags"]}};
            }
            auto settings = local;
            for (auto it = remoteSettings.cbegin(); it != remoteSettings.cend(); ++it)
                if (!settings.contains(it.key()))
                    settings.insert(it.key(), it.value());
            files.insert("repertoires.json", json(repertoires));
            files.insert("studies.json", json(studies));
            files.insert("settings.json", json(settings));
            files.insert("manifest.json", json(QVariantMap{{"format", 1},
                                                           {"device", device},
                                                           {"stamp", QDateTime::currentMSecsSinceEpoch()}}));
            if (const auto error = t64::writeSyncBundle(bundle, files, passphrase); !error.isEmpty())
                return {{"error", error}};
            return out;
        } catch (const std::exception &e) {
            return {{"error", QString::fromUtf8(e.what())}};
        }
    }));
}

void Controller::applySyncSettings(const QVariantMap &remote) {
    for (const auto &key : syncedPreferences)
        if (remote.contains(key))
            setPreference(key, remote[key]);
    const auto byName = [](const QVariantMap &row) { return row["name"].toString().toLower(); };
    settings_.setValue("profiles",
                       unite(settings_.value("profiles").toList(), remote["profiles"].toList(), byName));
    settings_.setValue("layouts",
                       unite(settings_.value("layouts").toList(), remote["layouts"].toList(), byName));
    settings_.setValue("onlineAccounts", unite(settings_.value("onlineAccounts").toList(),
                                               remote["onlineAccounts"].toList(), [](const QVariantMap &row) {
                                                   return row["site"].toString() + "/" +
                                                          row["user"].toString().toLower();
                                               }));
    emit profilesChanged();
    emit accountsChanged();
    emit layoutsChanged();
}
