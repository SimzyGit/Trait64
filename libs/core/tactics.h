// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "game.h"
namespace t64 {
enum class TacticalKind {
    Fork,
    AbsolutePin,
    KingSkewer,
    DiscoveredCheck,
    DoubleCheck,
    // Une pièce du camp qui vient de jouer peut être prise avec gain : `attackers` sont
    // les pièces adverses qui la prennent, `targets` la pièce elle-même.
    Hanging,
    // Une pièce à longue portée restée en place menace, par la ligne que le coup a
    // ouverte, une pièce qu'elle gagnerait.
    DiscoveredAttack,
    // Si l'adversaire passait son tour, `attackers` donnerait mat en un coup.
    MateThreat
};
struct TacticalPiece {
    std::string square;
    chess::PieceType type;
    bool white = false;
    bool operator==(const TacticalPiece &) const = default;
};
struct TacticalFact {
    TacticalKind kind;
    std::vector<TacticalPiece> attackers, targets;
    bool operator==(const TacticalFact &) const = default;
};
// Immediately before/after one legal move. New/changed patterns, not forced wins.
// Non-king fork targets exclude captures that would expose the attacker's king.
std::vector<TacticalFact> explainTactics(const chess::Board &before, const chess::Board &after);
// Ce que rapporte, en points (pion 1, cavalier et fou 3, tour 5, dame 9), la prise de
// `target` par la pièce en `from` une fois tous les échanges joués sur cette case, chaque
// camp reprenant par sa pièce la moins chère et s'arrêtant quand cela lui coûte. Les
// pièces alignées derrière comptent ; les clouages sont ignorés.
int exchangeValue(const chess::Board &board, chess::Square from, chess::Square target);
// La meilleure prise du camp au trait, zéro s'il n'en a aucune qui rapporte.
int bestCaptureGain(const chess::Board &board);
// PGN graphical annotations on the resulting position. No arrows through blockers.
std::string tacticalAnnotations(const std::vector<TacticalFact> &facts);
} // namespace t64
