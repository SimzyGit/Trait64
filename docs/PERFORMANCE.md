# Mesures de lecture et limites de qualification

## Index allégé (schéma 12) — 12 novembre 2026

Corpus : parties de maîtres réelles (`mega2400` et `mega2600`, 79 demi-coups en moyenne,
1 217 564 parties en tout), Release GCC 13.3 / Qt 6.4.2, machine à **2 cœurs et 7 Go**,
deux lecteurs. `pgn_benchmark` : import par lots avec pause et reprise, puis empreinte
du fichier, contrôle d'intégrité et mesures de lecture. Les débits incluent tout cela.

| Parties | Index | Durée totale | Parties/s | Base | Par partie | Pic mémoire |
|---:|---|---:|---:|---:|---:|---:|
| 10 000 | ancien (schéma 11) | 76 s | 131 | 217 Mo | 21,7 Ko | 18 Mo |
| 10 000 | allégé | 24 s | 409 | 73 Mo | 7,3 Ko | 18 Mo |
| 100 000 | allégé | 330 s | 303 | 697 Mo | 7,0 Ko | 803 Mo |
| 500 000 | allégé | 2 309 s | 217 | 3 604 Mo | 7,2 Ko | 1 170 Mo |

Sur les 10 000 parties, les PGN normalisés, les empreintes de doublons et les positions
des 40 premiers demi-coups sont **identiques** à ceux de l'ancien index, ligne à ligne. Le pic mémoire vient du cache de pages prêté à l'import
(un huitième de la mémoire, 64 Mo à 1 Go) ; il est rendu à la fin.

Lectures sur la base de 500 000 parties, première exécution, cache de fichiers chaud (`search_probe`) :

| Recherche | Temps |
|---|---:|
| Page des 200 parties récentes | 2 ms |
| Position exacte au 15ᵉ coup (indexée) | 1 ms |
| Position exacte au 30ᵉ coup, puis la dernière de la partie (rejouées) | 1 ms |
| Arbre d'ouvertures, position de départ ou 4ᵉ coup | < 1 ms |
| Même structure de pions qu'au 30ᵉ coup | 66–80 ms |
| Matériel d'une finale de tours | 17 ms |
| Matériel de la position finale d'une partie | 100 ms |
| Fragment de nom de joueur | 15–19 ms |
| Liste des joueurs | 9–11 ms |
| Dossier d'une position du 4ᵉ coup | 23–26 ms |
| Position exacte au 4ᵉ coup (des dizaines de milliers de parties) | 1,3–1,7 s |
| Recherche des doublons | 1,7–1,8 s |
| Dossier de la position de départ | 2,2–2,7 s |

**Limites.** Le débit baisse avec la taille (303 → 217 parties/s de 100 000 à 500 000) :
les index rangés par clé de position sont écrits partout à la fois. Les trois dernières
lignes du tableau restent au-dessus de la seconde. Un million est en cours de mesure ;
2, 5 et 10 millions ne sont pas mesurés.

## Import réel : deux optimisations ciblées — 23 octobre 2026

Trois exécutions successives sur les mêmes **10 000 parties / 661 314 positions**
Lichess de janvier 2013 (CC0), Release GCC 13.3 / Qt 6.4.2. Quatre lecteurs,
transactions de 250, pause/fermeture/reprise à 5 000. Même outil de mesure dans
les trois exécutables ; seule l'implémentation de `database.cpp` change.

| Code mesuré | Import complet | Pic RSS du processus | SQLite et journaux |
|---|---:|---:|---:|
| Avant les deux corrections (`06a4321`) | 93,114 s | 18 788 Kio | 371 650 560 octets |
| Index des empreintes (`69250f0`) | 57,649 s | 18 684 Kio | 371 662 848 octets |
| Index + matériel (`e21e8d3`) | 42,498 s | 18 304 Kio | 371 658 752 octets |

Temps réduit de **54,4 %** dans cet essai (26,3 % supplémentaires par rapport
au seul dédoublonnage). Hôte partagé, une mesure par variante, caches du système
non purgés ; pas d'intervalle de confiance ni de promesse de débit sur Voyager.
Les tailles peuvent différer légèrement : UUID et dates d'import sont propres
à chaque exécution. L'intégrité, les sauvegardes et les restaurations passent.
La sauvegarde du dernier état prend 3,090 s ; la restauration avec ouverture et
contrôle prend 3,525 s. Le parsing et SHA-256 canonique prennent 11,063 s avec
un lecteur, 3,345 s avec quatre, avec mêmes parties/positions et empreinte.

