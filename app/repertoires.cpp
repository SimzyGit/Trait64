// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"

namespace {
void require(bool ok, const QString &error) {
    if (!ok)
        throw std::runtime_error(error.toStdString());
}
} // namespace

void Controller::refreshRepertoires(bool overview) {
    if (!repertoiresEnabled_)
        return;
    if (overview) {
        ++repertoireGeneration_;
        if (cancelRepertoireRead_)
            cancelRepertoireRead_->store(true);
        repertoireReadPending_ = true;
        repertoireState_["loading"] = true;
        repertoireState_["error"] = QString();
        repertoireState_["gapsLoaded"] = false;
        repertoireState_["gaps"] = QVariantList();
        repertoireState_["weaknesses"] = QVariantList();
        if (!repertoireReadActive_)
            startRepertoireRead();
    }
    // Hide prepared answers while practising; this small indexed lookup stays on the GUI connection.
    try {
        repertoireState_["moves"] =
            selectedRepertoire_ && !trainingActive() &&
                    repertoireState_["chess960"].toBool() == game_.chess960
                ? db_->repertoireMoves(selectedRepertoire_, QString::fromStdString(game_.nodes[node_].fen))
                : QVariantList();
    } catch (const std::exception &e) {
        repertoireState_["moves"] = QVariantList();
        repertoireState_["error"] = QString::fromUtf8(e.what());
    }
    emit repertoiresChanged();
}

void Controller::selectRepertoire(qint64 id) {
    try {
        require(!busy_, "Attendez la fin du traitement.");
        repertoireState_ = db_->repertoire(id);
        selectedRepertoire_ = id;
        refreshRepertoires();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::renameRepertoire(qint64 id, const QString &name) {
    try {
        require(!busy_ && !trainingActive(), "Terminez le traitement ou l’entraînement.");
        db_->renameRepertoire(id, name);
        refreshRepertoires();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::deleteRepertoire(qint64 id) {
    try {
        require(!busy_ && !trainingActive(), "Terminez le traitement ou l’entraînement.");
        db_->deleteRepertoire(id);
        refreshRepertoires();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::editRepertoireMove(qint64 id, const QString &fen, const QString &uci, const QString &note,
                                    bool principal) {
    try {
        require(!busy_ && !trainingActive(), "Terminez le traitement ou l’entraînement.");
        db_->editRepertoireMove(id, fen, uci, note, principal);
        refreshRepertoires();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::removeRepertoireMove(qint64 id, const QString &fen, const QString &uci) {
    try {
        require(!busy_ && !trainingActive(), "Terminez le traitement ou l’entraînement.");
        db_->removeRepertoireMove(id, fen, uci);
        refreshRepertoires();
    } catch (const std::exception &e) {
        fail(e);
    }
}
QVariantMap Controller::compareRepertoires(qint64 first, qint64 second) {
    try {
        auto result = db_->compareRepertoires(first, second);
        result["ready"] = true;
        return result;
    } catch (const std::exception &e) {
        fail(e);
        return {{"ready", false}};
    }
}
void Controller::openRepertoirePosition(qint64 id, const QString &fen) {
    try {
        require(!busy_ && !trainingActive(), "Terminez le traitement ou l’entraînement.");
        const auto rep = db_->repertoire(id);
        setPosition(fen, rep["chess960"].toBool());
        selectedRepertoire_ = id;
        refreshRepertoires();
        emit showAnalysis();
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::startRepertoireTraining(qint64 id, bool includeFuture) {
    try {
        require(!busy_, "Attendez la fin du traitement.");
        const auto exercise = db_->nextRepertoirePosition(id, includeFuture);
        require(!exercise.isEmpty(), "Aucune position à réviser dans ce répertoire.");
        if (trainingActive())
            finishTraining();
        checkpoint();
        beforeTraining_ = game_;
        beforeNode_ = node_;
        beforeId_ = gameId_;
        beforeDirty_ = dirty_;
        beforeFlipped_ = flipped_;
        cancelReview();
        stopPlaying();
        repertoireExercise_ = exercise;
        selectedRepertoire_ = id;
        trainingComplete_ = trainingFailed_ = false;
        solutionIndex_ = 0;
        game_ = t64::Game(exercise["fen"].toString().toStdString(), exercise["chess960"].toBool());
        game_.tags["Event"] = exercise["name"].toString().toStdString();
        node_ = 0;
        selected_ = -1;
        flipped_ = game_.boardAt(0).sideToMove() == chess::Color::BLACK;
        forgetEngineLines();
        emit engineChanged();
        refreshRepertoires();
        refresh();
        emit showAnalysis();
    } catch (const std::exception &e) {
        fail(e);
    }
}

bool Controller::playRepertoireAnswer(const QString &uci) {
    if (repertoireExercise_.isEmpty())
        return false;
    if (trainingComplete_)
        return true;
    QVariantMap accepted;
    for (const auto &value : repertoireExercise_["answers"].toList())
        if (value.toMap()["uci"] == uci)
            accepted = value.toMap();
    if (accepted.isEmpty()) {
        trainingFailed_ = true;
        emit message(tr("Ce coup ne figure pas dans les réponses préparées. Réessayez ; cela ne signifie pas "
                        "qu’il est mauvais."));
        return true;
    }
    auto candidate = game_;
    const auto node = candidate.play(node_, uci.toStdString());
    candidate.nodes[node].comment = accepted["note"].toString().toStdString();
    db_->reviewRepertoire(repertoireExercise_["repertoire"].toLongLong(),
                          repertoireExercise_["fen"].toString(), !trainingFailed_);
    game_ = std::move(candidate);
    node_ = node;
    selected_ = -1;
    solutionIndex_ = 1;
    trainingComplete_ = true;
    refreshRepertoires();
    refresh();
    emit message(trainingFailed_ ? tr("Position terminée. Nouveau rappel programmé demain.")
                                 : tr("Coup du répertoire retrouvé. Prochaine révision programmée."));
    return true;
}
