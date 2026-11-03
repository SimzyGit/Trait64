<div align="center">

# Trait64

**Logiciel d’échecs libre pour Linux : bases de parties, analyse avec Stockfish, préparation d’ouvertures et entraînement. Sans compte, sans cloud, sans publicité.**

[![Version](https://img.shields.io/github/v/release/SimzyGit/Trait64?include_prereleases&label=version)](https://github.com/SimzyGit/Trait64/releases)
[![License: AGPL-3.0-or-later](https://img.shields.io/badge/licence-AGPL--3.0--or--later-blue.svg)](LICENSE)
![Linux](https://img.shields.io/badge/plateforme-Linux-FCC624.svg)
[![Linux CI](https://github.com/SimzyGit/Trait64/actions/workflows/linux.yml/badge.svg)](https://github.com/SimzyGit/Trait64/actions/workflows/linux.yml)

[English](README.en.md) · **Français**

</div>

![Trait64 : échiquier, notation, arbre et moteur](docs/screenshots/analysis.png)

## Télécharger

**Linux (Debian, Ubuntu, Mint, Fedora…)** : une seule commande dans un terminal.

```bash
curl -fsSL https://raw.githubusercontent.com/SimzyGit/Trait64/main/install.sh | bash
```

Elle télécharge la dernière version, vérifie son empreinte SHA-256 et ajoute **Trait64** au menu des applications, sans mot de passe ni compilation. Les mises à jour se font ensuite depuis le logiciel (**Aide → Mises à jour**). Pour désinstaller : même commande suivie de `-s -- --uninstall`.

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

Trait64 est en **alpha** et évolue vite : voir les [nouveautés](docs/CHANGELOG.md), l'[état du projet](docs/PROJECT-STATUS.md) et la [feuille de route](docs/ROADMAP.md).

## Premiers pas

1. Sur l'accueil, cliquez **Exemples**, puis ouvrez une partie dans la **Bibliothèque**.
2. Parcourez les coups avec ← et →. Jouez un autre coup pour créer une variante.
3. Installez un moteur (**Bibliothèque → Téléchargements → Moteurs**), puis cliquez **Analyser**, ou **Bilan** pour analyser toute la partie.
4. Ajoutez vos comptes en ligne avec **Fichier → Comptes Lichess et Chess.com**.

**Ctrl K** ouvre la palette de commandes : toutes les actions y sont accessibles au clavier.

## Documentation

| | |
| --- | --- |
| [Fonctions en détail](docs/FEATURES.md) | tout ce que fait Trait64, raccourcis, données et limites |
| [Installer et compiler](docs/BUILDING.md) | paquets, AppImage, compilation depuis les sources |
| [Comparaison](docs/COMPARISON.md) | Trait64 face à ChessBase, Scid vs. PC et En Croissant |
| [Moteurs et bases](docs/ENGINES-DATABASES.md) · [Mises à jour](docs/UPDATES.md) | catalogue vérifié et canaux de mise à jour |
| [Architecture](docs/ARCHITECTURE.md) · [Tests](docs/TESTING.md) | pour contribuer au code |

## Contribuer

Les signalements de bugs, idées et contributions sont bienvenus : lisez [CONTRIBUTING.md](CONTRIBUTING.md), puis ouvrez une [demande](https://github.com/SimzyGit/Trait64/issues/new/choose). Une faille de sécurité se signale en privé ([SECURITY.md](SECURITY.md)).

## Licence et composants

Code Trait64 : **GNU AGPL v3 ou ultérieure**. [LICENSE](LICENSE) et [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Le nom et l’identité visuelle restent distincts de la licence du code ; voir [TRADEMARKS.md](TRADEMARKS.md).

Trait64 réutilise `chess-library` sous MIT. Qt est lié dynamiquement depuis les paquets système. Les moteurs UCI restent des programmes séparés : le catalogue peut télécharger des versions vérifiées de Stockfish, Arasan et PlentyChess, mais aucun moteur n’est incorporé au binaire Trait64. Les collections PGN restent également des téléchargements provenant de leurs sources. Aucun compte, publicité, télémétrie ou service distant n’est nécessaire pour l’usage local. Les programmes moteurs sélectionnés s’exécutent avec les droits de l’utilisateur : choisis des exécutables de confiance.