La première correction contraint la recherche de doublons à l'index `games_digest`
existant : sans statistiques ANALYZE, SQLite parcourait parfois toutes les
parties actives à chaque insertion. La seconde évite la compilation répétée
d'une expression régulière pour chaque position ; un filtrage des lettres FEN
suivi du même tri conserve exactement la clé matérielle historique. Les deux
corrections conservent le schéma 7 et les protections transactionnelles.

Les corrections sont intégrées à **dev.55**. Le palier CLI de 100 000 est mesuré
ci-dessous ; la réactivité Qt est qualifiée séparément. Aucun résultat à
500 000/1/2/5/10 millions ni sur le PC Voyager n'est revendiqué.

Rapports : [benchmarks/2026-10-23](VALIDATION.md).
Protocole : [REPRODUCE.md](VALIDATION.md).

## 100 000 parties réelles — hôte local, 24 octobre 2026

Même fichier source : 100 000 parties / 6 817 385 positions, quatre lecteurs,
pause à 50 000, réouverture/reprise et une lecture concurrente suivie d'une attente de 100 ms avant la suivante.
Ce protocole et la charge de l'hôte partagé diffèrent de la comparaison de 10k
ci-dessus : ne pas transformer leur rapport en facteur de ralentissement ou
d'accélération du code. Une observation, caches non purgés, SQLite 3.45.1,
Qt 6.4.2, Release GCC 13.3 ; aucune compilation locale simultanée.

| Opération | Résultat observé |
|---|---:|
| Import avec tous les index, vérifications et reprise | 1 250,857 s (20 min 51 s) |
| Réouverture SQLite, après import | 2,785 ms |
| Contrôle d'intégrité de l'original | 110,028 s ; OK |
| Sauvegarde complète | 36,032 s ; copie vérifiée ensuite |
| Restauration, ouverture et contrôle | 228,293 s ; OK |
| Pic RSS du processus d'import | 18 352 Kio |
| Base et journaux | 3 836 055 552 octets |

La RSS ne comprend pas le cache de fichiers du noyau. L'import s'arrête
volontairement en pause à 100 000 sur les 121 332 parties du fichier. Aucune
partie n'est dupliquée pour atteindre le volume.

Lectures finales, cinq échantillons avec cache non purgé :

| Lecture | Médiane | Maximum |
|---|---:|---:|
| Première page de 200 parties | 1,768 ms | 2,231 ms |
| Page suivante par curseur | 2,378 ms | 3,630 ms |
| Joueur exact | 1,453 ms | 4,489 ms |
| Fragment de joueur | 2,411 ms | 6,274 ms |
| Chargement d'une partie | 2,549 ms | 2,953 ms |
| Position finale d'une partie | 18,269 ms | 45,731 ms |
| Arbre depuis le départ | 0,142 ms | 0,198 ms |

Pendant l'import, **11 045 lectures** donnent une médiane de **3,014 ms**, un
p95 de **12,506 ms** et un maximum de **4 031,415 ms**. Cette longue attente
reste à caractériser avec des traces horodatées et des répétitions contrôlées ;
elle ne doit pas être masquée par la médiane. Ce n'est pas une mesure de Qt.
Les filtres peu sélectifs, matériaux fréquents, pages éloignées et combinaisons
complexes ne sont pas couverts exhaustivement par ces lectures.

Rapport complet : [material-100k.json](VALIDATION.md).
Les valeurs ont été récupérées depuis la sortie complète observée avant la
réinitialisation du dossier temporaire, puis enregistrées dans Git ; provenance
et limites : [recovery-provenance.json](VALIDATION.md).

## Qualification GitHub terminée — 25 octobre 2026

[Résultats, JSON bruts et protocole](VALIDATION.md).
Code produit dev.55 (`550f28f`), sonde instrumentée `67f49ee`.
**Deux runners différents** pour Qt et le stockage, chacun avec deux CPU logiques ;
une mesure par scénario, caches non purgés. Ne pas comparer ces temps entre eux
ou avec l'hôte local pour en déduire un gain du code.

