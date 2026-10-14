## Imports reprenables, répertoires et préparation

- **Imports PGN reprenables** : progression, vitesse et estimation du temps restant ; pause puis reprise après fermeture ou interruption. Les parties et leur point de reprise sont validés ensemble par lots de 250. Le lot incomplet est annulé.
- **Vérification des fichiers** : SHA-256 complet avant chaque reprise, possibilité de retrouver un PGN déplacé, refus d’un fichier modifié et verrou entre instances. Retirer une tâche ne supprime pas ses parties.
- **Répertoires personnels par position** : nom, camp, classique ou Chess960, transpositions partagées, coup principal et alternatives, commentaires/NAG, parties modèles, fusion d’études et import/export PGN.
- **Répétition espacée des répertoires** : toutes les réponses préparées sont acceptées ; calendrier partagé entre transpositions, révision libre et suivi des rappels manqués. Quitter un exercice restaure l’étude.
- **Couverture locale** : réponses adverses absentes du répertoire, classées par fréquence dans la bibliothèque. Leur absence ne prouve pas une nouveauté théorique.
- **Préparation par position** : filtres combinés par base, joueur exact/couleur, période et moyenne des Elo renseignés ; calcul annulable en arrière-plan, parties exemples et export CSV avec FEN et filtres.
- **Migrations SQLite 5 et 6** avec sauvegarde des bases précédentes. Neuf suites automatisées couvrent notamment l’arrêt réel d’un processus pendant un import, les transpositions, les calendriers et les interactions QML.

Guide : `docs/REPERTOIRES-IMPORTS.md`.

## Fonctions existantes conservées

- Trois moteurs téléchargeables avec SHA-256 épinglé : Stockfish 19, Arasan 26 et PlentyChess 8 ; profils et options UCI indépendants. PlentyChess nécessite glibc 2.38 ou plus.
- 42 collections PGN optionnelles : champions classiques, titres FIDE séparés, championnes disponibles, matchs et ouvertures ; conditions du fournisseur affichées.
- Bases nommées, dédoublonnage et appartenance à plusieurs bases, historique, import/export, recherche multicritère annulable et pages de 200 parties par curseur.
- Analyse UCI, MultiPV, comparaison de 2 à 4 moteurs, graphe et bilan de la ligne principale, cartes d’entraînement existantes, annotations et personnalisation.
- Mises à jour GitHub Releases en arrière-plan : Stable/Bêta/Développement, nouveautés, Mettre à jour/Plus tard/Ignorer, contrôle SHA-256 et sauvegarde avant installation.
- Accès aux dépôts privés facultatif ; aucun jeton intégré. Remplacement atomique AppImage et installation Debian avec autorisation système.
- Sauvegardes automatiques toutes les six heures, 14 instantanés, sauvegardes pré-mise à jour séparées et préservation des sessions illisibles.
- CI Debian 12/13, tests mémoire du cœur, paquets .deb et AppImage x86_64. Une modification ordinaire de main produit uniquement une préversion Développement après validation.

## Limites de cette préversion

La série 0.3 reste en alpha. Le catalogue complet n’est pas achevé. Les imports ont une reprise fiable, mais les performances sur 1/2/5/10 millions de **PGN réels** ne sont pas encore qualifiées. L’import PGN d’un répertoire reste limité à 16 Mio et 200 parties ; certaines opérations de répertoire restent synchrones.

Les rappels manqués mesurent la mémorisation, pas la qualité des coups ni un niveau Elo. Les statistiques reposent sur les métadonnées connues des parties. L’analyse moteur de toutes les branches, les explications tactiques approfondies, les cours de finales/calcul/visualisation, les formats supplémentaires et la reconnaissance d’images restent à développer. Un essai sur le PC Voyager cible reste nécessaire.

Les installations 0.1/0.2 nécessitent un premier passage manuel à 0.3, car elles ne disposent pas du module de mise à jour. Voir `docs/NEXT-STEPS.md`, `docs/ROADMAP.md` et `docs/PERFORMANCE.md` pour les limites et la reprise précise.
