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
