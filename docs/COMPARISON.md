# Trait64 face à ChessBase, Scid vs. PC et En Croissant

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
| Import direct Lichess / Chess.com | ✅ Comptes synchronisés, nouvelles parties seulement | ✅ | ◐ | ✅ |
| Cloud requis pour l’usage principal | ❌ | ❌ mais services cloud intégrés | ❌ | ❌ |
| Sauvegardes automatiques locales | ✅ | ◐ | ◐ | ◐ |
| Format propriétaire nécessaire | ❌ | ◐ Écosystème CBH/CBV | ◐ SI4 + PGN | ❌ |
| Interface moderne pensée desktop | ✅ Qt6 | ✅ | ◐ Interface Tk historique | ✅ |
| Maturité | 🧪 **Alpha** | 🟢 Produit mature | 🟢 Projet mature | 🟢 Projet actif |

**Légende :** ✅ pris en charge · ◐ partiel, différent ou dépendant d’un composant/service · ❌ non pris en charge actuellement · 🧪 expérimental / prévu mais non validé.

> **Important :** ChessBase reste nettement plus mature sur la recherche professionnelle, les très grandes bases, la préparation d’adversaire et son écosystème de données. Scid vs. PC bénéficie de nombreuses années de développement et d’outils spécialisés. En Croissant possède déjà une excellente intégration des moteurs, bases téléchargeables et plateformes en ligne. Trait64 se différencie surtout par son approche **local-first, libre, Linux-first, sans compte ni télémétrie**, avec l’ambition de réunir ces usages dans un environnement cohérent.

Sources de référence pour cette comparaison : [ChessBase 18](https://shop.chessbase.com/en/products/chessbase_18_program_only), [Scid vs. PC](https://sourceforge.net/projects/scidvspc/), [En Croissant](https://encroissant.org/docs/). La comparaison reflète les informations publiques disponibles et les fonctions de Trait64 documentées dans ce dépôt ; elle pourra évoluer avec les versions.
