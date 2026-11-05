# Changements

## Profils, études et confort — 5 novembre 2026

- **Page Profils** : chaque joueur suivi a sa page, avec « Analyser le profil » qui passe en revue les parties synchronisées encore sans bilan et alimente le tableau de bord.
- **Onglet Études**, à la manière d'En Croissant et de Pawn Appétit : import d'un dossier entier, y compris les études imbriquées les unes dans les autres.
- **Ranger par ouverture** (Bibliothèque) : chaque partie reçoit la collection de sa famille d'ouverture d'après son code ECO, sans rien télécharger. Les parties sans code ECO restent inchangées.
- **Pièces et échiquiers** : seize palettes d'échiquier au lieu de neuf, dont sept qui tiennent dans la pénombre, chacune avec un aperçu de ses deux cases. On peut désormais garder plusieurs jeux de pièces installés et passer de l'un à l'autre, au lieu qu'un import remplace le précédent.
- **Barre d'évaluation** le long de l'échiquier, et courbe de la partie au-dessus du moteur.
- **Portrait** : chaque profil peut recevoir une photo ; Trait64 en range une copie réduite dans son dossier de données, le profil ne dépend donc pas d’un fichier que l’on peut déplacer. Sans photo, les initiales tiennent la place.
- **L'interface anglaise parle anglais partout** : les cent soixante messages du logiciel étaient écrits en français dans le code et le restaient quand on choisissait l'anglais. Un test refuse désormais toute phrase sans traduction.
- **Fluidité** : les lignes de la bibliothèque et du carnet de coups sont recyclées au défilement plutôt que rebâties une à une, et les pièces ne sont plus redessinées à chaque pixel quand on tire une poignée de panneau.
- **Titre et drapeau** : le titre FIDE se détache du nom comme sur Lichess, et le drapeau de la fédération accompagne le joueur.
- Le logo sert désormais d'icône de l'application, sur les trois systèmes.
- Plus de bridage du moteur : fils d'exécution et table de hachage suivent la machine.
- Corrections : les colonnes de la bibliothèque ne tombaient pas sous leurs titres ; le survol d'un menu peignait le texte en noir sur noir ; le `.deb` était refusé par les installateurs graphiques de Debian ; Windows et macOS annonçaient une version de développement.

## Bilan, comptes en ligne et installation — 3 novembre 2026

