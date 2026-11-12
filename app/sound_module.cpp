// SPDX-License-Identifier: AGPL-3.0-or-later
// Module sonore : la seule partie de Trait64 liée à Qt Multimedia. Voir app/sounds.cpp.
#include <QCoreApplication>
#include <QSoundEffect>
#include <QUrl>
#include <QtGlobal>
#include <array>

extern "C" Q_DECL_EXPORT void t64_sound_play(int sound) {
    // Créés au premier coup, puis gardés : un effet prêt part sans délai. Ils appartiennent
    // à l'application, qui les détruit avant la pile audio et non après.
    static const std::array<QSoundEffect *, 3> effects = [] {
        std::array<QSoundEffect *, 3> made{};
        const char *const names[] = {"move", "capture", "check"};
        for (size_t i = 0; i < made.size(); ++i) {
            made[i] = new QSoundEffect(QCoreApplication::instance());
            made[i]->setSource(QUrl(QString("qrc:/assets/sounds/%1.wav").arg(names[i])));
            made[i]->setVolume(0.7);
        }
        return made;
    }();
    if (sound >= 0 && sound < int(effects.size()))
        effects[size_t(sound)]->play();
}
