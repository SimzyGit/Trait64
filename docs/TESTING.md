# Vérification 0.3

## Qualification des imports — 23 octobre 2026

Le test de stockage compte les instructions SQLite de la vraie recherche de
doublons dans une base sans statistiques ANALYZE ; il vérifie les doublons actifs
et la corbeille. Les clés matérielles réellement enregistrées sont comparées à
l'ancienne représentation, puis recherchées et réindexées, y compris après
rollback. Fixtures : captures/variantes, sous-promotions, prise en passant et
roque Chess960. Quatre suites ciblées passent après restauration du commit
`e21e8d3` : core, storage, resumable_import et pgn_benchmark_safety (1,55 s).

Le mode `desktop_tests --benchmark-import /chemin/corpus.pgn --benchmark-games 100000`
utilise une bibliothèque temporaire, quatre lecteurs, un timer de 10 ms et des
changements de page toutes les secondes. Le JSON donne le nombre réellement
traité, les latences des événements et les avertissements QML. Le seuil déclenche
une demande de pause ; le lot déjà validé peut dépasser légèrement ce seuil.
Ce mode est opt-in et ne remplace pas les tests habituels. Exécution sans écran :
`QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`. Cela ne mesure pas le GPU,
Wayland ni la cadence d'affichage du PC Voyager.

Mesures et protocole : [benchmarks/2026-10-23](benchmarks/2026-10-23/).
La compilation complète, ce mode Qt et le palier 100 000 restent à valider sur le
dernier commit. Ne pas confondre la CI réussie de `69250f0` avec cette validation.

## Audit global — validation finale du 22 octobre 2026

