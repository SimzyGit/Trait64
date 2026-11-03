# Validation et mesures

Résultats de test conservés sur main. Les données brutes de chaque rapport sont dans
[VALIDATION.json](VALIDATION.json), sous la clé de leur ancien chemin (par exemple
`verification-ui-2026-10-28.json` ou `benchmarks/2026-10-25/gui/gui-100k.json`).
Les fichiers d'origine restent consultables dans la branche [archive/validation-2026-10](https://github.com/SimzyGit/Trait64/tree/archive/validation-2026-10/docs).
Les outils de mesure sont dans [tools/benchmarks](../tools/benchmarks).

## Rapports

| Rapport | Date |
| --- | --- |
| `benchmarks/2026-10-23/before-10k.json` | 2026-10-23 |
| `benchmarks/2026-10-23/commands.json` | 2026-10-23 |
| `benchmarks/2026-10-23/digest-10k.json` | 2026-10-23 |
| `benchmarks/2026-10-23/local-tests.txt` | 2026-10-23 |
| `benchmarks/2026-10-23/logical-equivalence.json` | 2026-10-23 |
| `benchmarks/2026-10-23/material-10k.json` | 2026-10-23 |
| `benchmarks/2026-10-23/method.json` | 2026-10-23 |
| `benchmarks/2026-10-23/parse-100k-4.json` | 2026-10-23 |
| `benchmarks/2026-10-23/parse-10k-1.json` | 2026-10-23 |
| `benchmarks/2026-10-23/parse-10k-4.json` | 2026-10-23 |
| `benchmarks/2026-10-23/validation.json` | 2026-10-23 |
| `benchmarks/2026-10-24/corpus-qualification.yml` | 2026-10-24 |
| `benchmarks/2026-10-24/github/40097867485/gui-100k-process.json` | 2026-10-24 |
| `benchmarks/2026-10-24/github/40097867485/method.json` | 2026-10-24 |
| `benchmarks/2026-10-24/github/40097867485/outcome.json` | 2026-10-24 |
| `benchmarks/2026-10-24/material-100k.json` | 2026-10-24 |
| `benchmarks/2026-10-24/pragma-comparison.json` | 2026-10-24 |
| `benchmarks/2026-10-24/recovery-provenance.json` | 2026-10-24 |
| `benchmarks/2026-10-24/update-selector.json` | 2026-10-24 |
| `benchmarks/2026-10-25/corpus-qualification.yml` | 2026-10-25 |
| `benchmarks/2026-10-25/gui/gui-100k-process.json` | 2026-10-25 |
| `benchmarks/2026-10-25/gui/gui-100k.json` | 2026-10-25 |
| `benchmarks/2026-10-25/gui/method.json` | 2026-10-25 |
| `benchmarks/2026-10-25/integrity-local-tests.txt` | 2026-10-25 |
| `benchmarks/2026-10-25/qualification-summary.json` | 2026-10-25 |
| `benchmarks/2026-10-25/source-provenance.json` | 2026-10-25 |
| `benchmarks/2026-10-25/storage/material-100k-process.json` | 2026-10-25 |
| `benchmarks/2026-10-25/storage/material-100k.json` | 2026-10-25 |
| `benchmarks/2026-10-25/storage/method.json` | 2026-10-25 |
| `benchmarks/2026-10-25/storage/reindex-10k-process.json` | 2026-10-25 |
| `benchmarks/2026-10-25/storage/reindex-10k.json` | 2026-10-25 |
| `benchmarks/2026-10-25/storage/restored-content-100k.json` | 2026-10-25 |
| `benchmarks/2026-10-25/storage/storage-profile-100k.json` | 2026-10-25 |
| `benchmarks/2026-10-25/update-selector.json` | 2026-10-25 |
| `notebook-layout-2026-10-28.json` | 2026-10-28 |
| `notebook-local-tests-2026-10-28.txt` | 2026-10-28 |
| `performance-2026-10-10.json` | 2026-10-10 |
| `performance-cursors-2026-10-13.json` | 2026-10-13 |
| `performance-cursors-before-2026-10-12.json` | 2026-10-12 |
| `performance-pgn-2026-10-15.json` | 2026-10-15 |
| `ui-validation/2026-10-27/local.json` | 2026-10-27 |
| `ui-validation/2026-10-27/matrix.json` | 2026-10-27 |
| `ui-validation/2026-10-27/update-selector.json` | 2026-10-27 |
| `verification-debug-2026-10-22.json` | 2026-10-22 |
| `verification-explanations-2026-10-16.json` | 2026-10-16 |
| `verification-integrity-2026-10-25.json` | 2026-10-25 |
| `verification-notebook-2026-10-28.json` | 2026-10-28 |
| `verification-performance-2026-10-24.json` | 2026-10-24 |
| `verification-repertoires-2026-10-16.json` | 2026-10-15 |
| `verification-tactics-2026-10-19.json` | 2026-10-19 |
| `verification-ui-2026-10-28.json` | 2026-10-28 |

