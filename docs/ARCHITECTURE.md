# Architecture de l’alpha

Monolithe modulaire de bureau, sans serveur ni appels réseau intégrés.

| Module | Rôle |
|---|---|
| `libs/core` | C++ standard, sans Qt : validation FEN, règles via chess-library, arbre de jeu, parseur/exporteur PGN, perft |
| `libs/database` | Qt SQL : SQLite version 1, transactions, index de positions, entraînements, sauvegarde ; conteneur d’échange expérimental |
| `libs/engine` | QProcess : protocole UCI asynchrone, négociation, annulation sérialisée, parsing MultiPV |
| `app` | Contrôleur QObject, session, actions, orchestration des tâches et états |
| `ui` | Qt Quick/QML : six espaces, échiquier, panneaux, dialogues |
| `tests` | Tests de règles, PGN, persistance/récupération, Stockfish et interface |

Chaque coup stocke son parent, ses enfants ordonnés, UCI, SAN dérivée, FEN dérivée, commentaire et NAG. L’enfant 0 est la continuation principale. Le chemin complet depuis la racine est rejoué pour les répétitions et envoyé au moteur ; une FEN seule n’est pas suffisante pour cet historique.

Le PGN est l’autorité persistée d’une étude. L’index de positions est reconstruit dans la même transaction que la sauvegarde, avec position canonique (placement, trait, roques, prise en passant légale). L’explorateur compte une seule fois chaque partie/coup. Les répétitions ne gonflent pas les résultats.

L’import parse et valide le PGN dans un worker QtConcurrent, puis utilise sa propre connexion SQLite dans une transaction. Aucune connexion Qt SQL n’est partagée entre threads. Écriture WAL + synchronous FULL, contraintes et requêtes préparées. Sauvegarde `VACUUM INTO`, puis copie atomique vers la destination.

Le moteur reçoit une demande à la fois. Un changement de position envoie `stop`, attend le `bestmove` de l’ancienne recherche puis lance la demande la plus récente. Ce `bestmove` périmé n’est jamais appliqué à l’échiquier. La sortie est bornée et les coups de PV sont revérifiés.

Les modèles QML sont encore des QVariantList reconstruites : simple à vérifier pour l’alpha, inadapté à des millions de lignes. Ils devront devenir des QAbstractItemModel incrémentaux avant de lever les limites documentées. L’import PGN garde un lot en mémoire ; le streaming est un chantier séparé.

La session est écrite atomiquement après 600 ms et à la fermeture. Elle contient le PGN et un nœud remappé après sérialisation, car l’ordre des identifiants peut changer au réimport. Les tests de bureau doivent couvrir cette récupération et les commentaires sur des branches. Les modifications non enregistrées sont conservées dans la bibliothèque avant de changer d’étude. L’application principale utilise un verrou de processus pour éviter deux sessions simultanées dans le même dossier.
