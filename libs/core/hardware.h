// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

namespace t64 {

// Mémoire vive installée, en mégaoctets, ou 0 si le système ne la révèle pas.
long long installedMemoryMb();

// Valeurs de départ d'un moteur UCI sur cette machine. Les anciennes valeurs fixes —
// un fil et 128 Mo — n'utilisaient qu'une fraction d'un processeur moderne.
int recommendedThreads();
int recommendedHashMb();
// Ce que Trait64 proposait avant de lever ses plafonds : un huitième de la mémoire,
// 4 Go au plus. Sert seulement à reconnaître un réglage jamais touché.
int legacyHashMb();

// Lecteurs PGN en parallèle. Même raisonnement, mais la lecture sature le disque bien
// avant le processeur : au-delà de huit, on n'y gagne plus rien.
int recommendedImportWorkers();

} // namespace t64
