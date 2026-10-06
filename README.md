# Trait64

Un atelier d’échecs libre, local et personnel pour **étudier, analyser, préparer et mémoriser**.

**Version 0.1.0 alpha — application de bureau C++23 / Qt6, conçue d’abord pour Linux Voyager / Debian.**
Le code livré est une première application fonctionnelle. Il ne constitue pas encore l’intégralité du catalogue de fonctionnalités imaginé pour Trait64. Le périmètre exact et la suite sont décrits dans [docs/ROADMAP.md](docs/ROADMAP.md).

![Atelier d’analyse Trait64](docs/screenshots/analysis.png)

## Installer sur ton PC

Debian 12 ou 13, Ubuntu 24.04 et leurs dérivés avec Qt ≥ 6.4 sont les cibles de l’installateur. La compilation utilise les bibliothèques de **ton** système ; aucun binaire Ubuntu n’est présenté comme universel pour Debian.

1. Sur GitHub : **Code → Download ZIP / Télécharger ZIP**. Décompresse l’archive.
2. Ouvre un terminal dans le dossier `Trait64-main`.
3. Lance :

```bash
bash installer-linux.sh
```

Le script demande ton mot de passe uniquement pour installer les paquets Debian, compile, exécute les tests, puis installe Trait64 dans `~/.local`. Deux compilations simultanées par défaut pour limiter la mémoire utilisée. La première installation nécessite Internet et peut prendre plusieurs minutes. **L’utilisation du logiciel est ensuite hors ligne**, Stockfish compris.

Ouvre **Trait64** dans le menu des applications, ou :

```bash
~/.local/bin/trait64
```

Pour une mise à jour, télécharge la nouvelle version et relance le même script. Les données sont stockées hors du dossier source. N’exécute pas tout le script avec `sudo`.

## Ce qui fonctionne

- Échiquier interactif : clic ou glisser-déposer, coups légaux, promotion au choix, roques, prise en passant, coordonnées, inversion, indications d’échec et de fin de partie.
- Navigation et branches de variantes ; promotion d’une variante en ligne principale ; suppression de branche ; commentaires de position et NAG.
- PGN : plusieurs parties, variantes imbriquées, commentaires, balises personnalisées, FEN initiale ; export interopérable, copie PGN/FEN. Import en tâche de fond avec transaction globale : aucun import partiel silencieux si une partie est invalide.
- Bibliothèque SQLite : enregistrement, recherche par noms/événement/ECO/date/collection, résultat, favoris, position exacte, corbeille restaurable ; doublons PGN exacts ignorés à l’import.
- Analyse UCI locale : Stockfish ou autre moteur compatible, plusieurs variantes, évaluation du point de vue des Blancs, profondeur, vitesse, insertion d’une ligne dans l’étude. Processus asynchrone et arrêt de l’analyse précédente avant de changer de position.
- Jeu local contre Stockfish, camp au choix, force et temps de réflexion réglables.
- Explorateur d’ouvertures issu de la ligne principale de vos propres parties, statistiques de résultats.
- Cartes d’entraînement tirées de vos variantes : réponse adverse automatique, solution, suivi des réussites et répétition espacée simple.
- Thème clair/sombre, panneaux d’analyse redimensionnables, raccourcis, commandes rapides, flèches/cercles temporaires sur l’échiquier.
- Récupération automatique de la session ; conservation de l’étude modifiée avant changement ; sauvegarde complète SQLite avec les entraînements ; vérification d’intégrité.

## Premiers pas

Depuis l’accueil, **Charger les exemples**, puis ouvre une partie dans la bibliothèque. Parcours les coups avec les flèches. Reviens en arrière et joue un autre coup pour créer une variante.

Dans **Paramètres**, le chemin Stockfish est détecté si possible (souvent `/usr/games/stockfish`). Clique **Démarrer / appliquer**, puis **Analyser** dans l’atelier. L’analyse utilise le niveau maximal ; le réglage de force concerne le jeu contre le moteur.

Pour mémoriser une ligne, place-toi **avant** son premier coup, puis **Créer une carte depuis ici**. L’entraînement reprend au maximum 40 demi-coups de la continuation principale. Il vérifie la ligne choisie, et non toutes les solutions équivalentes d’un problème tactique.

