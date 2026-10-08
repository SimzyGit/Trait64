# Périmètre livré et travaux restants

La version 0.2 est une application locale enrichie. Elle n’achève pas le catalogue d’environ 245 fonctions envisagé pour Trait64. Les fonctions ci-dessous sont celles réellement présentes dans le code.

| Domaine | Livré en 0.2, en plus de 0.1 | Travaux restants |
|---|---|---|
| Apparence | 4 jeux SVG + Unicode, import SVG personnel, 8 palettes, couleurs des cases/fond/repères, préférences persistantes | Textures, animations avancées, logo final |
| Échiquier | Chess960, 960 départs numérotés, éditeur graphique FEN, flèches/cercles persistants | Autres variantes |
| Études | 16 onglets restaurables, annuler/rétablir, duplication, fusion d’arbres, diagramme PNG, étude PDF | Chapitres, références et documents attachés, mise en page avancée |
| Fichiers | EPD classique, import PGN et export de bibliothèque par flux | Polyglot, SCID, ChessBase, T64 compact Move16/TLV/BLAKE3 |
| Bibliothèque | Schéma 2, index des variantes/matériel/Elo, filtres, pagination, doublons de ligne principale, réindexation annulable | Indexation massive incrémentale, dédoublonnage intelligent avec choix de fusion, recherche structurelle fine |
| Analyse | Bilan principal, graphe, pertes en centipions, annotations, profils moteur, profondeur et chemin Syzygy | Comparaison simultanée de moteurs, analyse des branches, budgets adaptatifs, interface de tablebases dédiée |
| Ouvertures | Dictionnaire ECO CC0 embarqué, balises depuis une position, fusion d’études | Couverture/lacunes de répertoire, nouveautés, livres d’ouverture |
| Entraînement | Cartes depuis erreurs, activité quotidienne/hebdomadaire, objectif | Solutions équivalentes, cours de finales, agenda détaillé, certification tactique |
| Jeu local | Deux humains ou moteur, pendule avec incrément, abandon/réclamation de nulle | Moteur-moteur, périodes multiples, handicap et arbitrage complet |
| Préparation | Fiches joueurs, résultats par couleur, ouvertures, dernières parties | Styles d’adversaires, préparation et gestion de tournois |
| Interface | 7 espaces, personnalisation, petites fenêtres à partir de 960 × 640 | Panneaux détachables, raccourcis personnalisables, traductions complètes |
| Plateforme | Paquets Debian 12/13 amd64, installateur source Debian/Ubuntu/Voyager, tests et CI | AppImage/Flatpak, Windows/macOS, mises à jour intégrées, API/plugins |
| Réseau | Aucun service requis | Import distant volontaire, partage, communauté ; jeu en ligne exclu du périmètre |

Priorité suivante : retours sur le PC cible et corpus représentatifs, modèles Qt incrémentaux, amélioration de l’analyse et du répertoire. Conserver PGN et les sauvegardes SQLite comme sorties récupérables pendant l’évolution du format T64 expérimental.