| Mesure | Résultat |
|---|---:|
| Qt : import réel de 100 000 parties | 1 842,604 s (30 min 42 s) |
| Qt : intervalle du timer de 10 ms, médiane / p95 / maximum | 10,106 / 10,786 / 32,742 ms |
| Qt : changements Accueil/Analyse / avertissements QML | 1 842 réussis / 0 |
| Qt : pic RSS du processus entier | 125 400 Kio |
| CLI : 100k avec pause/reprise à 50k | 2 224,097 s (37 min 4 s) |
| CLI : 21 961 lectures concurrentes, médiane / p95 / maximum | 1,068 / 1,487 / 7,506 ms |
| CLI : intégrité / sauvegarde / restauration contrôlée | 37,774 / 98,014 / 92,731 s ; OK |
| CLI : pic RSS du processus entier | 22 736 Kio |
| Réindexation complète de 10 000 parties | 49,642 s ; intégrité OK |

Qt utilise un rendu logiciel sans écran. Le timer mesure des intervalles entre
événements, pas la latence d'un clic ni la cadence d'images. Le démarrage sur
bibliothèque vide prend 729,219 ms ; l'ouverture complète de l'application sur
100k n'est pas mesurée. La garde de la sonde a été portée de 30 à 90 minutes et
le rapport est émis avant les assertions ; la cible est atteinte sans expiration.

Les 100 000 PGN de l'original, de la sauvegarde et de la restauration ont tous
l'empreinte canonique attendue, avec 6 817 385 lignes dans chacune des deux tables
de positions. Il ne s'agit pas d'une comparaison de chaque octet des index.
Original fermé : 3 836 047 360 octets ; copies compactées par sauvegarde :
3 530 797 056 octets chacune.

Le profil `dbstat` attribue **93,82 % des pages** aux positions, ouvertures et
index associés. Réduire ce coût exige de préserver les recherches exactes,
variantes, agrégats et clés historiques ; un changement du schéma nécessitera
une migration 8 sauvegardée. Les paliers 500k et plusieurs millions ne sont
toujours pas qualifiés.

Le pic SQL local de 4,031 s n'a pas été reproduit sur ce runner différent.
Sa cause reste inconnue ; les nouvelles valeurs ne remplacent pas l'ancienne
observation. Aucun essai matériel Voyager n'est revendiqué.

## Vérification manuelle d'intégrité — dev.57

