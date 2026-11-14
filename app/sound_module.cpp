// SPDX-License-Identifier: AGPL-3.0-or-later
// Module sonore Linux : la seule partie de Trait64 liée au client PulseAudio, que PipeWire
// sert aussi. Voir app/sounds.cpp. Pas de Qt ici : rien d'autre à embarquer.
#include <cstdint>
#include <cstring>
#include <pulse/simple.h>
#include <string>
#include <thread>

namespace {
std::uint32_t little(const char *at, int bytes) {
    std::uint32_t value = 0;
    for (int i = bytes - 1; i >= 0; --i)
        value = (value << 8) | static_cast<unsigned char>(at[i]);
    return value;
}
} // namespace

// `wav` : un fichier WAV entier, PCM 16 bits. Rendu tout de suite ; le son joue dans un
// fil à part, qui garde sa propre copie des échantillons.
extern "C" __attribute__((visibility("default"))) void t64_sound_play(const char *wav, int size) {
    if (!wav || size < 12 || std::memcmp(wav, "RIFF", 4) != 0 || std::memcmp(wav + 8, "WAVE", 4) != 0)
        return;
    pa_sample_spec spec{PA_SAMPLE_S16LE, 0, 0};
    std::string samples;
    for (int at = 12; at + 8 <= size;) {
        const auto length = little(wav + at + 4, 4);
        const int body = at + 8;
        if (length > std::uint32_t(size - body))
            return;
        if (std::memcmp(wav + at, "fmt ", 4) == 0 && length >= 16) {
            if (little(wav + body, 2) != 1 || little(wav + body + 14, 2) != 16)
                return; // autre chose que du PCM 16 bits
            spec.channels = std::uint8_t(little(wav + body + 2, 2));
            spec.rate = little(wav + body + 4, 4);
        } else if (std::memcmp(wav + at, "data", 4) == 0)
            samples.assign(wav + body, length);
        at = body + int(length + (length & 1));
    }
    if (!spec.rate || !spec.channels || samples.empty() || !pa_sample_spec_valid(&spec))
        return;
    std::thread([spec, samples = std::move(samples)] {
        int error = 0;
        pa_simple *stream = pa_simple_new(nullptr, "Trait64", PA_STREAM_PLAYBACK, nullptr, "Coup", &spec,
                                          nullptr, nullptr, &error);
        if (!stream)
            return;
        if (pa_simple_write(stream, samples.data(), samples.size(), &error) == 0)
            pa_simple_drain(stream, &error);
        pa_simple_free(stream);
    }).detach();
}
