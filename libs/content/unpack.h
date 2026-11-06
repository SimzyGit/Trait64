// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QString>

namespace t64 {
// Rend le contenu d'un fichier, déplié s'il est compressé (gzip, zstd, bzip2, xz). Le
// format se reconnaît aux premiers octets et non à l'extension : un « .pgn » qui est en
// réalité un gzip s'ouvre alors, et un « .pgn.gz » qui ne l'est pas aussi. `limit` borne
// la taille rendue — la taille dépliée n'étant pas annoncée, on s'arrête dès le
// dépassement plutôt que de laisser quelques kilo-octets remplir la mémoire.
QByteArray unpackIfCompressed(const QString &path, qint64 limit);
} // namespace t64
