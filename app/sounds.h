// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <string_view>

namespace t64 {
enum class BoardSound { Move, Capture, Check };
// Ce qu'un coup fait entendre, d'après sa notation : l'échec prime sur la prise.
BoardSound soundFor(std::string_view san);
// Joue le son avec ce que le système fournit : PlaySound sous Windows, AudioToolbox sous
// macOS, le client PulseAudio (que PipeWire sert aussi) sous Linux. Vrai quand le son a
// été confié au système ; faux s'il n'y a pas d'écran ou rien pour le jouer.
bool playSound(BoardSound sound);
} // namespace t64
