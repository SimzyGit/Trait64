// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QFile>
#include <QSaveFile>

namespace {
void require(bool ok, const QString &error) {
    if (!ok)
        throw std::runtime_error(error.toStdString());
}
} // namespace

void Controller::refreshRepertoires(bool overview) {
    if (overview) {
        repertoires_ = db_->repertoires();
        QVariantMap selected;
        for (const auto &item : repertoires_)
            if (item.toMap()["id"].toLongLong() == selectedRepertoire_)
                selected = item.toMap();
        repertoireState_.clear();
        if (!selected.isEmpty()) {
            repertoireState_ = db_->repertoire(selectedRepertoire_);
            for (auto it = selected.begin(); it != selected.end(); ++it)
                repertoireState_[it.key()] = it.value();
            repertoireState_["gaps"] = db_->repertoireGaps(selectedRepertoire_);
            repertoireState_["weaknesses"] = db_->repertoireWeaknesses(selectedRepertoire_);
        } else
            selectedRepertoire_ = 0;
    }
    // Hide prepared answers until the exercise has been completed or explicitly revealed.
    repertoireState_["moves"] =
        selectedRepertoire_ && !trainingActive() && repertoireState_["chess960"].toBool() == game_.chess960
            ? db_->repertoireMoves(selectedRepertoire_, QString::fromStdString(game_.nodes[node_].fen))
            : QVariantList();
    emit repertoiresChanged();
}

void Controller::selectRepertoire(qint64 id) {
    try {
        db_->repertoire(id);
        selectedRepertoire_ = id;
        refreshRepertoires();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::createRepertoire(const QString &name, bool white) {
    try {
        require(!busy_ && !trainingActive(), "Terminez le traitement ou l’entraînement.");
        selectedRepertoire_ = db_->createRepertoire(name, white, game_);
        if (gameId_ && !dirty_)
            db_->addToRepertoire(selectedRepertoire_, game_, gameId_);
        refreshRepertoires();
        emit message("Répertoire créé depuis l’étude et ses variantes.");
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
void Controller::addStudyToRepertoire(qint64 id) {
    try {
        require(!busy_ && !trainingActive(), "Terminez le traitement ou l’entraînement.");
        db_->addToRepertoire(id, game_, dirty_ ? 0 : gameId_);
        selectedRepertoire_ = id;
        refreshRepertoires();
        emit message("L’étude et ses variantes ont été fusionnées dans le répertoire.");
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::importRepertoire(qint64 id, const QString &url) {
    try {
        require(!busy_ && !trainingActive(), "Terminez le traitement ou l’entraînement.");
        QFile input(localPath(url));
        require(input.open(QIODevice::ReadOnly), input.errorString());
        require(input.size() <= 16 * 1024 * 1024, "Import de répertoire limité à 16 Mio par fichier.");
        const auto bytes = input.readAll();
        require(input.error() == QFileDevice::NoError, input.errorString());
        db_->importRepertoire(id, t64::parsePgn(bytes.toStdString(), 200));
        selectedRepertoire_ = id;
        refreshRepertoires();
        emit message("PGN ajouté au répertoire, avec ses variantes et transpositions.");
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::exportRepertoire(qint64 id, const QString &url) {
    try {
        const auto path = localPath(url);
        db_->validateOutputPath(path);
        const auto bytes = QByteArray::fromStdString(db_->exportRepertoire(id).pgn());
        QSaveFile output(path);
        require(output.open(QIODevice::WriteOnly) && output.write(bytes) == bytes.size() && output.commit(),
                output.errorString());
        emit message("Répertoire exporté en PGN. Les transpositions sont signalées dans les commentaires.");
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
        engineLines_.clear();
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
        emit message("Ce coup ne figure pas dans les réponses préparées. Réessayez ; cela ne signifie pas "
                     "qu’il est mauvais.");
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
    emit message(trainingFailed_ ? "Position terminée. Nouveau rappel programmé demain."
                                 : "Coup du répertoire retrouvé. Prochaine révision programmée.");
    return true;
}
