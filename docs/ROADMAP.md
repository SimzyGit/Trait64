# Périmètre livré et travaux restants

La version 0.3 est une nouvelle étape de l’application locale. Elle n’achève pas le catalogue d’environ 245 fonctions envisagé pour Trait64. Les fonctions ci-dessous sont celles réellement présentes dans le code.

| Domaine | Fonctions présentes après 0.3 | Travaux restants |
|---|---|---|
| Apparence | 4 jeux SVG + Unicode, import SVG personnel, 8 palettes, couleurs des cases/fond/repères, préférences persistantes | Textures, animations avancées, logo final |
| Échiquier | Chess960, 960 départs numérotés, éditeur graphique FEN, flèches/cercles persistants | Autres variantes |
| Études | 16 onglets restaurables, annuler/rétablir, duplication, fusion d’arbres, diagramme PNG, étude PDF | Chapitres, références et documents attachés, mise en page avancée |
| Fichiers | EPD classique, import PGN et export de bibliothèque par flux | Polyglot, SCID, ChessBase, T64 compact Move16/TLV/BLAKE3 |
| Bibliothèque | Schéma 3, FTS5, index variantes/matériel/Elo/joueurs, compteurs et arbres incrémentaux, filtres exacts, pagination, doublons, réindexation annulable | Qualification sur des corpus PGN réels de plusieurs millions de parties, import par lots reprenable, modèles Qt incrémentaux, dédoublonnage avec choix de fusion, recherche structurelle fine |
| Analyse | Bilan principal, graphe, pertes en centipions, annotations, profils, comparaison simultanée de 2 à 4 UCI, MultiPV, profondeur et Syzygy | Analyse des branches, budgets adaptatifs, options UCI arbitraires, interface de tablebases dédiée |
| Ouvertures | Dictionnaire ECO CC0 embarqué, balises depuis une position, fusion d’études | Couverture/lacunes de répertoire, nouveautés, livres d’ouverture |
| Entraînement | Cartes depuis erreurs, activité quotidienne/hebdomadaire, objectif | Solutions équivalentes, cours de finales, agenda détaillé, certification tactique |
| Jeu local | Deux humains ou moteur, pendule avec incrément, abandon/réclamation de nulle | Moteur-moteur, périodes multiples, handicap et arbitrage complet |
| Préparation | Fiches joueurs, résultats par couleur, ouvertures, dernières parties | Styles d’adversaires, préparation et gestion de tournois |
| Interface | 7 espaces, personnalisation, petites fenêtres à partir de 960 × 640 | Panneaux détachables, raccourcis personnalisables, traductions complètes |
| Plateforme | Paquets Debian 12/13 amd64, AppImage, installateur source, tests CI et Releases automatisées | Qualification sur le PC Voyager cible, Flatpak, Windows/macOS, API/plugins |
| Fiabilité | Sauvegardes périodiques, avant mise à jour/migration, corbeille, session atomique et préservation des sessions illisibles | Restauration guidée de tout le dossier utilisateur, installation et sauvegarde pré-mise à jour entièrement asynchrones |
| Réseau | Vérification facultative GitHub Releases, trois canaux, SHA-256, notification, téléchargement et installation | Signature indépendante du mainteneur, import distant volontaire, partage ; jeu en ligne exclu du périmètre |
| Fonctions avancées | Études PDF/PNG, bilan moteur, exercices issus des erreurs | Reconnaissance de diagrammes depuis images, rapports de répertoire, détection de faiblesses, cours pédagogiques complets |

Priorité suivante : répertoires structurés et entraînement, qualification de gros corpus, puis reconnaissance de positions et rapports approfondis. Voir [NEXT-STEPS.md](NEXT-STEPS.md) pour reprendre sans recréer l’application. Conserver PGN et les sauvegardes SQLite comme sorties récupérables pendant l’évolution du format T64 expérimental.
