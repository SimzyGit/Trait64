# Reprise du développement après 0.3

## État fonctionnel à conserver

Application C++23/Qt6/QML, monolithe modulaire. Les fonctions 0.1/0.2 restent présentes. La branche `main` contient le système de mise à jour, la distribution Linux et les modules ajoutés en 0.3. Les commits ordinaires sur main sont publiés uniquement comme préversions `X.Y.Z-dev.N` après toutes les validations. Ne pas réécrire l’application ni remplacer les contrôles existants par des maquettes.

- `libs/updates` : sélection SemVer et canaux, authentification privée facultative, réseau asynchrone, contrôles taille/SHA-256/hôte, remplacement atomique AppImage et installation Debian via pkexec/apt-get.
- `app/reliability.cpp` : sauvegarde pré-mise à jour, sauvegardes périodiques et rotation. Le contrôleur restaure les sessions et conserve les fichiers illisibles.
- `libs/database/scaling.cpp` : migration sauvegardée vers le schéma 3, index, FTS5 trigramme facultatif et agrégats SQL transactionnels. Ajouter une migration version 4 pour les futurs changements, sans modifier rétroactivement le schéma 3 publié.
- `libs/engine/comparison.*`, `ui/CompareEngines.qml` : 2–4 processus, budgets partagés, PV associées à la FEN, navigation et insertion.
- `packaging/` : AppImage, inventaire des bibliothèques et notices, sources Debian correspondantes distribuées à côté du binaire. Stockfish et les tablebases sont externes.

## Interface du 11 octobre 2026

`ui/Home.qml` contient l’accueil carnet et ses signaux vers Main. `ui/Main.qml` conserve les sept pages et leurs index, avec navigation numérotée en haut. `TButton` et `TTabButton` portent les contrôles communs. Les commandes de position sont dans le menu sous l’échiquier ; paramètres, mises à jour et aide dans le menu principal. Ne pas réintroduire la barre latérale sans décision de design.

Les préférences sauvegardées priment sur les nouveaux défauts (palette Trait64, fond encre/papier, accent cuivre adapté à la luminance). Aucune migration n’est nécessaire. Le monogramme de lancement reste provisoire et inchangé.

La capture `--smoke-test` accepte `T64_SCREENSHOT_PAGE=0..6` et `T64_SCREENSHOT_THEME=light` en plus de la taille et du chemin ; elle utilise des données temporaires. Le test de navigation recherche les délégués dans l’arbre visuel Qt Quick : ils ne sont pas nécessairement descendants QObject de la fenêtre.

## Prochaines étapes concrètes

1. **Gestionnaire de moteurs** : conserver la comparaison 2–4 UCI déjà livrée, mais ajouter un véritable inventaire de moteurs installés et des profils indépendants par moteur. Chaque profil doit conserver binaire, version, Hash, Threads, MultiPV, Syzygy, limites et options UCI. Ajouter un catalogue téléchargeable vérifié (provenance, licence, plateforme, SHA-256) et ne démarrer aucun moteur tant que l’utilisateur ne le demande. Candidats initiaux : Stockfish, Arasan et PlentyChess, uniquement si leur licence et leur mode de distribution sont compatibles.
2. **Gestionnaire de bases et collections** : permettre plusieurs bases locales, sélection/activation claire, import en arrière-plan, reprise et métadonnées de provenance. Une partie peut appartenir à plusieurs collections tout en n’étant stockée qu’une fois. Préparer des packs optionnels de parties, notamment par champion du monde, avec licence/source explicite, empreinte, version, déduplication et historique de mise à jour.
3. **Recherche par ouverture et filtres combinés** : étendre les filtres existants pour combiner ECO, nom d’ouverture/variante, joueur, couleur, événement, date, Elo, résultat, collection et position. Prévoir des filtres enregistrables et une recherche annulable sur les gros corpus ; tester les combinaisons et les index avant de complexifier l’interface.
4. **Répertoires** : entités personnelles nom/camp, liens vers études et positions, choix principal/alternatives, couverture des réponses adverses, transpositions et lacunes. Migration 4 sauvegardée ; tests sur branches et transpositions avant interface. Ne pas confondre une collection de parties avec un répertoire complet.
5. **Entraînement** : modèles de cartes par thème, apprentissage de répertoire par position, solutions équivalentes validées par moteur, évaluation adaptée aux finales, séances de calcul et visualisation, suivi par thème. Les cartes actuelles valident seulement une ligne connue, avec calendrier simple.
6. **Gros corpus** : qualifier un import de PGN réels longs avec plusieurs millions de parties ; mesurer temps, mémoire, taille et restauration. Ajouter modèles Qt incrémentaux, recherche annulable en arrière-plan, pagination par curseur, reprise d’import et progression de migration. Le benchmark livré n’est pas cette qualification.
7. **Analyse** : branches complètes, file de tâches persistante, budgets adaptatifs, options UCI exposées à partir du handshake, tablebases locales dédiées. Préparer l’adversaire par couleur et position ; distinguer absence dans la base et vraie nouveauté théorique.
8. **Outils avancés** : rapport PDF de faiblesses/répertoire, cours/annotations pédagogiques, reconnaissance de diagramme par modèle local correctement licencié avec validation manuelle de la FEN. Aucun bouton OCR factice n’est présent.
9. **Fiabilité** : sauvegarde pré-mise à jour hors thread UI, restauration guidée bibliothèque/session/préférences/pièces personnelles, tests d’installation sur le PC Voyager en X11/Wayland. Ajouter une signature de release si une clé de maintenance est gérée explicitement.

## Vérifier avant de publier

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Stockfish doit être installé pour ne pas ignorer la suite moteur. Tests sans interface physique : `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`. Le pipeline teste Debian 12/13, les paquets installés, l’AppImage et les sanitizers avant publication. Un tag stable reste une décision explicite. Après une version stable, augmenter la version de base dans CMake pour que les préversions suivantes soient sémantiquement plus récentes. À base égale, SemVer classe `beta` avant `dev` : on ne rétrograde pas automatiquement de dev vers beta.

Les outils AppImage sont épinglés par empreinte. Si une release continue amont remplace ses assets, actualiser les identifiants et SHA-256 après vérification des sources ; ne jamais désactiver le contrôle pour faire passer la CI. Le téléchargement des sources LGPL/GPL embarquées fait partie de la publication.

Pour les 0.1/0.2 déjà installées, amorçage manuel obligatoire : elles ne possèdent pas le module de mise à jour. Le dépôt privé nécessite le compte GitHub de l’utilisateur ; ne jamais intégrer de jeton au programme.
