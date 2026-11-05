#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Chaque phrase montrée au joueur doit avoir sa traduction anglaise.

Le logiciel est écrit en français ; l'anglais vient de translations/en.json, cherché
par le texte source. Une phrase sans entrée retombe sur le français : l'interface
anglaise se met alors à parler français par endroits, et cela ne se voit qu'à l'usage.
Ce test lit les sources plutôt que l'intention, et nomme ce qui manque.
"""
import json
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parent.parent
translations = json.loads((root / "translations" / "en.json").read_text(encoding="utf-8"))

# Des noms de clés et des valeurs techniques traversent les mêmes appels que les
# phrases ; les traduire serait une erreur, pas un oubli.
TECHNICAL = {"reason", "known", "title", "ok", "PNG", "Smoke: save/load mismatch"}

# tr(...) en C++ et qsTr(...) en QML : ce sont les deux portes de la traduction.
SOURCES = {
    "app/*.cpp": (
        r'\btr\("((?:[^"\\]|\\.)*)"\)',
        # Le texte d'une exception part tel quel vers l'affichage, qui le traduit.
        r'runtime_error\("((?:[^"\\]|\\.)*)"\)',
        r'require\([^;]*?,\s*"((?:[^"\\]|\\.)*)"\s*\)',
    ),
    "ui/*.qml": (r'qsTr\("((?:[^"\\]|\\.)*)"\)',),
}


def wanted():
    for pattern, expressions in SOURCES.items():
        for path in sorted(root.glob(pattern)):
            text = path.read_text(encoding="utf-8")
            for expression in expressions:
                for found in re.findall(expression, text):
                    phrase = found.replace('\\"', '"').replace("\\n", "\n")
                    if re.search(r"[a-zA-Zà-ÿÀ-Ÿ]", phrase) and phrase not in TECHNICAL:
                        yield path.relative_to(root), phrase


missing = sorted({(str(path), phrase) for path, phrase in wanted() if phrase not in translations})
if missing:
    for path, phrase in missing:
        print(f"{path}: pas de traduction pour {phrase!r}", file=sys.stderr)
    print(f"\n{len(missing)} phrase(s) sans traduction anglaise.", file=sys.stderr)
    raise SystemExit(1)

print(f"{len(set(p for _, p in wanted()))} phrases, toutes traduites")
