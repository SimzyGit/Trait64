# Vérification

`ctest --test-dir build --output-on-failure` exécute :

| Suite | Ce qu’elle vérifie |
|---|---|
| `core` | Perft initiale profondeur 4 = 197281 ; Kiwipete profondeur 3 = 97862 ; finale profondeur 4 = 43238 ; roques, sous-promotion, prise en passant, mat et répétition ; PGN multijeux, commentaires, NAG, variantes imbriquées, round trip ; entrées invalides |
| `storage` | Import et dédoublonnage, mise à jour stable, recherche, paramètres SQL, index de positions, corbeille/restauration, progression d’entraînement, sauvegarde SQLite ; archive T64, troncature, écriture déchirée, récupération, profil inconnu |
| `engine` | Véritable Stockfish : handshake, MultiPV, interruption et remplacement de recherche, exclusion d’un ancien bestmove, coup légal, fermeture. Si Stockfish est absent, le test est marqué **skipped**, pas réussi |
| `desktop_smoke` | Chargement QML et rendu de tous les espaces, enregistrement/réouverture d’une partie, absence d’avertissements QML ; dossier de données temporaire |
| `desktop_interactions` | Événements souris réels QtTest sur clic et glisser-déposer, édition de notes pendant la navigation, bouton Enregistrer, variantes, entraînement et restauration de session |

Le cœur ne dépend pas de Qt : `-DT64_BUILD_GUI=OFF`. Les tests GUI utilisent le backend logiciel Qt et la plateforme `offscreen`, sans serveur graphique. `-DT64_SANITIZERS=ON` active AddressSanitizer et UndefinedBehaviorSanitizer.

Le pipeline CI est défini pour Debian 12 et 13. La validation du pipeline distant doit être constatée dans GitHub Actions ; la présence du fichier de configuration ne constitue pas un résultat de test. Le script utilisateur compile et teste sur la machine cible avant installation.

Pas encore certifié : gros corpus de plusieurs millions de parties, tous les moteurs UCI, autres variantes d’échecs, fichiers PGN non UTF-8, chaque GPU/serveur Wayland/X11, Windows/macOS. Les tests de récupération simulent des écritures interrompues ; ils ne remplacent pas une campagne de coupures électriques matérielles.

Validation initiale dans l’environnement de développement : Linux x86_64 Ubuntu 24.04, GCC 13.3, Qt 6.4.2 et Stockfish 16 ; cinq suites passent. Le cœur passe également avec ASan/UBSan ; LeakSanitizer a dû être désactivé localement (`ASAN_OPTIONS=detect_leaks=0`) car l’environnement empêche son inspection de `/proc`. Cela ne valide donc pas l’absence de fuite mémoire. La CI standard garde sa configuration par défaut.
