#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Le jeu d'icônes de Trait64, dessiné ici et régénéré par ce script.

Style : un trait de 1,7 sur une grille de 24, et un aplat de la même couleur à 22 %
qui porte le sens (la case, la pièce, la page). L'interface teinte l'icône entière :
l'aplat suit la couleur d'accent. Les motifs viennent de l'échiquier — cases, pion,
cavalier, couronne — plutôt que des pictogrammes génériques des sites web.

    python3 tools/icons.py assets/icons
"""
import os
import sys

STROKE = "#A4AFB9"
HEAD = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%s" '
        'stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">' % STROKE)


def tint(d):
    """L'aplat : même couleur, translucide, sans trait."""
    return f'<path d="{d}" fill="{STROKE}" fill-opacity=".22" stroke="none"/>'


def solid(d):
    return f'<path d="{d}" fill="{STROKE}" stroke="none"/>'


def pawn(x, y, s=1.0, filled=True):
    """Un pion dont la base est centrée en (x, y), 12 unités de haut à l'échelle 1.
    Plein en petit : un contour de 1,7 l'empâtait ; en contour et aplat en grand."""
    def p(dx, dy):
        return f"{x + dx * s:.2f} {y + dy * s:.2f}"
    r = 2.4 * s
    head = f"M{x - r:.2f} {y - 9.4 * s:.2f}a{r:.2f} {r:.2f} 0 1 0 {2 * r:.2f} 0a{r:.2f} {r:.2f} 0 1 0 {-2 * r:.2f} 0Z"
    body = (f"M{p(-2.6, -6.4)}H{x + 2.6 * s:.2f}L{p(1.6, -5.2)}C{p(1.6, -3.2)} {p(2.4, -2.4)} {p(4.2, -1.6)}"
            f"L{p(4.6, 0)}H{x - 4.6 * s:.2f}L{p(-4.2, -1.6)}C{p(-2.4, -2.4)} {p(-1.6, -3.2)} {p(-1.6, -5.2)}Z")
    if filled:
        return solid(head + body)
    return tint(head + body) + f'<path d="{head}"/><path d="{body}"/>'


def knight(x, y, s=1.0):
    """Un cavalier plein tourné vers la gauche, base centrée en (x, y), l'œil évidé."""
    pts = [(-4.6, 0), (-4.6, -1.6), (-1.8, -5.6), (-5.4, -6.6), (-6.3, -8.9), (-2.8, -13.4),
           (-1.8, -15.8), (-0.3, -14), (2.6, -13.3), (4.8, -9.6), (4.6, 0)]
    d = "M" + "L".join(f"{x + a * s:.2f} {y + b * s:.2f}" for a, b in pts) + "Z"
    ex, ey, er = x - 1.2 * s, y - 10.8 * s, .9 * s
    eye = f"M{ex - er:.2f} {ey:.2f}a{er:.2f} {er:.2f} 0 1 1 {2 * er:.2f} 0a{er:.2f} {er:.2f} 0 1 1 {-2 * er:.2f} 0Z"
    return f'<path d="{d}{eye}" fill="{STROKE}" fill-rule="evenodd" stroke="none"/>'


def board(x, y, w, n=2):
    """Un morceau d'échiquier n×n, cases foncées en aplat."""
    c = w / n
    dark = "".join(f"M{x + i * c:.2f} {y + j * c:.2f}h{c:.2f}v{c:.2f}h{-c:.2f}Z"
                   for i in range(n) for j in range(n) if (i + j) % 2)
    return tint(dark) + f'<rect x="{x}" y="{y}" width="{w}" height="{w}" rx="1"/>'


def page(extra="", fold=True):
    """Une feuille au coin coupé en biais, la marque des documents de Trait64."""
    return (tint("M6 3h8l5 5v13H6z") + '<path d="M6 3h8l5 5v12a1 1 0 0 1-1 1H6a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z"/>'
            + ('<path d="M14 3v5h5"/>' if fold else "") + extra)


