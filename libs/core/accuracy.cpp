// SPDX-License-Identifier: AGPL-3.0-or-later
#include "accuracy.h"
#include <algorithm>
#include <cmath>

namespace t64 {
namespace {
// Nombre de pièces lourdes et légères encore en jeu : quatorze au départ, deux dames,
// quatre tours, quatre cavaliers, quatre fous. Pions et rois exclus, car ils restent.
int majorsAndMinors(const chess::Board &board) {
    int count = 0;
    for (int square = 0; square < 64; ++square) {
        const auto piece = board.at(chess::Square(square));
        if (piece == chess::Piece::NONE)
            continue;
        const auto type = piece.type();
        if (type != chess::PieceType::PAWN && type != chess::PieceType::KING)
            ++count;
    }
    return count;
}
} // namespace

double winPercent(double centipawns) {
    return 50 + 50 * (2 / (1 + std::exp(-0.00368208 * centipawns)) - 1);
}

double moveAccuracy(double winPercentBefore, double winPercentAfter) {
    // Améliorer sa position ne se punit pas : le moteur voyait simplement moins loin.
    if (winPercentAfter >= winPercentBefore)
        return 100;
    const double lost = winPercentBefore - winPercentAfter;
    const double raw = 103.1668100711649 * std::exp(-0.04354415386753951 * lost) - 3.166924740191411;
    // Le point ajouté est la marge d'incertitude de Lichess : elle évite qu'un coup
    // irréprochable soit noté 99 pour une broutille d'arrondi du moteur.
    return std::clamp(raw + 1, 0., 100.);
}

double aggregateAccuracy(const std::vector<double> &moveAccuracies) {
    if (moveAccuracies.empty())
        return -1;
    double sum = 0, inverseSum = 0;
    for (const double value : moveAccuracies) {
        const double bounded = std::clamp(value, 0., 100.);
        sum += bounded;
        // Un coup noté zéro annulerait la moyenne harmonique : on le plafonne par le bas
        // plutôt que de l'écarter, pour qu'une gaffe pèse lourd sans tout effacer.
        inverseSum += 1 / std::max(bounded, 1.);
    }
    const double count = double(moveAccuracies.size());
    const double arithmetic = sum / count, harmonic = count / inverseSum;
    return std::clamp((arithmetic + harmonic) / 2, 0., 100.);
}

AccuracySummary summarize(const Game &game, const std::vector<int> &nodes,
                          const std::vector<NodeScore> &scores) {
    AccuracySummary summary;
    std::vector<double> marks[2][4];
    const auto count = std::min(nodes.size(), scores.size());
    for (size_t i = 1; i < count; ++i) {
        const auto &before = scores[i - 1], &after = scores[i];
        if (!before.known || !after.known)
            continue;
        const int parent = nodes[i - 1];
        const bool white = game.boardAt(parent).sideToMove() == chess::Color::WHITE;
        const int side = white ? 0 : 1;
        const double sign = white ? 1 : -1;
        // Les évaluations sont comptées du point de vue des Blancs : on les retourne pour
        // celui qui joue, car ce sont ses chances de gain qui le notent.
        const double mark =
            moveAccuracy(winPercent(before.centipawns * sign), winPercent(after.centipawns * sign));
        // La phase se lit sur la position d'avant le coup : celle que le joueur avait
        // devant lui au moment de décider.
        const int part = int(phaseOf(game.boardAt(parent), int(i) - 1));
        marks[side][part].push_back(mark);
        marks[side][3].push_back(mark);
        // Mêmes seuils que les étiquettes du bilan, pour qu'un chiffre et un mot ne se
        // contredisent jamais à l'écran.
        const double loss = std::max(0., (before.centipawns - after.centipawns) * sign);
        if (loss >= 250)
            ++summary.blunders[side];
        else if (loss >= 100)
            ++summary.mistakes[side];
        else if (loss >= 50)
            ++summary.inaccuracies[side];
    }
    for (int side = 0; side < 2; ++side) {
        summary.all[side] = aggregateAccuracy(marks[side][3]);
        summary.moves[side][3] = int(marks[side][3].size());
        for (int part = 0; part < 3; ++part) {
            summary.phase[side][part] = aggregateAccuracy(marks[side][part]);
            summary.moves[side][part] = int(marks[side][part].size());
        }
    }
    return summary;
}

Phase phaseOf(const chess::Board &board, int ply) {
    const int pieces = majorsAndMinors(board);
    if (pieces <= 6)
        return Phase::Endgame;
    // Quatre pièces échangées, ou dix coups joués de part et d'autre : dans les deux cas
    // la théorie est derrière nous et le joueur décide seul.
    if (pieces <= 10 || ply >= 20)
        return Phase::Middlegame;
    return Phase::Opening;
}

} // namespace t64
