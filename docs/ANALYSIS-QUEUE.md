# Analyses persistantes

Depuis l’espace Analyse, ouvrir le menu `⋮` puis **Analyses en arrière-plan**. Choisir le moteur courant ou un profil enregistré, le temps par position et la portée (ligne principale ou toutes les variantes), puis **Ajouter à la file**. Chaque tâche conserve une copie PGN indépendante : déplacer des pièces, modifier une note ou fermer l’étude n’altère pas son calcul.

Les tâches sont traitées dans l’ordre d’ajout, par un moteur à la fois. Le nombre de threads, la mémoire Hash, le MultiPV et les options viennent du profil choisi. La réduction de force et la réflexion pendant le temps adverse sont désactivées pour cette analyse. Les fonctions principales ne nécessitent aucune connexion.

**Pause** arrête la recherche en cours en conservant les positions entièrement analysées. **Reprendre** recalcule la position interrompue puis continue. **Tout suspendre** met aussi les tâches en attente en pause. À la fermeture du logiciel, la tâche active est suspendue. Après un plantage, une tâche encore marquée « en cours » devient reprenable ; les tâches déjà en attente peuvent démarrer. Un verrou par bibliothèque empêche deux instances de traiter simultanément la même file.

Les résultats affichent les variantes légales reçues, les évaluations du point de vue des Blancs, la profondeur et, si le moteur les fournit, les probabilités WDL. Ces probabilités décrivent le modèle du moteur, pas le niveau du joueur. Les mats sont conservés comme des distances de mat : ils ne deviennent jamais des pertes artificielles en centipions dans le bilan.

**Ouvrir une copie** crée une nouvelle étude annotée. **Exporter PGN** produit également un fichier partiel si la tâche n’est pas terminée. Les commentaires, annotations et coups d’origine restent présents. Les évaluations utilisent `[%eval ...]`. Les PV sont décrites dans les commentaires et ajoutées comme variantes lorsqu’elles ne prolongent pas la ligne jouée ; aucune suite moteur ne remplace la fin réelle de la partie. Des tags indiquent le moteur, son SHA-256, les réglages et le nombre de positions traitées.

## Reprise et cache

L’identité enregistrée comprend le SHA-256 du binaire, les options UCI effectivement déclarées et les empreintes des fichiers de réseau accessibles. Une reprise avec un moteur ou un réseau modifié est refusée : créer une nouvelle tâche permet de conserver séparément les anciennes conclusions.

**Vider le cache** retire les évaluations réutilisables sans effacer les résultats de la file. Les pages SQLite libérées pourront être réutilisées par les prochains enregistrements. Le cache est limité à 10 000 positions. Sa clé comprend l’identité et les paramètres du moteur, la durée, la variante, la FEN complète et toute la suite de coups depuis la racine. Deux positions identiques atteintes par des historiques différents ne sont pas confondues. Les résultats réutilisés portent la mention « cache ».

Le cache est désactivé lorsque des ressources externes ne peuvent pas être identifiées, notamment des tables Syzygy ou un réseau annoncé mais introuvable sur disque. Un réseau NNUE incorporé au moteur n’est pas présumé identique à un fichier que le moteur pourrait charger ailleurs. L’analyse reste disponible dans ce cas. Le logiciel n’efface jamais automatiquement des analyses terminées : la file conserve au maximum 500 tâches, à exporter puis retirer au besoin. Les résultats sont affichés par pages de 200 positions ; les études soumises sont limitées à 16 Mio et 100 000 positions.

## Organisation du code

- `libs/database/analyses.cpp` : migration 7 avec sauvegarde préalable, tâches et copies PGN, résultats, cache et transaction résultat/point de reprise.
- `libs/analysis/queue.*` : worker Qt, connexions SQLite et UCI propres au thread, verrou, états, empreintes et export.
- `app/analyses.cpp` et `ui/AnalysisQueue.qml` : profils, copie de l’étude, commandes, résultats et export.
- `tests/analysis_tests.cpp` : Stockfish réel, MultiPV, cache, variantes, pause/reprise, moteur modifié, processus réellement tué et migration 6 → 7.

Le bilan existant et la comparaison simultanée de plusieurs moteurs restent accessibles. Les budgets adaptatifs, le traitement de bases entières et les explications tactiques justifiées constituent des étapes distinctes de la feuille de route.

Référence UCI : [documentation officielle Stockfish](https://official-stockfish.github.io/docs/stockfish-wiki/UCI-Protocol-and-Stockfish-Commands.html).
