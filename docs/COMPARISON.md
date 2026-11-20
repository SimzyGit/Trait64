# Trait64 face à ChessBase, Scid vs. PC et En Croissant

Trait64 ne cherche pas à reproduire un logiciel existant à l’identique : son objectif est de combiner **travail local, bases de parties, analyse moteur, préparation et entraînement** dans une interface moderne et libre. Ce tableau compare les orientations et fonctions principales ; il ne remplace pas une comparaison exhaustive des milliers de détails de chaque logiciel.

| Fonction / philosophie | **Trait64 0.3 alpha** | **ChessBase 18** | **Scid vs. PC** | **En Croissant** |
|---|:---:|:---:|:---:|:---:|
| Logiciel libre / code source ouvert | ✅ AGPL-3.0+ | ❌ Propriétaire | ✅ GPLv2 | ✅ Open source |
| Gratuit | ✅ | ❌ | ✅ | ✅ |
| Linux | ✅ Cible principale | ❌ Natif Windows | ✅ | ✅ |
| Windows / macOS | ✅ Publiés à chaque version (`.zip`, `.dmg`) ; mise à jour automatique sous Linux seulement | ✅ Windows | ✅ | ✅ |
| Fonctionnement local hors ligne | ✅ | ✅ | ✅ | ✅ |
| Compte obligatoire pour les fonctions locales | ❌ | ❌ | ❌ | ❌ |
| Gestion de bases de parties | ✅ SQLite | ✅ Très avancée | ✅ Très avancée | ✅ |
| Import / export PGN | ✅ | ✅ | ✅ | ✅ |
| FEN | ✅ | ✅ | ✅ | ✅ |
| Recherche joueurs / événements / ECO / Elo / dates | ✅ | ✅ | ✅ | ✅ |
| Recherche de positions | ✅ | ✅ | ✅ | ✅ |
| Arbre / explorateur d’ouvertures | ✅ | ✅ | ✅ | ✅ |
| Classement ECO automatique des parties sans balise | ✅ | ✅ | ✅ | ❌ |
| Dossier d’une position (score et tranches de force des joueurs) | ✅ | ✅ Rapport d’ouverture | ◐ Statistiques simples | ◐ Statistiques simples |
| Écart entre votre répertoire et vos propres parties | ✅ | ❌ | ❌ | ❌ |
| Import d’un dossier d’études (En Croissant, Pawn Appétit), PGN compressés compris | ✅ | ❌ | ❌ | ◐ Son propre format |
| Livres d’ouvertures | ✅ Polyglot `.bin`, lecture et écriture | ✅ CTG et livres maison | ✅ Polyglot | ❌ Non documenté |
| Analyse avec moteurs UCI locaux | ✅ | ✅ | ✅ | ✅ |
| MultiPV | ✅ | ✅ | ✅ | ✅ |
| Flèches du moteur sur l’échiquier | ✅ Trois premières lignes | ✅ | ✅ | ✅ |
| Pièces animées, glisser ou clic-clic | ✅ | ✅ | ◐ Sans animation | ✅ |
| Lecture automatique d’une partie | ✅ | ✅ | ✅ | ❌ |
| Détection des moteurs déjà installés | ✅ | ◐ Moteurs fournis | ❌ Ajout manuel | ◐ Téléchargement intégré |
| Comparaison simultanée de plusieurs moteurs | ✅ 2–4 profils | ✅ | ✅ Plusieurs moteurs | ◐ Plusieurs moteurs configurables |
| Syzygy | ✅ Via moteur compatible | ✅ | ✅ | ◐ API / moteur selon usage |
| Analyse complète d’une partie + graphe | ✅ | ✅ | ✅ | ✅ |
| Préparation d’ouvertures / répertoire | ✅ Positions, transpositions, filtres | ✅ Très avancée | ✅ | ✅ |
| Entraînement / répétition espacée | ✅ Puzzles notés, cartes et positions de répertoire | ✅ | ◐ Outils d’entraînement | ✅ Puzzles / entraînement |
| Chess960 | ✅ | ◐ | ◐ | ✅ |
| Jeu contre un moteur | ✅ | ✅ | ✅ | ✅ |
| Tournois moteur contre moteur | ✅ Toutes rondes, 2–8 profils, écart Elo | ✅ Selon outils associés | ✅ | ❌ / non central |
| Bases téléchargeables intégrées | ✅ 42 collections | ✅ Écosystème ChessBase | ◐ Import manuel / ressources | ✅ Gestionnaire intégré |
| Import direct Lichess / Chess.com | ✅ Comptes synchronisés, nouvelles parties seulement | ✅ | ◐ | ✅ |
| Cloud requis pour l’usage principal | ❌ | ❌ mais services cloud intégrés | ❌ | ❌ |
| Sauvegardes automatiques locales | ✅ | ◐ | ◐ | ◐ |
| Format propriétaire nécessaire | ❌ | ◐ Écosystème CBH/CBV | ◐ SI4 + PGN | ❌ |
| Interface moderne pensée desktop | ✅ Qt6 | ✅ | ◐ Interface Tk historique | ✅ |
| Maturité | 🧪 **Alpha** | 🟢 Produit mature | 🟢 Projet mature | 🟢 Projet actif |

