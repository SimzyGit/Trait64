// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QString>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace t64 {
struct PuzzleRecord {
    QString ref, fen, solution, themes;
    int rating = 1500, popularity = 0;
};
// Reads the Lichess puzzle export (CSV, optionally .zst/.gz/.bz2/.xz compressed).
// Each row's first move is the opponent's: it is applied, so `fen` is the solver's position
// and `solution` starts with the solver's move. Rows with an illegal move are skipped.
// Returns the number of puzzles delivered; `batch` receives them in groups of 2,000.
int readLichessPuzzles(const QString &path, int max, int minRating, int maxRating,
                       const std::function<void(std::vector<PuzzleRecord> &&)> &batch,
                       const std::shared_ptr<std::atomic_bool> &canceled = {});
// Parses one CSV row; false if the row is a header, malformed or illegal.
bool parseLichessPuzzle(const QString &line, PuzzleRecord &out);
} // namespace t64
