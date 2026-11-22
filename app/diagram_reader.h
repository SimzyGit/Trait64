// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QImage>
#include <QString>

namespace t64 {
// Lit un diagramme dans une image : capture d'écran d'un site, diagramme imprimé ou
// numérisé, recadré sur l'échiquier (une marge unie est tolérée). Rend une FEN, Blancs au
// trait, ou une chaîne vide en disant pourquoi dans *why. Une photo prise de biais ou une
// feuille de notation manuscrite ne se lisent pas ainsi.
QString readDiagram(const QImage &image, QString *why = nullptr);
} // namespace t64
