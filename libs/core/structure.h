// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "chess.hpp"
#include <string>
#include <vector>
namespace t64 {
// Ce qu'un coup change à la structure, constaté en comparant la position d'avant et celle
// d'après. Ce sont des faits de géométrie, pas des jugements : un pion isolé peut être
// fort, un roque ouvert sans danger. Le verdict reste celui des suites du moteur.
enum class StructureKind {
    IsolatedPawn,    // un pion du joueur n'a plus de pion ami sur les colonnes voisines
    DoubledPawns,    // deux pions du joueur sur la même colonne
    PassedPawn,      // un pion du joueur n'a plus de pion adverse devant lui ni à côté
    EnemyPassedPawn, // le coup laisse un pion passé à l'adversaire
    WeakenedShield,  // moins de pions devant le roi du joueur
    OpenFileNearKing // plus aucun pion du joueur sur une colonne du roi ou voisine
};
struct StructureChange {
    StructureKind kind;
    std::string where; // une case ("d4") ou une colonne ("g")
    bool operator==(const StructureChange &) const = default;
};
std::vector<StructureChange> structureChanges(const chess::Board &before, const chess::Board &after);
} // namespace t64
