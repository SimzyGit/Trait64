// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QString>
#include <atomic>
#include <functional>
#include <memory>

class QIODevice;

namespace t64 {
// Les bases d'En Croissant au format SQLite — celles que le projet distribue sous
// l'extension .db3, de Lumbra's Gigabase à MillionBase. Chaque coup y tient sur un
// octet : le rang du coup dans la liste des coups légaux, telle que Shakmaty la
// produit (voir libs/core/moveorder.h). On les relit en PGN, que le reste du logiciel
// sait déjà importer.
namespace croissant {
// Vrai si le fichier porte les tables de ce format. Un SQLite quelconque n'en est pas un.
bool isCroissantBase(const QString &path);
// Le titre inscrit dans la base, ou un nom tiré du fichier.
QString title(const QString &path);
// Nombre de parties annoncé par la base, 0 si elle ne le dit pas.
int games(const QString &path);
// Écrit tout le PGN dans `out`. `progress` reçoit les parties écrites et le total.
// Rend le nombre de parties écrites ; celles qu'on ne sait pas relire sont comptées
// à part et passées, plutôt que d'arrêter l'import de neuf millions de parties sur une.
struct Outcome {
    int written = 0, skipped = 0;
};
Outcome exportPgn(const QString &path, QIODevice &out, const std::function<void(int, int)> &progress,
                  const std::shared_ptr<std::atomic_bool> &canceled = {});
// Rend le texte des coups d'une partie à partir du blob et de la position de départ.
// Exposée pour les tests : c'est là que vit le décodage.
std::string movetext(const QByteArray &blob, const std::string &fen);
} // namespace croissant
} // namespace t64
