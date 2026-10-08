# Vérification 0.2

`ctest --test-dir build --output-on-failure` exécute cinq suites :

| Suite | Scénarios vérifiés |
|---|---|
| `core` | Perft initiale profondeur 4 = 197281, Kiwipete profondeur 3 = 97862, finale profondeur 4 = 43238 ; coups spéciaux, répétition et mat ; 960 départs distincts, roque Chess960 avec roi immobile, PGN variante ; arbre PGN et lecture en flux, commentaires/RAV imbriqués, round trip, fusion idempotente, EPD et rejets d’entrées invalides |
| `storage` | Import/dédoublonnage, mise à jour stable, filtres SQL, positions de variantes/matériel/Elo, joueurs, corbeille, révisions quotidiennes, sauvegarde ; migration version 1 → 2 ; import en flux annulé sans résultat partiel ; export en flux et protection des chemins ; récupération T64 après troncature/écriture déchirée |
| `engine` | Véritable Stockfish : handshake, MultiPV, annulation/remplacement, exclusion des anciens bestmove, conservation du dernier score exact, résultats de recherche terminée, coup Chess960 légal, fermeture |
| `desktop_smoke` | Chargement et rendu des 7 espaces, enregistrement/réouverture, aucune alerte QML ; dossier temporaire isolé |
| `desktop_interactions` | Événements QtTest clic/glisser, notes et navigation, sauvegarde et entraînement, 48 SVG, styles/couleurs, annuler/rétablir, onglets et session, flèches PGN, profils moteur, bilan et annulation, ECO, exports PDF/PNG, roque 960, pendule/incrément, dialogues et géométrie à 960 × 720 |

Le test moteur est **skipped**, pas réussi, si Stockfish est absent. Les tests GUI utilisent `QT_QPA_PLATFORM=offscreen` et `QT_QUICK_BACKEND=software`. Ils ne remplacent pas un essai sur le GPU et l’écran du PC cible. Le cœur compile sans Qt avec `-DT64_BUILD_GUI=OFF`. `-DT64_SANITIZERS=ON` active AddressSanitizer et UndefinedBehaviorSanitizer.

La CI `.github/workflows/linux.yml` construit en Release dans Debian 12 et Debian 13, exécute les cinq suites, crée un DEB, l’installe et lance le test de démarrage sur le paquet installé. Une tâche séparée vérifie le cœur avec les sanitizers. Les résultats effectifs sont visibles dans GitHub Actions ; un fichier de pipeline seul n’est pas une preuve de réussite.

Vérification locale de cette version : **5 suites sur 5 réussies**, dont 1 967 contrôles du cœur, 55 du stockage et 104 des interactions de bureau ; le cœur passe aussi sous ASan/UBSan. Le redimensionnement immédiatement après sauvegarde et les captures 1440 × 940 sont vérifiés.

Le développement utilise Ubuntu 24.04 x86_64, GCC 13.3, Qt 6.4.2 et Stockfish 16. Localement LeakSanitizer doit être désactivé (`ASAN_OPTIONS=detect_leaks=0`) car l’environnement bloque son inspection de `/proc` ; cela ne valide pas l’absence de fuites. La tâche CI conserve sa configuration par défaut.

Le bilan est une heuristique à seuils fixes, pas un estimateur Elo. Syzygy est une option transmise au moteur : aucun jeu complet de fichiers de tablebases n’est embarqué ou qualifié. Pas de certification pour des millions de parties, tous les moteurs UCI, chaque GPU/Wayland/X11, les PGN non UTF-8, Windows/macOS ou l’arbitrage FIDE complet. Les tests de récupération ne remplacent pas des coupures électriques matérielles.

Les captures documentaires se reproduisent avec `T64_SCREENSHOT=/chemin/image.png`, éventuellement `T64_PREVIEW=appearance` et `T64_SCREENSHOT_WIDTH=1440 T64_SCREENSHOT_HEIGHT=940`, sur `trait64 --smoke-test`. Ces variables ne s’appliquent qu’au mode de test.
