# Changements

## 0.3 — préversions de développement

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
