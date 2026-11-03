# Refonte native de Trait64 — 28 octobre 2026

La [PR #13](https://github.com/SimzyGit/Trait64/pull/13), demandée le 26 octobre, est intégrée et publiée dans **[v0.3.0-dev.64](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.64)** le 28 octobre 2026. Base : dev.57 ; tête testée `dd0e53c674a4043f697577187d190db0fe2c65ef`, fusion `15708fb857c709af61bbfc56f9f5efa1392ccb87`, arbres identiques.

## Interface réalisée

Navigation latérale compacte ou développée, grand échiquier à l’accueil, panneaux adaptables et palette de commandes au clavier. Police, texte de 90 à 150 %, interface de 90 à 125 %, contraste renforcé, animations réduites et libellés français/anglais sont persistés.

Corrections : commandes coupées à petite taille ou en grandes polices, blocage du profil Joueurs vide, titre d’aide tronqué, choix de promotion et couleurs des dialogues. Les valeurs des listes restent stables après changement de langue. Board.qml, Piece.qml et le schéma SQLite 7 sont conservés.

Les dix entrées de navigation donnent accès aux sept pages et aux dialogues existants, reliés au contrôleur Qt/C++. Les pièces Lichess, annotations et index de pages 0–6 sont conservés. Les noms et contenus utilisateur restent inchangés.

## Validation

La sonde locale finale a réalisé 1 180 contrôles (captures comprises), 9 configurations, 63 vues de pages, 100 ouvertures de dialogues et une vue de navigation développée : 164 captures, zéro avertissement QML. Navigation clavier, commande exécutée, préférences et étude non enregistrée contrôlées. Démarrage et interactions locaux passent ; les 13 suites locales complètes avaient passé sur e0e13d9 en 84,52 s, sans suite ignorée. [Résultats locaux](VALIDATION.md) · [Matrice](VALIDATION.md).

[CI finale de PR](https://github.com/SimzyGit/Trait64/actions/runs/40516973650) et [CI de publication](https://github.com/SimzyGit/Trait64/actions/runs/40644804206) réussies. Les 13 suites passent sous Debian 12/13 et Qt ASan/UBSan ; le cœur passe également sous sanitizers. Paquets Debian installés et AppImage démarrés. Tag, cinq fichiers publiés, tailles, SHA-256 et sélection de la mise à jour depuis dev.57 vérifiés ; aucune archive Actions. [Rapport de livraison](VALIDATION.md).

Dimensions : 960 × 640, 1366 × 768, 1440 × 940, 1920 × 1080 et 2560 × 1600 ; combinaisons français/anglais, clair/sombre et tailles jusqu’à 150 % / 125 %. Reproduction dans [TESTING](TESTING.md).

## Limites

Essais en rendu logiciel hors écran ; aucun essai matériel Voyager/Wayland/120 Hz. Certains diagnostics C++ et descriptifs de catalogue restent français. Un échec local isolé de préparation est documenté dans [local.json](VALIDATION.md), non reproduit lors des relances ni dans les CI finales ; diagnostic ajouté, assertion inchangée. Aucun nouveau benchmark de grandes bases.
