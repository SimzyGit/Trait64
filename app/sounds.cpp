// SPDX-License-Identifier: AGPL-3.0-or-later
// Aucune bibliothèque audio embarquée : Qt Multimedia amenait FFmpeg, GStreamer et des
// dizaines de mégaoctets pour quelques bruits de bois. Chaque système sait jouer un court
// fichier WAV ; on s'en sert. Sous Linux, le client PulseAudio vit dans un module à part,
// chargé au premier coup : un système qui ne l'a pas démarre quand même, muet.
#include "sounds.h"
#define MINIMP3_IMPLEMENTATION
#include <minimp3.h>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    return {"bois",        "feutre",        "clic",        "marbre",           "retro",
            "lichess-sfx", "lichess-piano", "lichess-nes", "lichess-futuriste"};
}
namespace {
const char *const names[] = {"move", "capture", "check", "castle", "promote", "mate"};
// Dans un dossier personnel, les noms acceptés pour chaque son, dans l'ordre : ceux de
// Trait64, puis ceux de Lichess (« Move », « Checkmate ») et de Chess.com (« move-self »,
// « move-check », « game-end »), puis un son voisin. Les sons standard de Lichess et ceux
// de Chess.com ne sont pas libres : on ne les livre pas, l'utilisateur pose les siens.
const QStringList aliases[] = {{"move", "move-self"},
                               {"capture"},
                               {"check", "move-check", "move", "move-self"},
                               {"castle", "move", "move-self"},
                               {"promote", "promotion", "move", "move-self"},
                               {"mate", "checkmate", "game-end", "victory", "check", "move-check"}};
// Un MP3 (les jeux de Lichess et de Chess.com sont en MP3) décodé en WAV PCM 16 bits.
QByteArray mp3ToWav(const QByteArray &mp3) {
    mp3dec_t decoder;
    mp3dec_init(&decoder);
    mp3dec_frame_info_t info{};
    std::array<mp3d_sample_t, MINIMP3_MAX_SAMPLES_PER_FRAME> frame{};
    QByteArray pcm;
    int channels = 0, rate = 0;
    const auto *data = reinterpret_cast<const uint8_t *>(mp3.constData());
    for (qsizetype at = 0; at < mp3.size();) {
        const int samples = mp3dec_decode_frame(&decoder, data + at, int(mp3.size() - at), frame.data(), &info);
        if (info.frame_bytes <= 0)
            break;
        at += info.frame_bytes;
        if (samples <= 0)
            continue;
        if (!channels) {
            channels = info.channels;
            rate = info.hz;
        }
        if (info.channels != channels)
            continue;
        pcm.append(reinterpret_cast<const char *>(frame.data()), qsizetype(samples) * channels * 2);
    }
    if (pcm.isEmpty() || channels < 1 || rate < 8000)
        return {};
    QByteArray wav;
    const auto put32 = [&](quint32 v) { wav.append(reinterpret_cast<const char *>(&v), 4); };
    const auto put16 = [&](quint16 v) { wav.append(reinterpret_cast<const char *>(&v), 2); };
    wav.append("RIFF");
    put32(quint32(36 + pcm.size()));
    wav.append("WAVEfmt ");
    put32(16);
    put16(1);
    put16(quint16(channels));
    put32(quint32(rate));
    put32(quint32(rate * channels * 2));
    put16(quint16(channels * 2));
    put16(16);
    wav.append("data");
    put32(quint32(pcm.size()));
    return wav + pcm;
}
// Un WAV PCM 16 bits : le seul que scaled() sache régler et que tous les systèmes jouent.
bool pcm16(const QByteArray &wav) {
    if (wav.size() < 44 || !wav.startsWith("RIFF") || wav.mid(8, 4) != "WAVE")
        return false;
    for (qsizetype at = 12; at + 8 <= wav.size();) {
        quint32 length = 0;
        std::memcpy(&length, wav.constData() + at + 4, 4);
        if (std::memcmp(wav.constData() + at, "fmt ", 4) == 0 && length >= 16 && at + 24 <= wav.size()) {
            quint16 format = 0, bits = 0;
            std::memcpy(&format, wav.constData() + at + 8, 2);
            std::memcpy(&bits, wav.constData() + at + 22, 2);
            return format == 1 && bits == 16;
        }
        at += 8 + qsizetype(length + (length & 1));
    }
    return false;
}
// Le fichier d'un son dans un dossier choisi par l'utilisateur, ou rien.
QByteArray customClip(const QString &folder, BoardSound sound) {
    const QDir dir(folder);
    const auto files = dir.entryList({"*.wav", "*.WAV", "*.mp3", "*.MP3"}, QDir::Files);
    for (const auto &name : aliases[int(sound)])
        for (const auto &file : files)
            if (QFileInfo(file).completeBaseName().compare(name, Qt::CaseInsensitive) == 0) {
                QFile in(dir.filePath(file));
                // Un bruit de coup tient en quelques centaines de kilo-octets.
                if (in.size() > 4 * 1024 * 1024 || !in.open(QIODevice::ReadOnly))
                    continue;
                auto bytes = in.readAll();
                if (QFileInfo(file).suffix().compare("mp3", Qt::CaseInsensitive) == 0)
                    bytes = mp3ToWav(bytes);
                if (pcm16(bytes))
                    return bytes;
            }
    return {};
}
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
    // Un chemin absolu désigne un dossier de sons personnels : relu à chaque modification,
    // et complété par le bois pour ce qui manque.
    const bool custom = QDir::isAbsolutePath(theme) && !theme.startsWith(':');
    const auto chosen = custom || soundThemes().contains(theme) ? theme : QStringLiteral("bois");
    const int level = std::clamp(volume, 0, 100);
    const auto stamp = custom ? QFileInfo(theme).lastModified().toMSecsSinceEpoch() : 0;
    const auto key = QString("%1/%2/%3/%4").arg(chosen, names[int(sound)]).arg(level).arg(stamp);
    if (const auto found = cache.constFind(key); found != cache.cend())
        return *found;
    auto wav = custom ? customClip(chosen, sound) : QByteArray();
    if (wav.isEmpty()) {
        QFile file(QString(":/assets/sounds/%1/%2.wav").arg(custom ? "bois" : chosen, names[int(sound)]));
        if (file.open(QIODevice::ReadOnly))
            wav = file.readAll();
    }
    const auto clip = scaled(std::move(wav), level);
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
    // dans le cache, sous l'empreinte de ses octets (un dossier personnel a des « / »).
    static QHash<QString, SystemSoundID> ids;
    const auto key = QString::number(qHash(clip), 16);
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
