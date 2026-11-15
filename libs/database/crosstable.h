// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QString>
#include <QVariantMap>
#include <vector>

namespace t64 {
// Une partie d'un tournoi, telle que le tableau croisé en a besoin.
struct CrossGame {
    qint64 id = 0;
    QString white, black, result;
    int whiteElo = 0, blackElo = 0;
    int round = 0; // 0 : ronde inconnue
};

// Le tableau d'un tournoi : classement aux points, départagé par le Sonneborn-Berger puis
// le nombre de victoires, avec la performance (Ra + 400 × (V − D) / N, sur les seuls
// adversaires classés). Rend
//   players : [{rank, name, elo, score, games, sonneborn, wins, performance,
//               cells : un texte par adversaire (« 1 », « ½0 », « × » pour soi),
//               cellGames : les parties de chaque case,
//               rounds : par ronde « 12○1 » (rang de l'adversaire, ○ Blancs ● Noirs, résultat)}]
//   rounds : le nombre de rondes connues, 0 si aucune
//   matrix : vrai si le tableau carré se lit (au plus 20 joueurs, tous se sont rencontrés
//            au moins à moitié) ; sinon on montre les rondes, comme un open suisse.
QVariantMap crosstable(const std::vector<CrossGame> &games);
} // namespace t64
