# Architecture de la version 0.3

Monolithe modulaire de bureau, sans serveur. Les mises à jour et le catalogue de téléchargements utilisent le réseau à la demande ; les fonctions d’échecs restent locales.

| Module | Rôle |
|---|---|
| `libs/core` | C++ standard sans Qt : FEN, règles classiques/Chess960, arbre, PGN et lecture en flux, EPD, fusion, perft |
| `libs/database` | Qt SQL : SQLite schéma 7, transactions, index, statistiques joueurs/entraînements, sauvegarde ; archive expérimentale |
| `libs/engine` | QProcess : UCI asynchrone, options, annulation sérialisée, MultiPV et résultats terminés |
| `libs/content` | Qt Network/Concurrent et libarchive : catalogue embarqué, téléchargement, vérification et extraction protégée |
| `libs/updates` | Qt Network : Releases GitHub, canaux, SHA-256, téléchargement borné et installation |
| `app` | Contrôleur QObject, onglets/session/historique, apparence, bilan, pendules, orchestration |
| `ui` | Qt Quick/QML : 7 espaces, échiquier et pièces SVG, panneaux, dialogues |
| `assets` | Pièces sous licences libres, dictionnaire ECO CC0, exemples et notices |
| `tests` | Règles et formats, persistance/récupération/migration, Stockfish et interactions UI |

Chaque coup stocke parent, enfants ordonnés, UCI, SAN et FEN dérivées, commentaire et NAG. L’enfant 0 est la continuation principale. Le chemin depuis la racine est rejoué pour les répétitions et envoyé au moteur. Chess960 utilise le drapeau de variante, le roque UCI roi-vers-tour et les droits Shredder-FEN. Le PGN reste l’autorité persistée d’une étude ; flèches et cercles utilisent les commentaires `[%cal …]` et `[%csl …]`.

SQLite utilise WAL, synchronous FULL, clés étrangères et requêtes préparées. Le schéma 2 ajoute un index de toutes les positions accessibles, le matériel, les Elo, une empreinte de ligne principale et un journal des révisions. La migration sauvegarde d’abord la base version 1, puis applique DDL et réindexation dans une transaction. L’explorateur reste basé sur les lignes principales et compte une seule fois chaque partie/coup.

L’import de fichiers PGN lit une partie à la fois dans un worker QtConcurrent, avec sa propre connexion SQLite ; aucune connexion n’est partagée entre threads. L’import classique et la réindexation sont atomiques et annulables. Les écritures utilisateur vers la bibliothèque sont suspendues pendant ces opérations. L’export PGN écrit directement vers QSaveFile. Le collage, l’EPD et l’archive T64 conservent une représentation bornée en mémoire. Les sauvegardes passent par `VACUUM INTO`, puis une copie atomique. Les chemins d’export résolus ne peuvent viser la base ouverte ni ses journaux.

Un moteur reçoit une recherche à la fois. Le changement de position envoie `stop`, attend l’ancien `bestmove`, puis lance la dernière demande. Cet ancien coup n’est pas appliqué. Les PV sont revérifiées. Les résultats exacts sont conservés séparément des bornes temporaires du moteur. Le bilan utilise un second processus UCI, une position après l’autre ; un identifiant de génération neutralise les réponses différées après annulation/changement d’étude.

Les onglets conservent arbre, nœud, état enregistré et historique borné. La session est écrite atomiquement après 600 ms et à la fermeture, avec chemins UCI pour remapper les nœuds après sérialisation. L’historique annuler/rétablir ne persiste pas au redémarrage. Les onglets d’une autre base perdent leurs identifiants avant sauvegarde dans la base active. Un verrou de processus protège le dossier utilisateur.

Les modèles QML sont des QVariantList reconstruites. La pagination borne l’affichage, mais les corpus de plusieurs millions de parties nécessitent encore des modèles incrémentaux et une qualification de performances. La pendule utilise QElapsedTimer monotone ; navigation ou arrêt de partie la suspend. Le format T64 reste le profil expérimental 0.1, distinct de la version du logiciel.


Le schéma 3 garde une table de compteurs, des comptes par joueur, une arête unique par partie/position/coup et les statistiques d’ouvertures agrégées. Des déclencheurs SQL les maintiennent dans la même transaction que les écritures. La corbeille et la restauration retirent/rétablissent les contributions ; les répétitions ne gonflent pas les comptes. FTS5 avec tokenizer trigramme indexe les métadonnées, avec repli LIKE si le SQLite de la distribution ne le fournit pas. Les recherches par position/matériel partent de leurs index plutôt que de parcourir les parties.

