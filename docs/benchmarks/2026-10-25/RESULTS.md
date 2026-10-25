# Résultats sur corpus réel — 25 octobre 2026

Les deux jobs de [qualification 40179661406](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/40179661406)
ont réussi. Code produit dev.55 (`550f28f`), sonde `67f49ee`. Méthode complète :
[REPRODUCE.md](REPRODUCE.md). Les mesures locales du 24 octobre restent distinctes.

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

## Restauration et stockage

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

Rapports bruts : [Qt](gui/gui-100k.json), [CLI](storage/material-100k.json),
[réindexation](storage/reindex-10k.json),
[contenu restauré](storage/restored-content-100k.json),
[profil SQLite](storage/storage-profile-100k.json).
Aucune archive Actions ; corpus et bases non ajoutés à Git.
