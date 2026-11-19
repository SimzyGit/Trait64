// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QString>
#include <atomic>
#include <functional>
#include <memory>
#include <string>

class QIODevice;

namespace t64 {
// Les bases de SCID et de Scid vs. PC au format 4 : trois fichiers côte à côte, .si4
// (l'index : une fiche de 47 octets par partie), .sn4 (les noms) et .sg4 (les coups).
// Chaque coup tient sur un octet : le rang de la pièce dans la liste que SCID tient pour
// chaque camp, et quatre bits qui disent où elle va. On rejoue cette liste à l'identique,
// on relit les parties en PGN, et le reste du logiciel les importe comme d'habitude.
// Le format 5 (.si5, .sn5, .sg5, celui de SCID 5) se lit aussi : l'index et les noms y
// sont rangés autrement, les parties sont codées de la même façon.
namespace scid {
// Le chemin de l'index, quel que soit celui des trois fichiers que l'on a choisi.
QString indexPath(const QString &path);
// Vrai si les trois fichiers sont là et que l'index a la forme de son format.
bool isScidBase(const QString &path);
// Nombre de parties annoncé par l'index, 0 s'il ne se lit pas.
int games(const QString &path);
struct Outcome {
    int written = 0, skipped = 0;
};
// Écrit tout le PGN dans `out` ; `progress` reçoit les parties écrites et le total. Une
// partie qu'on ne sait pas relire est comptée et passée, sans arrêter le reste.
Outcome exportPgn(const QString &path, QIODevice &out, const std::function<void(int, int)> &progress = {},
                  const std::shared_ptr<std::atomic_bool> &canceled = {});
} // namespace scid
} // namespace t64
