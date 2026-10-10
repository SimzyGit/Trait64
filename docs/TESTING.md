# Vérification 0.3

`ctest --test-dir build --output-on-failure` exécute six suites :

| Suite | Scénarios vérifiés |
|---|---|
| `core` | Perft initiale profondeur 4 = 197281, Kiwipete profondeur 3 = 97862, finale profondeur 4 = 43238 ; coups spéciaux, répétition et mat ; 960 départs distincts, roque Chess960 avec roi immobile, PGN variante ; arbre PGN et lecture en flux, commentaires/RAV imbriqués, round trip, fusion idempotente, EPD et rejets d’entrées invalides |
| `updates` | SemVer, canaux, préversions/drafts, ignorance, absence de digest, hôtes trompeurs, absence de fuite du jeton sur redirection, erreur réseau/JSON, annulation, empreinte incorrecte, installation atomique, ancien binaire conservé et sauvegarde échouée bloquante |
| `storage` | Import/dédoublonnage, mise à jour stable, filtres SQL, positions de variantes/matériel/Elo, joueurs, corbeille, révisions quotidiennes, sauvegarde ; migration version 1 → 3, recherche FTS5/exacte, agrégats cohérents après édition/répétition/corbeille/restauration/réindexation et suppression en cascade ; import en flux annulé sans résultat partiel ; export en flux et protection des chemins ; récupération T64 après troncature/écriture déchirée |
| `engine` | Véritable Stockfish : handshake, MultiPV, annulation/remplacement, exclusion des anciens bestmove, conservation du dernier score exact, résultats de recherche terminée, coup Chess960 légal, comparaison de deux vrais processus avec changements rapides de position, fermeture |
| `desktop_smoke` | Chargement et rendu des 7 espaces, enregistrement/réouverture, aucune alerte QML ; dossier temporaire isolé |
| `desktop_interactions` | Événements QtTest clic/glisser, notes et navigation, sauvegarde et entraînement, 48 SVG, styles/couleurs, annuler/rétablir, onglets et session, flèches PGN, profils moteur, bilan et annulation, ECO, exports PDF/PNG, roque 960, pendule/incrément, dialogues et géométrie à 960 × 720, comparaison UCI avec lignes réelles dans QML, sauvegarde pré-mise à jour et automatique, préservation des octets d’une session illisible |

Le test moteur est **skipped**, pas réussi, si Stockfish est absent. Les tests GUI utilisent `QT_QPA_PLATFORM=offscreen` et `QT_QUICK_BACKEND=software`. Ils ne remplacent pas un essai sur le GPU et l’écran du PC cible. Le cœur compile sans Qt avec `-DT64_BUILD_GUI=OFF`. `-DT64_SANITIZERS=ON` active AddressSanitizer et UndefinedBehaviorSanitizer.

La CI `.github/workflows/linux.yml` construit en Release dans Debian 12 et Debian 13, exécute les six suites, crée un DEB, l’installe et lance le test de démarrage sur le paquet installé. Elle construit aussi une AppImage dans Debian 12 et exécute son test de démarrage, rassemble les sources des bibliothèques distribuées puis publie une Release complète. Une tâche séparée vérifie le cœur avec les sanitizers. Les résultats effectifs sont visibles dans GitHub Actions ; un fichier de pipeline seul n’est pas une preuve de réussite.

Vérification locale de cette version : **6 suites sur 6 réussies**. Le redimensionnement après sauvegarde, les dialogues et les captures 960 × 720 sont vérifiés. La chaîne GitHub complète a effectivement publié `v0.3.0-dev.6` ; consulter les exécutions suivantes pour la version finale enrichie.

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
