# Architecture de la version 0.2

Monolithe modulaire de bureau, sans serveur ni appels réseau intégrés.

| Module | Rôle |
|---|---|
| `libs/core` | C++ standard sans Qt : FEN, règles classiques/Chess960, arbre, PGN et lecture en flux, EPD, fusion, perft |
| `libs/database` | Qt SQL : SQLite schéma 2, transactions, index, statistiques joueurs/entraînements, sauvegarde ; archive expérimentale |
| `libs/engine` | QProcess : UCI asynchrone, options, annulation sérialisée, MultiPV et résultats terminés |
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
