# Trait64 — état du projet au 13 novembre 2026

> **Alpha 0.3 · C++23 / Qt6/QML · SQLite · Linux, Windows et macOS · AGPL-3.0-or-later**

Ce document présente **le code fusionné dans `main` et les versions réellement publiées**. Le détail des changements est dans le [journal](CHANGELOG.md), ce qui reste à faire dans la [feuille de route](ROADMAP.md), les mesures dans [PERFORMANCE.md](PERFORMANCE.md). L'interface, le moteur et les données restent utilisables localement, sans compte ni connexion obligatoire.

## Dernière version publiée

**[v0.3.0-dev.177](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.177)** — 13 novembre 2026. Construction, tests et publication Linux verts (Debian 12 et 13, sanitizers), constructions et tests Windows et macOS verts. Localement : 20 suites vertes.

Le lancement de l'AppImage est rejoué sur 12 distributions à chaque version ; la dernière passe relevée ici est verte sur dev.173.

## Ce qui est intégré

| Domaine | Capacités réellement développées |
| --- | --- |
| Interface | Navigation latérale, panneaux adaptables, clavier, thèmes, français/anglais |
| Échiquier | Glisser ou clic-clic, pièces animées, sons (paquet .deb), flèches du moteur, menace, pièces prises et avance matérielle, lecture automatique, molette en option |
| Études | Variantes, Chess960, PGN annoté, onglets, explorateur de fichiers avec arbre, corbeille du système, export PGN, EPD et page HTML autonome |
| Moteurs | UCI, détection des moteurs installés, catalogue téléchargeable, MultiPV, comparaison de 2 à 4 moteurs, recherche sans plafond de profondeur, de fils ni de mémoire autre que la machine |
| Analyse | Graphique et bilan, file persistante, cache, précision ; explication de chaque coup avec sa raison tirée des deux suites du moteur |
| Motifs | Fourchette, clouage et enfilade au roi, échec à la découverte, échec double, pièce en prise, attaque à la découverte, menace de mat en un coup |
| Bibliothèques | SQLite schéma 12, index allégé (positions sur 20 coups, signatures au-delà, recherche de position complète), import parallèle et reprenable, bases `.db3` d'En Croissant |
| En ligne (facultatif) | Explorateur Lichess, base des maîtres, tables de finales, comptes Lichess et Chess.com, lien de partie ou d'étude Lichess |
| Ouvertures et préparation | ECO, livres Polyglot, répertoires par position, dossier de position, préparation filtrée |
| Entraînement | Puzzles classés, cartes issues des erreurs, répétition des répertoires, exercice de coordonnées |
| Distribution | Paquets .deb, AppImage vérifiant son hôte, Windows et macOS construits et testés en CI, mise à jour proposée à chaque version |

## Mesures de référence

Un million de parties de maîtres réelles : import en 85 minutes sur deux cœurs, 7,5 Go, intégrité vérifiée ; page de parties en 3 ms, position du 30ᵉ coup en 1 ms. Le dossier de la position de départ y demande encore 6 à 17 s. Deux, cinq et dix millions ne sont pas mesurés.

## Limites à ne pas présenter comme levées

- Les explications reposent sur les suites calculées par le moteur : un calcul plus long peut dire autre chose. Déviations, surcharges, structures de pions et plans ne sont pas reconnus.
- L'import d'étude Lichess et les requêtes Lichess en ligne ne sont testés que par faux réseau.
- Les sons ne sont livrés que dans le .deb, et n'ont pas été écoutés lors des tests.
- Aucun essai sur le matériel réel de l'utilisateur (Voyager, Wayland, écran 120 Hz).
- Formats SCID et ChessBase, tables Syzygy locales, recherche par placement partiel, OCR, échiquiers électroniques : non faits.
- Le format `.t64` reste expérimental. Pas de version stable 1.0.

## Historique des jalons vérifiés

| Version | Évolution principale |
| --- | --- |
| [dev.177](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.177) | Pourquoi un coup est mauvais, menace, pièces prises, études Lichess, export HTML, coordonnées |
| [dev.175](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.175) | Index allégé (schéma 12), un million de parties qualifié, sons des coups |
| [dev.173](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.173) | Échiquier fluide, pièces qui glissent, détection des moteurs, flèches du moteur |
| [dev.171](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.171) | Explorateur d'études, moteur sans plafond de puissance |
| [dev.168](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.168) | Lichess en ligne (explorateur, maîtres, finales), bases `.db3` d'En Croissant, test sur 12 distributions |
| [dev.64](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.64) | Refonte native, navigation latérale, lecture personnalisable et régressions visuelles |
| [dev.57](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.57) | Vérification d'intégrité en arrière-plan et annulable, relance réentrante protégée |
| [dev.55](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.55) | Import PGN accéléré, protocole de performance et restauration |
| [dev.52](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.52) | Audit massif, corrections de fiabilité et tests ASan/UBSan élargis |
| [dev.46](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.46) | Motifs tactiques, flèches et annotations PGN |
| [dev.43](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.43) | Compréhension des variantes du bilan |
| [dev.42](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.42) | Répertoires asynchrones et publication directe sans archives Actions |
| [dev.33](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.33) | Imports reprenables, répertoires et préparation |
| [dev.25](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.25) | Catalogue moteurs/bases et recherches annulables |

Les changements précis sont détaillés dans les [Releases](https://github.com/SimzyGit/Trait64/releases), le [changelog](CHANGELOG.md), la [roadmap](ROADMAP.md) et les [tests](TESTING.md).

## Principes de communication

- Différencier **codé**, **testé localement**, **validé en CI**, **testé sur le PC cible** et **publié**.
- Ne présenter ni les mesures synthétiques ni une PR ouverte comme une fonctionnalité stabilisée.
- Ne pas attribuer un Elo fictif d’après une seule partie.
- Ne pas promettre le jeu en ligne : Trait64 est un atelier local d’analyse et de préparation.
- Respecter la licence AGPL-3.0-or-later et les licences de tous les composants et corpus externes.
