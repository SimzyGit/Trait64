# Reproduire la qualification du 23 octobre

Corpus Lichess standard classé, janvier 2013, publié sous CC0 :
https://database.lichess.org/standard/lichess_db_standard_rated_2013-01.pgn.zst
Licence : https://database.lichess.org/.

Le PGN décompressé mesure 92 811 021 octets. SHA-256 :
`8963b6a1620a0e9c77e5515a0744ec133e86869487188af047bb0a74400dee37`.
Le fichier complet contient 121 332 parties. Les préfixes sont lus en ordre ;
aucune partie n'est dupliquée pour simuler un volume supérieur.

## Trois états comparés

| État | Source de `libs/database/database.cpp` |
|---|---|
| Avant | `06a4321e4a6d7e7ff4446d6d73ff39d3555c913a` |
| Index des doublons | `69250f06543e6fcd666a1d33ef6e86ec5122580c` |
| Index et matériel | `e21e8d31f2795c06c0f2d912500a91fbee7e1559` |

Les autres objets sont identiques. Le benchmark enrichi est celui de `69250f0`,
également présent dans `e21e8d3`, pour mesurer la restauration aussi sur l'ancien
code. Dans l'essai archivé, les deux références ont été produites en recompilant
leur `database.cpp` avec les mêmes options, puis en remplaçant uniquement cet
objet dans une copie de `libt64storage.a` avant édition des liens. Empreintes des
exécutables et commandes exactes : `commands.json`.

Depuis un clone Git complet, créer trois worktrees aux commits ci-dessus.
Dans celui de référence, reprendre uniquement `tests/pgn_benchmark.cpp` depuis
`69250f0`. Compiler chaque worktree avec :

```sh
cmake -S . -B build-perf -G Ninja -DCMAKE_BUILD_TYPE=Release -DT64_BENCHMARKS=ON
cmake --build build-perf --target pgn_benchmark --parallel 1
```

Exécuter successivement, sans compilation concurrente, vers des fichiers neufs :

```sh
./build-perf/pgn_benchmark --input /corpus/janvier-2013.pgn --threads 1 --limit 10000 --report /mesures/parse-1.json
./build-perf/pgn_benchmark --input /corpus/janvier-2013.pgn --threads 4 --limit 10000 --report /mesures/parse-4.json
./build-perf/pgn_benchmark --input /corpus/janvier-2013.pgn --threads 4 --limit 10000 --pause-at 5000 --database /mesures/etat-neuf.sqlite --backup --report /mesures/etat-neuf.json
```

Changer les noms de sortie pour chaque état. L'outil refuse les collisions entre
rapport, base, journaux et copies, et les destinations déjà occupées. La dernière
ligne crée trois bases : original, sauvegarde et restauration. Les conserver
jusqu'à la comparaison logique, puis archiver les petits rapports ; jamais les
corpus ou bases dans Git. La mémoire publiée est celle du processus de benchmark.

## Validation du contenu

Comparer en ordre les colonnes échiquéennes de `games` (hors UUID et date de
modification générés), toutes les lignes de `positions`, `all_positions`,
`opening_edges`, `opening_stats`, `library_counts`, `player_counts` et
`source_games`. `logical-equivalence.json` contient les requêtes exactes,
comptes et SHA-256 des lignes JSON ordonnées. Les tables internes FTS et les
métadonnées des tâches d'import sont exclues de cette empreinte ; intégrité SQLite
et tests de recherche complètent cette comparaison. Vérifier aussi la restauration.

## Palier suivant et interface

Après contrôle du disque, reprendre la commande avec `--limit 100000`,
`--pause-at 50000`, `--probe-reads` et des sorties neuves. L'indexation est incluse
dans l'import ; `--reindex` mesure en plus une reconstruction transactionnelle
complète, avec un besoin temporaire de disque supérieur. Les lectures SQL
concurrentes ne mesurent pas la fluidité de l'interface.

```sh
cmake --build build-perf --target desktop_tests --parallel 1
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build-perf/desktop_tests --benchmark-import /corpus/janvier-2013.pgn --benchmark-games 100000 > /mesures/gui-100k.json
```

Le mode Qt possède son dossier temporaire et n'ouvre aucune base utilisateur.
Il change de page pendant l'import et relève les événements d'un timer de 10 ms,
y compris le délai du dernier rafraîchissement. Ce n'est pas une mesure des images
par seconde. Vérifier le nombre réellement traité, le statut, les avertissements
et le maximum des délais, pas uniquement la médiane.

Hôte partagé, caches non purgés, une observation par variante : ces mesures
ne garantissent aucun débit et ne qualifient pas Voyager. Ne pas lancer
500 000 ou plusieurs millions sans corpus assez grand et sans espace pour
la base, les journaux et les copies. Une estimation n'est pas un test.

