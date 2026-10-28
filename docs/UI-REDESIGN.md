# Refonte native de Trait64 — intégrée le 28 octobre 2026

La [PR #13](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/13) est fusionnée dans main (`15708fb857c709af61bbfc56f9f5efa1392ccb87`). Le code est validé ; la [publication automatisée](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/40644804206) est en cours. La dernière release confirmée reste dev.57 tant que les paquets ne sont pas vérifiés.

## Interface réalisée

Navigation latérale compacte ou développée, grand échiquier à l’accueil, panneaux adaptables et palette de commandes au clavier. Police, texte de 90 à 150 %, interface de 90 à 125 %, contraste renforcé, animations réduites et libellés français/anglais sont persistés.

Corrections : commandes coupées à petite taille ou en grandes polices, blocage du profil Joueurs vide, titre d’aide tronqué, choix de promotion et couleurs des dialogues. Les valeurs des listes restent stables après changement de langue. Board.qml, Piece.qml et le schéma SQLite 7 sont conservés.

Navigation : dix entrées donnant accès aux sept pages et aux dialogues existants du contrôleur Qt/C++. Pièces Lichess, annotations, noms et contenus utilisateur conservés.

## Validation acquise

Tête `dd0e53c674a4043f697577187d190db0fe2c65ef`, arbre identique au commit fusionné. La [CI de PR](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/40516973650) est entièrement réussie : cœur ASan/UBSan et 13 suites Qt ASan/UBSan, Debian 12 et 13 ; deux paquets installés et AppImage démarrée. Aucune archive Actions.

Sonde locale du 27 octobre : 1 180 contrôles (captures comprises), 9 configurations, 63 vues de pages, 100 dialogues et une vue de navigation développée ; 164 captures, zéro avertissement QML. Géométrie, navigation clavier, commande exécutée, préférences et étude non enregistrée contrôlées. [Rapport](ui-validation/2026-10-27/local.json) · [Matrice](ui-validation/2026-10-27/matrix.json).

Deux captures documentaires remplacées depuis le code fusionné, compilé localement comme 0.3.0-dev.64. Dimensions vérifiées dans la matrice : 960 × 640 à 2560 × 1600, français/anglais, clair/sombre, texte jusqu’à 150 % et interface jusqu’à 125 %.

## Limites

Essais en rendu logiciel hors écran ; aucun essai matériel Voyager/Wayland/120 Hz. Certains diagnostics C++ et descriptifs de catalogue restent français. Un échec local isolé de préparation est documenté dans [local.json](ui-validation/2026-10-27/local.json), non reproduit lors des relances ni dans les CI finales ; diagnostic ajouté, assertion inchangée. Aucun nouveau benchmark de grandes bases.
