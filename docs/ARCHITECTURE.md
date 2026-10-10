# Architecture de la version 0.3

Monolithe modulaire de bureau, sans serveur. Le module facultatif de mises à jour est le seul client réseau intégré ; les fonctions d’échecs restent locales.

| Module | Rôle |
|---|---|
| `libs/core` | C++ standard sans Qt : FEN, règles classiques/Chess960, arbre, PGN et lecture en flux, EPD, fusion, perft |
| `libs/database` | Qt SQL : SQLite schéma 3, transactions, index, statistiques joueurs/entraînements, sauvegarde ; archive expérimentale |
| `libs/engine` | QProcess : UCI asynchrone, options, annulation sérialisée, MultiPV et résultats terminés |
| `libs/updates` | Qt Network : Releases GitHub, canaux, SHA-256, téléchargement borné et installation |
| `app` | Contrôleur QObject, onglets/session/historique, apparence, bilan, pendules, orchestration |
| `ui` | Qt Quick/QML : 7 espaces, échiquier et pièces SVG, panneaux, dialogues |
| `assets` | Pièces sous licences libres, dictionnaire ECO CC0, exemples et notices |
| `tests` | Règles et formats, persistance/récupération/migration, Stockfish et interactions UI |

Chaque coup stocke parent, enfants ordonnés, UCI, SAN et FEN dérivées, commentaire et NAG. L’enfant 0 est la continuation principale. Le chemin depuis la racine est rejoué pour les répétitions et envoyé au moteur. Chess960 utilise le drapeau de variante, le roque UCI roi-vers-tour et les droits Shredder-FEN. Le PGN reste l’autorité persistée d’une étude ; flèches et cercles utilisent les commentaires `[%cal …]` et `[%csl …]`.

SQLite utilise WAL, synchronous FULL, clés étrangères et requêtes préparées. Le schéma 2 ajoute un index de toutes les positions accessibles, le matériel, les Elo, une empreinte de ligne principale et un journal des révisions. La migration sauvegarde d’abord la base version 1, puis applique DDL et réindexation dans une transaction. L’explorateur reste basé sur les lignes principales et compte une seule fois chaque partie/coup.

L’import de fichiers PGN lit une partie à la fois dans un worker QtConcurrent, avec sa propre connexion SQLite ; aucune connexion n’est partagée entre threads. L’import et la réindexation sont atomiques et annulables. Les écritures utilisateur vers la bibliothèque sont suspendues pendant ces opérations. L’export PGN écrit directement vers QSaveFile. Le collage, l’EPD et l’archive T64 conservent une représentation bornée en mémoire. Les sauvegardes passent par `VACUUM INTO`, puis une copie atomique. Les chemins d’export résolus ne peuvent viser la base ouverte ni ses journaux.

Un moteur reçoit une recherche à la fois. Le changement de position envoie `stop`, attend l’ancien `bestmove`, puis lance la dernière demande. Cet ancien coup n’est pas appliqué. Les PV sont revérifiées. Les résultats exacts sont conservés séparément des bornes temporaires du moteur. Le bilan utilise un second processus UCI, une position après l’autre ; un identifiant de génération neutralise les réponses différées après annulation/changement d’étude.

Les onglets conservent arbre, nœud, état enregistré et historique borné. La session est écrite atomiquement après 600 ms et à la fermeture, avec chemins UCI pour remapper les nœuds après sérialisation. L’historique annuler/rétablir ne persiste pas au redémarrage. Les onglets d’une autre base perdent leurs identifiants avant sauvegarde dans la base active. Un verrou de processus protège le dossier utilisateur.

Les modèles QML sont des QVariantList reconstruites. La pagination borne l’affichage, mais les corpus de plusieurs millions de parties nécessitent encore des modèles incrémentaux et une qualification de performances. La pendule utilise QElapsedTimer monotone ; navigation ou arrêt de partie la suspend. Le format T64 reste le profil expérimental 0.1, distinct de la version du logiciel.


Le schéma 3 garde une table de compteurs, des comptes par joueur, une arête unique par partie/position/coup et les statistiques d’ouvertures agrégées. Des déclencheurs SQL les maintiennent dans la même transaction que les écritures. La corbeille et la restauration retirent/rétablissent les contributions ; les répétitions ne gonflent pas les comptes. FTS5 avec tokenizer trigramme indexe les métadonnées, avec repli LIKE si le SQLite de la distribution ne le fournit pas. Les recherches par position/matériel partent de leurs index plutôt que de parcourir les parties.

La comparaison UCI possède 2 à 4 processus indépendants. Les sorties sont bornées par le parseur UCI, validées comme coups légaux et associées à la FEN. Navigation : effacement immédiat des anciennes lignes, stop, temporisation de 180 ms puis dernière position demandée. Notifications QML au maximum toutes les 50 ms. Les profils sont plafonnés à 1 Gio de hash total, les threads répartis avec au moins un thread par moteur.

La vérification de mises à jour ne démarre pas en mode test. Le téléchargement est asynchrone et les redirections contrôlées. La sauvegarde précédant l’installation et le remplacement final restent synchrones ; sur une très grosse base, cette dernière phase peut immobiliser temporairement l’interface. Les sauvegardes périodiques utilisent, elles, un worker avec une connexion SQLite distincte.
