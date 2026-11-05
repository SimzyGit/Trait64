// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include "hardware.h"
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
                    emit message(tr("Ce moteur possède déjà un profil. Vous pouvez en mémoriser d’autres dans les paramètres."));
                    return;
                }
            profiles << QVariantMap{{"name", item["title"].toString() + " " + item["version"].toString()},
                                    {"enginePath", path},
                                    {"threads", t64::recommendedThreads()},
                                    {"hash", t64::recommendedHashMb()},
                                    {"multiPV", 3},
                                    {"skill", 12},
                                    {"moveTime", 1000},
                                    {"analysisDepth", 24},
                                    {"syzygyPath", ""},
                                    {"uciOptions", QVariantMap()}};
            settings_.setValue("engineProfiles", profiles);
            emit preferencesChanged();
            emit message(tr("Profil ajouté : ") + item[tr("title")].toString() +
                         tr(". Ouvrez les paramètres pour le charger."));
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
        emit message(tr("Base exportée en PGN."));
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
    bool wholeFamily = aliases.contains(name);
    for (const auto &english : aliases)
        wholeFamily = wholeFamily || name == english || name == english + " defense" ||
                      name == english + " defence";
    name = aliases.value(name, name);
    QSet<QString> codes;
    // La variable de boucle s'appelait « family » comme le drapeau ci-dessus, qu'elle
    // masquait : le test plus bas lisait donc un pointeur toujours non nul, et prenait
    // toujours la branche « commence par ». Chercher un fragment de variante — Najdorf,
    // par exemple — ne rendait rien, aucune ouverture ne commençant par ce mot.
    for (const auto *volume : {"a", "b", "c", "d", "e"}) {
        QFile file(":/assets/openings/" + QString(volume) + ".tsv");
        if (!file.open(QIODevice::ReadOnly))
            continue;
        while (!file.atEnd()) {
            const auto columns = QString::fromUtf8(file.readLine()).split('\t');
            // A family belongs at the start of the name: "Franco-Sicilian"
            // is a French line, not the Sicilian family. Variant fragments
            // (e.g. Najdorf) still search anywhere in the complete name.
            if (columns.size() >= 2 && (wholeFamily ? columns[1].startsWith(name, Qt::CaseInsensitive)
                                                    : columns[1].contains(name, Qt::CaseInsensitive)))
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
    requestLibrarySearch();
}