La comparaison UCI possède 2 à 4 processus indépendants. Les sorties sont bornées par le parseur UCI, validées comme coups légaux et associées à la FEN. Navigation : effacement immédiat des anciennes lignes, stop, temporisation de 180 ms puis dernière position demandée. Notifications QML au maximum toutes les 50 ms. Les profils sont plafonnés à 1 Gio de hash total, les threads répartis avec au moins un thread par moteur.

La vérification de mises à jour ne démarre pas en mode test. Le téléchargement est asynchrone et les redirections contrôlées. La sauvegarde précédant l’installation et le remplacement final restent synchrones ; sur une très grosse base, cette dernière phase peut immobiliser temporairement l’interface. Les sauvegardes périodiques utilisent, elles, un worker avec une connexion SQLite distincte.


Le schéma 4 relie les parties à leurs sources par `source_games` et garde un historique d’import. Les compteurs de sources suivent la corbeille et les suppressions via des déclencheurs transactionnels. Deux sources peuvent partager le même PGN exact sans dupliquer sa partie. La migration crée une sauvegarde avant modification.

`app/search.cpp` gère un seul worker SQL à la fois, avec une connexion propre au thread. La dernière demande remplace celles en attente ; génération et drapeau atomique empêchent une réponse périmée d’alimenter la liste. `sqlite3_progress_handler` interrompt la requête en cours, puis est retiré par RAII. La fermeture annule et rejoint le worker. Les pages contiennent 200 lignes plus une sentinelle pour déterminer l’existence de la suite ; les curseurs précédents sont conservés, sans OFFSET dans l’interface. Le tri par modification utilise deux plages disjointes bornées afin de chercher aussi sur l’identifiant quand les dates sont égales.

Le catalogue embarqué épingle tailles et SHA-256 des moteurs. Les PGN d’un fournisseur sans empreinte publiée sont identifiés à réception sans prétendre à une authentification indépendante. L’extraction borne taille et nombre d’entrées et rejette liens, chemins sortants et fichiers spéciaux. Un moteur installé ajoute un profil, sans l’exécuter automatiquement. Les options sont validées contre le handshake UCI avant envoi et sauvegardées par profil.

## Schémas 5 et 6

Le schéma 5 ajoute `import_jobs`. Le mode PGN reprenable écrit les lots, les liens de source et l’offset dans une transaction IMMEDIATE ; une interruption conserve uniquement les transactions terminées. La reprise vérifie le SHA-256 complet avant de rejoindre le curseur. Un QLockFile par base empêche deux workers reprenables simultanés et permet la récupération après arrêt du processus. Le worker possède sa connexion SQL et communique par progression protégée par mutex.

Le schéma 6 ajoute répertoires, positions, arêtes et journal de rappels. La clé de position conserve les quatre premiers champs FEN ; le camp et la variante appartiennent au répertoire. Chaque position partage son calendrier entre les transpositions. L’export développe chaque position une fois, ce qui borne les cycles et l’explosion de variantes. Les suppressions élaguent seulement les positions devenues inaccessibles depuis la racine.

Les exercices de répertoire et les anciennes cartes partagent `trainingActive()` pour les gardes d’édition, le gel de session et la restauration de l’étude. Les réponses multiples viennent du répertoire ; aucune équivalence moteur n’est inventée. Le calendrier simple 1/3/double (maximum 365 jours, retour à 1 en cas d’échec) reste séparé des anciennes cartes.

La préparation par position interroge l’index `positions_fen`, déduplique les couples partie/coup avant les agrégats, et applique les filtres de source/métadonnées. Un worker Qt sérialisé, une connexion SQL indépendante, une génération et un drapeau d’annulation SQLite empêchent les résultats périmés. Le CSV conserve les filtres et la FEN. Ces statistiques dépendent exclusivement du corpus local.

## File d’analyses

Le schéma 7 conserve les copies PGN, les paramètres et identités UCI, les résultats ordonnés et un cache limité à 10 000 positions. Un résultat et l’incrément du point de reprise sont validés dans la même transaction. Le worker `libs/analysis` crée sa connexion SQLite et son processus UCI dans un QThread séparé, sous verrou par bibliothèque. Les signaux portent une génération pour ignorer les retours d’une bibliothèque fermée. Les notifications de progression sont bornées à 10 Hz ; les résultats sont paginés par 200. Voir ANALYSIS-QUEUE pour la politique de cache, les limites et les exports.
