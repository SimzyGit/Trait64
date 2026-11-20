// SPDX-License-Identifier: AGPL-3.0-or-later
// Aucune bibliothèque audio embarquée : Qt Multimedia amenait FFmpeg, GStreamer et des
// dizaines de mégaoctets pour quelques bruits de bois. Chaque système sait jouer un court
// fichier WAV ; on s'en sert. Sous Linux, le client PulseAudio vit dans un module à part,
// chargé au premier coup : un système qui ne l'a pas démarre quand même, muet.
#include "sounds.h"
#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <algorithm>
#include <array>
#include <cstring>
#ifdef Q_OS_WIN
// windows.h d'abord : mmsystem.h en dépend. La ligne vide garde cet ordre au formatage.
#include <windows.h>

#include <mmsystem.h>
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
    if (san.find('#') != std::string_view::npos)
        return BoardSound::Mate;
    if (san.find('+') != std::string_view::npos)
        return BoardSound::Check;
    if (san.find('=') != std::string_view::npos)
        return BoardSound::Promote;
    if (san.rfind("O-O", 0) == 0)
        return BoardSound::Castle;
    return san.find('x') != std::string_view::npos ? BoardSound::Capture : BoardSound::Move;
}
QStringList soundThemes() {
    return {"bois", "feutre", "clic", "marbre", "retro"};
}
namespace {
const char *const names[] = {"move", "capture", "check", "castle", "promote", "mate"};
// Les échantillons PCM 16 bits du bloc « data », multipliés par volume / 100.
QByteArray scaled(QByteArray wav, int volume) {
    if (volume >= 100 || wav.size() < 12)
        return wav;
    for (qsizetype at = 12; at + 8 <= wav.size();) {
        quint32 length = 0;
        std::memcpy(&length, wav.constData() + at + 4, 4);
        const qsizetype body = at + 8;
        if (qsizetype(length) > wav.size() - body)
            break;
        if (std::memcmp(wav.constData() + at, "data", 4) == 0) {
            auto *samples = reinterpret_cast<qint16 *>(wav.data() + body);
            for (qsizetype i = 0; i < qsizetype(length) / 2; ++i)
                samples[i] = qint16(samples[i] * volume / 100);
            break;
        }
        at = body + qsizetype(length + (length & 1));
    }
    return wav;
}
} // namespace

QByteArray soundClip(BoardSound sound, const QString &theme, int volume) {
    // Lus une fois et gardés : PlaySound et PulseAudio lisent la mémoire pendant que le
    // son joue. Une entrée par jeu, son et volume ; il n'y en a jamais qu'une poignée.
    static QHash<QString, QByteArray> cache;
    const auto chosen = soundThemes().contains(theme) ? theme : QStringLiteral("bois");
    const int level = std::clamp(volume, 0, 100);
    const auto key = QString("%1/%2/%3").arg(chosen, names[int(sound)]).arg(level);
    if (const auto found = cache.constFind(key); found != cache.cend())
        return *found;
    QFile file(QString(":/assets/sounds/%1/%2.wav").arg(chosen, names[int(sound)]));
    const auto clip = file.open(QIODevice::ReadOnly) ? scaled(file.readAll(), level) : QByteArray();
    cache.insert(key, clip);
    return clip;
}

bool playSound(BoardSound sound, const QString &theme, int volume) {
    // Sans écran il n'y a personne pour entendre, et pas toujours de carte son.
    if (QGuiApplication::platformName() == "offscreen" || volume <= 0)
        return false;
    const auto clip = soundClip(sound, theme, volume);
    if (clip.isEmpty())
        return false;
#ifdef Q_OS_WIN
    // Le cache garde ces octets en vie : PlaySound les lit pendant qu'il joue.
    return PlaySoundA(clip.constData(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#elif defined(Q_OS_MACOS)
    // AudioToolbox ne joue qu'un fichier : chaque son, à chaque volume, est posé une fois
    // dans le cache.
    static QHash<QString, SystemSoundID> ids;
    const auto key = QString("%1-%2-%3").arg(theme, names[int(sound)]).arg(volume);
    auto id = ids.value(key, 0);
    if (!id) {
        const auto folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/sounds";
        QDir().mkpath(folder);
        const auto path = QString("%1/%2.wav").arg(folder, key);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
            file.write(clip);
        file.close();
        const auto bytes = path.toUtf8();
        if (auto url = CFURLCreateFromFileSystemRepresentation(
                nullptr, reinterpret_cast<const UInt8 *>(bytes.constData()), bytes.size(), false)) {
            AudioServicesCreateSystemSoundID(url, &id);
            CFRelease(url);
        }
        ids.insert(key, id);
    }
    if (!id)
        return false;
    AudioServicesPlaySystemSound(id);
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
