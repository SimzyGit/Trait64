# Reprise du développement — 19 octobre 2026

**État courant : [WORK-IN-PROGRESS.md](WORK-IN-PROGRESS.md).** Les PR #4 (stockage), #5 (imports parallèles), #6 (analyses persistantes) et #7 (répertoires asynchrones/publication sans archives Actions) sont intégrées à main. Les 11 suites et paquets passent sur Debian 12 et 13. La version distribuée est [v0.3.0-dev.46](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.46), vérifiée sans archive Actions. Le schéma reste 7. Les informations détaillées de la PR #3 ci-dessous décrivent l’historique ; suivre le point de reprise pour les tests et la publication actuels.

La PR #8 est également intégrée et publiée dans dev.46 : motifs tactiques et
repères PGN, 11 suites Debian 12/13 et paquets validés. Tag, cinq fichiers et
sélecteur de mises à jour vérifiés ; ne pas refaire ce lot ni reprendre un ancien checkout.

## État à conserver

Trait64 est une application de bureau C++23/Qt6/QML et SQLite, monolithe modulaire. Ne pas recommencer l’application. L’esthétique est en pause. Les fonctions locales ne nécessitent ni compte ni réseau. La liste détaillée des objectifs et les limites sont dans [ROADMAP.md](ROADMAP.md).

Le lot historique **PR #3 — imports reprenables, répertoires et préparation** a été intégré à main, puis prolongé par les PR #4 à #7. Il succédait à la PR #2 publiée dans `v0.3.0-dev.23`. Les modifications du README ajoutées séparément sur main (2873ed2 puis f85ee26) sont conservées. La version publiée courante est à vérifier dans les [Releases](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases), sans revenir à une ancienne copie locale.

### Historique du lot #3

- `libs/database/imports.cpp`, `app/imports.cpp`, `ui/Imports.qml` : schéma **5**, tâches PGN persistantes, SHA-256 complet avant reprise, verrou interprocessus, transactions de 250 parties comprenant le point de reprise, pause et fichier déplacé. L’import classique conserve son annulation intégrale.
- `libs/database/repertoires.cpp`, `app/repertoires.cpp`, `ui/Repertoires.qml` : schéma **6**, répertoires nommés Blancs/Noirs/classique/960, graphe de positions/transpositions, notes/NAG/priorités, modèles de parties et import/export PGN. Les branches supprimées de l’étude ne sont pas importées. La suppression d’une branche préserve les transpositions encore accessibles et leurs révisions.
- Entraînement de répertoire : plusieurs réponses préparées acceptées, calendrier par position, rappels manqués et restauration de l’étude. `trainingActive()` couvre les cartes existantes et les répertoires ; ne pas réutiliser `cardId_` comme identifiant de répertoire.
- Couverture : jusqu’à 100 réponses adverses absentes, observées dans la bibliothèque. Ce sont des lacunes du répertoire par rapport à ce corpus, pas des nouveautés théoriques ni des erreurs démontrées.
- `app/preparation.cpp`, `ui/Preparation.qml`, `Database::positionStatistics` : position, source, nom exact/couleur, période et moyenne des Elo connus ; worker annulable, générations contre les résultats périmés, exemples réels et CSV avec contexte. Une partie ne compte qu’une fois par coup même après répétition.

Les migrations existantes ne doivent pas être réécrites. Après le lot analyse (schéma 7), le prochain changement structurel nécessite une **migration 8 sauvegardée**. L’export de répertoire développe chaque position une fois pour borner les cycles ; les transpositions sont annotées et le graphe peut être réimporté. L’import de répertoire est atomique et limité à 16 Mio / 200 parties. La couverture et les imports/exports passent en arrière-plan dans le lot #7 ; certaines opérations d’édition restent synchrones.

### Validation de ce lot

Localement le 14 octobre : **9/9 suites réussies en 15,53 s**, Stockfish réel inclus. Tests : arrêt réel du processus au milieu d’un lot, reprise sans doublons, fichier déplacé/modifié, verrou récupéré, graphe/transpositions/cycles/Chess960, migration 1 → 6, notes et SRS, préparation filtrée/annulée, CSV, interfaces QML et sauvegardes. Une collision de nom avec la propriété finale `Dialog.result` a été corrigée dans le dialogue de préparation.

