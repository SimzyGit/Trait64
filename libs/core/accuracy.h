// SPDX-License-Identifier: AGPL-3.0-or-later
// Précision d'une partie, selon la méthode de Lichess : une perte en centipions ne dit
// rien à elle seule — en céder cent quand on gagne déjà largement ne coûte presque rien,
// les céder dans une position égale coûte la partie. On passe donc par les chances de
// gain, puis on note chaque coup par ce qu'il en a fait perdre.
#pragma once
#include "chess.hpp"
#include "game.h"
#include <vector>

namespace t64 {

// Chances de gain en pourcentage, du point de vue du joueur au trait, pour une
// évaluation en centipions. Constante de Lichess, ajustée sur ses propres parties.
double winPercent(double centipawns);

// Note d'un coup sur 100, d'après les chances de gain avant et après. Un coup qui
// n'enlève rien vaut 100 ; la note décroît d'autant plus vite que la perte est grande.
double moveAccuracy(double winPercentBefore, double winPercentAfter);

// Note d'un ensemble de coups. Moyenne arithmétique et harmonique à parts égales : la
// seconde pèse les mauvais coups plus lourd, ce qui évite qu'une gaffe unique se dilue
// dans une longue partie par ailleurs correcte. Renvoie -1 si la liste est vide, ce qui
// se lit « pas mesurable » et jamais « nul ».
double aggregateAccuracy(const std::vector<double> &moveAccuracies);

enum class Phase { Opening = 0, Middlegame = 1, Endgame = 2 };

// Phase d'une position. Fondée sur le nombre de pièces lourdes et légères encore en jeu,
// comme chez Lichess, avec une borne de coups pour qu'une partie sans échange finisse
// quand même par quitter l'ouverture. Le matériel ne faisant que décroître, une partie ne
// revient jamais à une phase antérieure.
Phase phaseOf(const chess::Board &board, int ply);

// Évaluation d'une position, du point de vue des Blancs. `known` est faux pour une
// séquence de mat, qui ne se convertit pas en chances de gain.
struct NodeScore {
    bool known = false;
    double centipawns = 0;
};

// Bilan chiffré d'une partie. Indice 0 pour les Blancs, 1 pour les Noirs ; les phases
// suivent l'ordre de Phase, et `all` porte la partie entière. Une note vaut -1 quand
// elle n'est pas mesurable, ce qui ne se confond pas avec un zéro.
struct AccuracySummary {
    double all[2]{-1, -1};
    double phase[2][3]{{-1, -1, -1}, {-1, -1, -1}};
    int moves[2][4]{};
    int blunders[2]{}, mistakes[2]{}, inaccuracies[2]{};
};

// Note une partie à partir des évaluations de sa ligne principale. `nodes` est la ligne
// principale, `scores` porte une entrée par nœud dans le même ordre. Un coup n'est noté
// que si les positions d'avant et d'après sont toutes deux évaluées.
AccuracySummary summarize(const Game &game, const std::vector<int> &nodes,
                          const std::vector<NodeScore> &scores);

} // namespace t64
