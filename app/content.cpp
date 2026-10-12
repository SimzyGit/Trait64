// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

void Controller::initContent() {
    content_ = std::make_unique<t64::Content>(dataPath_ + "/content");
    connect(content_.get(), &t64::Content::ready, this, [this](const QString &path, const QVariantMap &item) {
        if (item["kind"] == "engine") {
            auto profiles = settings_.value("engineProfiles").toList();
            for (const auto &profile : profiles)
                if (profile.toMap()["enginePath"] == path) {
                    emit message("Ce moteur possède déjà un profil. Vous pouvez en mémoriser d’autres dans "
                                 "les paramètres.");
                    return;
                }
            profiles << QVariantMap{{"name", item["title"].toString() + " " + item["version"].toString()},
                                    {"enginePath", path},
                                    {"threads", 1},
                                    {"hash", 128},
                                    {"multiPV", 3},
                                    {"skill", 12},
                                    {"moveTime", 1000},
                                    {"analysisDepth", 24},
                                    {"syzygyPath", ""},
                                    {"uciOptions", QVariantMap()}};
            settings_.setValue("engineProfiles", profiles);
            emit preferencesChanged();
            emit message("Profil ajouté : " + item["title"].toString() +
                         ". Ouvrez les paramètres pour le charger.");
        } else
            importSourceFile(path, item);
    });
    connect(content_.get(), &t64::Content::aboutToRemove, this, [this](const QString &path) {
        if (settings_.value("enginePath").toString() == path) {
            stopPlaying();
            cancelReview();
            engine_.shutdown();
            setPreference("enginePath", "");
        }
        comparison_.stop();
        emit preferencesChanged();
    });
}
void Controller::exportSource(const QString &url, qint64 source) {
    try {
        const auto path = localPath(url);
        db_->validateOutputPath(path);
        QSaveFile out(path);
        if (!out.open(QIODevice::WriteOnly))
            throw std::runtime_error(out.errorString().toStdString());
        db_->exportSource(out, source);
        if (!out.commit())
            throw std::runtime_error(out.errorString().toStdString());
        emit message("Base exportée en PGN.");
    } catch (const std::exception &e) {
        fail(e);
    }
}
QVariantMap Controller::openingFilters(const QVariantMap &filters) const {
    auto result = filters;
    auto name = filters.value("openingName").toString().trimmed().toLower();
    result.remove("openingCodes");
    result.remove("openingRequested");
    if (name.isEmpty())
        return result;
    const QMap<QString, QString> aliases{{"sicilienne", "sicilian"},
                                         {"française", "french"},
                                         {"francaise", "french"},
                                         {"espagnole", "ruy lopez"},
                                         {"italienne", "italian"},
                                         {"anglaise", "english"},
                                         {"scandinave", "scandinavian"},
                                         {"écossaise", "scotch"},
                                         {"ecossaise", "scotch"},
                                         {"gambit dame", "queen's gambit"},
                                         {"gambit roi", "king's gambit"},
                                         {"est-indienne", "king's indian"},
                                         {"ouest-indienne", "queen's indian"},
                                         {"nimzo-indienne", "nimzo-indian"},
                                         {"catalane", "catalan"},
                                         {"slave", "slav"}};
    name = aliases.value(name, name);
    QSet<QString> codes;
    for (const auto *family : {"a", "b", "c", "d", "e"}) {
        QFile file(":/assets/openings/" + QString(family) + ".tsv");
        if (!file.open(QIODevice::ReadOnly))
            continue;
        while (!file.atEnd()) {
            const auto columns = QString::fromUtf8(file.readLine()).split('\t');
            if (columns.size() >= 2 && columns[1].contains(name, Qt::CaseInsensitive))
                codes.insert(columns[0]);
        }
    }
    result["openingCodes"] = QStringList(codes.begin(), codes.end());
    result["openingRequested"] = true;
    return result;
}
void Controller::searchLibrary(const QString &query, const QString &result, bool favorite, bool position,
                               bool trash, const QVariantMap &filters) {
    query_ = query;
    resultFilter_ = result;
    favoritesOnly_ = favorite;
    positionFilter_ = position;
    trashFilter_ = trash;
    advancedFilters_ = openingFilters(filters);
    refreshLibrary();
}
