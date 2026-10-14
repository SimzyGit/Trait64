# Vérification 0.3

`ctest --test-dir build --output-on-failure` exécute neuf suites :

| Suite | Scénarios vérifiés |
|---|---|
| `core` | Perft initiale profondeur 4 = 197281, Kiwipete profondeur 3 = 97862, finale profondeur 4 = 43238 ; coups spéciaux, répétition et mat ; 960 départs distincts, roque Chess960 avec roi immobile, PGN variante ; arbre PGN et lecture en flux, commentaires/RAV imbriqués, round trip, fusion idempotente, EPD et rejets d’entrées invalides |
| `content` | Catalogue hors ligne, HTTPS/hôtes, SHA-256, annulation, extraction bornée, chemins et liens dangereux, permissions et reçus |
| `updates` | SemVer, canaux, préversions/drafts, ignorance, absence de digest, hôtes trompeurs, absence de fuite du jeton sur redirection, erreur réseau/JSON, annulation, empreinte incorrecte, installation atomique, ancien binaire conservé et sauvegarde échouée bloquante |
| `storage` | Import/dédoublonnage, mise à jour stable, filtres SQL, positions de variantes/matériel/Elo, joueurs, corbeille, révisions quotidiennes, sauvegarde ; migration version 1 → 6, recherche FTS5/exacte, agrégats cohérents après édition/répétition/corbeille/restauration/réindexation et suppression en cascade ; import en flux annulé sans résultat partiel ; export en flux et protection des chemins ; récupération T64 après troncature/écriture déchirée |
| `repertoires` | Transpositions et réponses multiples, priorités, commentaires/NAG, PGN réimportable, cycles, Chess960, branches orphelines, fusion atomique, calendrier partagé, lacunes de couverture et suppressions sans perte des parties modèles |
| `resumable_import` | Pause avec rollback du lot incomplet, reprise à l’octet après réouverture, empreinte et fichier déplacé, doublons, historique unique, verrou concurrent, PGN invalide/vide, arrêt réel du processus pendant une transaction et récupération du verrou et des index |
| `engine` | Véritable Stockfish : handshake, MultiPV, annulation/remplacement, exclusion des anciens bestmove, conservation du dernier score exact, résultats de recherche terminée, coup Chess960 légal, comparaison de deux vrais processus avec changements rapides de position, fermeture |
| `desktop_smoke` | Chargement et rendu des 7 espaces, enregistrement/réouverture, aucune alerte QML ; dossier temporaire isolé |
| `desktop_interactions` | Événements QtTest clic/glisser, notes et navigation, sauvegarde et entraînement, 48 SVG, styles/couleurs, annuler/rétablir, onglets et session, flèches PGN, profils moteur, bilan et annulation, ECO, exports PDF/PNG, roque 960, pendule/incrément, dialogues et géométrie à 960 × 720, comparaison UCI avec lignes réelles dans QML, sauvegarde pré-mise à jour et automatique, préservation des octets d’une session illisible |

Le test moteur est **skipped**, pas réussi, si Stockfish est absent. Les tests GUI utilisent `QT_QPA_PLATFORM=offscreen` et `QT_QUICK_BACKEND=software`. Ils ne remplacent pas un essai sur le GPU et l’écran du PC cible. Le cœur compile sans Qt avec `-DT64_BUILD_GUI=OFF`. `-DT64_SANITIZERS=ON` active AddressSanitizer et UndefinedBehaviorSanitizer.

La CI `.github/workflows/linux.yml` construit en Release dans Debian 12 et Debian 13, exécute les neuf suites, crée un DEB, l’installe et lance le test de démarrage sur le paquet installé. Elle construit aussi une AppImage dans Debian 12 et exécute son test de démarrage, rassemble les sources des bibliothèques distribuées puis publie une Release complète. Une tâche séparée vérifie le cœur avec les sanitizers. Les résultats effectifs sont visibles dans GitHub Actions ; un fichier de pipeline seul n’est pas une preuve de réussite.

Vérification historique avant catalogue : **6 suites sur 6 réussies** ; les vérifications du lot actuel sont détaillées plus bas. Le redimensionnement après sauvegarde, les dialogues et les captures 960 × 720 sont vérifiés. La chaîne GitHub complète a effectivement publié `v0.3.0-dev.6` ; consulter les exécutions suivantes pour la version finale enrichie.

Le développement utilise Ubuntu 24.04 x86_64, GCC 13.3, Qt 6.4.2 et Stockfish 16. Localement LeakSanitizer doit être désactivé (`ASAN_OPTIONS=detect_leaks=0`) car l’environnement bloque son inspection de `/proc` ; cela ne valide pas l’absence de fuites. La tâche CI conserve sa configuration par défaut.

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
