# Périmètre livré et travaux restants

La vision de Trait64 est un environnement complet d’analyse, de préparation et d’entraînement. La version 0.1 réalise un ensemble local utilisable ; elle n’achève pas le catalogue d’environ 245 fonctions envisagé.

| Domaine | Livré en 0.1 | Suite prévue, non implémentée |
|---|---|---|
| Échiquier | Règles classiques, coups spéciaux, clic/glisser, promotion, FEN, flèches temporaires | Chess960, éditeur graphique de positions, annotations persistantes |
| Études | Arbre, variantes, commentaires, NAG, métadonnées, session récupérable | Onglets, chapitres, images, références, mise en page/impression |
| Fichiers | PGN multi-parties/variantes, FEN, archive T64 expérimentale | EPD, Polyglot, formats SCID/ChessBase, T64 compact final |
| Bibliothèque | SQLite, index positions principales, recherche, favoris, collections, corbeille | Indexation massive incrémentale, recherche matérielle/structurelle, fusion intelligente, doublons positionnels |
| Analyse | UCI, MultiPV, score Blancs, lignes insérables, profondeur, nœuds/s | Analyse de partie, graphe, classification d’erreurs, tablebases, plusieurs moteurs, budgets CPU adaptatifs |
| Ouvertures | Explorateur de base locale, statistiques, cartes tirées des lignes | Détection ECO automatique, fusion de répertoires, lacunes/couverture, nouveautés, livres d’ouverture |
| Entraînement | Cartes de lignes, réponse automatique, progression, intervalles 1/2/4… jours | Génération de tactiques depuis erreurs, solutions équivalentes, finales spécialisées, objectifs, agenda |
| Jeu local | Adversaire UCI, force, temps fixe, camp au choix | Pendule complète, handicap, parties moteur-moteur, annotations automatiques |
| Préparation | Recherche par joueur et position | Fiches adversaires, styles, statistiques fines, préparation de tournoi |
| Interface | Six espaces, clair/sombre, séparateur redimensionnable, raccourcis, commandes | Panneaux détachables, presets, raccourcis personnalisables, traductions complètes, logo final |
| Plateforme | Installation utilisateur Debian/Ubuntu, CMake, tests, CI | AppImage/Flatpak, validation Windows/macOS, mises à jour, API/plugins |
| Réseau | Aucun requis | Import distant volontaire, partage d’études, communauté ; jeu en ligne exclu du périmètre |

Priorités techniques : fiabiliser l’alpha sur le PC cible ; enrichir les tests de flux ; rendre la notation et l’indexation incrémentales ; concevoir et versionner le véritable stockage compact avant toute promesse de compatibilité .t64 durable. Conserver PGN/SQLite comme sorties récupérables pendant cette évolution.
