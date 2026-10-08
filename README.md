# Trait64

Un atelier d’échecs libre, local et personnel pour **étudier, analyser, préparer et mémoriser**.

**Version 0.2.0 alpha — application de bureau C++23 / Qt6, conçue d’abord pour Linux Voyager / Debian.**
Cette version enrichit l’application locale avec la personnalisation, Chess960 et le bilan de partie. Il ne constitue pas encore l’intégralité du catalogue de fonctionnalités imaginé pour Trait64. Le périmètre exact et la suite sont décrits dans [docs/ROADMAP.md](docs/ROADMAP.md).

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

- **Apparence** : cinq styles de pièces (Classique, Merida, Spatial, Pixel, Unicode), import de vos douze SVG, huit palettes d’échiquier, couleurs personnelles des cases et du fond, thème clair/sombre, coordonnées et repères réglables. Tous les choix sont mémorisés.
- **Échiquier** : clic/glisser, coups légaux, promotion au choix, échecs classiques et Chess960 (960 positions numérotées), éditeur graphique de position, inversion, flèches et cercles persistants dans le PGN.
- **Études** : arbre de variantes, promotion/suppression de branches, commentaires, NAG, métadonnées, annuler/rétablir, jusqu’à 16 onglets récupérés au redémarrage, duplication et fusion de deux études de même racine.
- **Fichiers** : PGN multijeux avec variantes, FEN, import/export EPD classique, conteneur T64 expérimental, diagramme PNG avec votre apparence et annotations, étude PDF avec diagramme et notation. L’import PGN depuis un fichier travaille partie par partie, en arrière-plan, avec progression et annulation transactionnelle. L’export PGN de bibliothèque écrit aussi partie par partie.
- **Bibliothèque** : SQLite, recherche par joueur/événement/ECO/date/Elo/collection/résultat, matériel et positions incluant les variantes ; favoris, corbeille restaurable, pagination, détection des doublons de ligne principale, réindexation annulable. Fiches joueurs, scores et ouvertures issues de votre base.
- **Analyse** : moteur UCI local, MultiPV, insertion d’une ligne, profils de réglages, limite de profondeur, chemin Syzygy transmis aux moteurs compatibles. Bilan automatique de la ligne principale avec graphe, pertes en centipions, annotations et création de cartes depuis les erreurs.
- **Jeu local** : contre un moteur ou à deux sur le même ordinateur, camp/force/réflexion réglables, pendule avec incrément, abandon et réclamation de nulle.
- **Ouvertures** : explorateur des lignes principales de vos parties, résultats statistiques, reconnaissance ECO hors ligne à partir de 3 865 positions nommées et attribution des balises à l’étude.
- **Entraînement** : cartes depuis vos lignes ou les erreurs analysées, réponses adverses automatiques, solution, suivi quotidien/hebdomadaire, objectif et répétition espacée simple.
- **Fiabilité** : session atomique, sauvegarde SQLite complète, contrôle d’intégrité, sauvegarde avant migration du schéma, imports entièrement annulés en cas d’erreur, protection de la bibliothèque contre un export sur elle-même.

![Choix des pièces et couleurs](docs/screenshots/appearance.png)

## Premiers pas

Depuis l’accueil, **Charger les exemples**, puis ouvre une partie dans la bibliothèque. Parcours les coups avec les flèches. Reviens en arrière et joue un autre coup pour créer une variante.

Dans **Paramètres**, le chemin Stockfish est détecté si possible (souvent `/usr/games/stockfish`). Clique **Démarrer / appliquer**, puis **Analyser** dans l’atelier. L’analyse utilise le niveau maximal ; le réglage de force concerne le jeu contre le moteur.

Le bouton **Pièces & couleurs** personnalise l’échiquier et le fond. Dans l’atelier, **Bilan** analyse la ligne principale ; les seuils de classement sont des repères simples, pas une mesure certifiée du niveau de jeu. Les fichiers Syzygy sont à fournir séparément.

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

Par défaut sur Linux : `~/.local/share/Trait64/Trait64/` (ou le dossier XDG équivalent). Le chemin effectif est affiché dans **Paramètres**. La bibliothèque est `library.sqlite` ; `session.json` contient les onglets et leurs positions actives. La migration 0.1 → 0.2 crée d’abord un fichier `.before-v2-….sqlite` à côté de la base. Une ancienne version du logiciel ne doit pas ouvrir une base déjà migrée. Les paramètres sont enregistrés dans le dossier de configuration Qt de l’utilisateur.

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

Alpha : le [périmètre restant](docs/ROADMAP.md) est explicite. Les tests automatiques couvrent les scénarios documentés dans [docs/TESTING.md](docs/TESTING.md), sans garantir l’absence de tout bug.

Import PGN depuis un fichier : pas de plafond global de 64 Mio, mais **64 Mio par partie**, 100 000 nœuds par étude et 128 niveaux de variantes. Le collage PGN, l’EPD et le profil T64 restent limités à 64 Mio et 10 000 parties/positions. Résultats paginés par 1 000 ; affichage de notation limité à 10 000 coups. Session limitée à 16 onglets et 64 Mio, historique annuler/rétablir à 64 étapes et 16 Mio par étude. Les corpus de plusieurs millions de parties n’ont pas été qualifiés.

La recherche de positions inclut les variantes ; l’explorateur statistique utilise les lignes principales. Le bilan analyse uniquement la ligne principale : seuils fixes de 50/100/250 centipions pour imprécision/erreur/gaffe, dépendants du moteur et du budget choisi. La création de cartes depuis ce bilan concerne les échecs classiques et conserve une ligne de solution, sans reconnaître toutes les solutions équivalentes. Les résultats du bilan restent en mémoire ; **Annoter l’étude** enregistre les évaluations dans le PGN.

Pas encore : comparaison simultanée de moteurs, moteur contre moteur, pendules à plusieurs périodes, Polyglot/SCID/ChessBase, suivi de couverture d’un répertoire, organisation de tournois, synchronisation, plugins, imports distants, format T64 compact final, AppImage/Flatpak et validation Windows/macOS. Le monogramme T64 reste provisoire.

## Licence et composants

Code Trait64 : **GNU AGPL v3 ou ultérieure**. [LICENSE](LICENSE) et [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Le nom et l’identité visuelle restent distincts de la licence du code ; voir [TRADEMARKS.md](TRADEMARKS.md).

Trait64 réutilise `chess-library` sous MIT. Qt est lié dynamiquement depuis les paquets système ; Stockfish s’exécute séparément. Aucun compte, publicité, télémétrie ou service distant n’est nécessaire. Les programmes moteurs sélectionnés s’exécutent avec les droits de l’utilisateur : choisis des exécutables de confiance.