- **Bilan en deux passages** : un premier passage rapide va du début vers la fin, puis un passage complet remonte de la fin vers le début. Le moteur garde sa table de hachage d'une position à l'autre : en remontant, il connaît déjà la suite de la partie, ce qui rend les évaluations des positions précédentes plus fiables (c'est ce que font les analyses « backward » de ChessBase et SCID). Un seul passage reste possible.
- **Comptes Lichess et Chess.com** (Fichier → Comptes) : enregistrez vos pseudos, Trait64 ne récupère à chaque synchronisation que les parties jouées depuis la précédente, à la demande ou au démarrage. Retirer un compte garde ses parties.
- **Installation en une commande** : `install.sh` télécharge la dernière version publiée, vérifie son empreinte SHA-256 et l'ajoute au menu des applications, sans sudo. README refait avec des captures à jour.
- Carnet en tableau : l'étiquette « Variante » n'apparaît plus qu'au début de chaque variante.

## Interface classique — 3 novembre 2026

L'interface suit désormais la disposition que connaissent la plupart des joueurs (ChessBase, SCID, analyse Lichess), modernisée : barre de menus, onglets d'espaces de travail, grand échiquier encadré des joueurs (couleur, nom, Elo ou pendule), notation, arbre et livre à droite, moteur dessous, bibliothèque en tableau dense. Thème clair par défaut, gris neutres et un seul accent ; le thème sombre et les tailles restent réglables. L'arbre d'ouvertures montre une barre de résultats Blancs / nulles / Noirs par coup. Le carnet garde des colonnes compactes sur grand écran.

Corrections : changer de langue renvoyait sur l'Accueil ; sur une petite fenêtre, la rangée d'onglets poussait des boutons hors de l'écran.

## Petits ajouts — 3 novembre 2026

- **Écriture de livres Polyglot** : Bibliothèque → « Exporter un livre Polyglot » écrit un `.bin` des coups joués au moins deux fois, pondérés par le nombre de parties. Les clés du schéma 8 sont des clés Polyglot ; un aller-retour écriture/lecture est testé, roques compris.
- **Diagrammes SVG** : l'export de diagramme accepte `.svg` en plus de `.png`, avec la même apparence, les flèches et les cercles. Les pièces sont des groupes transformés, lisibles par les moteurs SVG Tiny comme par les navigateurs.
- **Horloges et évaluations PGN** : `[%clk]` et `[%eval]` (parties Lichess) s'affichent à côté du coup ; `[%cal]`/`[%csl]` ne polluent plus les commentaires. Le PGN enregistré reste inchangé.

## Analyse en lot et nouveautés — 3 novembre 2026

- **Analyse des parties affichées** : la file d'analyse reçoit en une fois jusqu'à 200 parties correspondant aux filtres de la bibliothèque (lignes principales), avec le budget et le profil choisis.
- **Chercher la nouveauté** (menu de l'étude) : premier coup de la ligne principale qu'aucune autre partie de la bibliothèque n'a joué depuis cette position, avec le nombre de parties qui l'atteignent et les suites connues. Les coups de la partie elle-même ne comptent pas.

## Dossier adversaire — 3 novembre 2026

La fiche joueur devient un dossier de préparation : lignes réellement jouées sur les six premiers demi-coups (Blancs et Noirs, avec fréquence et score), résultats selon la longueur de partie, ouvertures à cibler (plus faibles scores sur au moins trois parties terminées) et courbe d'Elo. Un bouton importe directement les parties publiques d'un pseudo Lichess ou Chess.com.

## Puzzles tactiques — 3 novembre 2026

Banque de puzzles locale (Entraînement → Puzzles) : import direct de l'export Lichess (CSV, compressé `.zst` ou non, licence CC0) avec plafond et plage de classement, et puzzles tirés de vos erreurs analysées (Bilan → « Créer des puzzles des erreurs »). Chaque puzzle est choisi près de votre classement (Elo, K = 32), parmi ceux non résolus, par thème (mat en 1/2/3, fourchette, clouage…) et origine. Au dernier coup, tout mat est accepté ; un indice ou la solution comptent comme un échec. Chrono et historique des tentatives conservés.

## Schéma 8 : positions en clés Zobrist — 3 novembre 2026

Les quatre tables de positions (`positions`, `all_positions`, `opening_edges`, `opening_stats`) utilisent une clé entière de 64 bits au lieu de la FEN texte, en tables sans `rowid`. Sur 20 000 parties générées (≈ 1,9 million de positions, mêmes fichier et machine) : **1 010 Mo → 413 Mo (−59 %)**, import 192 s → 189 s, recherche par position 2,79 → 1,74 ms, arbre d'ouvertures 0,05 → 0,03 ms. Ces mesures ne remplacent pas la qualification sur plusieurs millions de parties réelles.

**Recherche par structure de pions** : dans la bibliothèque, « Même structure de pions » retrouve les parties qui atteignent la structure de l'échiquier, quelles que soient les autres pièces (clé indexée, calculée aussi pour les bases migrées).

La migration sauvegarde d'abord la base (`.before-v8-…`), convertit les clés existantes sans relire les PGN, puis compacte le fichier. Deux positions ne diffèrent plus par une case en passant impossible à jouer, comme le veut la règle de répétition. Une collision de clés 64 bits est théoriquement possible mais négligeable (de l'ordre d'une chance sur cinquante pour un milliard de positions distinctes) ; elle ne pourrait qu'ajouter une partie à un résultat de recherche.

## Gains rapides — 3 novembre 2026

- **Livres Polyglot** : onglet Livre à côté de l'Explorateur, part de chaque coup ; chemin du `.bin` dans Paramètres. Recherche dichotomique dans le fichier, clés vérifiées contre les valeurs de référence du format.
- **Import Lichess / Chess.com** par pseudo, sans compte, depuis la fenêtre des imports. Lichess ne récupère ensuite que les nouvelles parties ; les parties sont rangées sous une source par joueur.
- **Force du moteur en Elo** dans la partie locale (`UCI_LimitStrength` + `UCI_Elo`, bornés à la plage du moteur) ; repli sur le niveau 0–20 si le moteur ne le propose pas.
- **Carnet de coups continu** en option (ordre PGN, variantes entre parenthèses) et **notation figurine** (♘ ♗ ♖ ♕ ♔), affichage seulement.
- **Glisser-déposer** d'un PGN, FEN, EPD ou T64 dans la fenêtre.
- **Tournois de moteurs** toutes rondes entre 2 et 8 profils : couleurs alternées sur la même ouverture (liste intégrée ou livre Polyglot), arbitrage complet (mat, pat, répétition, 50 coups, matériel insuffisant, coup illégal, moteur muet), classement avec écart Elo estimé ; les parties rejoignent la bibliothèque sous leur propre source.
- **Correction** : fermer l'application pendant qu'un moteur tarde à quitter pouvait la faire planter.
- **CI Windows et macOS** (portabilité, sans publication). Schéma SQLite 7 inchangé.

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


## Vérification d'intégrité — dev.57, 25 octobre 2026

- Contrôle manuel déplacé dans un worker avec sa connexion SQLite et annulation pendant la requête ; état de tâche visible et étude non enregistrée conservée.
- Refus du chevauchement, attente à la fermeture, absence de faux succès après erreur/interruption ; résultat capturé avant le signal de fin pour préserver les relances réentrantes.
- Tests déterministes SQL/Qt et 12 suites locales/CI, contrôles ASan/UBSan, DEB installés et AppImage démarrés ; schéma 7 inchangé.
- Qualification séparée de dev.55 terminée : Qt 100k, 1 842 navigations sans avertissement QML, contenu de tous les PGN sauvegardés/restaurés identique, réindexation 10k et profil SQLite.
- Pic SQL local de 4,031 s non reproduit sur un autre hôte ; cause inconnue. Pas de qualification Voyager, 500k ou plusieurs millions. Sauvegarde manuelle et contrôle préalable d'ouverture encore synchrones.
- PR #12 fusionnée ; release, tag, cinq fichiers, SHA-256 et sélecteur réel depuis dev.55 vérifiés. Aucune archive Actions.

## Optimisation des imports — dev.55, 24 octobre 2026

- Requête de doublons bornée par l'index des empreintes existant, sans changement du schéma 7.
- Clés matérielles identiques aux anciennes, calculées sans compiler une expression régulière à chaque position ; même normalisation dans la recherche.
- Tests du travail SQLite et des clés persistées, captures, variantes, promotions, prise en passant, Chess960 et annulation finale de réindexation.
- Benchmark enrichi : restauration réelle, réindexation, lectures concurrentes, distributions et mémoire. Mode Qt opt-in pour mesurer les événements/navigation pendant un import réel.
- Comparaison complète conservée sur 10 000 parties ; import local de 100 000 avec pause/reprise, intégrité, sauvegarde et restauration validés. Pic de lecture concurrente de 4,031 s documenté, qualification Qt séparée.
- PR #10 fusionnée, dev.55 publiée : 12 suites Debian 12/13 et Qt ASan/UBSan, paquets et AppImage, cinq fichiers/SHA-256 et sélecteur de mises à jour vérifiés. Aucun corpus, SQLite ou archive Actions ajouté.

## Explications des suites du bilan — 16 octobre 2026

- Relecture légale des suites avant/après le coup joué, avec captures, promotions, roques, échecs, mats, pats et compte matériel conventionnel.
- Dialogue « Comprendre ce coup », comparaison des deux suites, provenance moteur et ouverture d’une copie annotée sans modifier l’étude d’origine.
- 15 contrôles du cœur ajoutés, tests Stockfish et clic réel sur le bouton d’ouverture, capture à 960 × 640.
- Aucun changement de schéma ; limites et guide dans MOVE-EXPLANATIONS.md.

## Publication des brouillons — 16 octobre 2026

- La création utilise directement la réponse REST de GitHub ; les contrôles et la publication ciblent l’identifiant exact de la release.
- Attente bornée si le nouveau brouillon tarde à apparaître dans la liste utilisée par le téléversement. Aucun nouvel essai de création, aucun écrasement d’une version publiée ou d’un brouillon étranger.
- Quatre tests de régression supplémentaires, soit 17 tests de publication ; correction des descriptions périmées du schéma et des fonctions livrées.

## Répertoires asynchrones et distribution

- Création, fusion, import/export des répertoires en arrière-plan, progression et annulation sans ajout partiel. Notes et révisions préservées.
- Statistiques et réponses adverses manquantes calculées sans bloquer l’interface ; résultats périmés ignorés.
- Analyse de toutes les variantes d’un répertoire par profil moteur, dans la file persistante.
- Correction du nom sélectionné à l’ouverture du dialogue et des notifications en petite fenêtre ; changement de bibliothèque refusé pendant un exercice.
- Tests Debian 12/13, DEB installés et AppImage puis publication directe dans GitHub Releases, sans archives Actions. Manifeste et SHA-256 vérifiés ; une version publiée n’est pas écrasée.

## File d’analyses persistante — 15 octobre 2026

- Analyse hors ligne en arrière-plan, toutes les branches ou ligne principale, profils et MultiPV, pause/reprise et récupération après plantage. Copie PGN indépendante de l’étude ouverte.
- Migration 7 sauvegardée, résultats et point de reprise atomiques, verrou par bibliothèque, pages de 200 résultats et cache borné à 10 000 positions. Identité SHA-256/options/ressources et historique complet pour la réutilisation ; ressources non identifiées exclues du cache.
- Export et ouverture d’une copie annotée, variantes légales et provenance, conservation des commentaires et de la fin réellement jouée.
- Scores UCI typés, WDL du moteur et compteurs ; scores malformés rejetés et suppression des fausses pertes en centipions lors des mats.
- Tests avec vrai Stockfish, moteur modifié, pause/reprise, processus tué, Chess960, pagination et étude éditée pendant l’analyse.
- Correction du plantage au redimensionnement après une comparaison UCI sur Qt 6.4 ; dialogue de file vérifié à 960 × 720 et exports empêchés d’écraser les verrous, même via un lien de bibliothèque.

## Import parallèle et stockage GitHub — 15 octobre 2026

- Parsing PGN avec 1 à 8 lecteurs, file bornée, restitution ordonnée sur le thread du consommateur et un seul écrivain SQLite. Préférence persistante accessible dans les imports reprenables.
- Notation SAN historique surspécifiée acceptée seulement pour un unique coup légal ; export canonique et refus des ambiguïtés/fausses captures.
- Benchmark sur de vrais PGN, contrôles des chemins de sortie, import interrompu/repris et sauvegarde vérifiée. Mesures de 10 000 parties complètes enregistrées dans PERFORMANCE, sans qualification abusive de millions de parties.
- 10 suites locales réussies ; test du benchmark activé en CI Debian 12/13.
- Nettoyage effectif de 58 archives Actions (8 371 207 728 octets), dix releases préservées ; suppression des archives des PR, conservation d'un jour pour les publications, purge automatique après succès et quotidienne.

## Préparation par position — 13 octobre 2026

- Statistiques locales combinant position, base, adversaire exact/couleur, période et Elo moyen connu des deux joueurs. Les répétitions ne gonflent pas les comptes.
- Calcul Qt en arrière-plan, annulation SQLite et remplacement des demandes périmées. Invalidation lorsque les données changent ; rejet d’une suite appliquée depuis une autre position.
- Parties d’exemple réelles, résultats Blancs/Nulles/Noirs et Elo issus des métadonnées, export CSV avec FEN et filtres.

## Répertoires par position — 13 octobre 2026

- Migration 6 sauvegardée : répertoires nommés Blancs/Noirs, classique/Chess960, variantes et transpositions partagées.
- Fusion des études et import PGN atomique, annotations/NAG, priorité des coups, liens vers les parties modèles et export PGN borné avec transpositions signalées.
- Suppression de branches sans perdre les positions encore accessibles ni leurs révisions.
- Répétition espacée par position acceptant les différentes réponses préparées, révision libre, suivi des rappels manqués et restauration de l’étude après entraînement. Les cartes existantes sont conservées.
- Réponses adverses manquantes calculées depuis la bibliothèque locale ; aucune nouveauté théorique déduite d’une simple absence.

## Imports reprenables — 13 octobre 2026

- Mode PGN par lots de 250, avec pause et reprise après redémarrage ou plantage ; conservation atomique des parties, liens de source et point de reprise. L’import classique reste entièrement annulable.
- SHA-256 du fichier complet vérifié avant chaque reprise ; fichier original déplaçable, fichier modifié refusé. Verrou interprocessus récupérable après arrêt brutal.
- Progression en octets et parties validées, débit et estimation du temps restant pendant l’import. Gestionnaire accessible depuis la bibliothèque et le menu.
- Migration 5 sauvegardée, tâches persistantes et historique unique à la fin ; retirer une tâche conserve les parties importées.

## Recherche asynchrone — 12 octobre 2026

- Recherches SQL en arrière-plan, annulables au niveau SQLite ; seule la dernière demande peut alimenter la liste.
- Pages de 200 résultats par curseur, retour à la page précédente, absence de page suivante fictive ; ordre stable avec égalités et valeurs absentes.
- Les familles d’ouvertures ne capturent plus les variantes homonymes d’une autre famille (ex. Franco-Sicilienne). Les filtres de base et d’ouverture restent actifs pendant la navigation. Les erreurs de recherche sont affichées sans conserver des résultats trompeurs.
- Dépendance de compilation `libsqlite3-dev` ajoutée à l’installation et à la CI Debian.

## En développement — moteurs et bases

- Options UCI découvertes au handshake et contrôlées selon leurs types et limites.
- Réglages propres à chaque profil, y compris les options du moteur ; les analyses désactivent la réduction de force.
- Répertoire de travail du moteur adapté aux réseaux NNUE et livres locaux.
- Catalogue de trois moteurs externes vérifiés (Stockfish, Arasan, PlentyChess), compatible avec le système détecté, progression/annulation et extraction protégée. Pas de lancement automatique.
- 42 collections PGN à la demande : champions, championnes disponibles, matchs historiques et ouvertures. Source et conditions explicites.
- Migration 4 sauvegardée : bases nommées, appartenance multiple sans doublons exacts, compteurs transactionnels, historique des imports et export PGN d’une base.
- Filtres combinant base, ouverture par nom/ECO, plage ECO, joueur/couleur, date, tournoi, Elo, matériel et position. Annuler une recherche avancée ne modifie plus les filtres actifs.
- Interface existante conservée.

## 0.3 — préversions de développement

Refonte de l’interface : navigation supérieure numérotée, accueil « carnet » avec position réelle, identité encre/papier/cuivre, typographie serif et notation monospace. Palette Trait64 ajoutée sans écraser les choix personnels. Commandes de l’échiquier regroupées et clavier désactivé pendant les menus ; focus visible des boutons. Géométrie et navigation testées à 960 × 640. Aucun changement de schéma ou de données pour cette refonte.

Mises à jour GitHub vérifiées (trois canaux), accès au dépôt privé sans clé intégrée, installation .deb/AppImage et sauvegarde préalable ; automatisation des tests et Releases. Sauvegardes périodiques en arrière-plan et rotation de 14 instantanés.

SQLite schéma 3 : recherche FTS5 trigramme avec repli LIKE, filtres exacts, compteurs et statistiques d’ouvertures incrémentaux, classement des joueurs indexé. Migration sauvegardée et transactionnelle.

Comparaison simultanée de 2 à 4 moteurs UCI, MultiPV indépendant, affichage des hits Syzygy, navigation sans variantes périmées, insertion de PV et budgets de mémoire/threads partagés.

Corrections : tri sémantique strict des versions, annulation de connexion GitHub CLI, propagation des statistiques après corbeille/restauration/réindexation, panneau de paramètres défilant et raccourcis bloqués pendant les dialogues.

## 0.2.0 alpha

Personnalisation des pièces/échiquier/fond ; Chess960 et éditeur de position ; annotations persistantes ; onglets et annuler/rétablir ; fusion/duplication ; bilan automatique et graphe ; cartes depuis erreurs ; profils UCI et configuration Syzygy ; pendules locales ; ECO ; fiches joueurs et recherche avancée ; imports/exports PGN en flux ; EPD, PNG et PDF.

Corrections ciblées pendant la vérification :

- Chemins des ressources SVG corrigés et chargement des 48 images testé.
- Dépendance explicite au greffon SVG séparé de Debian 13, y compris avec une installation sans paquets recommandés.
- Contraste des aperçus, du texte de fond et des boutons ajusté ; panneaux vérifiés sur petite fenêtre et navigation latérale défilante.
- Plantage de redimensionnement après actualisation des statistiques corrigé en conservant les composants des tableaux de bord.
- Résultats exacts du moteur conservés face à ses bornes intermédiaires ; réponses de bilan périmées ignorées après annulation.
- Restauration de la bonne position de variante et des annotations entre onglets/redémarrages ; orientation préservée après entraînement.
- Migration SQLite et réindexation dans une transaction, avec sauvegarde avant version 2 ; import annulé entièrement restauré.
- Écritures concurrentes vers la base empêchées pendant un import/réindexage.
- Export et sauvegarde protégés contre la base active, ses alias symboliques et ses journaux.
- Validation renforcée des FEN (matériel et droits de roque) et des opérations EPD.

Le format `.t64` reste expérimental et inchangé. Les détails du périmètre restant figurent dans ROADMAP.md ; cette version ne représente pas l’ensemble du catalogue initial.

## 0.1.0 alpha

Première application locale : règles classiques, variantes PGN, bibliothèque SQLite, analyse/jeu UCI, explorateur, cartes d’entraînement, sauvegardes et récupération de session. Compatibilité GCC 12 et paquets Debian 12/13.
