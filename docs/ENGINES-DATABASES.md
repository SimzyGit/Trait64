# Moteurs, bases et recherches

## Télécharger et régler un moteur

Ouvrez **Bibliothèque → Téléchargements → Moteurs**. Stockfish, Arasan et PlentyChess disposent de versions et d’empreintes vérifiées dans le catalogue. Le téléchargement est volontaire ; le moteur n’est pas lancé automatiquement.

Une installation ajoute un profil. Ouvrez **Profils moteurs**, choisissez-le puis **Charger**. Cliquez **Démarrer / appliquer** pour découvrir les options réellement déclarées par ce moteur. Réglez les threads, la mémoire, le MultiPV et ses options propres, puis choisissez un nom et cliquez **Mémoriser**. Vous pouvez ainsi conserver plusieurs réglages du même moteur et passer d’un profil à l’autre. La limitation de force concerne le jeu ; les analyses utilisent toute la force du moteur.

Les binaires du catalogue ciblent Linux x86_64. La compatibilité glibc est vérifiée avant téléchargement : PlentyChess 8 nécessite glibc 2.38 et ne se télécharge donc pas sur Debian 12. Un moteur UCI déjà installé peut toujours être choisi manuellement dans les paramètres.

**Supprimer le moteur** retire son installation courante, sans effacer les parties. Les profils sont conservés ; il faut réinstaller ce moteur ou choisir un autre exécutable avant de les réutiliser. Une mise à jour conserve aussi les anciennes versions sur disque ; leur gestion complète depuis l’interface reste à développer.

## Ajouter une base de parties

Dans **Téléchargements**, choisissez les champions, championnes disponibles, événements historiques ou ouvertures. Les 42 collections proviennent de PGN Mentor. Leur téléchargement gratuit n’est pas une licence ouverte de redistribution ; **Source et conditions** ouvre la page du fournisseur. Les fichiers ne sont pas incorporés à Trait64.

Après téléchargement, le PGN est importé dans une base nommée. Une partie identique peut appartenir à plusieurs bases sans être dupliquée dans la bibliothèque. L’historique indique le fichier, la provenance, la date et les nombres de parties ajoutées/déjà présentes. L’empreinte d’un fichier téléchargé est enregistrée ; PGN Mentor ne fournit pas de SHA-256 amont permettant de l’authentifier indépendamment.

Choisissez la base dans la liste au-dessus des parties. **Exporter cette base** produit son PGN. **Effacer le PGN téléchargé** supprime le fichier de téléchargement, pas les parties importées. Les importations et leur historique restent disponibles hors ligne. Un import interrompu est annulé intégralement ; il n’est pas encore reprenable à mi-fichier.

## Retrouver une partie

La barre principale cherche les fragments de joueur, tournoi, code ECO et autres métadonnées. Pour un nom d’ouverture, utilisez **Recherche avancée → Ouverture** : par exemple « sicilienne », « française » ou « Najdorf ». Les familles traduites sont distinguées des variantes homonymes d’autres familles.

Combinez librement la base, le joueur et sa couleur, l’ouverture, la plage ECO, les dates, les Elo, le tournoi, le résultat, les favoris, le matériel ou la position de l’échiquier. Le champ d’ouverture sélectionne les codes ECO de notre dictionnaire : plusieurs variantes peuvent partager le même code. Pour retrouver précisément une variante, placez sa position sur l’échiquier et cochez **Position de l’échiquier**.

Les résultats arrivent en arrière-plan, par pages de 200. **Annuler la recherche** interrompt SQLite ; une nouvelle recherche remplace la précédente. Les filtres restent actifs avec **Page suivante** et **Page précédente**. Après une modification des données ou des filtres, la navigation repart de la première page. Les pages ne constituent pas un instantané historique si un autre processus modifie la base entre deux demandes.

La qualification sur plusieurs millions de parties complètes reste en cours : [mesures et limites](PERFORMANCE.md). Les sauvegardes SQLite préservent aussi favoris et entraînements, contrairement à un simple export PGN.
