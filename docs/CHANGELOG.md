# Changements

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
