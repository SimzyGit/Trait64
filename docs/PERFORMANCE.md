# Mesures de lecture et limites de qualification

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

Reproduction : voir [TESTING.md](TESTING.md). Données brutes : [performance-2026-10-10.json](performance-2026-10-10.json).


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

Données brutes : [avant correction](performance-cursors-before-2026-10-12.json), [après correction](performance-cursors-2026-10-13.json). La commande `./build/database_benchmark 1000000` inclut les deux parcours. Les tests de non-régression vérifient aussi les passages entre groupes de dates, les valeurs absentes et les cinq tris.

Les essais sur 1, 2, 5 et 10 millions de **parties PGN réelles avec toutes leurs positions et variantes** ne sont pas encore réalisés. Les tris globaux et filtres peu sélectifs restent à mesurer : le worker évite de bloquer l’interface, sans rendre automatiquement toute requête rapide.

## Import reprenable (13 octobre 2026)

Le nouveau mode conserve un lot de 250 parties par transaction et un curseur en octets dans la même transaction. La mémoire ne contient qu’une partie parsée à la fois (limite de 64 Mio par enregistrement PGN) et un tampon SHA-256 de 1 Mio. Une reprise relit le fichier complet pour vérifier son empreinte, puis rejoint directement le point de parsing conservé. Le débit et le temps restant affichés concernent la lecture et l’indexation après cette vérification ; ils restent des estimations observées pendant la session.

Les tests fonctionnels vérifient la reprise après arrêt du processus ; ils ne constituent pas un benchmark de 1/2/5/10 millions de PGN réels. Ces mesures, la parallélisation du parsing et la réduction du coût d’indexation restent à qualifier.
