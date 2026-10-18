// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "game.h"
namespace t64 {
enum class TacticalKind { Fork, AbsolutePin, KingSkewer, DiscoveredCheck, DoubleCheck };
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
// PGN graphical annotations on the resulting position. No arrows through blockers.
std::string tacticalAnnotations(const std::vector<TacticalFact> &facts);
} // namespace t64