def slabs():
    """Une base : trois plateaux d'échiquier empilés, vus de biais."""
    return (tint("M12 3l8 4-8 4-8-4z")
            + '<path d="M12 3l8 4-8 4-8-4z"/><path d="M4 11.5l8 4 8-4"/><path d="M4 16l8 4 8-4"/>'
            + '<path d="M8 5l8 4M16 5l-8 4"/>')


I = {
    # Fichiers et bases
    "database": slabs(),
    "base": slabs(),
    "new": page('<path d="M12.5 11.5v6M9.5 14.5h6"/>'),
    "file": page('<path d="M9 13h6M9 16.5h4"/>'),
    "pdf": page('<path d="M8.6 18v-4h1.3a1.2 1.2 0 0 1 0 2.4H8.6M13 14v4h.9a2 2 0 0 0 0-4z"/>'),
    "report": page('<path d="M9 18v-2.5M12 18v-5M15 18v-3.5"/>'),
    "folder": tint("M3 8h18v11H3z") + '<path d="M3 6.5V18a1 1 0 0 0 1 1h16a1 1 0 0 0 1-1V9a1 1 0 0 0-1-1h-8l-2-2.5H4a1 1 0 0 0-1 1z"/>',
    "folder-open": tint("M5 11h17l-3 8H3z") + '<path d="M3 18V6a1 1 0 0 1 1-1h5l2 2.5h7a1 1 0 0 1 1 1V11"/><path d="M3 19l2.6-7.2a1 1 0 0 1 .9-.8H21a.8.8 0 0 1 .8 1L19.3 18.4a1 1 0 0 1-.9.6z"/>',
    "import": board(13, 13, 8) + '<path d="M9 3v10M5 9l4 4 4-4"/><path d="M3 17v2a2 2 0 0 0 2 2h5"/>',
    "export": board(3, 13, 8) + '<path d="M15 13V3M11 7l4-4 4 4"/><path d="M21 17v2a2 2 0 0 1-2 2h-5"/>',
    "save": tint("M7 13h10v8H7z") + '<path d="M5 3h11l5 5v12a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V5a2 2 0 0 1 2-2z"/><path d="M7 3v5h8V3"/><path d="M7 21v-8h10v8"/>',
    "backup": tint("M7 18a4.5 4.5 0 0 1-.6-9 6 6 0 0 1 11.4 1.6A3.8 3.8 0 0 1 17 18z") + '<path d="M7 18a4.5 4.5 0 0 1-.6-9 6 6 0 0 1 11.4 1.6A3.8 3.8 0 0 1 17 18"/><path d="M12 21v-8M9 16l3-3 3 3"/>',
    "restore": tint("M7 18a4.5 4.5 0 0 1-.6-9 6 6 0 0 1 11.4 1.6A3.8 3.8 0 0 1 17 18z") + '<path d="M7 18a4.5 4.5 0 0 1-.6-9 6 6 0 0 1 11.4 1.6A3.8 3.8 0 0 1 17 18"/><path d="M12 13v8M9 18l3 3 3-3"/>',
    "sync": tint("M12 7a5 5 0 1 1 0 10 5 5 0 0 1 0-10z") + '<path d="M19.5 10A8 8 0 0 0 5 7.2L4 8.5"/><path d="M4 4v4.5h4.5"/><path d="M4.5 14A8 8 0 0 0 19 16.8l1-1.3"/><path d="M20 20v-4.5h-4.5"/>',
    "resume": tint("M12 5a7 7 0 1 1 0 14 7 7 0 0 1 0-14z") + '<path d="M4 12a8 8 0 1 0 2.4-5.7L4 8.7"/><path d="M4 4v4.7h4.7"/><path d="M12 8v4l2.8 1.8"/>',
    "quit": tint("M4 4h7v16H4z") + '<path d="M11 4H5a1 1 0 0 0-1 1v14a1 1 0 0 0 1 1h6"/><path d="M15 16l4-4-4-4M19 12H9"/>',
    "trash": tint("M6 7h12l-1 13H7z") + '<path d="M4 7h16M9 7V4h6v3"/><path d="M6 7l1 13a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1l1-13"/><path d="M10 11v6M14 11v6"/>',
    "pencil": tint("M15.5 4.5l4 4L9 19l-5 1 1-5z") + '<path d="M15.5 4.5l4 4L9 19l-5 1 1-5z"/><path d="M13.5 6.5l4 4"/>',
    "tag": tint("M3 12V4a1 1 0 0 1 1-1h8l9 9-9 9z") + '<path d="M3 12V4a1 1 0 0 1 1-1h8l9 9-9 9z"/><path d="M7.5 6.5h1v1h-1z"/>',
    # Presse-papiers et édition
    "copy": tint("M9 9h11v12H9z") + '<rect x="9" y="9" width="11" height="12" rx="1"/><path d="M15 9V4a1 1 0 0 0-1-1H5a1 1 0 0 0-1 1v11a1 1 0 0 0 1 1h4"/>',
    "paste": tint("M6 5h12v16H6z") + '<rect x="5" y="4" width="14" height="17" rx="1.5"/><path d="M9 4V2.8h6V4"/><path d="M9 11h6M9 15h4"/>',
    "undo": '<path d="M9 14L4 9l5-5"/><path d="M4 9h10.5a5.5 5.5 0 0 1 0 11H11"/>' + tint("M11 16h3.5a1.5 1.5 0 0 0 0-3H11z"),
    "redo": '<path d="M15 14l5-5-5-5"/><path d="M20 9H9.5a5.5 5.5 0 0 0 0 11H13"/>' + tint("M13 16H9.5a1.5 1.5 0 0 1 0-3H13z"),
    "board": board(3, 3, 18, 4),
    "info": tint("M12 3a9 9 0 1 1 0 18 9 9 0 0 1 0-18z") + '<circle cx="12" cy="12" r="9"/><path d="M12 11v6"/>' + solid("M11 7h2v2h-2z"),
    "play": tint("M12 3a9 9 0 1 1 0 18 9 9 0 0 1 0-18z") + '<circle cx="12" cy="12" r="9"/>' + pawn(12, 17.5, .62),
    "qr": '<path d="M3 3h7v7H3zM14 3h7v7h-7zM3 14h7v7H3z"/>' + solid("M5.5 5.5h2v2h-2zM16.5 5.5h2v2h-2zM5.5 16.5h2v2h-2zM14 14h3v3h-3zM18 18h3v3h-3z") + '<path d="M20 14v1M14 20h1"/>',
    "image": tint("M3 15l5-5 7 7H3z") + '<rect x="3" y="4" width="18" height="16" rx="1.5"/><circle cx="16" cy="9" r="1.6"/><path d="M3 16l5-5 8 8"/>',
    # Recherche et base
    "search": tint("M10.5 4a6.5 6.5 0 1 1 0 13 6.5 6.5 0 0 1 0-13z") + '<circle cx="10.5" cy="10.5" r="6.5"/><path d="M15.5 15.5l5 5"/>' + solid("M8 8h2.5v2.5H8zM10.5 10.5H13V13h-2.5z"),
    "position": board(3, 3, 12, 3) + '<circle cx="17" cy="17" r="3"/><path d="M19.2 19.2l2 2"/>',
    "duplicates": tint("M3 3h12v12H3z") + '<rect x="3" y="3" width="12" height="12" rx="1"/><rect x="9" y="9" width="12" height="12" rx="1"/><path d="M12 15h6M15 12v6"/>',
    "merge": '<path d="M6 3v5a6 6 0 0 0 6 6 6 6 0 0 0 6-6V3"/><path d="M12 14v7M9 18l3 3 3-3"/>' + tint("M6 8a6 6 0 0 0 12 0z"),
    "reference": tint("M5 4h13v14H5z") + '<path d="M4 19V5a2 2 0 0 1 2-2h12v15H6a2 2 0 0 0-2 2 2 2 0 0 0 2 2h12"/>' + '<path d="M8.5 13l1-5 2 2.6L13.5 8l1 5z"/>',
    "live": tint("M12 9a3 3 0 1 1 0 6 3 3 0 0 1 0-6z") + '<circle cx="12" cy="12" r="2.6"/><path d="M7.5 7.5a6.4 6.4 0 0 0 0 9M16.5 16.5a6.4 6.4 0 0 0 0-9M4.6 4.6a10.4 10.4 0 0 0 0 14.8M19.4 19.4a10.4 10.4 0 0 0 0-14.8"/>',
    "accounts": tint("M2.5 20a6.5 6.5 0 0 1 13 0z") + '<circle cx="9" cy="8" r="3.5"/><path d="M2.5 20a6.5 6.5 0 0 1 13 0"/><path d="M16 4.6a3.5 3.5 0 0 1 0 6.8M18 14.5a6.5 6.5 0 0 1 3.5 5.5"/>',
    "compare": '<path d="M12 3v18M7 21h10"/>' + tint("M3 14l3-7 3 7a3 3 0 0 1-6 0zM15 14l3-7 3 7a3 3 0 0 1-6 0z") + '<path d="M5 6.5h14M3 14l3-7 3 7a3 3 0 0 1-6 0zM15 14l3-7 3 7a3 3 0 0 1-6 0z"/>',
    "trophy": tint("M8 4h8v5a4 4 0 0 1-8 0z") + '<path d="M8 4h8v5a4 4 0 0 1-8 0z"/><path d="M8 5.5H4.5a3 3 0 0 0 3.6 4M16 5.5h3.5a3 3 0 0 1-3.6 4"/><path d="M12 13v3.5M8 21h8M9.5 16.5h5L15 21H9z"/>',
    "chart": tint("M7 12h3v8H7zM12 8h3v12h-3zM17 14h3v6h-3z") + '<path d="M3 3v17a1 1 0 0 0 1 1h17"/><path d="M7 20v-8h3v8M12 20V8h3v12M17 20v-6h3v6"/>',
    "grid": board(3, 3, 18, 3),
    "layout": tint("M14 3h7v18h-7z") + '<rect x="3" y="3" width="18" height="18" rx="1.5"/><path d="M14 3v18M3 14h11"/>',
    # Partie et analyse
    "review": tint("M3 17l5-6 4 3 4-8 5 5v10H3z") + '<path d="M3 17l5-6 4 3 4-8 5 5"/><path d="M3 21h18"/>',
    "novelty": tint("M12 3l2.6 5.6 6 .7-4.5 4.1 1.2 6L12 16.4l-5.3 3 1.2-6-4.5-4.1 6-.7z") + '<path d="M12 3l2.6 5.6 6 .7-4.5 4.1 1.2 6L12 16.4l-5.3 3 1.2-6-4.5-4.1 6-.7z"/>' + '<path d="M12 9.5v4M10 11.5h4"/>',
    "openings": tint("M3 21V12l4-3 4 3v9z") + '<path d="M7 9V3M7 9l-4 3v9M7 9l4 3v9M11 12l5-4 5 4v9"/><path d="M16 8V5"/>',
    "analysis": tint("M10.5 4a6.5 6.5 0 1 1 0 13 6.5 6.5 0 0 1 0-13z") + '<circle cx="10.5" cy="10.5" r="6.5"/><path d="M15.5 15.5l5 5"/>' + knight(11.2, 15, .58),
    "engine": tint("M7 7h10v10H7z") + '<rect x="5" y="5" width="14" height="14" rx="1.5"/><path d="M9 2v3M15 2v3M9 19v3M15 19v3M2 9h3M2 15h3M19 9h3M19 15h3"/>' + pawn(12, 16.2, .5),
    "engines": tint("M7 7h10v10H7z") + '<rect x="5" y="5" width="14" height="14" rx="1.5"/><path d="M9 2v3M15 2v3M9 19v3M15 19v3M2 9h3M2 15h3M19 9h3M19 15h3"/>' + pawn(12, 16.2, .5),
    "stop": tint("M12 3a9 9 0 1 1 0 18 9 9 0 0 1 0-18z") + '<circle cx="12" cy="12" r="9"/>' + solid("M9 9h6v6H9z"),
    "queue": board(3, 3, 6, 2) + '<path d="M12 5h9M12 12h9M12 19h9"/><rect x="3" y="10" width="6" height="6" rx="1"/><rect x="3" y="17" width="6" height="4" rx="1"/>',
    "download": '<path d="M12 3v12M7 10l5 5 5-5"/><path d="M4 20h16"/>' + tint("M4 18h16v3H4z"),
    "sliders": '<path d="M4 6h9M17 6h3M4 12h3M11 12h9M4 18h11M19 18h1"/>' + tint("M15 4a2 2 0 1 1 0 4 2 2 0 0 1 0-4zM9 10a2 2 0 1 1 0 4 2 2 0 0 1 0-4zM17 16a2 2 0 1 1 0 4 2 2 0 0 1 0-4z") + '<circle cx="15" cy="6" r="2"/><circle cx="9" cy="12" r="2"/><circle cx="17" cy="18" r="2"/>',
    "move": '<path d="M5 19l5-5M5 19v-4M5 19h4"/>' + board(11, 3, 10, 2),
    # Entraînement
    "puzzle": tint("M10 3h4v2.5a1.5 1.5 0 0 0 3 0V3h4v7h-2.5a1.5 1.5 0 0 0 0 3H21v8h-7v-2.5a1.5 1.5 0 0 0-3 0V21H3v-8h2.5a1.5 1.5 0 0 0 0-3H3V3z") + '<path d="M10 3h4v2.5a1.5 1.5 0 0 0 3 0V3h4v7h-2.5a1.5 1.5 0 0 0 0 3H21v8h-7v-2.5a1.5 1.5 0 0 0-3 0V21H3v-8h2.5a1.5 1.5 0 0 0 0-3H3V3z"/>',
    "coordinates": board(7, 3, 14, 2) + '<path d="M3 5v.01M3 10v.01M3 15v.01M9 21h.01M14 21h.01M19 21h.01"/>',
    "endgame": tint("M8 21h8l-1-4H9z") + '<path d="M12 2.5v3M10.5 4h3"/><path d="M9 9.2a3 3 0 0 1 6 0c0 2.6-1 4.3-1 7.8h-4c0-3.5-1-5.2-1-7.8z"/><path d="M7 21h10M9 17h6"/>',
    "guess": tint("M12 3a9 9 0 1 1 0 18 9 9 0 0 1 0-18z") + '<circle cx="12" cy="12" r="9"/><path d="M9.5 9.5a2.5 2.5 0 1 1 3.5 2.3c-.6.3-1 .8-1 1.5v.7"/>' + solid("M11 16.2h2v2h-2z"),
    "help": tint("M12 3a9 9 0 1 1 0 18 9 9 0 0 1 0-18z") + '<circle cx="12" cy="12" r="9"/><path d="M9.5 9.5a2.5 2.5 0 1 1 3.5 2.3c-.6.3-1 .8-1 1.5v.7"/>' + solid("M11 16.2h2v2h-2z"),
    "cards": tint("M3 6h12v15H3z") + '<rect x="3" y="6" width="12" height="15" rx="1.5"/><path d="M7 3h11.5A2.5 2.5 0 0 1 21 5.5V17"/>' + pawn(9, 17.5, .5),
    "check": tint("M3 3h18v18H3z") + '<rect x="3" y="3" width="18" height="18" rx="2"/><path d="M7.5 12.5l3 3 6-7"/>',
    "training": tint("M12 3a9 9 0 1 1 0 18 9 9 0 0 1 0-18z") + '<circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="5"/>' + solid("M11 11h2v2h-2z"),
    # Affichage
    "theme": tint("M12 3a9 9 0 0 0 0 18z") + '<circle cx="12" cy="12" r="9"/><path d="M12 3v18"/>',
    "palette": tint("M12 3a9 9 0 0 0 0 18c1.1 0 1.8-.9 1.4-1.9-.4-1 .3-2.1 1.4-2.1H17a4 4 0 0 0 4-4c0-5.5-4-10-9-10z") + '<path d="M12 3a9 9 0 0 0 0 18c1.1 0 1.8-.9 1.4-1.9-.4-1 .3-2.1 1.4-2.1H17a4 4 0 0 0 4-4c0-5.5-4-10-9-10z"/>' + solid("M7 10h2v2H7zM9.5 6h2v2h-2zM14.5 6.5h2v2h-2z"),
    "two-boards": board(2, 7, 9, 2) + board(13, 7, 9, 2),
    "fullscreen": board(8, 8, 8, 2) + '<path d="M3 8V3h5M21 8V3h-5M3 16v5h5M21 16v5h-5"/>',
    "reset": '<path d="M4 12a8 8 0 1 0 2.4-5.7L4 8.7"/><path d="M4 4v4.7h4.7"/>' + board(9, 9, 6, 2),
    "bulb": tint("M12 3a6 6 0 0 0-3.5 10.9c.6.4 1 1.1 1 1.8V16h5v-.3c0-.7.4-1.4 1-1.8A6 6 0 0 0 12 3z") + '<path d="M9 19h6M10 22h4"/><path d="M12 3a6 6 0 0 0-3.5 10.9c.6.4 1 1.1 1 1.8V16h5v-.3c0-.7.4-1.4 1-1.8A6 6 0 0 0 12 3z"/>',
    "warning": tint("M12 3l10 18H2z") + '<path d="M10.3 4L2.6 18a2 2 0 0 0 1.7 3h15.4a2 2 0 0 0 1.7-3L13.7 4a2 2 0 0 0-3.4 0z"/><path d="M12 9v4"/>' + solid("M11 16h2v2h-2z"),
    "arrows": board(3, 13, 8, 2) + '<path d="M9 15L20 4M14 4h6v6"/>',
    "sound": tint("M11 5L6 9H3v6h3l5 4z") + '<path d="M11 5L6 9H3v6h3l5 4z"/><path d="M15.5 8.5a5 5 0 0 1 0 7M18.5 5.5a9 9 0 0 1 0 13"/>',
    "menu": '<path d="M3 6h18M3 12h18M3 18h12"/>' + tint("M17 16h4v4h-4z"),
    "settings": tint("M12 8a4 4 0 1 1 0 8 4 4 0 0 1 0-8z") + '<circle cx="12" cy="12" r="3.2"/><path d="M12 2.8l1.6 2.3 2.7-.6.6 2.7 2.4 1.4-1 2.5 1 2.5-2.4 1.4-.6 2.7-2.7-.6L12 21.2l-1.6-2.3-2.7.6-.6-2.7-2.4-1.4 1-2.5-1-2.5 2.4-1.4.6-2.7 2.7.6z"/>',
    # Aide
    "guide": tint("M2 4h6a4 4 0 0 1 4 4v13a3 3 0 0 0-3-3H2z") + '<path d="M2 4h6a4 4 0 0 1 4 4v13a3 3 0 0 0-3-3H2z"/><path d="M22 4h-6a4 4 0 0 0-4 4v13a3 3 0 0 1 3-3h7z"/>',
    "keyboard": tint("M2 6h20v12H2z") + '<rect x="2" y="6" width="20" height="12" rx="2"/><path d="M7 14h10"/>' + solid("M5.5 9h2v2h-2zM9.5 9h2v2h-2zM13.5 9h2v2h-2zM17.5 9h1.5v2h-1.5z"),
    "update": tint("M12 5a7 7 0 1 1 0 14 7 7 0 0 1 0-14z") + '<path d="M20 12a8 8 0 1 1-2.4-5.7L20 8.7"/><path d="M20 4v4.7h-4.7"/>' + pawn(12, 16, .45),
    "book": tint("M4 4.5A1.5 1.5 0 0 1 5.5 3H20v15H5.5A1.5 1.5 0 0 0 4 19.5z") + '<path d="M4 4.5A1.5 1.5 0 0 1 5.5 3H20v15H5.5A1.5 1.5 0 0 0 4 19.5z"/><path d="M4 19.5A1.5 1.5 0 0 0 5.5 21H20v-3"/>' + pawn(12, 15, .5),
    "shuffle": board(3, 3, 7, 2) + '<path d="M3 17h4l9-9h5M17 4l4 4-4 4"/><path d="M14 17h7M17 14l4 3-4 3"/>',
    # Parcours d'une partie : triangles pleins aux angles arrondis.
    "nav-first": solid("M18 6.5v11a.8.8 0 0 1-1.2.7L9.4 12.7a.8.8 0 0 1 0-1.4l7.4-5.5a.8.8 0 0 1 1.2.7z") + '<path d="M6 6v12"/>',
    "nav-back": solid("M16 6.5v11a.8.8 0 0 1-1.2.7L7.4 12.7a.8.8 0 0 1 0-1.4l7.4-5.5a.8.8 0 0 1 1.2.7z"),
    "nav-forward": solid("M8 6.5v11a.8.8 0 0 0 1.2.7l7.4-5.5a.8.8 0 0 0 0-1.4L9.2 5.8A.8.8 0 0 0 8 6.5z"),
    "nav-last": solid("M6 6.5v11a.8.8 0 0 0 1.2.7l7.4-5.5a.8.8 0 0 0 0-1.4L7.2 5.8A.8.8 0 0 0 6 6.5z") + '<path d="M18 6v12"/>',
    "nav-play": solid("M4 6.5v11a.8.8 0 0 0 1.2.7l6.4-5.5a.8.8 0 0 0 0-1.4L5.2 5.8A.8.8 0 0 0 4 6.5zM12 6.5v11a.8.8 0 0 0 1.2.7l6.4-5.5a.8.8 0 0 0 0-1.4l-6.4-5.5a.8.8 0 0 0-1.2.7z"),
    "nav-pause": solid("M7 5h3.5v14H7zM13.5 5H17v14h-3.5z"),
    "flip": '<path d="M7 20V5M4 8l3-3 3 3M17 4v15M14 16l3 3 3-3"/>' + tint("M10 9h4v6h-4z"),
    # Navigation principale
    "home": tint("M5 10l7-6 7 6v10H5z") + '<path d="M3 11l9-8 9 8"/><path d="M5 9.5V20a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V9.5"/>' + pawn(12, 19, .5),
    "library": tint("M4 4h4v16H4zM14 4l3.5-1 4 15.5-3.5 1z") + '<path d="M4 4h4v16H4zM9 4h4v16H9zM14 4.5l3.5-1 4 15.5-3.5 1z"/><path d="M4 8h4M9 8h4"/>',
    "players": pawn(12, 21.5, 1.55, filled=False),
    "profiles": tint("M3 4h18v16H3z") + '<rect x="3" y="4" width="18" height="16" rx="1.5"/>' + pawn(9, 17, .52) + '<path d="M14 10h4M14 13.5h4"/>',
    "preparation": tint("M12 3a9 9 0 1 1 0 18 9 9 0 0 1 0-18z") + '<circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="5"/><path d="M12 1.5V6M12 18v4.5M1.5 12H6M18 12h4.5"/>',
    "studies": tint("M4 4.5A1.5 1.5 0 0 1 5.5 3H20v15H5.5A1.5 1.5 0 0 0 4 19.5z") + '<path d="M4 4.5A1.5 1.5 0 0 1 5.5 3H20v15H5.5A1.5 1.5 0 0 0 4 19.5z"/><path d="M4 19.5A1.5 1.5 0 0 0 5.5 21H20v-3"/>' + knight(12.4, 15.2, .55),
    "tools": tint("M14.5 3.5a5 5 0 0 0-5.7 6.9L3 16.2 5.8 19l5.8-5.8a5 5 0 0 0 6.9-5.7l-3 3-3-.5-.5-3z") + '<path d="M14.5 3.5a5 5 0 0 0-5.7 6.9L3 16.2 5.8 19l5.8-5.8a5 5 0 0 0 6.9-5.7l-3 3-3-.5-.5-3z"/>',
}


def main(out):
    for name, body in I.items():
        with open(os.path.join(out, name + ".svg"), "w", encoding="utf-8") as f:
            f.write(HEAD + body + "</svg>\n")
    print(f"{len(I)} icônes écrites dans {out}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/icons")
