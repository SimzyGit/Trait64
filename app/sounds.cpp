// SPDX-License-Identifier: AGPL-3.0-or-later
// Aucune bibliothèque audio embarquée : Qt Multimedia amenait FFmpeg, GStreamer et des
// dizaines de mégaoctets pour trois bruits de bois. Chaque système sait jouer un court
// fichier WAV ; on s'en sert. Sous Linux, le client PulseAudio vit dans un module à part,
// chargé au premier coup : un système qui ne l'a pas démarre quand même, muet.
#include "sounds.h"
#include <QFile>
#include <QGuiApplication>
#include <array>
#ifdef Q_OS_WIN
#include <mmsystem.h>
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <AudioToolbox/AudioToolbox.h>
#include <QDir>
#include <QStandardPaths>
#else
#include <QCoreApplication>
#include <QLibrary>
#endif

namespace t64 {
BoardSound soundFor(std::string_view san) {
    if (san.find_first_of("+#") != std::string_view::npos)
        return BoardSound::Check;
    return san.find('x') != std::string_view::npos ? BoardSound::Capture : BoardSound::Move;
}

namespace {
const char *const names[] = {"move", "capture", "check"};
// Les trois fichiers, lus une fois et gardés : PlaySound et PulseAudio lisent la mémoire
// pendant que le son joue.
const std::array<QByteArray, 3> &clips() {
    static const std::array<QByteArray, 3> loaded = [] {
        std::array<QByteArray, 3> result;
        for (size_t i = 0; i < result.size(); ++i) {
            QFile file(QString(":/assets/sounds/%1.wav").arg(names[i]));
            if (file.open(QIODevice::ReadOnly))
                result[i] = file.readAll();
        }
        return result;
    }();
    return loaded;
}
} // namespace

bool playSound(BoardSound sound) {
    // Sans écran il n'y a personne pour entendre, et pas toujours de carte son.
    if (QGuiApplication::platformName() == "offscreen")
        return false;
    const auto &clip = clips()[size_t(sound)];
    if (clip.isEmpty())
        return false;
#ifdef Q_OS_WIN
    return PlaySoundA(clip.constData(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#elif defined(Q_OS_MACOS)
    // AudioToolbox ne joue qu'un fichier : les trois sons sont posés une fois dans le cache.
    static const std::array<SystemSoundID, 3> ids = [] {
        std::array<SystemSoundID, 3> made{};
        const auto folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/sounds";
        QDir().mkpath(folder);
        for (size_t i = 0; i < made.size(); ++i) {
            const auto path = QString("%1/%2.wav").arg(folder, names[i]);
            QFile file(path);
            if (file.open(QIODevice::WriteOnly))
                file.write(clips()[i]);
            file.close();
            const auto bytes = path.toUtf8();
            if (auto url = CFURLCreateFromFileSystemRepresentation(
                    nullptr, reinterpret_cast<const UInt8 *>(bytes.constData()), bytes.size(), false)) {
                AudioServicesCreateSystemSoundID(url, &made[i]);
                CFRelease(url);
            }
        }
        return made;
    }();
    if (!ids[size_t(sound)])
        return false;
    AudioServicesPlaySystemSound(ids[size_t(sound)]);
    return true;
#else
    using Play = void (*)(const char *, int);
    static const Play play = []() -> Play {
        const auto here = QCoreApplication::applicationDirPath();
        for (const auto &folder : {here, here + "/../lib/trait64", here + "/../lib"}) {
            QLibrary module(folder + "/t64sound");
            if (module.load())
                return reinterpret_cast<Play>(module.resolve("t64_sound_play"));
        }
        return nullptr;
    }();
    if (play)
        play(clip.constData(), int(clip.size()));
    return play != nullptr;
#endif
}
} // namespace t64
