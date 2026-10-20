## Audit global de fiabilité — 20 octobre 2026

- Récupération des sessions endommagées sans perte de l’original ; chemins restaurés uniquement depuis les variantes enregistrées.
- Annulation de réindexation atomique, même au dernier signal de progression.
- Compteurs FEN protégés ; refus d’un dépassement avant modification de l’étude.
- Anciennes réponses moteur exclues après une nouvelle recherche ou un arrêt réentrant ; options automatiques adaptées aux bornes du moteur.
- Fermeture sûre pendant une connexion GitHub ; correction d’un accès à de la mémoire libérée.
- Distinction entre fin de partie et nulle réclamable dans les analyses ; priorité correcte du pat.
- Cadence PGN cohérente avec l’horloge et protégée des dépassements entiers.
- Stress FEN/PGN reproductible et contrôles ASan/UBSan étendus à tous les modules dans la CI.

Rapport et limites : [DEBUG-AUDIT.md](DEBUG-AUDIT.md). Schéma 7 conservé. Publication en attente de validation complète.

## Motifs tactiques et repères sur l’échiquier

- Fourchettes/attaques multiples, clouages au roi, enfilades du roi, échecs à la découverte et doubles échecs détectés dans les suites légales du bilan.
- Pièces et cases explicites ; captures qui exposeraient le roi de l’attaquant exclues des cibles de fourchette. Un motif observé n’est pas présenté comme un gain forcé.
- Copie d’étude annotée : flèches vertes pour les attaques, marques jaunes pour les alignements, conservation dans le PGN exporté et étude d’origine préservée.
- Tests des faux positifs, sous-promotions, prise en passant, Chess960 et comparaison au générateur de coups légaux. Aucun changement de schéma.

Guide : `docs/TACTICAL-MOTIFS.md`. Les attaques à la découverte sans échec, plans stratégiques et sacrifices justifiés restent à développer.

## Comprendre les coups du bilan

- Deux suites consultables pour chaque coup analysé : coup joué suivi de la réponse moteur, et proposition moteur avant le coup.
- Captures et cases réelles, prise en passant, promotions, roques classiques/Chess960, échecs, mats et pats déterminés en rejouant uniquement des coups légaux.
- Comparaison du matériel présent, distincte de l’évaluation moteur ; nom du moteur, budget et profondeur affichés. Aucun niveau Elo estimé.
- Ouverture de la suite annotée dans une étude indépendante ; retour à l’étude d’origine avec ses résultats conservés.
- Limites explicites : suite moteur, pas preuve d’une défense forcée ; 128 demi-coups maximum, départ depuis une FEN sans historique des répétitions. Les thèmes tactiques et plans stratégiques restent à approfondir.

Guide : `docs/MOVE-EXPLANATIONS.md`.

## Nouveautés du lot répertoires

- Création, fusion, import/export des répertoires en arrière-plan, progression et annulation sans ajout partiel. Notes et révisions préservées.
- Statistiques et réponses adverses manquantes calculées sans bloquer l’interface ; résultats périmés ignorés.
- Analyse de toutes les variantes d’un répertoire par profil moteur, dans la file persistante.
- Correction du nom sélectionné à l’ouverture du dialogue et des notifications en petite fenêtre ; changement de bibliothèque refusé pendant un exercice.
- Tests Debian 12/13, DEB installés et AppImage puis publication directe dans GitHub Releases, sans archives Actions. Manifeste et SHA-256 vérifiés ; une version publiée n’est pas écrasée.
- Publication des brouillons : utilisation de la réponse de création et de l’identifiant exact, attente bornée si l’index GitHub tarde à refléter la création. Aucun brouillon dupliqué et aucun assouplissement des vérifications.

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

La série 0.3 reste en alpha. Le catalogue complet n’est pas achevé. Les imports ont une reprise fiable, mais les performances sur 1/2/5/10 millions de **PGN réels** ne sont pas encore qualifiées. L’import PGN d’un répertoire reste limité à 16 Mio et 200 parties ; l’édition des coups, le renommage, la suppression et les révisions restent synchrones. Les transpositions sont développées une seule fois lors de l’export ou de la préparation d’une analyse.

Les rappels manqués mesurent la mémorisation, pas la qualité des coups ni un niveau Elo. Les statistiques reposent sur les métadonnées connues des parties. La comparaison de répertoires, les explications tactiques approfondies, les cours de finales/calcul/visualisation, les formats supplémentaires et la reconnaissance d’images restent à développer. Un essai sur le PC Voyager cible reste nécessaire.

Les installations 0.1/0.2 nécessitent un premier passage manuel à 0.3, car elles ne disposent pas du module de mise à jour. Voir `docs/NEXT-STEPS.md`, `docs/ROADMAP.md` et `docs/PERFORMANCE.md` pour les limites et la reprise précise.
