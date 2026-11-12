// SPDX-License-Identifier: AGPL-3.0-or-later
#include "hardware.h"

#include <algorithm>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <unistd.h>
#endif

namespace t64 {

long long installedMemoryMb() {
    constexpr long long megabyte = 1024 * 1024;
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status))
        return 0;
    return static_cast<long long>(status.ullTotalPhys / megabyte);
#elif defined(__APPLE__)
    int name[2]{CTL_HW, HW_MEMSIZE};
    uint64_t bytes = 0;
    size_t size = sizeof(bytes);
    if (sysctl(name, 2, &bytes, &size, nullptr, 0) != 0)
        return 0;
    return static_cast<long long>(bytes / megabyte);
#else
    const long pages = sysconf(_SC_PHYS_PAGES), page = sysconf(_SC_PAGE_SIZE);
    if (pages <= 0 || page <= 0)
        return 0;
    return static_cast<long long>(pages) * page / megabyte;
#endif
}

int recommendedThreads() {
    // Un cœur reste libre : une analyse de fond ne doit pas rendre la machine poussive.
    // hardware_concurrency() répond 0 quand elle l'ignore, d'où le plancher.
    const unsigned detected = std::thread::hardware_concurrency();
    // Pas d'autre plafond que celui de Stockfish (1 024) : limiter à 32 bridait les
    // stations de travail, qui sont justement celles qu'on achète pour analyser.
    return std::clamp(static_cast<int>(detected) - 1, 1, 1024);
}

int recommendedImportWorkers() {
    return std::min(8, recommendedThreads());
}

int legacyHashMb() {
    const long long memory = installedMemoryMb();
    if (memory <= 0)
        return 128;
    int hash = 64;
    while (hash < 4096 && static_cast<long long>(hash) * 2 <= memory / 8)
        hash *= 2;
    return hash;
}

int recommendedHashMb() {
    const long long memory = installedMemoryMb();
    if (memory <= 0)
        return 128; // Mémoire inconnue : on s'en tient à la valeur prudente d'autrefois.
    // Un quart de la mémoire installée, arrondi à la puissance de deux inférieure — la
    // forme qu'attendent les moteurs. La table de transposition est ce qui permet au
    // moteur d'aller profond sans recalculer : un huitième plafonné à 4 Go la laissait
    // se remplir en une minute d'analyse sur une machine à 32 Go. Le quart laisse
    // encore de quoi tenir la base ouverte, et le plafond de 32 Go n'est là que pour
    // les machines où un quart dépasserait ce qu'une analyse sait remplir.
    const long long share = memory / 4;
    int hash = 64;
    while (hash < 32768 && static_cast<long long>(hash) * 2 <= share)
        hash *= 2;
    return hash;
}

} // namespace t64
