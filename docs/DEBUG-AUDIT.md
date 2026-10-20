# Audit de débogage global — 20 octobre 2026

Demande utilisateur : suspendre les ajouts et déboguer tout le logiciel.
Base exacte : `4e4cd3d36711c98a5952d26dca4b2531bc4c6264` (code de dev.46).

## Corrections et preuves de régression

| Défaut | Correction et scénario vérifié |
|---|---|
| Session JSON valide mais inexploitable remplacée sans copie de secours | Validation des onglets et chemins ; seuls les coups présents dans l’étude peuvent être restaurés. Tests `{}`, tableau vide, mauvais type, chemin absent ou illégal ; original conservé octet pour octet. Un JSON malformé ne produit qu’une copie de secours. |
| Annulation à la dernière notification de réindexation ignorée | Le refus de la notification finale déclenche aussi le rollback SQLite. Export et intégrité sont contrôlés après annulation. |
| Compteur sans prise/pion remis à zéro après 255 ; continuation au-delà du coup 32000 créant un nœud illisible | Compteurs internes élargis dans la dépendance, avec patch documenté. Les limites FEN publiques restent identiques ; rejet avant mutation de l’étude ou production d’une explication. Tests du dépassement, de l’annulation du coup et de l’étude inchangée. |
| Recherche UCI relancée depuis un signal de fin : ancienne réponse associée à la nouvelle position | Copie de la FEN et génération de recherche ; un arrêt ou remplacement réentrant invalide la livraison de l’ancien coup. Deux tests avec Stockfish réel. |
| Fermeture pendant une authentification GitHub : accès à des membres déjà détruits | Déconnexion des signaux des processus et de la réponse réseau avant destruction ; arrêt du processus d’authentification. Un faux `gh` isolé reste actif pendant la destruction. AddressSanitizer avait signalé un `heap-use-after-free` dans `Updater::fail`, appelé depuis la destruction de `QProcess`. |
| Nulle réclamable traitée comme fin forcée dans les analyses ; pat masqué par les 50 coups | Résolution des mats/pats/matériel insuffisant avant les motifs réclamables. Répétition et 50 coups continuent à recevoir une évaluation moteur réelle ; tests des nœuds analysés et de la priorité du pat. |
| Cadence PGN calculée avec les valeurs non bornées | Minutes et incrément bornés avant le calcul de l’horloge et de `TimeControl`. Test avec `INT_MAX`, attendu `10800+120`, sans dépassement signé. |
| Ancien cache réintroduisant une fausse fin de partie | Les entrées marquées terminales sont ignorées si la position actuelle ne l’est pas, puis remplacées par une vraie évaluation. Test d’un cache hérité injecté après une recherche réelle : recalcul nécessaire, puis réutilisation du cache réparé. Les analyses déjà terminées restent historiques ; en lancer une nouvelle applique la correction. |
| Options automatiques UCI ignorant les bornes déclarées par un moteur | Threads/Hash/MultiPV/Skill Level adaptés aux bornes annoncées ; identité du cache fondée sur les valeurs réellement envoyées. Les options personnalisées restent validées strictement. Un moteur de test refuse explicitement les valeurs hors limites. |

## Vérification

- Référence avant modification : **11/11 suites, 55,76 s**, GCC 13.3 / Qt 6.4.2, Stockfish 16 réel.
- Avertissements `-Wall -Wextra -Wpedantic -Wformat=2` étendus à tous les modules ; trois expressions logiques parenthésées sans changement de comportement.
- Nouvelle suite déterministe `robustness` : 80 études légales classiques/Chess960 avec variantes, notes, suppressions et fusions ; ordre et offsets du parseur en flux et parallèle ; 3 000 mutations FEN/PGN, avec round trip des entrées acceptées.
- Tests de publication (17) et de nettoyage (9), plus syntaxe des scripts de distribution.
- Test d’annulation d’export rendu déterministe : annulation avant dispatch. Un export minuscule peut légitimement être terminé avant l’instruction UI suivante ; le test ne suppose plus que l’annulation gagne cette course.
- CI étendue aux douze suites avec ASan/UBSan, Qt et Stockfish avant les paquets Debian 12/13. Le contrôle mémoire du cœur sans Qt est conservé.
- Résultats complets après correction et liens CI : en cours de validation. Ce fichier n’annonce pas une publication avant réussite des contrôles.

## Périmètre et limites

Revue du cœur et des formats, SQLite/migrations/imports/répertoires, analyses/UCI, contrôleur/session/historique/entraînement/pendule, interface QML, téléchargements/mises à jour, scripts et distribution. Pas de modification du schéma 7 ni d’ajout de fonctionnalité.

Les mutations sont un stress déterministe, pas une preuve exhaustive de robustesse. Les tests graphiques Qt sont hors écran avec rendu logiciel : aucun essai sur le PC Voyager ou son GPU n’est revendiqué. LeakSanitizer est désactivé pour la validation locale et la tâche Qt ; cela ne prouve pas l’absence de fuites. La tâche CI du cœur garde le contrôle par défaut. Les règles automatiques des 75 coups/cinq répétitions et tous les cas complexes de position morte ne sont pas ajoutés par ce correctif. Les bibliothèques tierces ne font pas l’objet d’un audit exhaustif ligne par ligne.
