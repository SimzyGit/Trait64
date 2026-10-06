// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QString>
namespace t64 {
// T64 experimental profile 0.1. PGN snapshots; not the future Move16 database.
class Archive {
  public:
    static QByteArray read(const QString &path);
    static void append(const QString &path, const QByteArray &pgn);
};
} // namespace t64