**Légende :** ✅ pris en charge · ◐ partiel, différent ou dépendant d’un composant/service · ❌ non pris en charge actuellement · 🧪 expérimental / prévu mais non validé.

> **Important :** ChessBase reste nettement plus mature sur la recherche professionnelle, les très grandes bases, la préparation d’adversaire et son écosystème de données. Scid vs. PC bénéficie de nombreuses années de développement et d’outils spécialisés. En Croissant possède déjà une excellente intégration des moteurs, bases téléchargeables et plateformes en ligne. Trait64 se différencie surtout par son approche **local-first, libre, Linux-first, sans compte ni télémétrie**, avec l’ambition de réunir ces usages dans un environnement cohérent.

Sources de référence pour cette comparaison : [ChessBase 18](https://shop.chessbase.com/en/products/chessbase_18_program_only), [Scid vs. PC](https://sourceforge.net/projects/scidvspc/), [En Croissant](https://encroissant.org/docs/). La comparaison reflète les informations publiques disponibles et les fonctions de Trait64 documentées dans ce dépôt ; elle pourra évoluer avec les versions.

## Relevé élargi — novembre 2026

Ce relevé ajoute aux trois logiciels du tableau ChessX, Lucas Chess, Nibbler, Lichess, Chess.com et Chessable. Il dit où Trait64 fait aussi bien, ce qui lui manque encore, et ce qu'il emprunte aux autres pour son apparence.

### Les références

| Outil | Genre | Ce qu'il fait de mieux |
|---|---|---|
| **ChessBase 18** | base de parties, Windows, payant | recherche de références, rapports de style et d'erreurs, préparation contre un adversaire, médailles, format 2CBH |
| **SCID vs. PC / Scid** | base de parties, libre | rapidité sur des millions de parties, arbre, recherches matérielles et de motifs |
| **En Croissant** | interface d'analyse, libre (Tauri) | interface moderne en onglets, import Lichess et Chess.com, répertoires, puzzles |
| **ChessX** | base de parties, libre (Qt) | légèreté, classement ECO, recherche de positions |
| **Lucas Chess** | entraînement, libre | dizaines de modes d'entraînement, adversaires par niveau |
| **Nibbler** | interface pour Lc0 | probabilités gain/nulle/perte, flèches de politique |
| **Lichess** | site | tableau d'analyse, explorateur, études partagées, « Apprendre de vos erreurs », analyse dans le nuage |
| **Chess.com** | site | bilan de partie guidé par moments clés, coup à réessayer, classement des coups |
| **Chessable** | site | répétition espacée des répertoires |

### Où Trait64 est au niveau

- **Base de parties** : SQLite local, import reprenable, ChessBase `.cbh` et SCID 4/5 lus directement, recherche multicritère, médailles, corbeille, sauvegardes.
- **Recherche** : position exacte, structure de pions, pièces sur des cases, manœuvres, finales atteintes, clés stratégiques, sous-ensemble de CQL. C'est le niveau de ChessBase et de Scid, sans le format propriétaire.
- **Analyse** : plusieurs moteurs UCI, bilan en deux passages, flèches, relations d'attaque, analyse assistée, analyse dans le nuage de Lichess, tables de Syzygy.
- **Préparation** : contre un adversaire depuis la bibliothèque et ses parties en ligne, fiche joueur avec style et liens. C'est l'équivalent des rapports de ChessBase 18.
- **Entraînement** : puzzles Lichess et puzzles tirés des gaffes, mats tirés de la bibliothèque, répétition espacée façon Chessable, exercice de Stoyko, coordonnées, finales.
- **Vie privée** : tout tourne localement, rien ne part sans demande. Ce n'est le cas ni de Lichess, ni de Chess.com, ni de ChessBase Cloud.

### Ce qui manque, par ordre d'utilité

| Écart | Vu chez | Effort | Décision |
|---|---|---|---|
| **Moments clés** : les trois tournants de la partie, signalés sur la courbe et dans la notation | Chess.com, Lichess | petit | fait (20 nov.) |
| **Symboles de qualité dans la notation** (?!, ?, ??) posés par le bilan, en couleur | Chess.com, Lichess | — | déjà présent (vue Tableau) |
| **Probabilités gain/nulle/perte** du moteur à côté de l'évaluation | Nibbler, Lichess | petit (UCI_ShowWDL) | fait (20 nov.) |
| Format **2CBH** et archives **.cbv** | ChessBase 18 | grand, non documenté | en attente de fichiers d'exemple |
| **Partie par correspondance** (Xfcc) | ChessBase, Arena | moyen, compte requis | en attente d'un compte d'essai |
| **Échiquiers électroniques** (DGT, Chessnut) | ChessBase, En Croissant | grand, matériel requis | en attente de matériel |
| **Synchronisation** entre appareils | ChessBase Cloud, sites | grand | à concevoir (dossier synchronisé ?) |
| **Saisie vocale** des coups | ChessBase 18 | moyen | pas prioritaire |
| **Texte généré par IA** (biographies, conseils) | ChessBase 18 | — | écarté : invérifiable, contraire à l'esprit local |

### Relevé complété — 20 novembre 2026

Le tour s'étend aux autres familles d'outils : interfaces de moteurs, suites commerciales, sites d'entraînement et applications de répertoire.

| Outil | Genre | Ce qu'il apporte | Chez Trait64 |
|---|---|---|---|
| **Fritz 20** (ChessBase) | interface de jeu et d'analyse, payant | niveaux de jeu « humains », entraîneur pendant la partie | jeu contre moteur à Elo limité, analyse assistée, pièces en prise |
| **HIARCS Chess Explorer** | base et analyse, payant (Windows, macOS) | arbre d'ouvertures rapide, bases jusqu'à 30 millions de parties | arbre et explorateur, recherche indexée, import reprenable |
| **Shredder GUI** | interface de moteur, payant | analyse intégrée en parcourant une base | analyse depuis la bibliothèque, partie suivante/précédente |
| **Arena**, **Banksia** | interfaces de moteurs, gratuites | tournois de moteurs, nombreux protocoles | tournoi toutes rondes UCI, livre Polyglot, Elo et sa marge, LOS (20 nov.) |
| **Cute Chess**, **fastchess** | matchs de moteurs, libres | Elo ± marge à 95 %, LOS, SPRT | Elo ± marge et LOS (20 nov.) ; SPRT non, Trait64 n'est pas un banc de test de moteurs |
| **PyChess** | interface libre | jeu en ligne (FICS), variantes | Chess960 ; jeu en ligne non, Lichess et Chess.com servent à cela |
| **Chessbook**, **Listudy** | répertoires, en ligne | couverture du répertoire, répétition espacée | couverture estimée, réponses à préparer, cartes à répétition espacée |
| **Aimchess** | rapports de progression, en ligne | points faibles par thème, sur vos parties | bilan de compétences, force estimée, profils réunissant les comptes |
| **ChessTempo** | tactique, en ligne | puzzles classés, finales | puzzles Lichess classés, puzzles tirés des gaffes, finales à jouer |
| **DecodeChess** | explication, en ligne | le pourquoi d'un coup, en phrases | « Comprendre ce coup » : menaces, matériel, ligne du moteur, sans texte inventé |
| **Chessify** | analyse dans le nuage, scanner | position photographiée ou capturée à l'écran | non : reconnaissance d'image écartée pour l'instant (voir ci-dessous) |

Écarts relevés en plus :

| Écart | Vu chez | Effort | Décision |
|---|---|---|---|
| **Marge d'erreur et LOS** dans les tournois de moteurs | Cute Chess, fastchess, Arena | petit | fait (20 nov.) |
| **Position depuis une photo** ou une capture d'écran | Chessify, ChessBase 18 | grand (reconnaissance d'image) | écarté pour l'instant : il faudrait un modèle livré avec le programme |
| **Adversaires « humains »** (réseaux Maia) | Lichess, Chessiverse, Fritz 20 | moyen (Lc0 et ses réseaux à télécharger) | à étudier : dépend de Lc0 sur la machine |
| **SPRT** pour arrêter un match quand l'écart est établi | fastchess, OpenBench | moyen | pas prioritaire hors développement de moteurs |

