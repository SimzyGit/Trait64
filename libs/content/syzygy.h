// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QString>
#include <QVariantMap>

namespace t64 {
// Les tables Syzygy posées sur l'ordinateur, lues par Fathom : le verdict d'une finale
// sans réseau, jusqu'au nombre de pièces des tables présentes. Le dossier est celui que
// reçoit aussi le moteur. Un seul dossier ouvert à la fois, pour tout le programme.
namespace syzygy {
struct Tables {
    int files = 0;   // fichiers .rtbw trouvés
    int largest = 0; // pièces de la plus grande table, 0 si aucune
};
// Ouvre le dossier s'il a changé, et dit ce qu'il contient. Vide : ferme les tables.
Tables open(const QString &folder);
// Même plan que OpeningExplorer::parseTablebase, avec « local » en plus. Vide si les
// tables ouvertes ne couvrent pas la position (trop de pièces, roque encore permis,
// fichier manquant) : on demande alors à Lichess.
QVariantMap probe(const QString &fen);
} // namespace syzygy
} // namespace t64
