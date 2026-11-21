## Getting started

Trait64 works on your computer: your games, studies and analyses stay with you, with no account or connection required.

1. **Open a game.** From the home page, *Examples* loads a few games; *File → Import games* (Ctrl+O) reads a PGN file or pasted text. A Lichess game or study link can be pasted in the same place.
2. **Install an engine.** An installed Stockfish is found automatically. Otherwise, *Engine → Engines and databases to download* installs a verified one, and the *Detect* button in the settings searches again.
3. **Analyse.** On the *Analysis* page, *Analyze* starts the engine on the position shown; *Review* goes through the whole game.

Pages are chosen in the left-hand bar: Home, Analysis, Library, Openings, Training, Players, Profiles, Studies and Tools. It folds into icons with « (or on its own when the window is narrow). *Ctrl+K* opens the command palette, which finds any action by name.

## The board

- **Play a move**: drag the piece, or click it then its destination square. Possible moves are marked.
- **Step through**: left and right arrows, Home and End; up and down switch between variations. *Space* starts automatic playback. The mouse wheel can also step through moves (setting *Mouse wheel: step through moves*).
- **Flip the board**: *F*.
- **Annotate**: right-click and drag draws an arrow, right-click on a square draws a circle; Shift, Ctrl or Alt change the colour. A double click, left or right, clears every arrow and circle.
- **Next to the players' names**, captured pieces and the material lead are shown.
- **Your games** open from your side: if you play Black (according to your profiles and accounts), the board is flipped.
- **Sounds**: move, capture, check, castling, promotion and mate each have their own, in five sets to choose from (wood, felt, click, marble, retro) and at the volume you like. *View → Mute sounds* silences them in one click.

*View → Pieces, board and colours* sets the pieces, colours, size, sounds, engine arrows and *Blindfold* mode. *View → Keyboard shortcuts* changes the keys. *View → Hanging pieces* circles in red what a capture would win.

## Notation and variations

Playing a different move from the game's creates a variation. The notation can be read as a table or as continuous text.

- **Delete a move and what follows**: right-click the move then *Delete from here*, or press *Delete*. *Ctrl+Z* undoes it.
- **Promote a variation**: right-click one of its moves, *Promote variation*.
- **Comments and symbols** (!, ?, !?, …) are added to the current move below the notation.

Each open study has its own tab, up to sixteen; they come back after a restart.

## Analysing with an engine

- *Analyze* runs the engine with no depth limit; several lines are shown (MultiPV). Clicking a line inserts it into the study.
- Blue arrows show the best moves; *Threat* (key *M*) shows in red what the opponent would play if you passed your turn.
- The engine's threads and memory are set from your machine; *Settings* lets you change them.
- Several engines can analyse side by side, and engine tournaments are run from *Engine → Engine tournament*. The standings give the Elo gap with its 95% error margin and the LOS, the likelihood that the engine is really stronger: below 95%, the result may still be down to chance.

## Understanding your mistakes

*Review* analyses the game and grades each move. First choose the **depth**: 18 for a quick look, 22 for a review comparable to Lichess's, 26 or 30 to go further. The engine uses every core but one; with *Two passes*, it goes through the game from start to end, then from the end back to the start, using what it learnt. The depth actually reached is shown under the progress bar, and the game's three key moments are numbered on the graph.

 Opening a move's explanation shows **why** it is worse than the engine's: mate allowed or missed, material lost or missed (with the piece, the square and the move that shows it), a piece left en prise, or otherwise a positional gap. Patterns (fork, pin, discovered attack, threat of mate…) and what the move changes to the pawns and the king's shelter are given alongside.

These explanations come from the lines the engine computed: a longer calculation may say otherwise.

## The library

Every game you save (*Ctrl+S*) lives in the library.

