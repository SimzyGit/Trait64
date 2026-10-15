# Point de reprise — priorité stockage GitHub, 15 octobre 2026

Objectif utilisateur conservé : poursuivre toute la feuille de route Trait64, sans recréer l’application. L’esthétique reste en pause ; aucun niveau Elo fictif. Le 15 octobre, l’utilisateur a demandé de libérer d’abord le stockage GitHub, sans oublier les développements en cours.

Code courant sur main : **62737b2e87eafc9358717a08de9392d2cc8a5ae7**, PR #5 fusionnée, schéma 6. Le lot d'import décrit ci-dessous est intégré et validé. Dernière version distribuée : **v0.3.0-dev.33** ; la nouvelle dev.36 attend le recalcul du quota GitHub (voir ci-dessous). Les modules d'analyse de la section « État de récupération » ne sont pas encore présents.

## Publication à reprendre après recalcul du quota

L'exécution main [38692749119](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/38692749119), numéro 36, a réussi les dix suites sur Debian 12 (13,63 s) et Debian 13 (13,75 s), les paquets DEB installés, l'AppImage et le cœur ASan/UBSan. **Seul le téléversement des archives Actions échoue** : `Artifact storage quota has been hit`, avec recalcul annoncé toutes les 6 à 12 heures. La publication est donc ignorée ; aucun tag/release dev.36 n'a été créé. Inventaire de cette exécution : zéro artefact. Les dix releases antérieures et leurs cinquante fichiers sont inchangés.

La suppression initiale a eu lieu le 15 octobre à 05:59 UTC : vérifier le recalcul ultérieurement, puis utiliser **Re-run failed jobs** pour cette exécution précise, sans relancer en boucle les compilations pendant l'attente. Le numéro de version restera dev.36. Vérifier ensuite les cinq fichiers de la release, leurs SHA-256 et l'exécution de purge `workflow_run`. Ne pas déclarer dev.36 disponible tant que cette vérification n'a pas réussi. Le connecteur GitHub expose `rerun_failed_workflow_run_jobs` pour ce suivi.

## Nettoyage du stockage effectué

Le 15 octobre à 05:59 UTC, l'exécution [38689692486](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/38689692486) a supprimé **58 archives Actions, soit 8 371 207 728 octets**. Inventaire final : zéro archive et zéro octet de cache. Les dix releases et leurs cinquante fichiers sont inchangés (identifiants, noms, tailles et empreintes contrôlés avant/après). Aucun code, branche ou historique n'a été supprimé.

