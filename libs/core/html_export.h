// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "game.h"
namespace t64 {
// Une étude en une seule page HTML : échiquier, coups, variantes et commentaires, sans
// aucun fichier ni adresse extérieure. Elle s'ouvre dans n'importe quel navigateur, hors
// ligne, et se parcourt à la souris ou aux flèches.
std::string studyHtml(const Game &game);
} // namespace t64