## Résultats sur corpus réel — 25 octobre 2026

Les deux jobs de [qualification 40179661406](https://github.com/SimzyGit/Trait64/actions/runs/40179661406)
ont réussi. Code produit dev.55 (`550f28f`), sonde `67f49ee`. Méthode complète :
[REPRODUCE.md](#reproduire-les-mesures-protocole-du-25-octobre). Les mesures locales du 24 octobre restent distinctes.

| Scénario | Résultat |
|---|---:|
| Qt : parties réellement importées | 100 000 |
| Qt : import et rafraîchissement final | 1 842,604 s (30 min 42 s) |
| Qt : démarrage sur bibliothèque vide | 729,219 ms |
| Qt : intervalle du timer, médiane / p95 / maximum | 10,106 / 10,786 / 32,742 ms |
| Qt : changements Accueil/Analyse | 1 842 ; réussis |
| Qt : avertissements QML / erreurs contrôleur | 0 / 0 |
| Qt : pic RSS du processus entier | 125 400 Kio |
| CLI : import 100k avec pause/reprise à 50k | 2 224,097 s (37 min 4 s) |
| CLI : lectures concurrentes, médiane / p95 / maximum | 1,068 / 1,487 / 7,506 ms ; 21 961 lectures |
| CLI : contrôle d'intégrité de l'original | 37,774 s ; OK |
| CLI : sauvegarde / restauration avec contrôle | 98,014 / 92,731 s ; OK |
| CLI : pic RSS du processus entier | 22 736 Kio |
| Réindexation complète de 10k | 49,642 s ; 10 000 parties, intégrité OK |

Le timer est demandé à **10 ms** : le tableau décrit les intervalles observés
entre ses événements, pas un temps de réponse à un clic ni une cadence d'images.
Qt emploie un rendu logiciel sans écran sur EPYC 7763, tandis que le stockage
emploie un autre runner EPYC 9V74 ; deux CPU logiques chacun. Les durées ne
permettent pas de calculer un surcoût de l'interface. Une seule observation,
caches non purgés, aucun résultat matériel Voyager.

L'essai Qt précédent s'arrêtait à sa garde de 30 minutes sans rapport exploitable.
La garde a été portée à 90 minutes dans la sonde, le rapport étant désormais
conservé avant les assertions. Cet essai a atteint sa cible sans expiration.

### Restauration et stockage

Les **100 000 PGN** de chacune des trois bases (original, sauvegarde, restauration)
ont la même empreinte canonique :
`eb0826cb0ba0f70ee446c1bf706224031a500c6ec5bd7e6028845bc1bf97ca1f`.
Chacune possède **6 817 385 lignes** dans chacune des tables `positions` et
`all_positions`. Ce contrôle complète l'intégrité SQLite ; il ne compare pas
chaque octet de tous les index.

Original fermé : **3 836 047 360 octets**. Sauvegarde et restauration :
**3 530 797 056 octets** chacune ; la sauvegarde utilise `VACUUM INTO`, d'où
une disposition physique plus compacte malgré le même contenu logique.
Les objets de positions et d'ouvertures occupent 93,82 %
des pages comptées par `dbstat`. C'est une priorité pour les prochains travaux
sur le format et les index, avec migration sauvegardée si le schéma change.

Le pic SQL local de **4,031 s** n'a pas été reproduit sur cet autre hôte.
Sa cause reste inconnue ; le maximum GitHub de 7,506 ms ne l'invalide pas.
500k et plusieurs millions de parties réelles restent non qualifiés.

Rapports bruts : [Qt](VALIDATION.json), [CLI](VALIDATION.json),
[réindexation](VALIDATION.json),
[contenu restauré](VALIDATION.json),
[profil SQLite](VALIDATION.json).
Aucune archive Actions ; corpus et bases non ajoutés à Git.

## Reproduire les mesures (protocole du 25 octobre)

### Qualification complémentaire — 25 octobre 2026

Code produit publié : `550f28f51f174b8d3b1d5f896928c2cfa09e04d4` (dev.55).
Code de mesure : `67f49eefbfabb0f635752ca1cdea7a1cb6056e45`, branche
`perf/qt-corpus-qualification`. Les 138 fichiers produit comparés sont identiques
par objet Git et permissions ; voir `source-provenance.json`. Seuls la sonde
optionnelle Qt, le workflow et son script changent côté exécution.

Les sondes sont compilées depuis ces sources en dehors de la publication. Le
champ `version: 0.3.0-dev.1` du rapport CLI est l'étiquette CMake par défaut ;
l'identité du code mesuré est donnée par les commits et empreintes ci-dessus.

#### Reproduction

Utiliser un worktree au commit de mesure, avec C++23, Qt6/QML, SQLite3,
libarchive, Ninja et les modules QML employés par l'application. Le workflow
archivé énumère les dépendances. Construire en Release :

```sh
cmake -S . -B build-perf -G Ninja -DCMAKE_BUILD_TYPE=Release -DT64_BENCHMARKS=ON
cmake --build build-perf --target pgn_benchmark desktop_tests --parallel 2
python3 scripts/qualify-real-corpus.py --input /corpus/source.pgn --output /mesures/qt-neuves --scenario gui
python3 scripts/qualify-real-corpus.py --input /corpus/source.pgn --output /mesures/stockage-neuves --scenario storage
```

Ces commandes sont successives sur une même machine. La qualification GitHub
emploie deux runners distincts, sans compilation concurrente à la mesure sur
un même runner. Ne pas comparer leurs temps comme si le matériel ou la charge
étaient identiques. Les caches système ne sont pas purgés.

Corpus réel : Lichess standard classé, janvier 2013 (CC0),
https://database.lichess.org/standard/lichess_db_standard_rated_2013-01.pgn.zst.
PGN : 92 811 021 octets, 121 332 parties, SHA-256
`8963b6a1620a0e9c77e5515a0744ec133e86869487188af047bb0a74400dee37`.
La taille et l'empreinte sont vérifiées avant de lancer les scénarios.
Les dossiers de sortie doivent être neufs et disposer de 14 Gio libres.

#### Ce qui est mesuré

Qt : véritable contrôleur et fenêtre QML, rendu logiciel sans écran, bibliothèque
vide au démarrage, quatre lecteurs PGN, minuterie demandée à 10 ms et navigation
programmée Accueil/Analyse chaque seconde. Demande de pause à 100 000 parties
validées ; le nombre final peut légèrement dépasser la cible. La durée inclut
la vérification du fichier et le rafraîchissement final. Le rapport est écrit
avant les assertions et contient la progression approximativement chaque minute.
La garde de 90 minutes arrête un essai incomplet ; elle n'est ni un seuil de
fluidité ni un résultat de performance. La version publiée de la sonde conserve
sa garde de 30 minutes. Aucun test de GPU, clic humain, cadence d'images ou
réouverture de toute l'application avec une grosse bibliothèque n'est revendiqué.

Stockage : import et reconstruction des index sur 10 000 parties, puis nouvelle
base de 100 000 parties avec pause/fermeture/reprise à 50 000. Un lecteur séparé
interroge la première page et attend 100 ms après chaque lecture : ce n'est pas
une cadence fixe. Cinq lectures finales par opération, intégrité SQLite,
sauvegarde et restauration dans deux nouveaux fichiers. Toutes les parties
canoniques des trois bases sont hachées dans l'ordre des identifiants ; leurs
comptes de positions sont vérifiés. Le contrôle ne compare pas chaque octet de
chaque index. Le profil `dbstat` est lu ensuite, hors périodes chronométrées.

Les rapports `*-process` proviennent de `os.wait4` : processus entier,
initialisation et nettoyage compris. RSS hors cache de fichiers du noyau.
Les JSON sont extraits des marqueurs `TRAIT64_REPORT_BEGIN/END` des journaux
GitHub. Pas d'archives Actions, de cache Actions ni de gros corpus dans Git.
Les copies du workflow, du script et de la sonde conservées ici documentent
l'exécution ; elles ne remplacent pas le checkout exact de mesure.

#### Essais précédents et limites

Le premier essai s'est arrêté avant mesure faute d'espace. L'essai
`40097867485` du 24 octobre a atteint la garde Qt de 30 minutes et n'a pas émis
son nombre de parties ni ses délais d'événements. Les seules données disponibles
sont conservées dans `../2026-10-24/github/40097867485/` ; aucune qualification
Qt 100k ne peut en être déduite. Les scénarios de stockage n'avaient pas commencé.

Le scénario actuel utilise des jobs indépendants avec `fail-fast: false`.
Retirer les SDK préinstallés .NET et Android ne concerne que les runners
GitHub jetables ; aucun fichier utilisateur, release ou archive du dépôt n'est
supprimé. Les mesures locales précédentes restent distinctes et ne sont pas
relancées pour reconstituer des fichiers perdus.

Une seule observation par scénario. Aucun résultat sur Voyager ni sur
500 000/1/2/5/10 millions de parties complètes. Les filtres fréquents, tris
alternatifs, pages éloignées, export et migrations restent à qualifier.
