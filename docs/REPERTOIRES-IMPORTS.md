# Imports et répertoires

## Reprendre un import PGN

Ouvrir **Bibliothèque → Imports reprenables**, puis choisir le PGN. Les nouvelles parties sont regroupées dans une base locale portant le nom du fichier. Une partie déjà présente est rattachée à cette base sans duplication.

La vérification SHA-256 précède la lecture et l’indexation. Les lots de 250 parties, leurs liens de source et le point de reprise sont validés ensemble. **Mettre en pause** conserve les lots validés ; le lot incomplet est annulé. Au redémarrage, le gestionnaire permet de reprendre la tâche. Il ne lance pas d’import automatiquement.

**Retrouver le PGN** accepte un autre chemin uniquement si l’empreinte est identique. Un fichier modifié est refusé. Ne pas modifier le PGN pendant son import. Une partie invalide bloque l’import au dernier lot validé ; corriger le fichier nécessite un nouvel import, les doublons exacts étant ignorés. **Retirer de cette liste** supprime seulement le point de reprise, jamais les parties.

L’import classique **+ Importer** reste disponible : l’interruption annule toute cette opération. Les archives T64 et fichiers EPD/FEN gardent leur fonctionnement actuel.

## Construire un répertoire

Depuis **Ouvertures → Mes répertoires**, créer un répertoire à partir de l’étude actuelle et choisir le camp préparé. Toutes les variantes accessibles de l’étude sont ajoutées. Les branches déjà supprimées de l’étude sont exclues. Le classique et Chess960 sont séparés.

Chaque position partage ses réponses entre les différents ordres de coups qui y conduisent. Les compteurs de coups FEN ne créent pas de doublons ; le trait, les droits de roque et la prise en passant restent pris en compte. On peut fusionner une autre étude commençant à la racine ou à une position déjà connue, choisir le coup principal, annoter les coups et retirer une branche. Une transposition encore accessible garde son historique de révision.

Les études enregistrées ajoutées sans modification fournissent un lien vers leur partie modèle. Les parties restent dans la bibliothèque lorsqu’un répertoire est supprimé.

L’import PGN d’un répertoire est atomique, limité à 16 Mio et 200 parties par fichier. L’export PGN conserve les coups, variantes, commentaires et NAG. Chaque position est développée une fois ; les retours et transpositions portent un commentaire indiquant que leur suite est développée ailleurs dans le même PGN. Le graphe peut être reconstruit lors d’un nouvel import dans Trait64. Un logiciel PGN classique peut nécessiter de rejoindre la position indiquée pour voir la suite.

## Traitements en arrière-plan

Créer, fusionner, importer et exporter ne bloquent plus l’interface. Le dialogue affiche le nombre de positions traitées et permet d’annuler. Pour un import de répertoire, une erreur ou une annulation retire **tout cet import** ; notes, coups et révisions déjà présents sont conservés. Un export n’écrase son fichier cible qu’après validation complète ; une annulation conserve le fichier précédent. Une demande reçue après la validation finale ne peut plus annuler l’opération achevée.

Les statistiques et réponses manquantes sont également calculées en arrière-plan. **Annuler l’actualisation** interrompt ce calcul ; **Actualiser** le relance. Un résultat interrompu n’est pas présenté comme une absence de lacunes. Les éditions ponctuelles et les révisions restent synchrones.

## Analyser le répertoire

**Analyser le répertoire** permet de choisir le réglage moteur actuel ou un profil enregistré, avec un budget de 0,1 à 10 secondes par position. Une copie PGN de toutes les variantes rejoint la file d’analyses persistante : pause, reprise, résultats et export annoté sont disponibles dans ce gestionnaire. L’étude, les notes du répertoire et l’historique des révisions restent conservés.

Chaque position de transposition est développée une fois. Cela borne les cycles et conserve le graphe préparé, sans analyser tous les historiques de répétition possibles. Les résultats dépendent du moteur et du temps choisis ; ils ne constituent pas une preuve de correction de toutes les lignes.

## Apprendre et repérer les lacunes

**Réviser les positions dues** affiche une position correspondant au camp préparé. Toutes les réponses enregistrées dans le répertoire sont acceptées, y compris les alternatives. Un autre coup n’est pas déclaré mauvais : il n’appartient simplement pas aux réponses préparées. Voir la solution ou se tromper avant de réussir marque ce rappel comme manqué.

Le calendrier est simple et explicite : une réussite programme 1 jour, puis 3 jours, puis double l’intervalle jusqu’à 365 jours ; un rappel manqué revient à 1 jour. Une transposition partage le même calendrier. La révision libre peut anticiper une échéance et met également ce calendrier à jour. Seuls les exercices terminés sont enregistrés. Quitter restaure l’étude, sa position et son orientation.

**Positions à retravailler** affiche les rappels manqués. **Réponses à préparer** présente les 100 réponses adverses absentes du répertoire les plus fréquentes dans la bibliothèque locale. Ces indications dépendent des parties importées ; elles ne démontrent pas une nouveauté théorique et n’évaluent pas la qualité échiquéenne des coups.

Les cartes de lignes existantes restent utilisables. La planification adaptative par niveau et la validation moteur d’autres réponses pendant un exercice restent à développer.

## Préparer un adversaire à partir d’une position

Dans **Ouvertures → Préparer**, ou depuis la fiche d’un joueur, ouvrir la préparation par position. Choisir la base, le nom exact du joueur et sa couleur dans la partie. Les années et la moyenne des deux Elo sont facultatives (0 signifie sans borne). Une borne Elo exclut les parties dont l’un des deux Elo manque ; une borne temporelle exclut les années inconnues.

Le calcul porte sur la position actuelle de l’échiquier et les lignes principales des parties filtrées. Il reconnaît les transpositions et compte une partie une seule fois pour chaque coup, même après répétition. Les victoires sont toujours indiquées du point de vue Blancs/Noirs, quelle que soit la couleur de l’adversaire sélectionné. L’Elo moyen ne provient que des en-têtes PGN renseignés.

La requête s’exécute en arrière-plan et peut être annulée. Une nouvelle demande remplace la précédente. Ouvrir une autre bibliothèque ou modifier ses parties invalide ces résultats. Un coup ne peut être joué depuis des statistiques relatives à une autre position. **Partie exemple** ouvre une partie réellement comprise dans les résultats.

L’export CSV inclut la FEN et les filtres utilisés, les comptes de résultats, les Elo connus et l’identifiant de la partie d’exemple. Les résultats ne dépendent d’aucun service distant.
