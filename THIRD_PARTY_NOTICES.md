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

## SQLite et polices

SQLite est utilisé par le greffon Qt SQL système (https://sqlite.org/). Les pièces sont les caractères Unicode de la police système DejaVu Sans, installée séparément (https://dejavu-fonts.github.io/). Aucune police ni image de tiers n’est copiée dans le dépôt.

Les exemples PGN sont des lignes pédagogiques écrites pour ce projet. Aucune base propriétaire ChessBase ni aucun actif de marque Lichess/En Croissant n’a été copié.
