// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include "hardware.h"
#include "syzygy.h"
#include "syzygy_download.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStorageInfo>

void Controller::initContent() {
    content_ = std::make_unique<t64::Content>(dataPath_ + "/content");
    connect(content_.get(), &t64::Content::ready, this, [this](const QString &path, const QVariantMap &item) {
        if (item["kind"] == "engine") {
            auto profiles = settings_.value("engineProfiles").toList();
            for (const auto &profile : profiles)
                if (profile.toMap()["enginePath"] == path) {
                    emit message(tr("Ce moteur possède déjà un profil. Vous pouvez en mémoriser d’autres "
                                    "dans les paramètres."));
                    return;
                }
            profiles << QVariantMap{{"name", item["title"].toString() + " " + item["version"].toString()},
                                    {"enginePath", path},
                                    {"threads", t64::recommendedThreads()},
                                    {"hash", t64::recommendedHashMb()},
                                    {"multiPV", 3},
                                    {"skill", 12},
                                    {"moveTime", 1000},
                                    {"analysisDepth", 0},
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
        wholeFamily =
            wholeFamily || name == english || name == english + " defense" || name == english + " defence";
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

QString Controller::syzygyStatus(const QString &folder) const {
    if (folder.trimmed().isEmpty())
        return tr("Sans tables, les finales sont demandées à Lichess.");
    const auto tables = t64::syzygy::open(localPath(folder.trimmed()));
    if (tables.files == 0)
        return tr("Aucune table Syzygy (.rtbw) dans ce dossier ni dans ses sous-dossiers.");
    return tr("%1 tables, jusqu’à %2 pièces : ces finales se lisent sans réseau, au-delà Lichess répond.")
        .arg(tables.files)
        .arg(tables.largest);
}

namespace {
void require(bool ok, const char *error) {
    if (!ok)
        throw std::runtime_error(error);
}
} // namespace

// Le dossier choisi, sinon un dossier à Trait64 : les tables y arrivent, et il devient
// celui des paramètres.
QString Controller::syzygyFolder() const {
    const auto chosen = settings_.value("syzygyPath").toString();
    return chosen.isEmpty() ? dataPath_ + "/syzygy" : chosen;
}

QVariantMap Controller::syzygyPack(const QString &pack) const {
    const auto folder = syzygyFolder();
    qint64 present = 0;
    for (QDirIterator it(QDir(folder).filePath(pack), {"*.rtbw", "*.rtbz"}, QDir::Files); it.hasNext();)
        present += it.nextFileInfo().size();
    QDir().mkpath(folder);
    return {{"bytes", t64::syzygy::packBytes(pack)},
            {"present", present},
            {"free", QStorageInfo(folder).bytesAvailable()},
            {"folder", QDir::toNativeSeparators(folder)}};
}

void Controller::downloadSyzygy(const QString &pack) {
    try {
        require(pack == "3-4-5" || pack == "6", "Paquet de tables inconnu.");
        require(!syzygyLoader_ || !syzygyLoader_->busy(), "Un téléchargement de tables est déjà en cours.");
        const auto sizes = syzygyPack(pack);
        // Un peu de marge : le disque plein à ras ne laisse plus vivre la bibliothèque.
        require(sizes["free"].toLongLong() + sizes["present"].toLongLong() >
                    sizes["bytes"].toLongLong() + 512LL * 1024 * 1024,
                "Pas assez de place libre sur ce disque pour ces tables.");
        const auto folder = syzygyFolder();
        if (settings_.value("syzygyPath").toString().isEmpty())
            setPreference("syzygyPath", folder);
        if (!syzygyLoader_) {
            syzygyLoader_ = new t64::SyzygyDownload(this);
            connect(syzygyLoader_, &t64::SyzygyDownload::progress, this,
                    [this](int done, int total, qint64 received) {
                        syzygyState_["done"] = done;
                        syzygyState_["total"] = total;
                        syzygyState_["received"] = received;
                        emit syzygyDownloadChanged();
                    });
            connect(syzygyLoader_, &t64::SyzygyDownload::finished, this, [this](const QString &error) {
                syzygyState_["running"] = false;
                syzygyState_["error"] = error;
                // Les tables ont changé : Fathom relit le dossier.
                t64::syzygy::open({});
                t64::syzygy::open(syzygyFolder());
                emit syzygyDownloadChanged();
                emit message(error.isEmpty()
                                 ? tr("Tables Syzygy installées : les finales se lisent sans réseau.")
                                 : error,
                             !error.isEmpty());
            });
        }
        syzygyState_ = {{"running", true}, {"pack", pack}, {"done", 0}, {"total", 0}, {"received", 0}};
        emit syzygyDownloadChanged();
        syzygyLoader_->start(folder, pack);
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::cancelSyzygyDownload() {
    if (syzygyLoader_)
        syzygyLoader_->cancel();
}