### Ce que Trait64 prend ailleurs pour son apparence

Le but n'est pas d'imiter un site d'échecs. Trait64 emprunte aux outils de travail qu'on garde ouverts des heures :

- **Logic Pro et Ableton Live** : une interface dense et calme, des contrôles fins et des aplats sans dégradé. La couleur est réservée à ce qui signifie quelque chose.
- **Things et Bear** : un thème clair de papier chaud plutôt que de gris neutres, une hiérarchie typographique nette, des marges régulières.
- **Lichess** : la sobriété du tableau d'analyse, et une courbe d'évaluation en blanc et noir plutôt qu'en couleur d'accent.
- **Les pendules de tournoi et le feutre des tables de club** : un vert d'accent profond plutôt que le bleu de toutes les applications.

Ce que Trait64 s'interdit : les dégradés violets, le verre dépoli, les grandes ombres, les émojis dans l'interface, les icônes « étincelle » et les textes promotionnels. Ce sont les marques des interfaces générées en série.

### Sources

- [HIARCS Chess Explorer](https://www.hiarcs.com/pc-chess-explorer.html)
- [Comparatif des outils d'ouvertures (Chessiverse)](https://chessiverse.com/compare/chess-opening-practice-tools-compared)

- [ChessBase 18 — nouveautés (aide officielle)](https://help.chessbase.com/CBase/18/Eng/whats_new_.htm)
- [John Watson : ChessBase 18, mettre à jour ou non ?](https://en.chessbase.com/post/john-watson-chessbase-18-review-chess-life)
- [En Croissant — présentation](https://www.linuxlinks.com/en-croissant-gui-chess-toolkit/) et [versions](https://github.com/franciscoBSalgueiro/en-croissant/releases)
- [Chess.com — nouveau Game Review](https://chess.com/news/view/game-review-design-update)
- [Lichess — journal des changements](https://www.lichess.org/changelog)
