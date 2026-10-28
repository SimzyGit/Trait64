# Refonte native de Trait64 — 27 octobre 2026

L’utilisateur a demandé l’intégration de la refonte le 26 octobre, avec une attention particulière aux bugs visuels. Base : main `f123ec77f60d75d36bf06d4c7e045bdc096d6b3f` (dev.57). Branche `ui/desktop-redesign`, [PR #13](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/13).

## Interface réalisée

Navigation latérale de dix entrées, compacte ou développée ; grand échiquier à l’accueil ; espace d’analyse à panneaux redimensionnables ; palette de commandes au clavier. Les actions restent reliées au contrôleur Qt/C++. Board.qml, Piece.qml, pièces Lichess, annotations, index de pages 0–6 et schéma SQLite 7 sont conservés. Aucun HTML de la maquette n’est embarqué, aucune illustration décorative n’est ajoutée.

Police système au choix, texte de 90 à 150 %, interface de 90 à 125 %, thèmes clair/sombre, contraste renforcé, animations réduites et largeur des panneaux sont persistés. Les libellés de l’interface passent du français à l’anglais sans réinitialiser les valeurs des listes. Les noms et contenus utilisateur restent inchangés. Certains diagnostics C++ et descriptifs du catalogue restent français.

## Corrections vérifiées

- Cadre de l’échiquier recalculé après redimensionnement.
- Bouton moteur et commandes d’étude accessibles avec les tailles maximales, même avec la navigation développée à 960×640.
- Profil Joueurs vide : suppression d’une boucle de mise en page Qt qui bloquait le démarrage.
- Pages et dialogues défilants ; cartes de statistiques adaptables ; sélection du catalogue conservée après traduction.
- Dernière correction issue des captures : retour à la ligne du titre de l’aide ; traduction des quatre choix de promotion.

## Validation et limites

La tête `e0e13d97f5bbc1d80655113fc1d7ddb16bebb34f` a passé les 13 suites locales avec Stockfish 16 en 84,52 s, sans suite ignorée. La sonde a réalisé 1 172 contrôles (dont les contrôles de capture), 9 configurations, 63 vues de pages, 100 ouvertures de dialogues et une vue avec la barre latérale développée ; 164 captures, zéro avertissement QML. Navigation clavier, exécution d’une commande, préférences persistées et conservation de l’étude non enregistrée sont contrôlées. Le test de démarrage couvre aussi le profil Joueurs vide.

[CI de cette tête](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/40471131364) réussie : cœur et Qt ASan/UBSan, Debian 12/13, paquets installés, AppImage et manifeste. Les derniers ajustements de l’aide et de la promotion sont soumis à une nouvelle CI avant fusion.

Dimensions : 960×640, 1366×768, 1440×940, 1920×1080, 2560×1600 ; combinaisons français/anglais, clair/sombre et tailles jusqu’à 150 % / 125 %. Tests avec rendu logiciel hors écran. Aucun essai sur le matériel Voyager/Wayland 2560×1600 à 120 Hz n’est revendiqué. Les anciens résultats 100k restent inchangés ; aucun nouveau benchmark ni qualification de plusieurs millions de parties.

La PR n’est pas encore publiée. La documentation de version sera mise à jour uniquement après vérification de la release.
