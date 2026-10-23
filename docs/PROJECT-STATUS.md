# Trait64 — état du projet au 23 octobre 2026

> **Alpha 0.3 · C++23 / Qt6/QML · SQLite · Linux d’abord · AGPL-3.0-or-later**

Ce document présente **le code fusionné dans `main` et les versions réellement publiées**. Il ne remplace ni le [suivi technique](NEXT-STEPS.md) ni la [feuille de route complète](ROADMAP.md). L’interface, le moteur et les données utilisateur doivent continuer à fonctionner localement, sans compte ou connexion obligatoire.

## Dernière version publiée et vérifiée

**[v0.3.0-dev.52](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.52)** — publication le **21 octobre 2026** ; audit de publication documenté le 22 octobre. La release contient l’AppImage Linux x86_64, les paquets Debian 12 et 13, les sources de redistribution et les empreintes SHA-256.

La [PR #9](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/9) a été fusionnée et a livré un **audit global de fiabilité**. Dix groupes de problèmes ont été corrigés : récupération des sessions, réindexation annulable, bornes FEN, réponses UCI réentrantes, fin de partie et nulles réclamables, anciens caches, options UCI, limites de pendule et durée de vie du système de mises à jour pendant l’authentification.

**Contrôles publiés** : 12/12 suites en CI sous Debian 12 et 13, ASan/UBSan sur le cœur et les modules Qt, 3 000 mutations FEN/PGN, installation puis démarrage des deux paquets Debian et test de démarrage AppImage. Voir [DEBUG-AUDIT.md](DEBUG-AUDIT.md) et [TESTING.md](TESTING.md). Ces tests ne remplacent **pas** un essai graphique sur Voyager/Wayland avec le matériel de l’utilisateur.

## Ce qui est intégré

| Domaine | Capacités réellement développées |
| --- | --- |
| Échiquier et études | Coups légaux, variantes, Chess960, FEN, PGN annoté, flèches/cases, onglets, fusion et sauvegarde |
| Moteurs | UCI, Stockfish/Arasan/PlentyChess téléchargeables, profils indépendants, MultiPV et comparaison de 2 à 4 moteurs |
| Analyse | Graphique et bilan, file persistante avec analyses de variantes, pause/reprise, cache contextualisé et export |
| Comprendre les coups | Variantes légalement rejouées, matériel/captures ; fourchettes, clouages/enfilades du roi, échecs à la découverte et doubles échecs, avec repères PGN |
| Bibliothèques | SQLite schéma 7, bases nommées, recherches combinées annulables, pagination par curseur, index de positions, import PGN parallèle et reprenable |
| Ouvertures et préparation | ECO, répertoires par position/transposition, import/export, couverture locale, préparation filtrée et export CSV |
| Entraînement | Cartes issues d’études/erreurs et révisions espacées simples des répertoires |
| Fiabilité et distribution | Sauvegardes automatiques, migrations sauvegardées, 3 canaux de mises à jour GitHub, CI Debian, AppImage et nettoyage des archives Actions |

**Limites importantes :** les motifs détectés ne démontrent pas toujours un gain forcé ; les analyses ne prétendent pas attribuer un **niveau Elo fictif**. Le jeu multijoueur en ligne reste hors périmètre.

## Développement en cours : ne pas confondre avec une release

La [PR #10 — Import PGN et qualification](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/10) est **ouverte au moment de ce relevé (23 octobre 2026)** et **n’est pas encore intégrée à `main`**. Elle propose des optimisations du dédoublonnage et de l’analyse du matériel, une mesure plus complète de la restauration et des sondes de réactivité Qt.

Sa description indique, sur **10 000 parties Lichess CC0 / 661 314 positions**, une évolution de **93,114 s à 42,498 s**, soit **−54,4 % dans ce protocole**. Cette mesure est **un résultat de la branche de travail**, pas une performance certifiée de dev.52 ni une validation sur des millions de parties. Les tests et benchmarks de plus grande ampleur restent à terminer. Suivre les résultats et la fusion de la PR avant d’actualiser le statut publié.

## Principaux travaux restants

1. **Grandes bases réelles** : qualifier 100 000 puis 1/2/5/10 millions de parties, indexations, mémoire, recherches, sauvegardes et restauration ; optimiser à partir des mesures.
2. **Pédagogie** : compléter les explications tactiques démontrables, variantes justifiées, thèmes et exercices ; développer un véritable corpus tactique libre et sa progression.
3. **Répertoires et adversaires** : couverture probabiliste, comparaison de répertoires, tendances et dossiers de préparation.
4. **Interopérabilité** : import volontaire Lichess/Chess.com, études, Polyglot, formats compatibles SCID/ChessBase.
5. **Entraînement** : finales, calcul, visualisation, solutions équivalentes vérifiées et répétition espacée enrichie.
6. **Fiabilité et diffusion** : restauration guidée, tests réels sur Voyager/Wayland/X11, Syzygy local, traductions puis éventuels ports Windows/macOS/Flatpak.

**À ne pas annoncer comme livrés :** performances certifiées sur plusieurs millions de PGN complets, OCR, intégration Lichess/Chess.com, format T64 compact définitif, tests matériels Voyager, version stable 1.0 ou compatibilité Windows/macOS.

## Historique des jalons vérifiés

| Version | Évolution principale |
| --- | --- |
| [dev.52](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.52) | Audit massif, corrections de fiabilité et tests ASan/UBSan élargis |
| [dev.46](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.46) | Motifs tactiques, flèches et annotations PGN |
| [dev.43](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.43) | Compréhension des variantes du bilan |
| [dev.42](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.42) | Répertoires asynchrones et publication directe sans archives Actions |
| [dev.33](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.33) | Imports reprenables, répertoires et préparation |
| [dev.25](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.25) | Catalogue moteurs/bases et recherches annulables |

Les changements précis sont détaillés dans les [Releases](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases), le [changelog](CHANGELOG.md), la [roadmap](ROADMAP.md) et les [tests](TESTING.md).

## Principes de communication

- Différencier **codé**, **testé localement**, **validé en CI**, **testé sur le PC cible** et **publié**.
- Ne présenter ni les mesures synthétiques ni une PR ouverte comme une fonctionnalité stabilisée.
- Ne pas attribuer un Elo fictif d’après une seule partie.
- Ne pas promettre le jeu en ligne : Trait64 est un atelier local d’analyse et de préparation.
- Respecter la licence AGPL-3.0-or-later et les licences de tous les composants et corpus externes.
