# Point de reprise — priorité stockage GitHub, 15 octobre 2026

Objectif utilisateur conservé : poursuivre toute la feuille de route Trait64, sans recréer l’application. L’esthétique reste en pause ; aucun niveau Elo fictif. Le 15 octobre, l’utilisateur a demandé de libérer d’abord le stockage GitHub, sans oublier les développements en cours.

Base publiée : main b78d52fe549a5b8ce80b2dac30e0b7765d663004, v0.3.0-dev.33, schéma 6. Les tâches suivantes ne sont pas encore publiées.

## Code présent dans ce point de sauvegarde

- Parseur PGN parallèle borné, 1 à 8 lecteurs, ordre/offsets conservés, consommateur sur son thread appelant, annulation/erreurs ; `libs/core/parallel.cpp` et extraction `scanPgnRecords`.
- Import reprenable paramétrable, préférence et contrôle QML `importWorkers` ; un seul écrivain SQLite et transactions/checkpoints inchangés.
- Tests de parsing parallèle et imports avec quatre lecteurs ; cible optionnelle `pgn_benchmark` sur un PGN réel.
- Dernière validation avant les extensions suivantes : neuf suites réussies.

## Travail de la séance précédente à récupérer et terminer

L’environnement repris le 15 octobre ne contient que le point ci-dessus. Les modifications plus récentes, décrites et codées dans le chat, n’ont pas atteint GitHub : ne pas les annoncer comme publiées. Reprendre le code du chat, puis revalider.

1. Compatibilité SAN surspécifiée : `Rd1d2` rencontré dans les anciens PGN Lichess, accepter seulement un unique coup légal. Régression avec la partie réelle et rejets de `Rd2`, `Rdd2`, origine/fausse capture invalides. Protéger le benchmark contre un chemin de rapport identique à la base/journaux.
2. File d’analyses : migration 7 avec sauvegarde, `libs/database/analyses.cpp`, `libs/analysis/queue.{h,cpp}`, `app/analyses.cpp`, `ui/AnalysisQueue.qml` et intégration contrôleur/CMake. Worker QThread dédié, connexions SQLite/UCI créées sur ce thread, verrou par bibliothèque, copie PGN immuable, analyses de toutes les branches ou ligne principale, pause/reprise et récupération après plantage, export PGN annoté/PV légales, pagination 200 positions.
3. Identité moteur SHA-256/options/NNUE, cache borné à 10 000 entrées avec historique UCI complet et compteurs FEN, exclusions des ressources non identifiées/Syzygy. Comparer les identités par JSON canonique pour éviter les différences QVariant/QStringList après relecture. Refuser la reprise avec un moteur modifié. Un double clic Reprendre ne doit pas annuler la tâche.
4. UCI : signal initialized après readyok, scoreType/scoreValue, nodes/time, WDL réellement transmis et orienté Blancs, rejeter les scores non numériques/inconnus. Bilan existant : perte en centipions seulement si les deux scores sont cp, sinon score de mat séparé ; ne pas calculer une perte artificielle de 10 000 cp.
5. Tests `analysis_tests` : vrai Stockfish, reprise, cache, verrou, processus interrompu réellement, binaire changé, branches, export légal. Ils passaient dans la séance précédente. Fixture UCI réservée aux tests pour WDL/types/bornes. Migration fixture 1→7 doit supprimer les trois tables d’analyse avant downgrade.
6. Dernier blocage à résoudre : test desktop d’intégration du contrôleur, assertion « Background branch analysis leaves the current study intact ». Inspecter l’état/erreur de la file et le mode `Controller::importText` avant de modifier le produit. Le test de mat corrigé utilise `7k/6pp/8/8/8/8/6PP/5RK1 w - - 0 1` puis `f1f8` (sans pion en f2). Corriger le booléen QML `enabled:!!root.queue.state.hasNext`. Le dialogue peuplé doit être contrôlé visuellement.

## Mesures réelles effectuées dans la séance précédente

Corpus CC0 Lichess janvier 2013, archive SHA256 aa40b3671fa3cf1072eb182892cd90b0e1e003a4a5943492f64b77e7f3fd1635 ; PGN décompressé 92 811 021 octets, SHA256 8963b6a1620a0e9c77e5515a0744ec133e86869487188af047bb0a74400dee37. Les 10 000 premières parties : 661 314 positions. Release/GCC13.3/Qt6.4.2, hôte Linux partagé. Parsing + réexport/hash : 11,038 s (1 lecteur), 3,571 s (4), hash canonique identique fe13b24aad0d68aa6d7fde69f14bf796c1d47904ff472acdf99e1ac7de41a2d8. Import avec pause à 5 000 puis reprise : 79,845 s ; 371 642 368 octets avec journaux, pic RSS 18 764 Kio. Intégrité ok, sauvegarde vérifiée (0,731 s). Les rapports JSON ne sont pas présents dans l’environnement repris : les régénérer avant publication des données brutes. Aucun essai 1/2/5/10 millions de parties complètes n’est validé.

Après le nettoyage GitHub : terminer/valider/publier ce lot en Développement, puis entraînements tactiques sous licence avec filtres et progression, répertoires asynchrones, explications tactiques vérifiables et reste de ROADMAP. Ne pas présenter l’ensemble des ~245 objectifs comme achevé.

Compilation locale : `source ../qt-env.sh`, `--parallel 1`, aucune édition C++/CMake pendant compilation, `set -e` avant build/tests. Un exécutable généré vide après restauration doit être régénéré, pas simplement rendu exécutable. Ne pas modifier les fichiers de travail lors du nettoyage : branche/worktree de maintenance séparée.