La durée locale de 110,028 s a conduit à retirer le contrôle manuel du fil UI.
La [PR #12](https://github.com/SimzyGit/Trait64/pull/12)
utilise un worker et une connexion dédiée, avec annulation SQLite, état visible
et fermeture attendue. Les tests d'interruption et de relance réentrante passent
localement et en CI. Ce correctif ne constitue pas une nouvelle mesure 100k.
La sauvegarde manuelle et le contrôle préalable à l'ouverture restent synchrones.
Voir [ASYNC-INTEGRITY.md](ASYNC-INTEGRITY.md).

## Mesures historiques conservées

Mesure du 10 octobre 2026 sur l’environnement de développement Linux x86_64 partagé, GCC 13.3 et Qt 6.4.2, compilation **Debug**. 11 lectures consécutives par opération, médiane et maximum observé ; la médiane représente surtout les lectures avec cache chaud.

Le générateur prépare **1 000 000 lignes de métadonnées synthétiques et 1 000 000 arêtes d’ouverture** avec les mêmes index, FTS5 et déclencheurs que l’application. Il ne charge pas toutes les positions de longues parties et ne mesure pas le parseur/importateur PGN. Ce résultat n’est donc **pas une certification de gestion de millions de parties complètes**.

| Opération via l’API Database | Médiane (ms) | Maximum (ms) |
|---|---:|---:|
| Première page, 1 000 lignes | 8.392 | 9.906 |
| Recherche de fragment, 100 résultats | 17.404 | 19.100 |
| Joueur exact, 100 résultats | 0.880 | 1.135 |
| Compteurs de bibliothèque | 0.074 | 0.275 |
| Arbre depuis la position initiale | 0.026 | 0.058 |
| Classement des 1 000 joueurs les plus présents | 4.073 | 4.733 |

Préparation du jeu synthétique : 119.8 s. Base après checkpoint : 1,020,379,136 octets. Ces nombres dépendent du matériel et ne prédisent pas la performance sur le PC utilisateur.

Le test a mis en évidence une sélection d’index défavorable aux filtres de joueur et aux recherches FTS suivies d’un tri. L’ajout d’index joueur/date de modification et le parcours guidé par FTS ont réduit, sur le même jeu, la médiane du joueur exact d’environ 283 ms à 0,88 ms, et celle du fragment de 145 ms à 17,4 ms. C’est une comparaison entre deux implémentations intermédiaires de 0.3, pas un benchmark des logiciels concurrents.

À qualifier ensuite : PGN réels, variantes, index de toutes les positions, import annulé/repris, migration et sauvegarde de grosses bases, recherche de positions communes/rares, tris globaux et pages très éloignées. Depuis le 12 octobre, la recherche s’exécute dans un worker avec annulation SQLite et pagination par curseur (200 lignes dans l’interface). La liste reste une QVariantList bornée ; les exports, certains profils statistiques et les migrations peuvent encore bloquer l’interface. Les mesures historiques ci-dessus portent sur le schéma 3, pas sur ces nouveaux scénarios.

Reproduction : voir [TESTING.md](TESTING.md). Données brutes : [performance-2026-10-10.json](VALIDATION.md).


## Pages éloignées — 13 octobre 2026, schéma 4

Même générateur, **1 000 000 métadonnées synthétiques et une arête par partie**, toutes avec la même date de modification, afin d’exercer le cas de nombreuses égalités. Lecture de 200 résultats après les 900 000 premiers ; 11 lectures par opération, compilation Debug, GCC 13.3, Qt 6.4.2, SQLite 3.45.1, environnement Linux x86_64 partagé.

| Parcours mesuré | Médiane (ms) | Maximum (ms) |
|---|---:|---:|
| Curseur corrigé, deux plages bornées | 2.475 | 2.980 |
| OFFSET 900000, mêmes 200 lignes | 52.590 | 56.298 |
| Première page historique, 1 000 lignes | 11.469 | 11.940 |
| Recherche exacte de joueur | 1.212 | 1.513 |
| Fragment FTS | 17.450 | 18.882 |

Le test compare aussi le contenu exact des deux pages. Le premier prototype de curseur par comparaison `(updated,id)` était **plus lent** : médiane 121.698 ms dans la mesure du 12 octobre, contre 53.251 ms pour OFFSET. L’examen du plan montrait une recherche d’index limitée à `updated`. La correction sépare les identifiants restants de la même date et les dates antérieures, borne chaque branche et fusionne les résultats triés. Elle utilise `updated` et `id` sans parcourir toutes les égalités précédentes. Ces résultats ne constituent pas une comparaison avec d’autres logiciels ni une garantie pour chaque tri/filtre ou corpus.

Base synthétique après checkpoint : 1 097 678 848 octets ; préparation : 128.108 s. Pic RSS du **processus de benchmark**, mesuré par `getrusage(RUSAGE_CHILDREN)` depuis Python : 14 300 Kio ; ce n’est ni la RAM de l’application graphique ni une mesure d’import PGN. Temps du processus complet : 129.227 s.

Données brutes : [avant correction](VALIDATION.md), [après correction](VALIDATION.md). La commande `./build/database_benchmark 1000000` inclut les deux parcours. Les tests de non-régression vérifient aussi les passages entre groupes de dates, les valeurs absentes et les cinq tris.

Les essais sur 1, 2, 5 et 10 millions de **parties PGN réelles avec toutes leurs positions et variantes** ne sont pas encore réalisés. Les tris globaux et filtres peu sélectifs restent à mesurer : le worker évite de bloquer l’interface, sans rendre automatiquement toute requête rapide.

## Import reprenable (13 octobre 2026)

Le nouveau mode conserve un lot de 250 parties par transaction et un curseur en octets dans la même transaction. Avec un seul lecteur, la mémoire ne contient qu’une partie parsée à la fois (limite de 64 Mio par enregistrement PGN) et un tampon SHA-256 de 1 Mio. Une reprise relit le fichier complet pour vérifier son empreinte, puis rejoint directement le point de parsing conservé. Le débit et le temps restant affichés concernent la lecture et l’indexation après cette vérification ; ils restent des estimations observées pendant la session.

Les tests fonctionnels vérifient la reprise après arrêt du processus ; ils ne constituent pas un benchmark de 1/2/5/10 millions de PGN réels. Ces mesures, la parallélisation du parsing et la réduction du coût d’indexation restent à qualifier.

Les répertoires et leurs fichiers PGN sont bornés (16 Mio/200 parties par import dans l’éditeur). Leur import, export, agrégation de rappels et détection de lacunes restent synchrones : une très grande préparation peut encore bloquer brièvement l’éditeur. Le calcul filtré des statistiques de préparation est asynchrone et annulable, avec l’index de positions existant ; les combinaisons sur des corpus réels de plusieurs millions ne sont pas encore mesurées.

## Import parallèle et protocole sur un vrai PGN — 15 octobre 2026

Le dialogue d'import permet désormais de choisir de 1 à 8 lecteurs (2 par défaut).
Le cadrage des enregistrements reste séquentiel ; seul leur parsing est parallèle.
La restitution se fait dans l'ordre du fichier sur le thread appelant. SQLite
conserve un seul écrivain et les mêmes transactions/points de reprise. La file
est bornée à deux enregistrements par lecteur et 32 Mio de texte brut en attente,
à l'exception d'un enregistrement isolé de 64 Mio maximum. La mémoire des arbres
parsés s'ajoute à ces octets : 32 Mio n'est pas une limite de RAM du processus.

Le benchmark optionnel accepte un PGN local réel et mesure parsing, import,
reprise, recherches et sauvegarde. Les sorties doivent être de nouveaux fichiers.
Il refuse les collisions entre rapport, base, journaux SQLite et sauvegarde,
y compris via un alias de dossier. Le corpus et les bases volumineuses ne sont
pas ajoutés au dépôt GitHub.

```sh
cmake -S . -B build-perf -G Ninja -DCMAKE_BUILD_TYPE=Release -DT64_BENCHMARKS=ON
cmake --build build-perf --target pgn_benchmark --parallel 1
./build-perf/pgn_benchmark --input /chemin/corpus.pgn --threads 1 --limit 10000 --report /chemin/parse-1.json
./build-perf/pgn_benchmark --input /chemin/corpus.pgn --threads 4 --limit 10000 --report /chemin/parse-4.json
./build-perf/pgn_benchmark --input /chemin/corpus.pgn --threads 4 --limit 10000 --pause-at 5000 --database /chemin/nouvelle-base.sqlite --backup --report /chemin/import.json
```

La comparaison du parsing exige un même nombre de parties/positions et la même
empreinte du PGN réexporté. Une mesure sur 10 000 parties ne qualifie pas les
objectifs de 1, 2, 5 et 10 millions de parties complètes.

Mesure réelle : premières **10 000 parties / 661 314 positions** du corpus
[Lichess janvier 2013](https://database.lichess.org/standard/lichess_db_standard_rated_2013-01.pgn.zst),
sous [CC0](https://database.lichess.org/), sur l'hôte Linux partagé, GCC 13.3,
Qt 6.4.2, compilation Release. PGN de 92 811 021 octets ; empreintes et rapports
bruts dans [performance-pgn-2026-10-15.json](VALIDATION.md).
L'identifiant embarqué `0.3.0-dev.1` est celui de la configuration locale de
mesure ; ce n'est pas une nouvelle version distribuée.

| Opération | Temps | Pic RSS du processus |
|---|---:|---:|
| Parsing, réexport et SHA-256 canonique — 1 lecteur | 10,126 s | 9 008 Kio |
| Parsing, réexport et SHA-256 canonique — 4 lecteurs | 3,637 s | 9 316 Kio |
| Import avec index, pause à 5 000, fermeture/réouverture et reprise à 10 000 | 72,462 s | 18 452 Kio |

Les deux sorties de parsing ont exactement la même empreinte canonique.
L'import s'arrête volontairement en pause à la limite de 10 000 ; le reste du
corpus n'est pas importé dans cet essai. La base avec journaux occupe
371 634 176 octets. Contrôle d'intégrité réussi ; sauvegarde effectuée en 0,809 s
puis rouverte et vérifiée. Les lectures chaudes mesurées sur cinq passages ont
des médianes de 0,700 ms (première page de 200), 0,992 ms (page suivante),
0,899 ms (joueur exact) et 0,091 ms (arbre depuis la position initiale).
Ce sont des observations ponctuelles sur cet hôte/corpus, pas une garantie de
débit sur Voyager ni une comparaison avec d'autres logiciels.