| Raccourci | Action |
|---|---|
| Ctrl N | Nouvelle étude (l’ancienne étude modifiée est conservée) |
| Ctrl O | Importer |
| Ctrl S | Enregistrer dans la bibliothèque |
| Ctrl K | Commandes rapides |
| ← / → | Position précédente / suivante |
| Début / Fin | Début / fin de la ligne |
| F | Retourner l’échiquier |
| Clic droit glissé | Flèche temporaire |

## Données et formats

Par défaut sur Linux : `~/.local/share/Trait64/Trait64/` (ou le dossier XDG équivalent). Le chemin effectif est affiché dans **Paramètres**. La bibliothèque est `library.sqlite` ; `session.json` contient l’étude en cours. Les paramètres sont enregistrés dans le dossier de configuration Qt de l’utilisateur.

Utilise **Outils → Sauvegarder** pour une copie cohérente de la bibliothèque, entraînements inclus. **Ouvrir une base** ouvre une sauvegarde séparément sans écraser la base principale. Au lancement suivant, la base principale redevient active ; une étude venant d’une autre base conserve son contenu dans la session sans réutiliser son identifiant dans la mauvaise base.

PGN est le format d’échange recommandé. Le conteneur `.t64` livré est le **profil expérimental 0.1**, avec instantanés PGN, alignement, empreintes SHA-256 et deux points de récupération. Il ne prétend **pas** implémenter déjà le futur format compact Move16/TLV/BLAKE3 décrit dans la conception : voir [docs/format/T64-0.1.md](docs/format/T64-0.1.md). Un export `.t64` existant ajoute une génération ; PGN exporte le contenu courant. Les exports PGN/T64 contiennent les parties, pas les favoris, collections ou entraînements : ceux-ci nécessitent la sauvegarde SQLite.

## Compiler sans l’installateur

Avec les dépendances Qt6 et CMake installées :

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/trait64
```

Tester seulement le cœur sans Qt :

```bash
cmake -S . -B build-core -DT64_BUILD_GUI=OFF
cmake --build build-core --parallel 2
ctest --test-dir build-core --output-on-failure
```

`T64_SANITIZERS=ON` active ASan/UBSan avec GCC/Clang. `cpack --config build/CPackConfig.cmake -G DEB` produit un paquet **pour la distribution de compilation** après une compilation sur un système normalement installé. La CI construit et teste les cibles Linux. Windows/macOS ne sont pas annoncés comme validés dans cette version.

## Limites actuelles

Échecs classiques seulement. Import limité à 64 Mio et 10 000 parties à la fois ; 100 000 nœuds par étude ; 128 niveaux de variantes ; affichage de 1 000 résultats de recherche et 10 000 coups maximum. La recherche de positions/exploration indexe les lignes principales, pas toutes les variantes. Recherche SQL LIKE, sans recherche floue/accent-insensible avancée. Les exemples intégrés sont pédagogiques, pas une base exhaustive.

Pas encore : analyse automatique de partie et graphe des erreurs, tablebases Syzygy, Chess960, gestion simultanée de plusieurs moteurs, indexation de millions de parties, onglets multiples, profils de joueurs, sources Lichess/Chess.com, services communautaires, synchronisation, plugins, format T64 compact final, paquet AppImage/Flatpak validé. Le logo final n’était pas disponible parmi les fichiers retrouvés ; le monogramme actuel est provisoire.

## Licence et composants

Code Trait64 : **GNU AGPL v3 ou ultérieure**. [LICENSE](LICENSE) et [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Le nom et l’identité visuelle restent distincts de la licence du code ; voir [TRADEMARKS.md](TRADEMARKS.md).

Trait64 réutilise `chess-library` sous MIT. Qt est lié dynamiquement depuis les paquets système ; Stockfish s’exécute séparément. Aucun compte, publicité, télémétrie ou service distant n’est nécessaire. Les programmes moteurs sélectionnés s’exécutent avec les droits de l’utilisateur : choisis des exécutables de confiance.
