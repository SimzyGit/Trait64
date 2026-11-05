// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QFileInfo>
#include <algorithm>

void Controller::initAnalyses() {
    connect(&analysisQueue_, &t64::AnalysisQueue::message, this, &Controller::message);
    connect(&analysisQueue_, &t64::AnalysisQueue::studyReady, this, [this](const QString &pgn) {
        try {
            if (trainingActive())
                throw std::runtime_error("Terminez l’entraînement avant d’ouvrir le résultat.");
            auto games = t64::parsePgn(pgn.toStdString());
            if (games.size() != 1)
                throw std::runtime_error("Résultat d’analyse illisible.");
            replaceGame(std::move(games.front()));
        } catch (const std::exception &e) {
            fail(e);
        }
    });
    analysisQueue_.setLibrary(db_->path());
}
void Controller::enqueueAnalysis(bool variations, int milliseconds, int profile) {
    try {
        if (trainingActive())
            throw std::runtime_error("Terminez l’entraînement avant de créer une analyse.");
        const auto config = analysisConfig(milliseconds, profile);
        analysisQueue_.enqueue(QString::fromStdString(game_.pgn()), config, variations);
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::enqueueLibrary(int milliseconds, int profile, int maxGames) {
    try {
        if (trainingActive()) throw std::runtime_error("Terminez l’entraînement avant de créer une analyse.");
        const auto config = analysisConfig(milliseconds, profile);
        // One clear message instead of one failed job per game.
        if (!QFileInfo(config["enginePath"].toString()).isExecutable())
            throw std::runtime_error("Choisissez un moteur UCI dans les paramètres.");
        auto filters = advancedFilters_;
        filters.remove("cursor");
        filters["limit"] = std::clamp(maxGames, 1, 5000);
        const auto position = positionFilter_ ? QString::fromStdString(game_.nodes[node_].fen) : QString();
        const auto rows = db_->search(query_, resultFilter_, favoritesOnly_, position, trashFilter_, filters);
        for (const auto &row : rows)
            analysisQueue_.enqueue(QString::fromStdString(db_->load(row.toMap()["id"].toLongLong()).pgn()), config, false);
        emit message(tr("%1 partie(s) ajoutée(s) à la file d’analyse (lignes principales).").arg(rows.size()));
    } catch (const std::exception &e) {
        fail(e);
    }
}
int Controller::reviewTimeCeiling(int depth) const {
    // Ce plafond n'est pas une cible : le moteur s'arrête normalement à la profondeur
    // demandée, bien avant. Il n'existe que pour qu'une position coriace ne retienne pas
    // une file de plusieurs milliers de parties. Il croît avec la profondeur, puisque le
    // temps nécessaire pour l'atteindre croît lui aussi.
    return std::clamp(depth * depth * 4, 200, 10000);
}
void Controller::reviewImported(int depth, int profile) {
    try {
        if (trainingActive())
            throw std::runtime_error("Terminez l’entraînement avant de lancer un bilan.");
        const int wanted = std::clamp(depth, 1, 40);
        if (importedGames_ <= 0)
            throw std::runtime_error("Aucune partie récemment importée à passer en revue.");
        const auto config = analysisConfig(reviewTimeCeiling(wanted), profile, wanted);
        // Un seul message clair, plutôt qu'un échec par partie.
        if (!QFileInfo(config["enginePath"].toString()).isExecutable())
            throw std::runtime_error("Choisissez un moteur UCI dans les paramètres.");
        const auto ids = db_->recentGamesToReview(importedGames_, wanted);
        if (ids.isEmpty()) {
            emit message(tr("Ces %1 partie(s) ont déjà été passées en revue au moins aussi loin.")
                             .arg(importedGames_));
            return;
        }
        for (const auto &value : ids) {
            const auto id = value.toLongLong();
            analysisQueue_.enqueue(QString::fromStdString(db_->load(id).pgn()), config, false, id);
        }
        emit message(tr("%1 partie(s) ajoutée(s) à la file de bilan, profondeur %2.")
                         .arg(ids.size())
                         .arg(wanted));
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::reviewProfile(int profileIndex, int depth, int enginePreset) {
    try {
        if (trainingActive())
            throw std::runtime_error("Terminez l’entraînement avant de lancer un bilan.");
        const auto names = profileAccountNames(profileIndex);
        if (names.isEmpty())
            throw std::runtime_error("Rattachez un compte à ce profil avant d’analyser ses parties.");
        const int wanted = std::clamp(depth, 1, 40);
        const auto config = analysisConfig(reviewTimeCeiling(wanted), enginePreset, wanted);
        // Un seul message clair, plutôt qu'un échec par partie.
        if (!QFileInfo(config["enginePath"].toString()).isExecutable())
            throw std::runtime_error("Choisissez un moteur UCI dans les paramètres.");
        const auto ids = db_->gamesToReviewFor(names, wanted);
        if (ids.isEmpty()) {
            emit message(tr("Toutes les parties de ce profil ont déjà été passées en revue au moins aussi loin."));
            return;
        }
        for (const auto &value : ids) {
            const auto id = value.toLongLong();
            analysisQueue_.enqueue(QString::fromStdString(db_->load(id).pgn()), config, false, id);
        }
        emit message(tr("%1 partie(s) de ce profil ajoutée(s) à la file de bilan, profondeur %2.")
                         .arg(ids.size())
                         .arg(wanted));
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::exportPolyglot(const QString &url, int minGames) {
    try {
        const int entries = db_->exportPolyglot(localPath(url), minGames);
        emit message(tr("Livre Polyglot écrit : %1 coup(s) joués au moins %2 fois.").arg(entries).arg(minGames));
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::findNovelty() {
    try {
        const auto result = db_->novelty(game_, gameId_);
        if (!result["found"].toBool()) {
            emit message(result[tr("reason")] == tr("known") ? tr("Toute la ligne principale est déjà jouée dans votre bibliothèque.")
                                                     : tr("La partie quitte la bibliothèque dès son départ."));
            return;
        }
        QStringList known;
        for (const auto &move : result["known"].toList())
            known << move.toMap()["san"].toString() + " (" + move.toMap()["count"].toString() + ")";
        const int ply = result["ply"].toInt();
        const auto number = QString::number(ply / 2 + 1) + (ply % 2 == 0 ? ". " : "… ");
        navigate(result["node"].toInt());
        emit message(tr("Nouveauté : %1%2 — position atteinte dans %3 partie(s)%4.")
                         .arg(number, result["san"].toString())
                         .arg(result["reached"].toLongLong())
                         .arg(known.isEmpty() ? QString() : ", suites connues : " + known.join(", ")));
    } catch (const std::exception &e) {
        fail(e);
    }
}
QVariantMap Controller::analysisConfig(int milliseconds, int profile, int depth) const {
    QVariantMap config;
    if (profile < 0) {
        for (const auto &key :
             QStringList{"enginePath", "threads", "hash", "multiPV", "syzygyPath", "uciOptions"})
            if (settings_.contains(key))
                config[key] = settings_.value(key);
    } else {
        const auto profiles = settings_.value("engineProfiles").toList();
        if (profile >= profiles.size())
            throw std::runtime_error("Profil moteur introuvable.");
        config = profiles[profile].toMap();
    }
    config["milliseconds"] = milliseconds;
    config["depth"] = depth;
    return config;
}
