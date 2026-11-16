// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QString>
#include <atomic>
#include <functional>
#include <memory>

class QIODevice;

namespace t64 {
// Les bases ChessBase au format classique : .cbh (une fiche de 46 octets par partie),
// .cbg (les coups), .cba (les annotations), .cbp et .cbt (joueurs et tournois), .cbc
// (commentateurs). Un coup tient le plus souvent sur un octet brouillé par le nombre de
// coups déjà lus : la pièce, par son rang dans la liste que ChessBase tient pour chaque
// camp, et son déplacement. On rejoue ces listes à l'identique et l'on relit les
// parties en PGN, commentaires, signes, cases et flèches compris. Ni le format 2CBH
// (ChessBase 18), ni les archives .cbv, ni les parties Chess960 ne sont lues.
namespace chessbase {
// Le chemin de l'index .cbh, quel que soit le fichier de la base choisi.
QString indexPath(const QString &path);
// Vrai si .cbh, .cbg, .cbp et .cbt sont là et que l'index a la forme attendue.
bool isChessBase(const QString &path);
// Fiches de l'index (parties et textes, effacées comprises), 0 s'il ne se lit pas.
int records(const QString &path);
struct Outcome {
    int written = 0, skipped = 0;
};
// Écrit tout le PGN dans `out` ; `progress` reçoit les parties écrites et le total des
// fiches. Une partie qu'on ne sait pas relire est comptée et passée.
Outcome exportPgn(const QString &path, QIODevice &out, const std::function<void(int, int)> &progress = {},
                  const std::shared_ptr<std::atomic_bool> &canceled = {});
} // namespace chessbase
} // namespace t64
