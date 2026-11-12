// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <string_view>

namespace t64 {
enum class BoardSound { Move, Capture, Check };
// Ce qu'un coup fait entendre, d'après sa notation : l'échec prime sur la prise.
BoardSound soundFor(std::string_view san);
// Joue le son si le module sonore est installé à côté du programme ; sinon ne fait rien.
// Vrai quand le son a été confié au module.
bool playSound(BoardSound sound);
} // namespace t64
