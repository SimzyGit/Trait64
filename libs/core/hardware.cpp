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
    return std::clamp(static_cast<int>(detected) - 1, 1, 32);
}

int recommendedImportWorkers() { return std::min(8, recommendedThreads()); }

int recommendedHashMb() {
    const long long memory = installedMemoryMb();
    if (memory <= 0)
        return 128; // Mémoire inconnue : on s'en tient à la valeur prudente d'autrefois.
    // Un huitième de la mémoire installée, arrondi à la puissance de deux inférieure —
    // la forme qu'attendent les moteurs — pour laisser de quoi tenir la base ouverte.
    const long long share = memory / 8;
    int hash = 64;
    while (hash < 4096 && static_cast<long long>(hash) * 2 <= share)
        hash *= 2;
    return hash;
}

} // namespace t64