La [PR #9](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/9) ajoute des régressions sur les sessions et chemins restaurés, l’annulation finale de réindexation, les compteurs FEN, les réponses moteur réentrantes, la destruction pendant l’authentification, les nulles réclamables, le cache hérité et les cadences extrêmes.

Les options UCI sont vérifiées avec un moteur déclarant des bornes strictes et Stockfish réel. Un dossier nommé `<empty>` empêche la création du fichier de journal erroné, y compris en exécution privilégiée ; les chaînes vides et les anciens marqueurs doivent permettre le démarrage et l’analyse.

Le stress conserve 80 études et 3 000 mutations pour tous les builds. Son délai est de 300 s sous sanitizers et 120 s sinon : le passage CI non optimisé a pris 125,86 s dans la PR. Douze suites passent localement, sous ASan/UBSan avec Qt, puis sur Debian 12 et 13. Les DEB installés et l’AppImage démarrent. La publication dev.52, le tag, les cinq fichiers et la sélection des mises à jour ont été vérifiés.

Résultats, échecs initiaux et limites : [DEBUG-AUDIT.md](DEBUG-AUDIT.md) ; mesures exactes : [verification-debug-2026-10-22.json](verification-debug-2026-10-22.json).

## Suites actuelles

`ctest --test-dir build --output-on-failure` exécute onze suites par défaut. Avec `-DT64_BENCHMARKS=ON`, une douzième suite `pgn_benchmark_safety` est ajoutée (Python 3 requis). La CI Debian active cette option :

| Suite | Scénarios vérifiés |
|---|---|
| `robustness` | Graine `0x64302026`, 80 études classiques/Chess960, variantes/notes/suppressions, round trip et fusion idempotente ; parseurs en flux et parallèle (1/2/8 workers), offsets croissants ; 3 000 mutations FEN/PGN |
| `core` | Perft initiale profondeur 4 = 197281, Kiwipete profondeur 3 = 97862, finale profondeur 4 = 43238 ; coups spéciaux, répétition et mat ; 960 départs distincts, roque Chess960 avec roi immobile, PGN variante ; arbre PGN et lecture en flux, commentaires/RAV imbriqués, round trip, fusion idempotente, EPD et rejets d’entrées invalides |
| `content` | Catalogue hors ligne, HTTPS/hôtes, SHA-256, annulation, extraction bornée, chemins et liens dangereux, permissions et reçus |
| `updates` | SemVer, canaux, préversions/drafts, ignorance, absence de digest, hôtes trompeurs, absence de fuite du jeton sur redirection, erreur réseau/JSON, annulation, empreinte incorrecte, installation atomique, ancien binaire conservé et sauvegarde échouée bloquante |
| `storage` | Import/dédoublonnage, mise à jour stable, filtres SQL, positions de variantes/matériel/Elo, joueurs, corbeille, révisions quotidiennes, sauvegarde ; migration version 1 → 7, recherche FTS5/exacte, agrégats cohérents après édition/répétition/corbeille/restauration/réindexation et suppression en cascade ; import en flux annulé sans résultat partiel ; export en flux et protection des chemins ; récupération T64 après troncature/écriture déchirée |
| `repertoires` | Transpositions et réponses multiples, priorités, commentaires/NAG, PGN réimportable, cycles, Chess960, branches orphelines, fusion atomique, calendrier partagé, lacunes de couverture et suppressions sans perte des parties modèles |
| `resumable_import` | Pause avec rollback du lot incomplet, reprise à l’octet après réouverture, empreinte et fichier déplacé, doublons, historique unique, verrou concurrent, PGN invalide/vide, arrêt réel du processus pendant une transaction et récupération du verrou et des index |
| `persistent_analysis` | Stockfish réel : copie PGN/variantes, MultiPV et WDL, export conservant la ligne jouée, cache et historique, budgets distincts, pause/reprise, binaire modifié, verrou concurrent, Chess960, pages 200/9, transaction idempotente, processus réellement tué puis repris, migration sauvegardée 6 → 7 |
| `engine` | Véritable Stockfish : handshake, MultiPV, annulation/remplacement, exclusion des anciens bestmove, conservation du dernier score exact, résultats de recherche terminée, coup Chess960 légal, comparaison de deux vrais processus avec changements rapides de position, fermeture |
| `desktop_smoke` | Chargement et rendu des 7 espaces, enregistrement/réouverture, aucune alerte QML ; dossier temporaire isolé |
| `desktop_interactions` | Événements QtTest clic/glisser, notes et navigation, sauvegarde et entraînement, 48 SVG, styles/couleurs, annuler/rétablir, onglets et session, flèches PGN, profils moteur, bilan et annulation, ECO, exports PDF/PNG, roque 960, pendule/incrément, dialogues et géométrie à 960 × 720, comparaison UCI avec lignes réelles dans QML, sauvegarde pré-mise à jour et automatique, préservation des octets d’une session illisible |

Le test moteur est **skipped**, pas réussi, si Stockfish est absent. Les tests GUI utilisent `QT_QPA_PLATFORM=offscreen` et `QT_QUICK_BACKEND=software`. Ils ne remplacent pas un essai sur le GPU et l’écran du PC cible. Le cœur compile sans Qt avec `-DT64_BUILD_GUI=OFF`. `-DT64_SANITIZERS=ON` active AddressSanitizer et UndefinedBehaviorSanitizer.

La CI `.github/workflows/linux.yml` construit en Release dans Debian 12 et Debian 13, exécute les douze suites, crée un DEB, l’installe et lance le test de démarrage sur le paquet installé. Elle construit aussi une AppImage dans Debian 12 et exécute son test de démarrage, rassemble les sources des bibliothèques distribuées puis publie une Release complète. Deux tâches préalables vérifient le cœur seul et tous les modules Qt avec ASan/UBSan. Les résultats effectifs sont visibles dans GitHub Actions ; un fichier de pipeline seul n’est pas une preuve de réussite.

Vérification historique avant catalogue : **6 suites sur 6 réussies** ; les vérifications du lot actuel sont détaillées plus bas. Le redimensionnement après sauvegarde, les dialogues et les captures 960 × 720 sont vérifiés. La chaîne GitHub complète a effectivement publié `v0.3.0-dev.6` ; consulter les exécutions suivantes pour la version finale enrichie.

Le développement utilise Ubuntu 24.04 x86_64, GCC 13.3, Qt 6.4.2 et Stockfish 16. Localement LeakSanitizer doit être désactivé (`ASAN_OPTIONS=detect_leaks=0`) car l’environnement bloque son inspection de `/proc` ; cela ne valide pas l’absence de fuites. La tâche CI du cœur conserve sa configuration par défaut ; celle de tous les modules Qt désactive LeakSanitizer mais arrête les tests sur toute erreur ASan/UBSan. Voir [DEBUG-AUDIT.md](DEBUG-AUDIT.md) pour les régressions de session, UCI, mémoire, annulation, cadence et fin de partie.

Le bilan est une heuristique à seuils fixes, pas un estimateur Elo. Syzygy est une option transmise au moteur : aucun jeu complet de fichiers de tablebases n’est embarqué ou qualifié. Pas de certification pour des millions de parties, tous les moteurs UCI, chaque GPU/Wayland/X11, les PGN non UTF-8, Windows/macOS ou l’arbitrage FIDE complet. Les tests de récupération ne remplacent pas des coupures électriques matérielles.

Les captures documentaires se reproduisent avec `T64_SCREENSHOT=/chemin/image.png`, éventuellement `T64_PREVIEW=appearance` et `T64_SCREENSHOT_WIDTH=1440 T64_SCREENSHOT_HEIGHT=940`, sur `trait64 --smoke-test`. Ces variables ne s’appliquent qu’au mode de test.


Les tests réseau utilisent des réponses déterministes dans une fausse couche HTTP, uniquement dans les tests. Le logiciel utilise réellement GitHub et de vrais moteurs. Les tests n’effectuent pas d’installation privilégiée sur le PC utilisateur : l’installation .deb est exercée dans les conteneurs CI, le contrôle SHA-256/remplacement AppImage dans des dossiers temporaires. Un essai final sur Voyager avec authentification GitHub et autorisation Polkit reste nécessaire.

Le benchmark optionnel `database_benchmark` crée des métadonnées synthétiques et une arête d’ouverture par partie, pas un corpus complet de positions. Il est exclu des tests courants :

```bash
cmake -S . -B build -DT64_BENCHMARKS=ON
cmake --build build --target database_benchmark --parallel 2
./build/database_benchmark 1000000
```

Les mesures reproductibles et leurs limites sont dans [PERFORMANCE.md](PERFORMANCE.md). Les fichiers temporaires sont supprimés à la sortie du benchmark.


### Refonte de l’interface — octobre 2026

Les interactions de bureau cliquent sur les sept onglets à 960 × 640, vérifient l’échiquier et les outils de position au-dessus du pied de page, ainsi que la conservation de l’accent personnel lors d’un changement de thème. Les scénarios existants de clic/glisser, variantes, notes, sauvegarde, onglets, entraînement et dialogues sont conservés.

Captures reproductibles sans toucher aux données utilisateur :

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
T64_SCREENSHOT=/tmp/trait64.png T64_SCREENSHOT_WIDTH=1440 T64_SCREENSHOT_HEIGHT=940 \
T64_SCREENSHOT_PAGE=0 ./build/trait64 --smoke-test
```

`T64_SCREENSHOT_PAGE=2` montre l’analyse ; `T64_SCREENSHOT_THEME=light` sélectionne le thème clair pour cette session temporaire. Les tailles contrôlées visuellement sont 1440 × 940, 1366 × 768 et 960 × 640.

## Catalogue et sources — 12 octobre 2026

Sept suites : core, content, updates, storage, engine, desktop_smoke, desktop_interactions. Local après recherche asynchrone : **7/7, 12,87 s** (12 octobre 2026). Le test content couvre hôtes/HTTPS, empreinte, annulation, archive ZIP, traversée de chemins, liens, réponses HTML, permissions et catalogue hors ligne. Storage couvre migration ancienne, dédoublonnage entre sources, appartenance multiple, annulation transactionnelle, export et compteurs après corbeille/restauration. Les dialogues catalogue et recherche sont ouverts dans le test Qt, qui échoue sur les avertissements QML.

Tests supplémentaires effectués avec les archives officielles réellement téléchargées (empreintes du catalogue) : Stockfish 19, Arasan 26, PlentyChess 8 ; chaque moteur démarre via UCI et retourne un coup légal. Steinitz.zip et WorldChamp2024.pgn sont extraits/parsés. `T64_CONTENT_FIXTURES=/dossier ./build/content_tests` répète ces tests si les fichiers y sont déjà présents. `T64_CONTENT_NETWORK=1 ./build/content_tests` teste volontairement un vrai téléchargement HTTPS PGN Mentor et la suppression du fichier extrait ; ce test réseau est exclu des tests hors ligne par défaut.

La compilation graphique nécessite `libarchive-dev` et `libsqlite3-dev`. Aucun test local ne constitue une installation sur le PC Voyager de l’utilisateur.

## Recherche asynchrone et curseurs

Nouveaux scénarios : toutes les pages comparées à une requête complète pour les cinq tris, égalités et valeurs SQL NULL comprises ; curseur invalide ; annulation avant requête et pendant une requête SQLite coûteuse ; connexion réutilisable après interruption. Le test coûteux utilise une vue de répétition virtuelle, pas un corpus mesuré de cent millions de parties.

Le scénario bureau importe 405 PGN légaux dans une base nommée, parcourt 200/200/5 résultats, revient en arrière, remplace 25 demandes successives, combine base/joueur/couleur/ouverture française, annule et relance, puis récupère après filtre invalide. Le catalogue doit afficher les 22 champions (classiques et titres FIDE séparés).

Le 13 octobre : benchmark du schéma 4 sur un million de lignes synthétiques, avec comparaison du contenu et du temps des pages par curseur/OFFSET après 900 000 résultats. Protocole, pic RSS du processus et données brutes dans PERFORMANCE.

Ajouts du 13 octobre : points de reprise PGN vérifiés avec CRLF, UTF-8, variantes et résultats absents ; interactions du gestionnaire d’imports, pause et reprise après destruction du contrôleur, actualisation des bases et absence d’erreurs QML. Validation locale du lot : 8/8 suites réussies, 13,63 s. La CI Debian 12/13 de cette étape a réussi sur le commit 4ed808f (exécutions 38403000333 et 38403050848).

Lot répertoires : 9/9 suites locales en 15,44 s ; éditeur QML à 960 × 640, réponse alternative acceptée, rappel manqué/révélé/successif, absence de double notation et restauration de l’étude.

## Préparation par position — 14 octobre 2026

Validation finale locale du lot : **9/9 suites réussies en 15,53 s**, avec Stockfish réel. La préparation couvre les filtres combinés source/joueur/couleur/année/Elo, les métadonnées manquantes, le dédoublonnage des répétitions, les bornes invalides et la réutilisation après annulation. Le scénario bureau ouvre le dialogue à 960 × 640, remplace rapidement des demandes, exporte le contexte en CSV, refuse un coup provenant d’une autre position et vérifie les erreurs QML.

Les résultats CI et la publication du lot sont suivis dans la [PR #3](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/3). Le protocole ne qualifie pas un corpus réel de plusieurs millions de parties ni une installation sur Voyager.

Le contrôle de taille du parcours de répertoire est effectué avant l’ajout des enfants (100 000 positions maximum), avec insertion élément par élément pour éviter un avertissement de GCC 14 sur la copie de plage. Les neuf suites locales ont été relancées après cette correction.

## Import parallèle et protection du benchmark — 15 octobre 2026

**10/10 suites locales réussies en 17,23 s**, avec Stockfish réel. Les scénarios
du cœur comparent 1, 2, 4 et 8 lecteurs : ordre, PGN canonique, offsets de reprise,
thread du consommateur, arrêt et propagation des erreurs. Une partie réelle
CC0 de Lichess vérifie la notation historique `Rd1d2`, avec rejets d'ambiguïtés,
d'origines incorrectes et de fausses captures, sans mutation de l'étude rejetée.

La suite du benchmark lance son vrai exécutable dans des dossiers temporaires :
collisions rapport/base/journaux/sauvegarde, fichiers préexistants, alias de dossier,
lien symbolique sans cible, équivalence du parsing à 1/4 lecteurs, import SQLite
et sauvegarde vérifiée. Ce sont de petits PGN de test ; aucune mesure synthétique
n'est présentée comme celle d'un corpus réel.

Le dialogue d'import a été inspecté à 960 × 720 : sélection de 1 à 8 lecteurs,
contrôles et texte accessibles. Les mesures sur corpus et leurs limites figurent
dans [PERFORMANCE.md](PERFORMANCE.md).

Lot analyses du 15 octobre : première passe complète **11/11 en 49,46 s** ; vérifications complémentaires de pagination et Chess960, 95 contrôles avec Stockfish réel. La suite desktop analyse une copie comprenant deux branches pendant que l’étude est modifiée et vérifie que cette dernière reste intacte. `T64_ANALYSIS_SCREENSHOT` permet une capture du dialogue rempli pendant ce scénario. Les scores non numériques, les types inconnus et les WDL invalides sont rejetés ; les mats sont distincts des centipions.

Régression Qt 6.4 : redimensionner à 960 × 720 après l’arrêt d’une comparaison de moteurs, puis ouvrir le dialogue peuplé. Les Repeater de résultats de comparaison ne résident plus directement dans un layout retenant des éléments supprimés. 404 vérifications desktop passent, capture incluse. Les sorties visant un verrou ou un journal de la base sont refusées même lorsque celle-ci est ouverte via un lien symbolique.

## Répertoires et publication sans archives — lot du 15 octobre

`repertoire_tests` vérifie l’annulation des créations/fusions/exports, l’atomicité d’un fichier multi-parties interrompu ou invalide, les limites 16 Mio/200 parties, la progression monotone, la conservation des notes/révisions et la libération du handler SQLite après interruption.

`desktop_tests` attend les résultats asynchrones, vérifie la réactivité pendant un véritable import avant annulation, la conservation du fichier exporté, le refus d’écrire sur la base ouverte et les résultats périmés après changement de bibliothèque. Avec `T64_STOCKFISH`, il analyse réellement une copie de répertoire et vérifie l’étude/révisions conservées. `T64_REPERTOIRE_SCREENSHOT` permet de capturer le dialogue à 960 × 640.

```sh
python3 -m unittest discover -s tests -p 'test_publish_release.py' -v
python3 -m unittest discover -s tests -p 'test_cleanup_actions.py' -v
bash -n packaging/ci-linux.sh
```

17 tests de publication couvrent fichiers manquants/inattendus/vides/liens, hash distant incorrect, reprise du brouillon, refus d’écraser une release ou un autre brouillon, tag discordant et canaux. Ils couvrent aussi le retard de visibilité d’un nouveau brouillon, le délai dépassé, un identifiant changé et l’utilisation des réponses REST de création/publication. Les échanges GitHub y utilisent un double de test. La CI 38797743659 a validé les 11 suites et paquets sur Debian 12/13, ainsi que l’AppImage. Voir WORK-IN-PROGRESS pour la publication réelle et `verification-repertoires-2026-10-16.json` pour les mesures datées.


## Publication effectivement vérifiée — 16 octobre 2026

La CI [38861352628](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/38861352628) a publié **v0.3.0-dev.42** au commit `fc941c17c53d6c5aa639433c6563f73469971ae9` : 11/11 suites Debian 12 (47,44 s) et Debian 13 (58,41 s), DEB installés, AppImage démarrée, 17 tests du publieur, 9 du nettoyage et cœur ASan/UBSan. Les cinq noms/tailles/SHA-256 distants correspondent au manifeste de compilation ; le tag résout le commit testé. Zéro archive Actions.

Vérification complémentaire avec les métadonnées réellement publiées et `libs/updates/releases.cpp` : les trois paquets sont sélectionnés pour une mise à jour dev.33 → dev.42 sur Développement, exclus de Stable/Bêta ; ignorance, version identique et rétrogradation restent bloquées. Cette lecture n’effectue aucune installation sur le PC utilisateur. Détails dans `verification-repertoires-2026-10-16.json`.


## Explications des coups — 16 octobre 2026

`core_tests` ajoute 15 vérifications : échange égal, orientation du matériel pour
les deux camps, prise en passant et case capturée, capture avec promotion,
sous-promotion, roque Chess960 avec roi immobile, mat à matériel égal, pat,
suites illégales/nulles/après mat, limite de 128 demi-coups et ligne vide. Le cœur
atteint 2 553 contrôles réussis.

Les interactions du bureau utilisent Stockfish réel, vérifient les deux suites,
le changement de sélection dans le dialogue, le clic sur « Ouvrir la suite »,
la position finale de la copie et le retour à l’étude avec son bilan inchangé.
`T64_EXPLANATION_SCREENSHOT=/chemin/image.png` capture le dialogue à 960 × 640.
La capture locale a été inspectée ; aucun avertissement QML.

Les 11 suites locales sont couvertes : 9 passent dans la passe complète ; deux
exécutables générés restaurés sans permission d’exécution sont remis exécutables,
puis `content` et `pgn_benchmark_safety` passent séparément. Le test du clic réel
est ensuite reconstruit et repasse en 17,31 s. Il ne s’agit pas d’une installation
sur le PC Voyager.

La [CI 38940592180](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/38940592180)
a ensuite validé le commit `3a6f5d4` : **11/11 Debian 12 en 49,00 s**, **11/11
Debian 13 en 60,91 s**, deux DEB installés/démarrés, AppImage démarrée, cœur
ASan/UBSan (14,23 s), 17 tests de publication et 9 de nettoyage. **dev.43 est
publiée**, ses cinq fichiers correspondent au manifeste en noms/tailles/SHA-256
et le tag pointe sur le commit testé. Zéro archive Actions. Le sélecteur réel de
mises à jour a relu ses métadonnées pour les trois paquets et les exclusions de
canal/version. Détails : `verification-explanations-2026-10-16.json`.

## Motifs tactiques — validation du 19 octobre 2026

Commit `ced2428`, fusion `1c86d53` (arbre identique). Local : 3 258 contrôles du
cœur, 11 suites avec Stockfish réel, 666 contrôles de l’interface avec deux
captures optionnelles. Fourchette et clouage testés par clic, repères exacts,
export/relecture PGN et étude d’origine intacte. Dialogue et échiquier inspectés
à 960 × 640. Compilation sans avertissement relevé.

CI [39127471728](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/39127471728) :
11/11 Debian 12 en 49,37 s, 11/11 Debian 13 en 60,14 s ; paquets installés,
AppImage, cœur ASan/UBSan, 17 tests de publication et 9 de nettoyage réussis.
Publication dev.46 confirmée par la CI 39206008438 : 11/11 Debian 12 en 50,19 s,
11/11 Debian 13 en 60,78 s, paquets installés et AppImage. Tag, noms/tailles/SHA-256
des cinq fichiers et sélection des trois paquets depuis les métadonnées réelles
vérifiés ; aucune archive Actions. Les canaux Stable/Bêta sont exclus, ainsi que
les versions ignorées, identiques ou plus anciennes. Rapport :
`verification-tactics-2026-10-19.json`. Aucun essai sur le PC Voyager.
