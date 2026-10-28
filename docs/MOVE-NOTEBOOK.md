# Carnet de coups côte à côte

Lot en cours, branche `ui/paired-move-notebook`, PR #14. Base : main
`5b41204047375d91f1e580756addd9879d59d7a4` / dev.64.

Le carnet de l'espace Analyse présente un numéro, le demi-coup blanc et le
demi-coup noir. Chaque cellule conserve son identifiant de nœud. Les paires
suivent exclusivement les relations parent/enfant de la même branche ; les
variantes sont des blocs distincts indentés. Les numéros et couleurs proviennent
de la position précédant le coup. Un départ FEN avec les Noirs garde une cellule
Blancs vide, et une fin après le coup blanc garde une cellule Noirs vide.

Les commentaires restent associés à leur demi-coup et occupent toute la largeur
avec retour à la ligne. La position initiale reste accessible. Les NAG restent
dans la cellule correspondante. La sélection utilise une bordure d'accent et une
police grasse. Le défilement suit la zone du coup, sans chercher à faire entrer
un commentaire long dans le viewport. Un rafraîchissement sans changement de
position ne doit pas ramener automatiquement le lecteur au coup courant.

La notation française traduit uniquement les initiales de pièces et promotions
à l'affichage. Les données internes, PGN et exports existants gardent le SAN
standard. `notation` reste disponible pour l'export PDF ; le nouveau modèle est
`moveRows`. Schéma SQLite 7 inchangé.

## Vérifications à effectuer

`ctest --test-dir build -R move_notebook --output-on-failure` : variantes
imbriquées, paires, numéros, FEN noir, NAG, commentaires, clics, navigation,
préférences, sauvegarde/réouverture et PGN standard. Définir
`T64_NOTEBOOK_SCREENSHOTS=/chemin/captures` pour capturer six configurations,
dont 960 × 640 à texte 150 % / interface 125 %, clair/sombre et contraste élevé.
Les autres suites restent requises, dont `desktop_layout` et
`desktop_interactions` avec Stockfish réel.

Les résultats seront consignés après exécution ; cette description n'est pas
une preuve de réussite. Aucun benchmark 100k n'est relancé pour ce lot.

## Vérification manuelle sur Voyager / Wayland

1. Sauvegarder une étude puis ouvrir un PGN avec variantes et commentaires.
2. Dans Analyse, cliquer alternativement les colonnes Blancs et Noirs : vérifier
   la position et la bordure du coup courant sur chaque branche.
3. Parcourir avec Gauche/Droite et Début/Fin ; revenir à la position initiale.
4. Réduire à 960 × 640, augmenter texte à 150 % et interface à 125 % ; vérifier
   colonnes, commentaires, barre de défilement et accès aux actions.
5. Changer langue, thème et contraste sans enregistrer : vérifier que la position
   et les commentaires restent intacts.
6. Ajouter une variante, un commentaire et une annotation, enregistrer puis
   rouvrir ; comparer le PGN exporté.
7. Démarrer/arrêter Stockfish et changer de position, vérifier le focus clavier.
8. Tester le défilement au pavé tactile et à la molette sur le matériel réel.

Les captures hors écran ne valident ni le GPU, ni Wayland, ni le matériel cible.
