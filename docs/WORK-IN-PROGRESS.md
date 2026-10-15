# Point de reprise — analyses persistantes, 15 octobre 2026

Objectif utilisateur conservé : poursuivre toute la feuille de route Trait64, sans recréer l’application. L’esthétique reste en pause ; aucun niveau Elo fictif. Le 15 octobre, l’utilisateur a demandé de libérer d’abord le stockage GitHub, sans oublier les développements en cours.

Base du lot : main `f1e4c629a9d1dcff8b6242636777ad211ebadfbc`, PR #5 intégrée. Branche courante **feat/persistent-analysis**, schéma **7**. Les modules d’analyse ont été réimplémentés et testés, sans reprendre le logiciel de zéro. Le premier commit UCI est sauvegardé sur GitHub (`91f4584d956a3266a68a0480637fdaeff778a81d`) ; le lot complet est conservé sur cette branche et doit être validé en CI avant fusion. Dernière version distribuée vérifiée : **v0.3.0-dev.33** ; dev.36 reste en attente du recalcul de quota décrit ci-dessous.

## Lot actuel et vérifications

- `libs/database/analyses.cpp`, `libs/analysis/queue.*`, `app/analyses.cpp`, `ui/AnalysisQueue.qml` : copie PGN indépendante, worker QThread, verrou, pause/reprise et récupération après plantage, toutes les branches ou ligne principale, profils/MultiPV, pagination et export annoté.
- Identité du binaire et des ressources, options réellement déclarées, comparaison JSON canonique ; cache 10 000 positions comprenant l’historique entier, les compteurs FEN et les paramètres. Ressources non résolues et Syzygy désactivent le cache.
- UCI : initialisation après readyok, scores cp/mate séparés, WDL réellement reçu et orienté Blancs, nodes/time, rejet des valeurs non numériques/inconnues. Les mats n’engendrent plus des pertes artificielles de 10 000 cp.
- Première passe complète : **11/11 suites en 49,46 s** avec Stockfish 16 réel. Vérifications supplémentaires : **95 contrôles d’analyse**, dont pause/destruction/reprise, cache, binaire modifié, processus réellement tué, verrou récupéré, Chess960, pagination 200/9 et migrations 1/6 → 7.
- Le test desktop édite une étude pendant l’analyse de sa copie ; les branches et commentaires restent distincts. `importText` importe en bibliothèque, il n’ouvre pas l’étude : le nouveau test construit explicitement les coups.
- Dialogue peuplé inspecté à 960 × 720, capture via `T64_ANALYSIS_SCREENSHOT`. Le redimensionnement a révélé un plantage du panneau de comparaison UCI fermé, causé par des éléments de Repeater retirés encore référencés par son layout. Les listes dynamiques utilisent désormais des positionneurs ; le test conserve le redimensionnement déclencheur et passe **404 vérifications**. Protection des chemins d’export étendue aux verrous, y compris si la base est ouverte via un lien.
- Prochaines actions : sauvegarder ce lot sur GitHub, PR et CI Debian 12/13, inspecter le résultat exact avant fusion. Puis poursuivre les corpus tactiques/licences, progression, répertoires asynchrones et explications tactiques vérifiables. Le détail fonctionnel est dans ANALYSIS-QUEUE ; toute la feuille de route n’est pas terminée.

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

## Mesures réelles et rapports conservés

Mesures régénérées le 15 octobre et désormais enregistrées dans `performance-pgn-2026-10-15.json` : 10 000 parties / 661 314 positions, parsing + réexport/hash identique à 1 et 4 lecteurs (10,126 s / 3,637 s), import avec pause/reprise 72,462 s, intégrité et sauvegarde vérifiées. Voir `PERFORMANCE.md` pour les conditions et limites. Aucun PGN volumineux ni fichier SQLite n'est ajouté au dépôt.

Historique de la séance précédente (rapports bruts alors absents, remplacés par les nouvelles mesures ci-dessus) :

Corpus CC0 Lichess janvier 2013, archive SHA256 aa40b3671fa3cf1072eb182892cd90b0e1e003a4a5943492f64b77e7f3fd1635 ; PGN décompressé 92 811 021 octets, SHA256 8963b6a1620a0e9c77e5515a0744ec133e86869487188af047bb0a74400dee37. Les 10 000 premières parties : 661 314 positions. Release/GCC13.3/Qt6.4.2, hôte Linux partagé. Parsing + réexport/hash : 11,038 s (1 lecteur), 3,571 s (4), hash canonique identique fe13b24aad0d68aa6d7fde69f14bf796c1d47904ff472acdf99e1ac7de41a2d8. Import avec pause à 5 000 puis reprise : 79,845 s ; 371 642 368 octets avec journaux, pic RSS 18 764 Kio. Intégrité ok, sauvegarde vérifiée (0,731 s). Les rapports JSON ne sont pas présents dans l’environnement repris : les régénérer avant publication des données brutes. Aucun essai 1/2/5/10 millions de parties complètes n’est validé.


Compilation locale : `source ../qt-env.sh`, `--parallel 1`, aucune édition C++/CMake pendant compilation, `set -e` avant build/tests. Un exécutable généré vide après restauration doit être régénéré, pas simplement rendu exécutable. Ne pas modifier les fichiers de travail lors du nettoyage : branche/worktree de maintenance séparée.
