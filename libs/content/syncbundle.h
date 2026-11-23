// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QMap>
#include <QString>

namespace t64 {
// Le paquet de synchronisation : une archive zip chiffrée en AES-256 par une phrase de
// passe, déposée dans un dossier que l'utilisateur synchronise lui-même (Syncthing,
// Nextcloud…). Écrit sous un nom temporaire puis renommé : l'autre appareil ne lit
// jamais un paquet à moitié écrit. Les deux rendent un message d'erreur, vide si tout
// s'est bien passé.
QString writeSyncBundle(const QString &path, const QMap<QString, QByteArray> &files,
                        const QString &passphrase);
QString readSyncBundle(const QString &path, const QString &passphrase, QMap<QString, QByteArray> *files);
} // namespace t64
