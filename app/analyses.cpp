// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"

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

QVariantMap Controller::analysisConfig(int milliseconds, int profile) const {
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
    return config;
}
