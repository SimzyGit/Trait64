<div align="center">

<img src="assets/trait64.svg" alt="" width="104" height="104">

# Trait64

**Your games, your analysis, your progress — on your own machine.**
Free chess software in the spirit of ChessBase and SCID: game databases, Stockfish analysis, opening preparation and training. No account, no cloud, no ads.

[![Version](https://img.shields.io/github/v/release/SimzyGit/Trait64?include_prereleases&label=version)](https://github.com/SimzyGit/Trait64/releases)
[![License: AGPL-3.0-or-later](https://img.shields.io/badge/license-AGPL--3.0--or--later-blue.svg)](LICENSE)
![Linux · Windows · macOS](https://img.shields.io/badge/platforms-Linux%20%C2%B7%20Windows%20%C2%B7%20macOS-2f66ad.svg)
[![Continuous integration](https://github.com/SimzyGit/Trait64/actions/workflows/linux.yml/badge.svg)](https://github.com/SimzyGit/Trait64/actions/workflows/linux.yml)

**English** · [Français](README.md)

</div>

![Trait64: board, notation, opening tree and engine](docs/screenshots/analysis.png)

## Download

**Linux** — one command, no compiling, no password:

```bash
curl -fsSL https://raw.githubusercontent.com/SimzyGit/Trait64/main/install.sh | bash
```

It fetches the latest release, verifies its SHA-256 checksum and adds Trait64 to your applications menu. Later updates happen inside the app (**Help → Updates**). To uninstall, append `-s -- --uninstall` to the same command.

Or pick a file from the [releases page](https://github.com/SimzyGit/Trait64/releases):

| System | File | Install |
| --- | --- | --- |
| Windows 10 or 11 (64-bit) | `Trait64-…-windows-x86_64.zip` | unzip, run `trait64.exe` |
| macOS (Apple silicon) | `Trait64-…-macos-arm64.dmg` | open the image, drag into Applications |
| Debian 12 · 13 | `trait64_…_debianXX_amd64.deb` | `sudo apt install ./file.deb` |
| Other 64-bit Linux | `Trait64-…-linux-x86_64.AppImage` | make it executable, then open |

The Debian package installs from a terminal: double-clicking it leads nowhere, since Debian no longer ships a graphical `.deb` installer by default.

The packages are unsigned: on first launch Windows (SmartScreen) and macOS (Gatekeeper) ask for confirmation. On macOS, right-click → **Open**.

**Then an engine.** None is bundled or imposed. Install Stockfish from your distribution (`sudo apt install stockfish`), or get Stockfish, Arasan or PlentyChess from **Library → Downloads**.

## Why Trait64

**Your data stays yours.** A SQLite database in your home folder, automatic backups, PGN and FEN in and out. No account to create, nothing sent to a server, no telemetry. The day Trait64 stops suiting you, your games are still readable elsewhere.

**A real database, not a list.** PGN import swallows hundreds of thousands of games and resumes where it left off if you interrupt it. You then search by player, opening, rating, date, result, exact position or pawn structure.

**Stockfish analysis, explained.** A game review runs the engine forwards then backwards, then gives you an accuracy percentage — for the whole game, and separately for the opening, the middlegame and the endgame. Every mistake is located, classified and explained.

**Progress measured on your own games.** Link your Lichess and Chess.com accounts to a profile, review your recent games, and the dashboard tells you where you stand: accuracy per phase, blunders per hundred moves, tactical success, and month-by-month progress. The comparison is with your own past, not with an invented average.

## What Trait64 does

- **Library** — resumable PGN import, multi-criteria search, player cards, collections, trash, automatic backups.
- **Online accounts** — Lichess and Chess.com, no password; each sync only fetches new games.
- **Player profiles** — one profile gathers your accounts and your FIDE record, whose ratings are read from the federation's official lists.
- **Analysis** — Stockfish or any UCI engine, several lines, engines compared side by side, a resumable background analysis queue.
- **Game review** — evaluation graph, accuracy per phase, blunders and inaccuracies explained, tactical motifs recognised, PGN annotation.
- **Reviewing a whole database** — after a sync or an import, one button reviews the entire batch at the depth you choose, without redoing what has already been seen.
- **Dashboard** — accuracy, blunders per hundred moves, tactics, month-by-month progress.
- **Openings** — statistical tree of your own database, Polyglot books, repertoires with transpositions, novelty detection, a preparation file against a given opponent.
- **Training** — rated puzzles (the free Lichess set and your own blunders), spaced-repetition cards, repertoire review.
- **Playing** — against the engine at a chosen rating, two players with a clock, engine tournaments.
- **Comfort** — light or dark theme, adjustable interface size, French and English, **Ctrl K** for the command palette.

![Game library](docs/screenshots/library.png)

Trait64 is **alpha** and moving fast: see the [changelog](docs/CHANGELOG.md), the [project status](docs/PROJECT-STATUS.md) and the [roadmap](docs/ROADMAP.md).

## First steps

1. On the home screen click **Samples**, then open a game in the **Library**.
2. Step through the moves with ← and →. Play a different move to start a variation.
3. Install an engine (**Library → Downloads → Engines**), then **Analyse** for the position, or **Review** for the whole game.
4. Add your accounts under **File → Lichess and Chess.com accounts**, sync, then review the imported batch.
5. Open **File → Your skills** to see where you stand.

## Documentation

| | |
| --- | --- |
| [Features in detail](docs/FEATURES.md) | everything Trait64 does, shortcuts, data and limits |
| [Install and build](docs/BUILDING.md) | packages, AppImage, building from source |
| [Comparison](docs/COMPARISON.md) | Trait64 against ChessBase, Scid vs. PC and En Croissant |
| [Engines and databases](docs/ENGINES-DATABASES.md) · [Updates](docs/UPDATES.md) | verified catalogue and update channels |
| [Architecture](docs/ARCHITECTURE.md) · [Testing](docs/TESTING.md) | to contribute code |

## Contributing

Bug reports, ideas and contributions are welcome: read [CONTRIBUTING.md](CONTRIBUTING.md), then open an [issue](https://github.com/SimzyGit/Trait64/issues/new/choose). Security issues should be reported privately ([SECURITY.md](SECURITY.md)).

## Licence and components

Trait64 code: **GNU AGPL v3 or later** — [LICENSE](LICENSE), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The name and visual identity stay separate from the code licence; see [TRADEMARKS.md](TRADEMARKS.md).

Trait64 reuses `chess-library` under MIT and links Qt dynamically from system packages. UCI engines remain separate programs: the catalogue downloads verified builds of Stockfish, Arasan and PlentyChess, but no engine is embedded in the binary. PGN collections likewise come from their original sources. No account, advertising, telemetry or remote service is needed for local use. An engine you choose runs with your own privileges: only run executables you trust.
