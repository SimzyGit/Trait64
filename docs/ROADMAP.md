# Périmètre livré et travaux restants

La version 0.3 est une nouvelle étape de l’application locale. Elle n’achève pas le catalogue d’environ 245 fonctions envisagé pour Trait64. Les fonctions ci-dessous sont celles réellement présentes dans le code.

## Prochain cycle de travail — cible 0.4

La prochaine étape doit partir de l’état 0.3 existant, sans réécriture. Les éléments ci-dessous sont **planifiés mais pas encore livrés dans `main`** :

1. **Gestionnaire de moteurs** : plusieurs moteurs installables et configurables séparément, avec profils indépendants (binaire, Hash, Threads, MultiPV, limites, Syzygy et options UCI). Prévoir un catalogue local de moteurs téléchargeables avec provenance, version, licence, plateforme, empreinte SHA-256 et validation avant installation. Les premiers candidats étudiés sont Stockfish, Arasan et PlentyChess, sous réserve de compatibilité de licence et de distribution.
2. **Gestionnaire de bases** : plusieurs bases locales ouvrables/activables indépendamment, import et mise à jour par lots, métadonnées de provenance et empreintes. Une partie doit pouvoir appartenir à plusieurs collections sans duplication physique.
3. **Collections historiques** : packs optionnels de parties, notamment des collections dédiées aux champions du monde, avec source, licence, date de génération, déduplication et historique d’import. Aucun corpus ne doit être embarqué sans droit de redistribution explicite.
4. **Recherche d’ouverture plus précise** : filtres combinables par ECO, nom d’ouverture/variante, joueur, couleur, date, Elo, événement, résultat, collection et position ; recherche annulable en arrière-plan sur les gros corpus.
5. **Reprise du catalogue fonctionnel** : après ce bloc, continuer les répertoires structurés, l’entraînement avancé, la qualification multi-millions de parties, l’analyse approfondie et les outils de préparation déjà listés plus bas.

Cette cible est un jalon de développement, pas une promesse de numéro de version stable. Une publication 0.4 ne doit être préparée qu’après tests réels, documentation des licences et validation des paquets Linux.

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

Priorité suivante : gestionnaire de moteurs et de bases, collections historiques/champions, filtres d’ouverture combinés, puis répertoires structurés et entraînement, qualification de gros corpus, reconnaissance de positions et rapports approfondis. Voir [NEXT-STEPS.md](NEXT-STEPS.md) et [NEXT-RELEASE.md](NEXT-RELEASE.md) pour reprendre sans recréer l’application. Conserver PGN et les sauvegardes SQLite comme sorties récupérables pendant l’évolution du format T64 expérimental.
