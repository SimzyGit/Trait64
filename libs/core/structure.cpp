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
    std::set<std::string> holes, backward;                 // cases
    int shield = 0, islands = 0;
};
// Les rangées où `side` a déjà joué ses pions : celles d'avant `rank`, de son point de vue.
Bitboard behind(Color side, int rank) {
    std::uint64_t mask = 0;
    for (int r = 0; r < 8; ++r)
        if (side == Color::WHITE ? r < rank : r > rank)
            mask |= 0xFFULL << (8 * r);
    return Bitboard(mask);
}
// Les colonnes sans aucun pion, des deux camps.
std::set<std::string> openFiles(const chess::Board &board) {
    std::set<std::string> result;
    const auto pawns = board.pieces(PieceType::PAWN);
    for (int f = 0; f < 8; ++f)
        if (!(pawns & file(f)))
            result.insert(std::string(1, char('a' + f)));
    return result;
}
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
    // Les îlots : des groupes de colonnes voisines qui portent des pions.
    bool inside = false;
    for (int f = 0; f < 8; ++f) {
        const bool here = bool(own & file(f));
        if (here && !inside)
            ++result.islands;
        inside = here;
    }
    // Pion arriéré : aucun pion ami sur les colonnes voisines à sa hauteur ou derrière lui,
    // donc plus personne pour le soutenir quand il avance (un pion isolé est compté à part).
    pawns = own;
    while (pawns) {
        const Square square(pawns.pop());
        const int f = int(square.file()), r = int(square.rank());
        const auto level = Bitboard(0xFFULL << (8 * r));
        if ((own & neighbours(f)) && !(own & neighbours(f) & (behind(side, r) | level)))
            result.backward.insert(name(f, r));
    }
    // Cases faibles : sur les 3e et 4e rangées du joueur, au centre élargi (c à f), aucun
    // pion ami ne peut plus venir les défendre (il en faudrait un, derrière, sur une
    // colonne voisine).
    for (int step = 2; step <= 3; ++step) {
        const int r = side == Color::WHITE ? step : 7 - step;
        for (int f = 2; f <= 5; ++f)
            if (!(own & neighbours(f) & behind(side, r)) &&
                board.at(Square(f + r * 8)).type() != PieceType::PAWN)
                result.holes.insert(name(f, r));
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
    added(openFiles(before), openFiles(after), StructureKind::OpenedFile, result);
    added(mine.holes, now.holes, StructureKind::WeakSquare, result);
    added(mine.backward, now.backward, StructureKind::BackwardPawn, result);
    if (now.islands > mine.islands)
        result.push_back({StructureKind::MorePawnIslands, std::to_string(now.islands)});
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
