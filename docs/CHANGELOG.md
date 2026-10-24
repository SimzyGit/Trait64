# Changements

## Optimisation des imports — en cours, 23 octobre 2026

- Requête de doublons bornée par l'index des empreintes existant, sans changement du schéma 7.
- Clés matérielles identiques aux anciennes, calculées sans compiler une expression régulière à chaque position ; même normalisation dans la recherche.
- Tests du travail SQLite et des clés persistées, captures, variantes, promotions, prise en passant, Chess960 et annulation finale de réindexation.
- Benchmark enrichi : restauration réelle, réindexation, lectures concurrentes, distributions et mémoire. Mode Qt opt-in pour mesurer les événements/navigation pendant un import réel.
- Comparaison complète conservée sur 10 000 parties ; validation 100 000 et distribution encore en cours. Aucun corpus ou fichier SQLite ajouté au dépôt.

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