La [PR #4](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/4), fusionnée dans main (`ec62a99d1807b3f3e45f68b46deb15a688f0954b`), supprime les téléversements temporaires sur les PR, réduit leur conservation à un jour pour les publications et ajoute une purge après succès ainsi qu'une purge quotidienne. Voir `GITHUB-STORAGE.md`. La CI au commit exact de la PR est réussie : 9/9 suites sur Debian 12, 9/9 sur Debian 13, cœur ASan/UBSan, DEB installés et AppImage. Aucun artefact n'a été créé par ces vérifications. Cette maintenance seule n'a pas changé la version distribuée.

## Code présent dans ce point de sauvegarde

- Parseur PGN parallèle borné, 1 à 8 lecteurs, ordre/offsets conservés, consommateur sur son thread appelant, annulation/erreurs ; `libs/core/parallel.cpp` et extraction `scanPgnRecords`.
- Import reprenable paramétrable, préférence et contrôle QML `importWorkers` ; un seul écrivain SQLite et transactions/checkpoints inchangés.
- Tests de parsing parallèle et imports avec quatre lecteurs ; cible optionnelle `pgn_benchmark` sur un PGN réel.
- Compatibilité SAN surspécifiée rétablie : `Rd1d2` n'est accepté que s'il correspond à un unique coup légal ; notation canonique à l'export, captures/origines invalides et ambiguïtés rejetées. Régression sur une partie réelle Lichess.
- Protection des sorties du benchmark : refus des collisions rapport/base/journaux/sauvegarde, des fichiers existants et des alias de dossier ; création du rapport sans écrasement. Six scénarios exécutent le vrai binaire.
- Validation du 15 octobre : **10/10 suites locales réussies en 17,23 s**, Stockfish réel inclus. Dialogue d'import contrôlé à 960 × 720. La CI Debian 12/13 compile et teste désormais aussi le benchmark optionnel avec de petits PGN.

## État de récupération du travail de la séance précédente

Au début de la reprise du 15 octobre, l'environnement ne contenait que l'import parallèle. Les modifications d'analyse plus récentes, décrites et codées dans le chat, n'ont pas atteint GitHub : ne pas les annoncer comme publiées. Le point 1 est maintenant rétabli et validé ; les points 2 à 6 restent à récupérer et terminer.

1. **Terminé le 15 octobre** : compatibilité SAN surspécifiée (`Rd1d2` rencontré dans les anciens PGN Lichess), régression avec la partie réelle et rejets de `Rd2`, `Rdd2`, origine/fausse capture invalides. Protection des chemins du benchmark testée.
2. File d’analyses : migration 7 avec sauvegarde, `libs/database/analyses.cpp`, `libs/analysis/queue.{h,cpp}`, `app/analyses.cpp`, `ui/AnalysisQueue.qml` et intégration contrôleur/CMake. Worker QThread dédié, connexions SQLite/UCI créées sur ce thread, verrou par bibliothèque, copie PGN immuable, analyses de toutes les branches ou ligne principale, pause/reprise et récupération après plantage, export PGN annoté/PV légales, pagination 200 positions.
3. Identité moteur SHA-256/options/NNUE, cache borné à 10 000 entrées avec historique UCI complet et compteurs FEN, exclusions des ressources non identifiées/Syzygy. Comparer les identités par JSON canonique pour éviter les différences QVariant/QStringList après relecture. Refuser la reprise avec un moteur modifié. Un double clic Reprendre ne doit pas annuler la tâche.
4. UCI : signal initialized après readyok, scoreType/scoreValue, nodes/time, WDL réellement transmis et orienté Blancs, rejeter les scores non numériques/inconnus. Bilan existant : perte en centipions seulement si les deux scores sont cp, sinon score de mat séparé ; ne pas calculer une perte artificielle de 10 000 cp.
5. Tests `analysis_tests` : vrai Stockfish, reprise, cache, verrou, processus interrompu réellement, binaire changé, branches, export légal. Ils passaient dans la séance précédente. Fixture UCI réservée aux tests pour WDL/types/bornes. Migration fixture 1→7 doit supprimer les trois tables d’analyse avant downgrade.
6. Dernier blocage à résoudre : test desktop d’intégration du contrôleur, assertion « Background branch analysis leaves the current study intact ». Inspecter l’état/erreur de la file et le mode `Controller::importText` avant de modifier le produit. Le test de mat corrigé utilise `7k/6pp/8/8/8/8/6PP/5RK1 w - - 0 1` puis `f1f8` (sans pion en f2). Corriger le booléen QML `enabled:!!root.queue.state.hasNext`. Le dialogue peuplé doit être contrôlé visuellement.

## Mesures réelles et rapports conservés

Mesures régénérées le 15 octobre et désormais enregistrées dans `performance-pgn-2026-10-15.json` : 10 000 parties / 661 314 positions, parsing + réexport/hash identique à 1 et 4 lecteurs (10,126 s / 3,637 s), import avec pause/reprise 72,462 s, intégrité et sauvegarde vérifiées. Voir `PERFORMANCE.md` pour les conditions et limites. Aucun PGN volumineux ni fichier SQLite n'est ajouté au dépôt.

Historique de la séance précédente (rapports bruts alors absents, remplacés par les nouvelles mesures ci-dessus) :

Corpus CC0 Lichess janvier 2013, archive SHA256 aa40b3671fa3cf1072eb182892cd90b0e1e003a4a5943492f64b77e7f3fd1635 ; PGN décompressé 92 811 021 octets, SHA256 8963b6a1620a0e9c77e5515a0744ec133e86869487188af047bb0a74400dee37. Les 10 000 premières parties : 661 314 positions. Release/GCC13.3/Qt6.4.2, hôte Linux partagé. Parsing + réexport/hash : 11,038 s (1 lecteur), 3,571 s (4), hash canonique identique fe13b24aad0d68aa6d7fde69f14bf796c1d47904ff472acdf99e1ac7de41a2d8. Import avec pause à 5 000 puis reprise : 79,845 s ; 371 642 368 octets avec journaux, pic RSS 18 764 Kio. Intégrité ok, sauvegarde vérifiée (0,731 s). Les rapports JSON ne sont pas présents dans l’environnement repris : les régénérer avant publication des données brutes. Aucun essai 1/2/5/10 millions de parties complètes n’est validé.

Prochaine reprise : finaliser la publication dev.36 après recalcul du quota, puis récupérer/implémenter la file d'analyses et ses tests (points 2 à 6 ci-dessus). Continuer avec les entraînements tactiques sous licence, filtres/progression, répertoires asynchrones, explications tactiques vérifiables et le reste de ROADMAP. Ne pas présenter l'ensemble des ~245 objectifs comme achevé. Le code courant et le point de reprise sont sur main ; ne pas repartir d'une ancienne branche.

Compilation locale : `source ../qt-env.sh`, `--parallel 1`, aucune édition C++/CMake pendant compilation, `set -e` avant build/tests. Un exécutable généré vide après restauration doit être régénéré, pas simplement rendu exécutable. Ne pas modifier les fichiers de travail lors du nettoyage : branche/worktree de maintenance séparée.
