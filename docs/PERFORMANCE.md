# Mesures de lecture — schéma 3

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

À qualifier ensuite : PGN réels, variantes, index de toutes les positions, import annulé/repris, migration et sauvegarde de grosses bases, recherche de positions communes/rares, tris globaux et pages très éloignées. La recherche/affichage utilisent encore des QVariantList bornées et certaines opérations restent synchrones.

Reproduction : voir [TESTING.md](TESTING.md). Données brutes : [performance-2026-10-10.json](performance-2026-10-10.json).
