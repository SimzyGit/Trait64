## Carnet de coups côte à côte — 28 octobre 2026

Le lecteur de l'espace Analyse présente désormais une ligne par numéro de coup,
avec le demi-coup blanc et le demi-coup noir côte à côte. Chaque cellule reste
cliquable, la sélection suit la bonne position et les variantes restent séparées.
Les départs FEN avec les Noirs et les lignes incomplètes conservent leur numéro
et leur colonne. Les commentaires longs et annotations restent accessibles.

Le SAN français est une transformation d'affichage : PGN, études et coups
internes gardent leur format standard. Les exports existants, les pièces et le
schéma SQLite 7 sont préservés. Le défilement suit le coup après le calcul de
mise en page, y compris dans la page entière aux tailles maximales.

Validation locale : 14 suites sans suite ignorée, Stockfish réel, 207 contrôles
du carnet incluant six captures ; matrice existante de neuf configurations et
164 captures. Trois passages des interactions ne reproduisent pas l'ancien
échec isolé des filtres ; sa cause reste inconnue. Rapports et limites :
https://github.com/SimzyGit/Trait64/blob/main/docs/MOVE-NOTEBOOK.md

À 960 × 640 avec texte 150 % / interface 125 %, la page reste défilante.
Les essais logiciels hors écran ne valident pas Voyager/Wayland/GPU.
Aucun benchmark 100k supplémentaire et aucune migration de base.

## Refonte native — dev.64, 28 octobre 2026

Navigation latérale compacte ou développée, grand échiquier à l’accueil, panneaux adaptables et palette de commandes au clavier. Police, texte de 90 à 150 %, interface de 90 à 125 %, contraste renforcé, animations réduites et libellés français/anglais sont persistés.

Corrections : commandes coupées à petite taille ou en grandes polices, blocage du profil Joueurs vide, titre d’aide tronqué, choix de promotion et couleurs des dialogues. Les valeurs des listes restent stables après changement de langue. Board.qml, Piece.qml et le schéma SQLite 7 sont conservés.

Les 13 suites passent sous Debian 12/13 et Qt ASan/UBSan ; le cœur passe également sous sanitizers. Paquets Debian installés et AppImage démarrés. Localement : 1 180 contrôles incluant ceux des captures, 9 configurations, 164 images et zéro avertissement QML. Tag, cinq fichiers publiés, tailles, SHA-256 et sélection de la mise à jour depuis dev.57 vérifiés ; aucune archive Actions. [Rapport](VALIDATION.md).

Essais en rendu logiciel hors écran ; aucun essai matériel Voyager/Wayland/120 Hz. Certains diagnostics C++ et descriptifs de catalogue restent français. Un échec local isolé de préparation est documenté dans [local.json](VALIDATION.md), non reproduit lors des relances ni dans les CI finales ; diagnostic ajouté, assertion inchangée. Aucun nouveau benchmark de grandes bases.

## Vérification de la base en arrière-plan — 25 octobre 2026

- « Vérifier la base » utilise désormais une connexion SQLite dédiée en arrière-plan. L'état est affiché et le bouton « Interrompre » permet d'annuler, en conservant l'étude ouverte.
- Les tâches qui se chevauchent sont refusées. Une interruption ou une erreur ne peut pas être présentée comme une vérification réussie. La relance immédiate depuis une notification de fin conserve le bon résultat.
- Douze suites locales et CI Debian 12/13 réussissent, avec contrôles ASan/UBSan et démarrage des paquets. Schéma 7 inchangé.
- La qualification séparée de dev.55 a importé 100 000 parties avec Qt sans avertissement QML, réindexé 10 000 parties et vérifié tous les PGN de la sauvegarde et de la restauration. Résultats et limites dans `docs/VALIDATION.md` ; aucun essai Voyager ni de plusieurs millions de parties n'est revendiqué.

La sauvegarde manuelle et le contrôle préalable à l'ouverture d'une bibliothèque restent des opérations synchrones distinctes. Détails : `docs/ASYNC-INTEGRITY.md`.

## Import PGN accéléré — dev.55, 24 octobre 2026

- Recherche des doublons par l'index des empreintes existant, et calcul du matériel sans expression régulière recompilée pour chaque position. Schéma 7, clés persistées et transactions conservés.
- Sur les mêmes 10 000 parties réelles, l'essai conservé passe de 93,114 à 42,498 secondes (−54,4 %). Les données échiquéennes et agrégats comparés restent identiques ; ce gain ne prédit pas chaque machine ou corpus.
- 100 000 parties importées localement avec pause/reprise, intégrité, sauvegarde et restauration contrôlées. Un pic de lecture concurrente de 4,031 secondes reste à caractériser. L'interface fait l'objet d'une mesure Qt distincte.
- Douze suites passent sous Debian 12/13 et avec Qt ASan/UBSan ; paquets installés et AppImage démarrés. Fichiers de release et sélection de la mise à jour depuis dev.52 vérifiés. Aucun essai Voyager/Wayland matériel ni de plusieurs millions de parties n'est revendiqué.

Détails et limites : [PERFORMANCE.md](PERFORMANCE.md), [TESTING.md](TESTING.md).

## Audit global de fiabilité — dev.52, 21 octobre 2026

- Récupération des sessions endommagées sans perte de l’original ; chemins restaurés uniquement depuis les variantes enregistrées.
- Annulation de réindexation atomique, même au dernier signal de progression.
- Compteurs FEN protégés ; refus d’un dépassement avant modification de l’étude.
- Anciennes réponses moteur exclues après une nouvelle recherche ou un arrêt réentrant ; options automatiques adaptées aux bornes du moteur.
- Chaînes UCI vides corrigées : désactiver le journal ne tente plus d’ouvrir un fichier `<empty>` et ne fait plus quitter Stockfish dans un dossier protégé.
- Fermeture sûre pendant une connexion GitHub ; correction d’un accès à de la mémoire libérée.
- Distinction entre fin de partie et nulle réclamable dans les analyses ; priorité correcte du pat.
- Anciennes entrées de cache marquées à tort comme fin de partie recalculées puis remplacées. Relancer une analyse pour actualiser un bilan historique.
- Cadence PGN cohérente avec l’horloge et protégée des dépassements entiers.
- Stress FEN/PGN reproductible et contrôles ASan/UBSan étendus à tous les modules dans la CI.

Publié dans [v0.3.0-dev.52](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.52). Douze suites réussies sur Debian 12 et 13, contrôles ASan/UBSan, paquets installés et AppImage validés. Rapport et limites : [DEBUG-AUDIT.md](DEBUG-AUDIT.md). Schéma 7 conservé.

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

Les installations 0.1/0.2 nécessitent un premier passage manuel à 0.3, car elles ne disposent pas du module de mise à jour. Voir `docs/PROJECT-STATUS.md`, `docs/ROADMAP.md` et `docs/PERFORMANCE.md` pour les limites et la reprise précise.
