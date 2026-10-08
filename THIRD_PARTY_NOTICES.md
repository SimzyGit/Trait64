# Composants tiers

## chess-library

- Source : https://github.com/Disservin/chess-library
- Révision copiée : `53e6a841dcda7059a2af363d85f785ef1817304a` (2026-07-26).
- Fichier : `third_party/chess-library/chess.hpp`, avec une adaptation de compatibilité GCC 12 documentée dans `third_party/chess-library/PATCHES.md`.
- SHA-256 du fichier amont avant adaptation : `f2c8e2e929641e2c71cbe9d8abd718cf3cac46c2a34531215ebd733905e98d7f`.
- Licence MIT, texte intégral conservé dans `third_party/chess-library/LICENSE`.
- Utilisé pour les règles, coups légaux, SAN/UCI, représentation des positions et fins de partie. Le parseur PGN avec arbre de variantes est propre à Trait64.

## Qt 6

https://www.qt.io/ — modules Core, Gui, Qml, Quick, QuickControls2, Sql, Concurrent ; Test pour les tests. Bibliothèques système liées dynamiquement. Les licences des modules Qt distribués par Debian/Ubuntu s’appliquent (notamment LGPL-3.0/GPL-3.0 selon le module et sa distribution). Les sources et avis légaux Qt sont disponibles via le gestionnaire de paquets et https://code.qt.io/. Aucun fichier de Qt n’est incorporé dans les sources de Trait64.

## Stockfish

https://stockfishchess.org/ et https://github.com/official-stockfish/Stockfish — GPL-3.0. Moteur externe, non incorporé et non modifié. L’installateur demande le paquet `stockfish` de la distribution. La communication utilise UCI. Le niveau « Skill Level » est spécifique aux moteurs qui exposent cette option. Les sources correspondantes au paquet installé sont disponibles dans les dépôts source de la distribution.

## Jeux de pièces SVG

48 fichiers provenant de [lichess-org/lila](https://github.com/lichess-org/lila), révision `5ae58154b2be1033dcb0252173b4fff4bb02e73a`, réutilisés selon leurs licences propres :

| Dossier | Auteur | Licence |
|---|---|---|
| `assets/pieces/cburnett` | Colin M. L. Burnett | GPL-2.0-or-later |
| `assets/pieces/merida` | Armando Hernandez Marroquin | GPL-2.0-or-later |
| `assets/pieces/spatial` | Maurizio Monge | MIT |
| `assets/pieces/pixel` | therealqtpi | AGPL-3.0-or-later |

Notices amont conservées dans `assets/pieces/LICHESS-COPYING.md`. Textes des licences GPL-2.0, AGPL-3.0 et MIT-Monge inclus dans ce dossier et installés avec le logiciel. Les SVG ne sont pas modifiés. `assets/UPSTREAM.json` donne URL, taille et SHA-256 des ressources copiées. La présence d’autres noms dans la notice amont complète n’indique pas leur incorporation dans Trait64.

## Dictionnaire d’ouvertures

[lichess-org/chess-openings](https://github.com/lichess-org/chess-openings), révision `65bb03f76c7f077984db01a2f2d0534e4181ddfe`, mise à disposition CC0-1.0. Sources `a.tsv` à `e.tsv`, notice amont et texte CC0 conservés dans `assets/openings`. `positions.tsv` est dérivé par `tools/build_opening_index.cpp` (3 865 positions). Les appellations d’ouverture restent celles de la source, généralement en anglais.

## SQLite et polices

SQLite est utilisé par le greffon Qt SQL système (https://sqlite.org/). Le style Unicode utilise la police système DejaVu Sans installée séparément (https://dejavu-fonts.github.io/). Aucune police n’est copiée dans le dépôt. Le greffon SVG de Qt est une dépendance système.

Les exemples PGN sont des lignes pédagogiques écrites pour ce projet. Aucune base propriétaire ChessBase ni aucun actif de marque En Croissant n’est incorporé. Les ressources libres citées ci-dessus n’impliquent aucune affiliation à leurs auteurs ni à Lichess.
