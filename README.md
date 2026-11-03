# Trait64

**Logiciel d’échecs libre pour Linux : bases de parties, analyse avec Stockfish, préparation d’ouvertures et entraînement. Sans compte, sans cloud, sans publicité.**

[![Version](https://img.shields.io/github/v/release/SimzyGit/Trait64?include_prereleases&label=version)](https://github.com/SimzyGit/Trait64/releases)
[![License: AGPL-3.0-or-later](https://img.shields.io/badge/licence-AGPL--3.0--or--later-blue.svg)](LICENSE)
![Linux](https://img.shields.io/badge/plateforme-Linux-FCC624.svg)
[![Linux CI](https://github.com/SimzyGit/Trait64/actions/workflows/linux.yml/badge.svg)](https://github.com/SimzyGit/Trait64/actions/workflows/linux.yml)

![Trait64 : échiquier, notation, arbre et moteur](docs/screenshots/analysis.png)

## Télécharger

**Linux (Debian, Ubuntu, Mint, Fedora…)** : une seule commande dans un terminal.

```bash
curl -fsSL https://raw.githubusercontent.com/SimzyGit/Trait64/main/install.sh | bash
```

Elle télécharge la dernière version, vérifie son empreinte SHA-256 et ajoute **Trait64** au menu des applications, sans mot de passe ni compilation. Les mises à jour se font ensuite depuis le logiciel (**Menu → Mises à jour**). Pour désinstaller : même commande suivie de `-s -- --uninstall`.

Vous préférez télécharger vous-même ? Sur la [page des versions](https://github.com/SimzyGit/Trait64/releases), prenez :

| Votre système | Fichier | Installation |
| --- | --- | --- |
| Debian 12 | `trait64_…_debian12_amd64.deb` | double-clic, ou `sudo apt install ./fichier.deb` |
| Debian 13 | `trait64_…_debian13_amd64.deb` | double-clic, ou `sudo apt install ./fichier.deb` |
| Autre Linux 64 bits | `Trait64-…-linux-x86_64.AppImage` | rendre exécutable (clic droit → Propriétés), puis ouvrir |

Windows et macOS : pas encore publiés. Le code compile déjà sur ces systèmes en intégration continue ; les installateurs viendront ensuite.

Aucun moteur n’est imposé : installez Stockfish depuis votre distribution (`sudo apt install stockfish`) ou téléchargez Stockfish, Arasan ou PlentyChess depuis **Bibliothèque → Téléchargements**.

## Ce que fait Trait64

- **Bibliothèque de parties** : import PGN rapide (même des centaines de milliers de parties), recherche par joueur, ouverture, Elo, date, position exacte ou structure de pions, fiches joueurs.
- **Vos parties Lichess et Chess.com** : enregistrez vos comptes, la synchronisation ne récupère que les nouvelles parties.
- **Analyse** : Stockfish ou tout moteur UCI, plusieurs lignes, comparaison de moteurs, bilan de partie (passage avant puis arrière) avec graphe, erreurs expliquées et motifs tactiques.
- **Ouvertures** : arbre statistique de votre base, livres Polyglot, répertoires avec transpositions, détection des nouveautés, dossier de préparation contre un adversaire.
- **Entraînement** : puzzles notés (base Lichess libre et vos propres gaffes), cartes à répétition espacée, révision de répertoire.
- **Jeu** : contre le moteur à un niveau Elo choisi, à deux avec pendule, tournois entre moteurs.
- **Vos données restent chez vous** : SQLite local, sauvegardes automatiques, PGN et FEN en entrée comme en sortie.

![Bibliothèque de parties](docs/screenshots/library.png)

**English summary —** Trait64 is a free, local-first chess database and analysis app for Linux (C++23, Qt 6): PGN libraries, UCI engines with two-pass game review, opening trees and Polyglot books, repertoires, Lichess/Chess.com account sync, rated puzzles, engine tournaments. Install: `curl -fsSL https://raw.githubusercontent.com/SimzyGit/Trait64/main/install.sh | bash`.

Projet en **alpha**, développé activement : [nouveautés](docs/CHANGELOG.md) · [état et limites](docs/PROJECT-STATUS.md) · [feuille de route](docs/ROADMAP.md).

## Comparaison avec des logiciels similaires

Trait64 ne cherche pas à reproduire un logiciel existant à l’identique : son objectif est de combiner **travail local, bases de parties, analyse moteur, préparation et entraînement** dans une interface moderne et libre. Ce tableau compare les orientations et fonctions principales ; il ne remplace pas une comparaison exhaustive des milliers de détails de chaque logiciel.

| Fonction / philosophie | **Trait64 0.3 alpha** | **ChessBase 18** | **Scid vs. PC** | **En Croissant** |
|---|:---:|:---:|:---:|:---:|
| Logiciel libre / code source ouvert | ✅ AGPL-3.0+ | ❌ Propriétaire | ✅ GPLv2 | ✅ Open source |
| Gratuit | ✅ | ❌ | ✅ | ✅ |
| Linux | ✅ Cible principale | ❌ Natif Windows | ✅ | ✅ |
| Windows / macOS | 🧪 CI de compilation, pas encore publié | ✅ Windows | ✅ | ✅ |
| Fonctionnement local hors ligne | ✅ | ✅ | ✅ | ✅ |
| Compte obligatoire pour les fonctions locales | ❌ | ❌ | ❌ | ❌ |
| Gestion de bases de parties | ✅ SQLite | ✅ Très avancée | ✅ Très avancée | ✅ |
| Import / export PGN | ✅ | ✅ | ✅ | ✅ |
| FEN | ✅ | ✅ | ✅ | ✅ |
| Recherche joueurs / événements / ECO / Elo / dates | ✅ | ✅ | ✅ | ✅ |
| Recherche de positions | ✅ | ✅ | ✅ | ✅ |
| Arbre / explorateur d’ouvertures | ✅ | ✅ | ✅ | ✅ |
| Livres d’ouvertures | ✅ Polyglot `.bin`, lecture et écriture | ✅ CTG et livres maison | ✅ Polyglot | ❌ Non documenté |
| Analyse avec moteurs UCI locaux | ✅ | ✅ | ✅ | ✅ |
| MultiPV | ✅ | ✅ | ✅ | ✅ |
| Comparaison simultanée de plusieurs moteurs | ✅ 2–4 profils | ✅ | ✅ Plusieurs moteurs | ◐ Plusieurs moteurs configurables |
| Syzygy | ✅ Via moteur compatible | ✅ | ✅ | ◐ API / moteur selon usage |
| Analyse complète d’une partie + graphe | ✅ | ✅ | ✅ | ✅ |
| Préparation d’ouvertures / répertoire | ✅ Positions, transpositions, filtres | ✅ Très avancée | ✅ | ✅ |
| Entraînement / répétition espacée | ✅ Puzzles notés, cartes et positions de répertoire | ✅ | ◐ Outils d’entraînement | ✅ Puzzles / entraînement |
| Chess960 | ✅ | ◐ | ◐ | ✅ |
| Jeu contre un moteur | ✅ | ✅ | ✅ | ✅ |
| Tournois moteur contre moteur | ✅ Toutes rondes, 2–8 profils, écart Elo | ✅ Selon outils associés | ✅ | ❌ / non central |
| Bases téléchargeables intégrées | ✅ 42 collections | ✅ Écosystème ChessBase | ◐ Import manuel / ressources | ✅ Gestionnaire intégré |
| Import direct Lichess / Chess.com | ✅ Par pseudo, incrémental sur Lichess | ✅ | ◐ | ✅ |
| Cloud requis pour l’usage principal | ❌ | ❌ mais services cloud intégrés | ❌ | ❌ |
| Sauvegardes automatiques locales | ✅ | ◐ | ◐ | ◐ |
| Format propriétaire nécessaire | ❌ | ◐ Écosystème CBH/CBV | ◐ SI4 + PGN | ❌ |
| Interface moderne pensée desktop | ✅ Qt6 | ✅ | ◐ Interface Tk historique | ✅ |
| Maturité | 🧪 **Alpha** | 🟢 Produit mature | 🟢 Projet mature | 🟢 Projet actif |

**Légende :** ✅ pris en charge · ◐ partiel, différent ou dépendant d’un composant/service · ❌ non pris en charge actuellement · 🧪 expérimental / prévu mais non validé.

> **Important :** ChessBase reste nettement plus mature sur la recherche professionnelle, les très grandes bases, la préparation d’adversaire et son écosystème de données. Scid vs. PC bénéficie de nombreuses années de développement et d’outils spécialisés. En Croissant possède déjà une excellente intégration des moteurs, bases téléchargeables et plateformes en ligne. Trait64 se différencie surtout par son approche **local-first, libre, Linux-first, sans compte ni télémétrie**, avec l’ambition de réunir ces usages dans un environnement cohérent.

Sources de référence pour cette comparaison : [ChessBase 18](https://shop.chessbase.com/en/products/chessbase_18_program_only), [Scid vs. PC](https://sourceforge.net/projects/scidvspc/), [En Croissant](https://encroissant.org/docs/). La comparaison reflète les informations publiques disponibles et les fonctions de Trait64 documentées dans ce dépôt ; elle pourra évoluer avec les versions.

## Installer : détails

L’installation rapide est décrite dans [Télécharger](#télécharger). Les fichiers publiés : une **AppImage x86_64** (glibc ≥ 2.36) et deux paquets **.deb**, pour Debian 12 et Debian 13, accompagnés de leurs empreintes `SHA256SUMS`. Sans FUSE, l’AppImage se lance avec `APPIMAGE_EXTRACT_AND_RUN=1 ./Trait64-…AppImage` ; l’installateur le règle tout seul. Sur Debian, `install.sh --deb` installe le paquet correspondant à votre version.

Après l’installation, **Menu → Mises à jour** vérifie les nouvelles versions (canaux Stable, Bêta, Développement), télécharge le fichier, vérifie son empreinte et installe après une sauvegarde. Voir [docs/UPDATES.md](docs/UPDATES.md).

### Compiler et installer depuis les sources

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

Pour mettre à jour une compilation locale, tu peux relancer le script ou utiliser le module de mise à jour qui installera l’AppImage dans `~/.local/bin/trait64`. Les données sont stockées hors du dossier source. N’exécute pas tout le script avec `sudo`.

## Ce qui fonctionne

- **Apparence** : cinq styles de pièces (Classique, Merida, Spatial, Pixel, Unicode), import de vos douze SVG, neuf palettes d’échiquier, couleurs personnelles des cases et du fond, thème clair/sombre, police et tailles, contraste renforcé, libellés français/anglais, notation figurine, coordonnées et repères réglables. Tous les choix sont mémorisés.
- **Échiquier** : clic/glisser, coups légaux, promotion au choix, échecs classiques et Chess960 (960 positions numérotées), éditeur graphique de position, inversion, flèches et cercles persistants dans le PGN.
- **Études** : arbre de variantes, promotion/suppression de branches, commentaires, NAG, métadonnées, annuler/rétablir, jusqu’à 16 onglets récupérés au redémarrage, duplication et fusion de deux études de même racine.
- **Fichiers** : PGN multijeux avec variantes, FEN, import/export EPD classique, conteneur T64 expérimental, diagramme PNG ou SVG avec votre apparence et annotations, étude PDF avec diagramme et notation. L’import PGN depuis un fichier travaille partie par partie, en arrière-plan, avec progression et annulation transactionnelle. Un gestionnaire séparé permet les imports PGN reprenables par lots, avec 1 à 8 lecteurs parallèles. L’export PGN de bibliothèque écrit aussi partie par partie. Import des parties publiques Lichess/Chess.com par pseudo (incrémental sur Lichess) et glisser-déposer de fichiers dans la fenêtre.
- **Bibliothèque** : SQLite schéma 8, bases nommées et historique, recherche de fragments indexée FTS5 (si disponible), statistiques et arbres d’ouvertures actualisés à chaque écriture, filtres exacts indexés, recherche par joueur/événement/ECO/date/Elo/collection/résultat, matériel et positions incluant les variantes ; favoris, corbeille restaurable, recherche annulable en arrière-plan et pages par curseur, détection des doublons de ligne principale, réindexation annulable. Fiches joueurs, scores et ouvertures issues de votre base.
- **Moteurs et bases à télécharger** : trois moteurs vérifiés (Stockfish, Arasan, PlentyChess) et 42 collections PGN, dont les champions classiques, des championnes, matchs historiques et ouvertures. Paramètres UCI indépendants par profil ; filtres combinés par base et ouverture. [Mode d’emploi](docs/ENGINES-DATABASES.md).
- **Analyse** : moteur UCI local, comparaison simultanée de 2 à 4 profils avec budgets CPU/hash partagés, MultiPV, insertion d’une ligne, profils de réglages, limite de profondeur, chemin Syzygy transmis aux moteurs compatibles. Bilan immédiat de la ligne principale avec graphe, pertes en centipions, annotations et création de cartes depuis les erreurs. Explications du bilan à partir de suites légales : réponse au coup joué ou proposition antérieure, captures/promotions/échecs/mats/pats et compte matériel ; motifs vérifiables (attaques multiples, clouages/enfilades du roi, échecs à la découverte et doubles échecs), ouverture d’une copie annotée avec repères PGN. File d’analyses persistante des lignes principales ou de toutes les variantes, copies indépendantes, pause/reprise, cache lié au moteur et à l’historique, export annoté ; scores de mat distincts des centipions et WDL lorsqu’il est fourni par le moteur.
- **Jeu local** : contre un moteur ou à deux sur le même ordinateur, camp/force/réflexion réglables, force limitée en Elo pour les moteurs compatibles, tournois toutes rondes entre 2 et 8 profils moteur avec classement, pendule avec incrément, abandon et réclamation de nulle.
- **Ouvertures** : explorateur des lignes principales de vos parties, résultats statistiques, livres Polyglot `.bin`, reconnaissance ECO hors ligne à partir de 3 865 positions nommées et attribution des balises à l’étude. Répertoires personnels par position et transposition, notes/priorités/alternatives, parties modèles, création/fusion/import/export PGN annulables en arrière-plan, lacunes observées dans la bibliothèque calculées sans bloquer l’interface, analyse moteur de toutes les variantes depuis une copie. Les transpositions sont développées une seule fois. Préparation par source, adversaire/couleur, période et Elo connu avec export CSV.
- **Entraînement** : puzzles tactiques notés (import de la base Lichess CC0 et puzzles tirés de vos erreurs, classement Elo, thèmes, indices), cartes depuis vos lignes ou les erreurs analysées, réponses adverses automatiques, solution, suivi quotidien/hebdomadaire, objectif et répétition espacée simple ; révision des positions de répertoire avec plusieurs réponses acceptées, calendrier et suivi des rappels manqués.
- **Mises à jour** : vérification GitHub en arrière-plan, Stable/Bêta/Développement, notes de version, téléchargement vérifié SHA-256, installation après sauvegarde et redémarrage.
- **Fiabilité** : sauvegardes automatiques toutes les 6 heures (14 instantanés), session atomique, sauvegarde SQLite complète, contrôle d’intégrité, sauvegarde avant migration du schéma, import classique annulé intégralement en cas d’erreur, import reprenable conservant les lots validés, protection de la bibliothèque contre un export sur elle-même.

![Apparence et langue](docs/screenshots/appearance.png)

## Premiers pas

Depuis l’accueil vide, **Découvrir les parties d’exemple**, puis ouvre une partie dans la bibliothèque. Parcours les coups avec les flèches. Reviens en arrière et joue un autre coup pour créer une variante.

Dans **Bibliothèque → Téléchargements → Moteurs**, tu peux installer un moteur vérifié, ou sélectionner un moteur UCI déjà présent sur le système depuis **Menu → Paramètres**. Stockfish installé par Debian est souvent détecté dans `/usr/games/stockfish`. Charge ou crée ensuite un profil, clique **Démarrer / appliquer**, puis **Analyser** dans l’atelier. L’analyse utilise toute la force du moteur ; la limitation de force concerne le jeu.

Le bouton **Apparence** personnalise l’échiquier, le fond, la police, les tailles et la langue de l’interface. Dans l’atelier, **Bilan** analyse la ligne principale ; les seuils de classement sont des repères simples, pas une mesure certifiée du niveau de jeu. Les fichiers Syzygy sont à fournir séparément.

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
| Ctrl Z / Ctrl Maj Z | Annuler / rétablir une modification |
| Clic droit glissé | Ajouter/enlever une flèche persistante |
| Clic droit sur une case | Ajouter/enlever un cercle persistant |
| Maj / Ctrl / Alt + clic droit | Annotation rouge / bleue / jaune |

## Données et formats

Par défaut sur Linux : `~/.local/share/Trait64/Trait64/` (ou le dossier XDG équivalent). Le chemin effectif est affiché dans **Paramètres**. La bibliothèque est `library.sqlite` ; `session.json` contient les onglets et leurs positions actives. Les migrations créent d’abord une copie `.before-v2-….sqlite` ou `.before-v3-….sqlite` jusqu’à `.before-v8-….sqlite` à côté de la base. La migration 8 convertit les positions en clés entières puis compacte le fichier (`VACUUM`) : prévoir temporairement le double de la taille de la base en espace libre. Le schéma 3 construit ses index lors de la première ouverture : cela peut prendre du temps sur un grand corpus. Une ancienne version du logiciel ne doit pas ouvrir une base déjà migrée. Les paramètres sont enregistrés dans le dossier de configuration Qt de l’utilisateur.

Utilise **Outils → Sauvegarder** pour une copie cohérente de la bibliothèque, entraînements inclus. **Ouvrir une base** ouvre une sauvegarde séparément sans écraser la base principale. Au lancement suivant, la base principale redevient active ; une étude venant d’une autre base conserve son contenu dans la session sans réutiliser son identifiant dans la mauvaise base.

PGN est le format d’échange recommandé. Le conteneur `.t64` livré est le **profil expérimental 0.1**, avec instantanés PGN, alignement, empreintes SHA-256 et deux points de récupération. Il ne prétend **pas** implémenter déjà le futur format compact Move16/TLV/BLAKE3 décrit dans la conception : voir [docs/format/T64-0.1.md](docs/format/T64-0.1.md). Un export `.t64` existant ajoute une génération ; PGN exporte le contenu courant. Les exports PGN/T64 contiennent les parties, pas les favoris, collections ou entraînements : ceux-ci nécessitent la sauvegarde SQLite.

## Compiler sans l’installateur

Avec les dépendances Qt6, CMake, `libarchive-dev` et `libsqlite3-dev` installées :

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

Alpha : le [périmètre restant](docs/ROADMAP.md) est explicite. Les tests automatiques couvrent les scénarios documentés dans [docs/TESTING.md](docs/TESTING.md), sans garantir l’absence de tout bug.

Import PGN depuis un fichier : pas de plafond global de 64 Mio, mais **64 Mio par partie**, 100 000 nœuds par étude et 128 niveaux de variantes. Le collage PGN, l’EPD et le profil T64 restent limités à 64 Mio et 10 000 parties/positions. Résultats de bibliothèque paginés par 200 ; affichage de notation limité à 10 000 coups. Session limitée à 16 onglets et 64 Mio, historique annuler/rétablir à 64 étapes et 16 Mio par étude. L’import d’un répertoire est limité à 16 Mio et 200 parties ; l’édition des coups, le retrait de branches, le renommage, la suppression et les révisions restent synchrones. Les corpus de plusieurs millions de parties n’ont pas été qualifiés.

La recherche de positions inclut les variantes ; l’explorateur statistique utilise les lignes principales. Le bilan analyse la ligne principale, par défaut en deux passages (rapide du début à la fin, puis complet de la fin vers le début) : seuils fixes de 50/100/250 centipions pour imprécision/erreur/gaffe, dépendants du moteur et du budget choisi. La création de cartes depuis ce bilan concerne les échecs classiques et conserve une ligne de solution, sans reconnaître toutes les solutions équivalentes. Les résultats du bilan immédiat restent en mémoire ; **Annoter l’étude** enregistre les évaluations dans le PGN. La file d’analyses séparée conserve ses tâches et résultats en SQLite, peut analyser les variantes et reprendre après une interruption.

Pas encore : versions Windows et macOS, Flatpak, pendules à plusieurs périodes, formats SCID/ChessBase, couverture probabiliste et comparaison de répertoires, recherche par placement partiel de pièces, plugins et format T64 compact final. Le monogramme T64 reste provisoire.

## Licence et composants

Code Trait64 : **GNU AGPL v3 ou ultérieure**. [LICENSE](LICENSE) et [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Le nom et l’identité visuelle restent distincts de la licence du code ; voir [TRADEMARKS.md](TRADEMARKS.md).

Trait64 réutilise `chess-library` sous MIT. Qt est lié dynamiquement depuis les paquets système. Les moteurs UCI restent des programmes séparés : le catalogue peut télécharger des versions vérifiées de Stockfish, Arasan et PlentyChess, mais aucun moteur n’est incorporé au binaire Trait64. Les collections PGN restent également des téléchargements provenant de leurs sources. Aucun compte, publicité, télémétrie ou service distant n’est nécessaire pour l’usage local. Les programmes moteurs sélectionnés s’exécutent avec les droits de l’utilisateur : choisis des exécutables de confiance.
