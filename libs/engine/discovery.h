// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QString>
#include <QStringList>

namespace t64 {
// Trouver les moteurs d'échecs déjà installés sur la machine. Trait64 ne cherchait que
// « stockfish » dans le PATH et dans /usr/games, une seule fois, au tout premier
// lancement : installer Stockfish ensuite, le poser dans Téléchargements ou le lancer
// depuis un menu de bureau — où le PATH ne contient pas /usr/games — le laissait
// introuvable pour toujours.
//
// Les dossiers où l'on regarde : ceux du PATH, les emplacements des paquets de chaque
// système, et les endroits où l'on dépose un moteur téléchargé à la main.
QStringList engineSearchFolders(const QString &home, const QString &pathVariable,
                                const QStringList &extra = {});
// Les exécutables de ces dossiers, et de leurs sous-dossiers directs, dont le nom est
// celui d'un moteur connu. Stockfish d'abord, puis par nom. Aucun n'est lancé.
QStringList findEngines(const QStringList &folders);
// Vrai si ce nom de fichier est celui d'un moteur connu (« stockfish-17-avx2 »,
// « lc0.exe », « Berserk-13 »…), et non une bibliothèque, une archive ou un LISEZMOI.
bool looksLikeEngine(const QString &fileName);
} // namespace t64
