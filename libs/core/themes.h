// SPDX-License-Identifier: AGPL-3.0-or-later
// Les clés stratégiques d'une position : des structures et des déséquilibres que l'on
// reconnaît à la seule géométrie (pion isolé en d, pions pendants, fous de couleurs
// opposées…), comme les clés de classement de ChessBase. Ce sont des constats, pas des
// jugements : le thème dit de quoi la partie a parlé, pas qui en a profité.
#pragma once
#include "chess.hpp"
#include "game.h"
#include <string_view>

namespace t64 {
enum Theme : unsigned {
    IsolatedQueenPawn = 1u << 0,  // pion isolé en d (d4 blanc ou d5 noir), sans pion c ni e du même camp
    HangingPawns = 1u << 1,       // pions c et d côte à côte, sans pion b ni e du même camp
    AdvancedPassedPawn = 1u << 2, // un pion passé sur la sixième ou la septième rangée
    DoubledPawns = 1u << 3,       // deux pions du même camp sur une colonne
    OppositeBishops = 1u << 4,    // un fou chacun, de couleurs opposées
    BishopPair = 1u << 5,         // la paire de fous d'un seul côté
    Carlsbad = 1u << 6,           // pions blancs d4 sans c, pions noirs c6 et d5 sans e (ou l'inverse)
    Maroczy = 1u << 7,   // pions c4 et e4 sans pion d, l'adversaire en d6 ou d7 sans pion c (ou l'inverse)
    Stonewall = 1u << 8, // d4-e3-f4 pour les Blancs ou d5-e6-f5 pour les Noirs
    QueenlessMiddlegame = 1u << 9 // plus de dames, mais encore au moins huit pièces
};
constexpr int themeCount = 10;
// La clé (anglais, pour les filtres et la traduction) du thème de rang `index`.
std::string_view themeKey(int index);
// Les thèmes présents dans la position.
unsigned positionThemes(const chess::Board &board);
// Les thèmes d'une partie : ceux qui tiennent au moins quatre demi-coups de la ligne
// principale passé le seizième, pour qu'une position fugace d'une ouverture ou d'un
// échange en cours ne classe pas la partie.
unsigned gameThemes(const Game &game);
} // namespace t64