- **Search**: by player, event, date, result, rating, opening; by exact position; by material; by pawn structure; by pieces on squares (*Ke1 Pd4 pd5*, uppercase for White — this last search replays every game and can be slow).
- **Merge**: *Database → Merge the games shown into one study* turns a search result into a single study, each game becoming a variation where it leaves the others.
- **Import** large databases: importing is parallel, resumable and checks integrity. *Database → Live tournaments* shows Lichess broadcasts: the boards of a round, and *Follow* opens a game in the analysis, where its moves arrive as it is played. The ⋮ menu of a game in the list opens the *Tournament crosstable*: standings, tie-breaks, performance, and a click on a result opens the game. En Croissant `.db3` databases and SCID format 4 databases (`.si4`, with their `.sn4` and `.sg4` alongside) can be read too.
- **Duplicates**: *Database → Duplicate moves* finds games saved twice.
- **Position dossier**: for the position shown, the moves played, results by level and the best games.

## Studies and files

The *Studies* page shows the tree of your PGN folder: folders to expand, search, creating, renaming and moving files. Deleting moves files to the system trash, never permanently.

*Export this study* (in the analysis ⋮ menu or through Ctrl+K) writes the study as PGN, the position as EPD, or the study as a **standalone HTML page** that opens in any browser, without Trait64.

## Openings and repertoires

- The *Openings* page shows the move tree of your games, Polyglot books and each position's ECO name.
- A **repertoire** gathers the lines you play. Its gaps tab gives the **estimated coverage** — the share of your library's games that stay within the repertoire — and the unprepared opponent replies, from most to least frequent.
- *Compare…* puts two repertoires side by side.
- The *Lichess* tab of the analysis shows what is played on Lichess or by masters in the position shown, and seven-piece endgame tables (connection needed). Lichess requires an account for this: in *File → Lichess and Chess.com accounts*, *Sign in with Lichess* opens your browser, you authorise Trait64, and the account is added or linked. You can also paste a token created on Lichess and choose the account to link it to. If you have Syzygy tables on your computer, set their folder in the settings: the endgames they cover are then read offline. The settings can also download them: 3 to 5 pieces (939 MiB) cover most endgames.

## Training

- **Puzzles**: chosen for your level, by theme; your results by theme are shown, and a missed puzzle comes back the next day, then three and seven days after each success.
- **Cards**: a line or a mistake to find again, repeated at intervals.
- **Repertoire**: find your prepared moves.
- **Guess the move**: replay a master game, looking for each move of one side.
- **Endgames to play**: basic endgames and the Lucena and Philidor positions, against the engine.
- **Coordinates**: thirty seconds to click the named squares.
- **Blindfold**: the pieces disappear, moves are still played.

## Your progress

The *Profiles* page gathers your Lichess and Chess.com accounts (*File → Lichess and Chess.com accounts*) and your FIDE record under one name. A sync fetches up to 5,000 games per account, then only the new ones; variant games (Crazyhouse, Atomic…) are left aside.

*Review this profile* sends every game of the profile to the analysis queue at the chosen depth (12, 18, 22 like Lichess, or 24). The report fills in as it goes: it says how many games have been reviewed out of the total, and how many are still waiting. A game already reviewed as deep is not redone. Accuracy by phase, errors per hundred moves, estimated strength and month-by-month progress are then read in the same place.

## Playing

*Play a game* sets you against an engine at the chosen strength (in Elo for engines that support it), or two people on the same computer, with a clock and increment.

## Backups and updates

- The library is backed up every six hours (the last fourteen are kept) and before every update.
- *File → Restore a backup* lists these backups; restoring replaces the library after backing it up in turn.
- When a new version is published, Trait64 offers it; the version button, bottom right, changes colour.

Your data is in the folder shown in *Settings*; the library is `library.sqlite`.

## Default shortcuts

| Key | Action |
|---|---|
| ← / → | Previous / next move |
| Home / End | Start / end of the line |
| ↑ / ↓ | Previous / next variation |
| Alt+← / Alt+→ | Previous / next game in the list |
| Space | Automatic playback |
| F | Flip the board |
| M | Threat |
| Ctrl+B / F11 | Board only / full screen |
| Ctrl+L | Type a move (“Nf3”, “e4”, “O-O”, or “e2e4”) then Enter |
| Delete | Delete the move and what follows |
| Ctrl+Z / Ctrl+Shift+Z | Undo / redo |
| Ctrl+N / Ctrl+O / Ctrl+S | New study / import / save |
| Ctrl+K | Command palette |

All can be changed in *View → Keyboard shortcuts*.
