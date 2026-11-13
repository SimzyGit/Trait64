// SPDX-License-Identifier: AGPL-3.0-or-later
#include "structure.h"
#include <algorithm>
#include <set>
namespace t64 {
namespace {
using chess::Bitboard;
using chess::Color;
using chess::PieceType;
using chess::Square;
constexpr std::uint64_t fileA = 0x0101010101010101ULL;
Bitboard file(int index) {
    return Bitboard(fileA << index);
}
Bitboard neighbours(int index) {
    return (index > 0 ? file(index - 1) : Bitboard()) | (index < 7 ? file(index + 1) : Bitboard());
}
// Les cases strictement devant `rank`, du point de vue de `side`.
Bitboard ahead(Color side, int rank) {
    std::uint64_t mask = 0;
    for (int r = 0; r < 8; ++r)
        if (side == Color::WHITE ? r > rank : r < rank)
            mask |= 0xFFULL << (8 * r);
    return Bitboard(mask);
}
std::string name(int f, int r) {
    return {char('a' + f), char('1' + r)};
}
struct Facts {
    std::set<std::string> isolated, doubled, passed, bare; // cases, colonnes, cases, colonnes
    int shield = 0;
};
Facts facts(const chess::Board &board, Color side) {
    Facts result;
    const auto own = board.pieces(PieceType::PAWN, side), theirs = board.pieces(PieceType::PAWN, ~side);
    auto pawns = own;
    while (pawns) {
        const Square square(pawns.pop());
        const int f = int(square.file()), r = int(square.rank());
        if (!(own & neighbours(f)))
            result.isolated.insert(name(f, r));
        if ((own & file(f)).count() > 1)
            result.doubled.insert(std::string(1, char('a' + f)));
        if (!(theirs & (file(f) | neighbours(f)) & ahead(side, r)))
            result.passed.insert(name(f, r));
    }
    const Square king = board.kingSq(side);
    const int kf = int(king.file()), kr = int(king.rank());
    for (int f = std::max(0, kf - 1); f <= std::min(7, kf + 1); ++f) {
        if (!(own & file(f)))
            result.bare.insert(std::string(1, char('a' + f)));
        // Le bouclier : les pions amis à une ou deux rangées devant le roi.
        for (int step = 1; step <= 2; ++step) {
            const int r = kr + (side == Color::WHITE ? step : -step);
            if (r >= 0 && r < 8 && (own & Bitboard::fromSquare(Square(f + r * 8))))
                ++result.shield;
        }
    }
    return result;
}
void added(const std::set<std::string> &before, const std::set<std::string> &after, StructureKind kind,
           std::vector<StructureChange> &out) {
    for (const auto &item : after)
        if (!before.contains(item))
            out.push_back({kind, item});
}
} // namespace
std::vector<StructureChange> structureChanges(const chess::Board &before, const chess::Board &after) {
    const auto us = before.sideToMove();
    const auto mine = facts(before, us), now = facts(after, us);
    const auto theirs = facts(before, ~us), theirsNow = facts(after, ~us);
    std::vector<StructureChange> result;
    added(mine.isolated, now.isolated, StructureKind::IsolatedPawn, result);
    added(mine.doubled, now.doubled, StructureKind::DoubledPawns, result);
    added(mine.passed, now.passed, StructureKind::PassedPawn, result);
    added(theirs.passed, theirsNow.passed, StructureKind::EnemyPassedPawn, result);
    // Le roi qui se déplace change de voisinage : comparer ses pions n'aurait pas de sens.
    // Et l'abri ne se juge que pour un roi parti sur une aile : au centre, avancer les
    // pions d et e est le jeu normal, pas un affaiblissement.
    const int kingFile = int(after.kingSq(us).file());
    if (before.kingSq(us) == after.kingSq(us) && (kingFile <= 2 || kingFile >= 5)) {
        if (now.shield < mine.shield) {
            const Square king = after.kingSq(us);
            result.push_back({StructureKind::WeakenedShield, name(int(king.file()), int(king.rank()))});
        }
        added(mine.bare, now.bare, StructureKind::OpenFileNearKing, result);
    }
    return result;
}
} // namespace t64
