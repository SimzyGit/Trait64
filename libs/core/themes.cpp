// SPDX-License-Identifier: AGPL-3.0-or-later
#include "themes.h"
#include <algorithm>
#include <array>
#include <bit>

namespace t64 {
namespace {
// Les pions d'un camp, colonne par colonne : un masque des rangées (bit 0 = première).
using Files = std::array<unsigned, 8>;
Files pawns(const chess::Board &board, chess::Color color) {
    Files files{};
    auto set = board.pieces(chess::PieceType::PAWN, color);
    while (set) {
        const int square = set.pop();
        files[size_t(square & 7)] |= 1u << (square >> 3);
    }
    return files;
}
bool on(const Files &f, int file, int rank) {
    return f[size_t(file)] >> (rank - 1) & 1;
}
int count(unsigned mask) {
    return std::popcount(mask);
}
// Vu des Blancs ; pour les Noirs, on retourne l'échiquier (rangée r → 9 - r).
Files mirrored(const Files &f) {
    Files out{};
    for (int file = 0; file < 8; ++file)
        for (int rank = 1; rank <= 8; ++rank)
            if (on(f, file, rank))
                out[size_t(file)] |= 1u << (8 - rank);
    return out;
}
// Les structures d'un camp, ses pions et ceux d'en face vus de son côté.
unsigned sideThemes(const Files &own, const Files &other) {
    unsigned themes = 0;
    constexpr int c = 2, d = 3, e = 4, b = 1, f = 5;
    if (on(own, d, 4) && count(own[d]) == 1 && !own[c] && !own[e])
        themes |= IsolatedQueenPawn;
    for (int rank : {4, 5})
        if (on(own, c, rank) && on(own, d, rank) && count(own[c]) == 1 && count(own[d]) == 1 && !own[b] &&
            !own[e])
            themes |= HangingPawns;
    for (int file = 0; file < 8; ++file) {
        if (count(own[size_t(file)]) >= 2)
            themes |= DoubledPawns;
        for (int rank : {6, 7}) {
            if (!on(own, file, rank))
                continue;
            bool passed = true;
            for (int side = std::max(0, file - 1); side <= std::min(7, file + 1) && passed; ++side)
                passed = (other[size_t(side)] >> rank) == 0;
            if (passed)
                themes |= AdvancedPassedPawn;
        }
    }
    if (on(own, d, 4) && !own[c] && on(other, c, 6) && on(other, d, 5) && !other[e])
        themes |= Carlsbad;
    if (on(own, c, 4) && on(own, e, 4) && !own[d] && (on(other, d, 6) || on(other, d, 7)) && !other[c])
        themes |= Maroczy;
    if (on(own, d, 4) && on(own, e, 3) && on(own, f, 4))
        themes |= Stonewall;
    return themes;
}
bool lightSquare(int square) {
    return ((square >> 3) + (square & 7)) % 2 == 1;
}
} // namespace

std::string_view themeKey(int index) {
    static constexpr std::string_view keys[themeCount] = {
        "isolatedQueenPawn", "hangingPawns",       "advancedPassedPawn", "doubledPawns",
        "oppositeBishops",   "bishopPair",         "carlsbad",           "maroczy",
        "stonewall",         "queenlessMiddlegame"};
    return index >= 0 && index < themeCount ? keys[index] : std::string_view();
}

unsigned positionThemes(const chess::Board &board) {
    using chess::Color;
    using chess::PieceType;
    const auto white = pawns(board, Color::WHITE), black = pawns(board, Color::BLACK);
    unsigned themes = sideThemes(white, black) | sideThemes(mirrored(black), mirrored(white));
    const auto bishops = [&](Color color) { return board.pieces(PieceType::BISHOP, color); };
    const auto wb = bishops(Color::WHITE), bb = bishops(Color::BLACK);
    if (wb.count() == 1 && bb.count() == 1 && lightSquare(wb.lsb()) != lightSquare(bb.lsb()))
        themes |= OppositeBishops;
    // La paire : deux fous de couleurs différentes d'un côté, pas de l'autre.
    const auto pair = [&](chess::Bitboard set) {
        bool light = false, dark = false;
        while (set) {
            const int square = set.pop();
            (lightSquare(square) ? light : dark) = true;
        }
        return light && dark;
    };
    if (pair(wb) != pair(bb))
        themes |= BishopPair;
    if (!board.pieces(PieceType::QUEEN, Color::WHITE) && !board.pieces(PieceType::QUEEN, Color::BLACK)) {
        int pieces = 0;
        for (const auto type : {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK})
            pieces += board.pieces(type, Color::WHITE).count() + board.pieces(type, Color::BLACK).count();
        if (pieces >= 8)
            themes |= QueenlessMiddlegame;
    }
    return themes;
}

unsigned gameThemes(const Game &game) {
    const auto line = game.mainline();
    std::array<int, themeCount> held{};
    unsigned themes = 0;
    for (size_t ply = 16; ply < line.size(); ++ply) {
        const auto here = positionThemes(chess::Board(game.nodes[line[ply]].fen));
        for (int i = 0; i < themeCount; ++i)
            if (here >> i & 1)
                ++held[size_t(i)];
    }
    for (int i = 0; i < themeCount; ++i)
        if (held[size_t(i)] >= 4)
            themes |= 1u << i;
    return themes;
}
} // namespace t64
