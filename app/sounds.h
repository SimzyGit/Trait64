// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <string_view>

namespace t64 {
enum class BoardSound { Move, Capture, Check, Castle, Promote, Mate };
// Ce qu'un coup fait entendre, d'après sa notation : le mat prime sur l'échec, l'échec
// sur la promotion, la promotion sur la prise.
BoardSound soundFor(std::string_view san);
// Les jeux de sons livrés (« bois » par défaut). Partout où un jeu est attendu, un chemin
// absolu de dossier vaut aussi : ses WAV (move, capture, check, castle, promote, mate, ou
// les noms de Lichess) remplacent les sons, le bois comblant ceux qui manquent.
QStringList soundThemes();
// Le fichier WAV d'un son, au volume voulu (0 à 100) : les échantillons sont mis à
// l'échelle, PlaySound et AudioToolbox n'ayant pas de réglage de volume.
QByteArray soundClip(BoardSound sound, const QString &theme, int volume);
// Joue le son avec ce que le système fournit : PlaySound sous Windows, AudioToolbox sous
// macOS, le client PulseAudio (que PipeWire sert aussi) sous Linux. Vrai quand le son a
// été confié au système ; faux s'il n'y a pas d'écran, pas de volume ou rien pour le jouer.
bool playSound(BoardSound sound, const QString &theme = QStringLiteral("bois"), int volume = 80);
} // namespace t64