La [PR #3](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/3) conserve les résultats historiques. Pour chaque nouveau lot, vérifier la CI au SHA exact puis la publication de main et ses fichiers. Aucune de ces validations ne remplace un essai sur le PC Voyager de l’utilisateur.

## Prochaines étapes, dans l’ordre

1. **Qualification des gros corpus.** Importer des PGN réels de 1, 2, 5 puis 10 millions de parties avec protocole reproductible : durée, mémoire, taille, recherche, interruption et restauration. Le benchmark déjà livré porte sur un million de lignes de métadonnées synthétiques, pas sur autant de parties complètes. Optimiser à partir des mesures : import parallèle borné, indexation progressive, modèles Qt incrémentaux, export et migrations en arrière-plan. Voir PERFORMANCE.
2. **Explications vérifiables et analyse.** La file persistante, toutes les branches et le cache avec provenance sont livrés ; le bilan compare désormais la réponse au coup joué et la proposition antérieure avec des faits légaux et un compte matériel (MOVE-EXPLANATIONS). Approfondir les motifs tactiques et la justification des alternatives. Détection tactique appuyée par des séquences légales et vérification moteur ; ne pas inventer de plans ou de niveau Elo. Les options Syzygy actuelles sont transmises aux moteurs ; un module local dédié et ses tests restent à développer.
3. **Répertoires approfondis.** Après le lot #7 (imports/exports/couverture asynchrones et analyse moteur des branches), poursuivre la couverture probabiliste, la comparaison de répertoires et les tendances adverses. Distinguer explicitement absence dans un corpus et nouveauté théorique.
4. **Sources et formats.** Import distant volontaire, formats ouverts supplémentaires et Polyglot, exploration SCID/ChessBase selon droits et documentation. Les 42 collections optionnelles actuelles sont des téléchargements PGN Mentor, pas un corpus redistribué sous licence ouverte. Ne pas annoncer une collection exhaustive de chaque championne.
5. **Entraînement.** Corpus tactique licencié, solutions équivalentes vérifiées par moteur, finales évaluées correctement, séances de calcul et visualisation, progression par thème. Le SRS de répertoire actuel accepte les réponses enregistrées ; il n’évalue pas les autres coups échiquéennement.
6. **Outils avancés.** Rapports PDF de répertoire/faiblesses, chapitres et outils pédagogiques, reconnaissance de diagrammes par modèle local licencié avec validation de la FEN. Les exports PDF/PNG d’étude existent déjà ; aucun bouton OCR factice n’est présent.
7. **Fiabilité et plateformes.** Restauration guidée de tout le dossier utilisateur ; sauvegarde pré-mise à jour et installation entièrement asynchrones ; essais Voyager X11/Wayland, GPU, GitHub privé et Polkit. Signature indépendante des releases uniquement lorsqu’une clé de maintenance est gérée explicitement. Windows/macOS/Flatpak restent non validés.

## Points d’architecture et distribution

- `libs/updates` : sélection SemVer et canaux, authentification privée facultative, réseau asynchrone, contrôles taille/SHA-256/hôte, remplacement atomique AppImage et installation Debian via pkexec/apt-get.
- `app/reliability.cpp` : sauvegardes périodiques, rotation et pré-mise à jour ; sessions atomiques et conservation des fichiers illisibles. Préserver les données utilisateur lors d’une mise à jour.
- `libs/database/scaling.cpp` : index/FTS5 trigramme facultatif et agrégats transactionnels (schéma 3). `sources.cpp` : bases et historique (schéma 4). `imports.cpp` et `repertoires.cpp` : schémas 5/6.
- `libs/engine/comparison.*` : 2–4 moteurs, budgets CPU/Hash partagés et PV liées à leur FEN. Profils/options issus du handshake, catalogue de versions épinglées. PlentyChess 8 exige glibc 2.38 ; respecter les minima du catalogue.
- `ui/Main.qml` conserve les sept espaces et leurs index. `ui/Home.qml`, `TButton`, `TTabButton` et les préférences personnelles sont conservés. Ne pas réintroduire une refonte esthétique dans ce lot.
- `packaging/` : DEB Debian 12/13, AppImage, inventaire des bibliothèques/notices et sources correspondantes distribuées. Moteurs et tablebases externes.

## Vérifier et publier

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Dans l’environnement de développement contraint, compiler avec **`--parallel 1`** et ne jamais modifier C++/CMake pendant une compilation. Le cache automoc hérité d’une interruption peut nécessiter une régénération. Stockfish doit être présent : une suite moteur ignorée n’est pas un succès. Tests GUI sans écran : `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`. Voir TESTING pour les détails.

La CI construit Debian 12/13, teste les paquets installés et l’AppImage, les sanitizers du cœur et rassemble les sources correspondantes avant publication. Les commits ordinaires sur main publient uniquement des **préversions Développement**. Stable/Bêta demandent un tag ou lancement explicite. Ne jamais désactiver une empreinte AppImage pour faire passer la CI ; vérifier une nouvelle archive amont avant de changer son identifiant/empreinte.

Après une version stable, augmenter la version de base CMake. À base égale, SemVer classe `beta` avant `dev` : ne pas rétrograder automatiquement. Les versions 0.1/0.2 nécessitent un amorçage manuel vers 0.3. Ne jamais intégrer un jeton GitHub au programme.
