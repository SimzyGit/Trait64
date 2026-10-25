# Qualification des 100 000 parties — 24 octobre 2026

**Protocole historique :** les essais Qt de ce jour ne sont pas qualifiés.
La suite avec jobs indépendants et rapport conservé en cas de délai dépassé
est décrite dans [le protocole du 25 octobre](../2026-10-25/REPRODUCE.md).

Deux hôtes sont distingués : le rapport local `material-100k.json` et les
rapports du runner GitHub dans `github/`. Aucun gain de performance n'est déduit
en comparant ces machines. La comparaison avant/après sur 10 000 parties reste
dans [le dossier du 23 octobre](../2026-10-23/REPRODUCE.md).

Code produit : dev.55, commit `550f28f`. Le workflow de qualification est sur
la branche `perf/qt-corpus-qualification`, commit `e34e545` ; ses seuls ajouts
exécutables sont le workflow et le script de mesure. Les sources de l'application,
des bibliothèques et des sondes sont identiques à celles de la version publiée.

## Corpus et exécution

Corpus réel Lichess standard classé, janvier 2013, CC0 :
https://database.lichess.org/standard/lichess_db_standard_rated_2013-01.pgn.zst

PGN décompressé : 92 811 021 octets, 121 332 parties ; SHA-256
`8963b6a1620a0e9c77e5515a0744ec133e86869487188af047bb0a74400dee37`.
Les premières 100 000 parties sont lues dans l'ordre, sans duplication.

Depuis un worktree au commit de qualification :

```sh
cmake -S . -B build-perf -G Ninja -DCMAKE_BUILD_TYPE=Release -DT64_BENCHMARKS=ON
cmake --build build-perf --target pgn_benchmark desktop_tests --parallel 2
python3 scripts/qualify-real-corpus.py --input /corpus/source.pgn --output /mesures/neuves
```

Dans l'environnement local contraint, compiler avec `--parallel 1`. Arrêter toute
compilation avant les mesures. Le dossier de sortie doit être neuf. Le script
vérifie le corpus avant parsing et exige au moins 14 Gio disponibles avant chaque
scénario ; ce seuil concerne ce corpus de 100k, pas les volumes supérieurs.

Ordre : Qt 100k, reconstruction des index 10k, import CLI 100k avec pause/reprise
et lecteur concurrent, sauvegarde/restauration, vérification de tous les PGN,
puis profil `dbstat`. Les scénarios sont successifs. Les fichiers PGN/SQLite
restent temporaires ; seuls les petits rapports et la méthode sont conservés.

Le premier lancement GitHub s'est arrêté avant les mesures par manque d'espace.
Le second supprime seulement deux SDK préinstallés inutilisés du runner GitHub
jetable (.NET et Android) et conserve le seuil de place libre. Il n'efface aucune
donnée du dépôt, release, archive Actions ou dossier utilisateur.

## Interprétation des rapports

- `method` décrit le compilateur, la machine, les versions et les empreintes des binaires.
- `*-process` mesure le processus complet, initialisation et nettoyage compris, avec `os.wait4` ; RSS du processus, hors cache de fichiers du noyau.
- `gui-100k` utilise une vraie fenêtre Qt/QML sans écran, en rendu logiciel, un timer demandé à 10 ms et des changements programmatiques entre Accueil et Analyse chaque seconde. Vérifier le nombre réellement traité, les avertissements, la médiane, le p95 et le maximum. Ce n'est pas un test de clic humain, de GPU ou de cadence d'images. Son démarrage part d'une bibliothèque vide.
- `material-100k` inclut parsing, tous les index et vérifications de reprise. La réouverture mesurée est celle de SQLite après import, pas celle de toute l'application avec une grosse bibliothèque. Cinq lectures finales par opération ; une première page de 200 résultats, suivie d’une attente de 100 ms avant la lecture suivante pendant l’import. Les distributions SQL ne mesurent pas la latence Qt.
- `reindex-10k` sépare le temps de reconstruction complète des index du temps d'import initial.
- `restored-content-100k` compare le SHA-256 de **tous** les PGN canoniques dans l'ordre des identifiants, ainsi que les comptes des deux tables de positions, sur l'original et ses deux copies. L'intégrité SQLite a déjà été vérifiée. Ce n'est pas une comparaison de chaque octet de tous les index.
- `storage-profile-100k` relève les tailles réelles des tables/index après les mesures ; il exclut WAL/SHM.

Les JSON sont délimités dans les journaux par `TRAIT64_REPORT_BEGIN nom` et
`TRAIT64_REPORT_END nom`. Aucune archive Actions n'est produite. Le rapport local
antérieur a été récupéré depuis sa sortie complète observée avant réinitialisation ;
voir `recovery-provenance.json`, sans le confondre avec une nouvelle mesure.

Une observation par scénario, caches non purgés ; les filtres fréquents, pages
éloignées et combinaisons complexes restent à explorer. Aucun résultat sur
500 000 ou plusieurs millions de parties, ni sur le matériel Voyager, n'est acquis.

Les copies du script et du workflow sont aussi conservées dans ce dossier. Depuis
la racine de main, le script s'exécute également avec
`python3 docs/benchmarks/2026-10-24/qualify-real-corpus.py --input /corpus/source.pgn --output /mesures/neuves`.
