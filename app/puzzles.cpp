// SPDX-License-Identifier: AGPL-3.0-or-later
// Puzzle training: bank import, selection near the player's rating, solving and rating.
#include "puzzles.h"
#include "controller.h"
#include <QCryptographicHash>
#include <QtConcurrent>
#include <cmath>

namespace {
void need(bool ok, const QString &message) {
    if (!ok)
        throw std::runtime_error(message.toStdString());
}
QString qs(const std::string &text) {
    return QString::fromStdString(text);
}
int squareIndex(const QString &sq) {
    return sq.size() == 2 && sq[0] >= 'a' && sq[0] <= 'h' && sq[1] >= '1' && sq[1] <= '8'
               ? (sq[1].unicode() - '1') * 8 + sq[0].unicode() - 'a'
               : -1;
}
} // namespace

QVariantMap Controller::puzzles() const {
    auto state = puzzleStats_;
    state["rating"] = settings_.value("puzzleRating", 1500).toInt();
    state["active"] = puzzleId_ != 0;
    state["current"] = puzzle_;
    state["started"] = puzzleStarted_;
    return state;
}

void Controller::refreshPuzzleStats() {
    try {
        puzzleStats_ = db_->puzzleStats();
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::startPuzzle(const QString &theme, const QString &source) {
    try {
        need(!busy_, "Un traitement est déjà en cours.");
        if (trainingActive() && !puzzleId_)
            finishTraining();
        const int rating = settings_.value("puzzleRating", 1500).toInt();
        // Les puzzles manqués dont le délai est passé d'abord : c'est en les revoyant qu'on
        // progresse, pas en en découvrant toujours de nouveaux.
        auto next = db_->duePuzzle(theme, source);
        if (next.isEmpty())
            next = db_->nextPuzzle(rating, 150, theme, source);
        need(!next.isEmpty(), source == "mine"
                                  ? "Aucun puzzle tiré de vos parties : analysez une partie (Bilan), "
                                    "puis « Créer des puzzles depuis les erreurs »."
                                  : "Aucun puzzle disponible : importez la base Lichess, ou élargissez "
                                    "le thème.");
        if (!puzzleId_) {
            beforeTraining_ = game_;
            beforeNode_ = node_;
            beforeId_ = gameId_;
            beforeDirty_ = dirty_;
            beforeFlipped_ = flipped_;
        }
        cancelReview();
        stopPlaying();
        puzzleTheme_ = theme;
        puzzleSource_ = source;
        puzzle_ = next;
        puzzleId_ = next["id"].toLongLong();
        cardId_ = 0;
        repertoireExercise_.clear();
        solution_ = next["solution"].toString().split(' ', Qt::SkipEmptyParts);
        solutionIndex_ = 0;
        trainingComplete_ = trainingFailed_ = false;
        game_ = t64::Game(next["fen"].toString().toStdString());
        node_ = 0;
        selected_ = -1;
        flipped_ = game_.boardAt(0).sideToMove() == chess::Color::BLACK;
        game_.tags["White"] = QString("Puzzle %1").arg(next["rating"].toInt()).toStdString();
        game_.tags["Black"] = next["source"] == "mine" ? "Mes erreurs" : "Lichess";
        forgetEngineLines();
        emit engineChanged();
        puzzleClock_.start();
        puzzleStarted_ = QDateTime::currentMSecsSinceEpoch();
        refresh();
        emit showAnalysis();
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::nextPuzzle() {
    startPuzzle(puzzleTheme_, puzzleSource_);
}

void Controller::puzzleHint() {
    if (!puzzleId_ || trainingComplete_ || solutionIndex_ >= solution_.size())
        return;
    // A hint shows which piece moves; like the solution, it makes the attempt a failure.
    trainingFailed_ = true;
    selected_ = squareIndex(solution_[solutionIndex_].left(2));
    refresh();
    emit message(tr("Indice : la pièce en %1 doit jouer.").arg(solution_[solutionIndex_].left(2)));
}

bool Controller::acceptPuzzleMate(const QString &uci) {
    // Lichess accepts any mate on the final move, not only the stored one.
    if (!puzzleId_ || solutionIndex_ + 1 != solution_.size())
        return false;
    try {
        auto board = game_.boardAt(node_);
        chess::Movelist legal;
        chess::movegen::legalmoves(legal, board);
        for (const auto &move : legal)
            if (qs(chess::uci::moveToUci(move)) == uci) {
                board.makeMove(move);
                chess::Movelist replies;
                chess::movegen::legalmoves(replies, board);
                return replies.empty() && board.inCheck();
            }
    } catch (const std::exception &) {
    }
    return false;
}

void Controller::finishPuzzle() {
    const bool success = !trainingFailed_;
    const double seconds = puzzleClock_.elapsed() / 1000.0;
    const int before = settings_.value("puzzleRating", 1500).toInt();
    const int difficulty = puzzle_["rating"].toInt();
    const double expected = 1.0 / (1.0 + std::pow(10.0, (difficulty - before) / 400.0));
    const int after =
        std::clamp(int(std::lround(before + 32 * ((success ? 1.0 : 0.0) - expected))), 100, 3500);
    settings_.setValue("puzzleRating", after);
    try {
        db_->recordPuzzle(puzzleId_, success, seconds, after);
    } catch (const std::exception &e) {
        fail(e);
    }
    refreshPuzzleStats();
    const auto delta = QString("%1%2").arg(after >= before ? "+" : "").arg(after - before);
    emit message(success
                     ? tr("Résolu en %1 s ! Classement %2 (%3).").arg(qRound(seconds)).arg(after).arg(delta)
                     : tr("Puzzle terminé avec aide. Classement %1 (%2).").arg(after).arg(delta));
}

void Controller::importPuzzles(const QString &url, int max, int minRating, int maxRating) {
    try {
        need(!busy_, "Un traitement est déjà en cours.");
        const auto path = localPath(url);
        need(QFileInfo(path).isFile(), "Fichier de puzzles introuvable.");
        busy_ = true;
        cancelTask_->store(false);
        taskProgress_->store(0);
        taskPoll_.start();
        emit busyChanged();
        const auto databasePath = db_->path();
        const auto canceled = cancelTask_;
        const auto progress = taskProgress_;
        max = std::clamp(max, 1, 6000000);
        importer_.setFuture(QtConcurrent::run([=] {
            ImportResult r;
            r.operation = "puzzles";
            try {
                t64::Database database(databasePath);
                int read = 0;
                t64::readLichessPuzzles(
                    path, max, minRating, maxRating,
                    [&](std::vector<t64::PuzzleRecord> &&batch) {
                        std::vector<t64::Puzzle> rows;
                        rows.reserve(batch.size());
                        for (auto &p : batch)
                            rows.push_back(
                                {p.ref, p.fen, p.solution, p.themes, "lichess", p.rating, p.popularity});
                        r.count += database.addPuzzles(rows);
                        read += int(batch.size());
                        progress->store(read);
                    },
                    canceled);
                r.duplicates = read - r.count;
            } catch (const std::exception &e) {
                r.canceled = canceled->load();
                r.error = QString::fromUtf8(e.what());
            }
            return r;
        }));
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::createMistakePuzzles() {
    try {
        need(!busy_, "Traitement en cours.");
        need(!game_.chess960, "Les puzzles sont limités aux échecs classiques.");
        need(!review_.isEmpty(), "Analysez d'abord la partie (Bilan).");
        std::vector<t64::Puzzle> rows;
        for (const auto &v : review_) {
            const auto row = v.toMap();
            if (row["loss"].toInt() < 100 || row["best"].toString().isEmpty())
                continue;
            auto line = row["best"].toString().split(' ', Qt::SkipEmptyParts);
            // The solver plays the first and the last move: keep an odd number of plies, at most five.
            while (line.size() > 5 || (line.size() > 1 && line.size() % 2 == 0))
                line.removeLast();
            const auto fen = row["fen"].toString();
            const auto ref = "mine:" + QCryptographicHash::hash((fen + ' ' + line.join(' ')).toUtf8(),
                                                                QCryptographicHash::Sha1)
                                           .toHex();
            // Rated by the size of the mistake: larger misses are usually easier to see.
            const int rating = row["loss"].toInt() >= 250 ? 1300 : 1600;
            rows.push_back({ref, fen, line.join(' '), row["label"].toString().toLower(), "mine", rating, 0});
        }
        const int added = db_->addPuzzles(rows);
        refreshPuzzleStats();
        publish();
        emit message(tr("%1 puzzle(s) créé(s) depuis les erreurs, %2 déjà présent(s).")
                         .arg(added)
                         .arg(int(rows.size()) - added));
    } catch (const std::exception &e) {
        fail(e);
    }
}
