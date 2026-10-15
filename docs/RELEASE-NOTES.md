## Analyses en arrière-plan et reprise

- File persistante accessible dans le menu de l’espace Analyse : moteur courant ou profil enregistré, MultiPV, temps par position et toutes les variantes.
- Pause/reprise, récupération après interruption, résultats conservés à chaque position et étude courante indépendante.
- Évaluations, WDL fournis par le moteur et variantes légales ; cache avec provenance et export PGN annoté, sans modifier la ligne réellement jouée.
- Les mats restent des scores de mat et ne gonflent plus les pertes en centipions du bilan.
- Migration 7 avec sauvegarde préalable. Tests Stockfish réels, arrêt brutal, changement de moteur, Chess960, pagination et édition simultanée.

Guide : `docs/ANALYSIS-QUEUE.md`. Les réseaux NNUE ou autres ressources non identifiables désactivent seulement le cache ; ils n’empêchent pas l’analyse.

## Import PGN parallèle et compatibilité des corpus anciens

- **Lecteurs parallèles réglables de 1 à 8** dans les imports reprenables, avec conservation de l'ordre des parties, de la pause/reprise et du point de sauvegarde. Deux lecteurs sont proposés par défaut.
- **PGN anciens** : acceptation d'une origine de pièce redondante, telle que `Rd1d2`, uniquement lorsqu'elle identifie un seul coup légal. Les coups ambigus, fausses captures et origines incorrectes sont refusés.
- **Validation sur 10 000 parties réelles** : résultats identiques à 1 et 4 lecteurs, pause à 5 000 puis reprise, intégrité et sauvegarde vérifiées. Les corpus de plusieurs millions de parties restent à qualifier.
- **Dix suites automatisées** dans la CI Linux, avec contrôles des sorties du benchmark pour protéger les bases, journaux et sauvegardes existants. Aucun changement de schéma de données dans ce lot.

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
