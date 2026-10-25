# Qualification complémentaire — 25 octobre 2026

Code produit publié : `550f28f51f174b8d3b1d5f896928c2cfa09e04d4` (dev.55).
Code de mesure : `67f49eefbfabb0f635752ca1cdea7a1cb6056e45`, branche
`perf/qt-corpus-qualification`. Les 138 fichiers produit comparés sont identiques
par objet Git et permissions ; voir `source-provenance.json`. Seuls la sonde
optionnelle Qt, le workflow et son script changent côté exécution.

Les sondes sont compilées depuis ces sources en dehors de la publication. Le
champ `version: 0.3.0-dev.1` du rapport CLI est l'étiquette CMake par défaut ;
l'identité du code mesuré est donnée par les commits et empreintes ci-dessus.

## Reproduction

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

## Ce qui est mesuré

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

## Essais précédents et limites

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
