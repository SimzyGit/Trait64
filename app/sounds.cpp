// SPDX-License-Identifier: AGPL-3.0-or-later
// Les sons vivent dans un module chargé à l'exécution, pas dans le programme : Qt
// Multimedia amène avec lui une pile audio entière, et un Trait64 qui ne la trouve pas
// doit rester un Trait64 qui démarre — muet, mais entier.
#include "sounds.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLibrary>

namespace t64 {
BoardSound soundFor(std::string_view san) {
    if (san.find_first_of("+#") != std::string_view::npos)
        return BoardSound::Check;
    return san.find('x') != std::string_view::npos ? BoardSound::Capture : BoardSound::Move;
}

bool playSound(BoardSound sound) {
    using Play = void (*)(int);
    static const Play play = []() -> Play {
        // Sans écran il n'y a personne pour entendre, et pas toujours de carte son.
        if (QGuiApplication::platformName() == "offscreen")
            return nullptr;
        const auto here = QCoreApplication::applicationDirPath();
        for (const auto &folder : {here, here + "/../lib/trait64", here + "/../PlugIns"}) {
            QLibrary module(folder + "/t64sound");
            if (module.load())
                return reinterpret_cast<Play>(module.resolve("t64_sound_play"));
        }
        return nullptr;
    }();
    if (play)
        play(int(sound));
    return play != nullptr;
}
} // namespace t64
