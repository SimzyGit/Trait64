// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QString>

namespace t64 {
// La clé qui rapproche deux écritures d'un même nom : sans accents, sans casse ni
// ponctuation. Pour un joueur, le nom de famille et l'initiale du prénom, qu'il soit écrit
// « Nom, Prénom » ou « Prénom Nom ». Vide si le nom est vide.
QString nameKey(const QString &name, bool player);
} // namespace t64
