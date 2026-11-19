// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <string>
#include <vector>
namespace t64 {
// L'adresse d'une position sur l'échiquier d'analyse de Lichess : elle s'ouvre sur un
// téléphone, sans rien installer, pour continuer l'analyse loin de l'ordinateur.
std::string lichessAnalysisUrl(const std::string &fen, bool chess960 = false);
// Un QR code en SVG : modules noirs sur fond blanc, avec la marge de quatre modules que
// les lecteurs attendent. Correction d'erreur moyenne. Vide si le texte est trop long.
std::string qrSvg(const std::string &text);
// Les mêmes modules en grille, marge comprise (vrai = noir), pour dessiner le code en
// image là où le SVG n'est pas lu. Vide si le texte est trop long.
std::vector<std::vector<bool>> qrModules(const std::string &text);
} // namespace t64
