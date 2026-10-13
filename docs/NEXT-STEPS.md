# Reprise du développement après 0.3

## État fonctionnel à conserver

Application C++23/Qt6/QML, monolithe modulaire. Les fonctions 0.1/0.2 restent présentes. La branche `main` contient le système de mise à jour, la distribution Linux et les modules ajoutés en 0.3. Les commits ordinaires sur main sont publiés uniquement comme préversions `X.Y.Z-dev.N` après toutes les validations. Ne pas réécrire l’application ni remplacer les contrôles existants par des maquettes.

- `libs/updates` : sélection SemVer et canaux, authentification privée facultative, réseau asynchrone, contrôles taille/SHA-256/hôte, remplacement atomique AppImage et installation Debian via pkexec/apt-get.
- `app/reliability.cpp` : sauvegarde pré-mise à jour, sauvegardes périodiques et rotation. Le contrôleur restaure les sessions et conserve les fichiers illisibles.
- `libs/database/scaling.cpp` : migration sauvegardée vers le schéma 3, index, FTS5 trigramme facultatif et agrégats SQL transactionnels. Le schéma 4 ajoute les sources et leur historique dans `sources.cpp`. Prévoir une migration 5 pour les prochains changements, sans modifier les schémas publiés.
- `libs/engine/comparison.*`, `ui/CompareEngines.qml` : 2–4 processus, budgets partagés, PV associées à la FEN, navigation et insertion.
- `packaging/` : AppImage, inventaire des bibliothèques et notices, sources Debian correspondantes distribuées à côté du binaire. Stockfish et les tablebases sont externes.

## Interface du 11 octobre 2026

`ui/Home.qml` contient l’accueil carnet et ses signaux vers Main. `ui/Main.qml` conserve les sept pages et leurs index, avec navigation numérotée en haut. `TButton` et `TTabButton` portent les contrôles communs. Les commandes de position sont dans le menu sous l’échiquier ; paramètres, mises à jour et aide dans le menu principal. Ne pas réintroduire la barre latérale sans décision de design.

Les préférences sauvegardées priment sur les nouveaux défauts (palette Trait64, fond encre/papier, accent cuivre adapté à la luminance). Aucune migration n’est nécessaire. Le monogramme de lancement reste provisoire et inchangé.

La capture `--smoke-test` accepte `T64_SCREENSHOT_PAGE=0..6` et `T64_SCREENSHOT_THEME=light` en plus de la taille et du chemin ; elle utilise des données temporaires. Le test de navigation recherche les délégués dans l’arbre visuel Qt Quick : ils ne sont pas nécessairement descendants QObject de la fenêtre.

## Prochaines étapes concrètes

1. **Répertoires** : entités personnelles nom/camp, liens vers études et positions, choix principal/alternatives, couverture des réponses adverses, transpositions et lacunes. Migration 5 sauvegardée ; tests sur branches et transpositions avant interface. Ne pas confondre une collection de parties avec un répertoire complet.
2. **Entraînement** : modèles de cartes par thème, apprentissage de répertoire par position, solutions équivalentes validées par moteur, évaluation adaptée aux finales, séances de calcul et visualisation, suivi par thème. Les cartes actuelles valident seulement une ligne connue, avec calendrier simple.
3. **Gros corpus** : qualifier un import de PGN réels longs avec plusieurs millions de parties ; mesurer temps, mémoire, taille et restauration. La recherche annulable et les pages par curseur sont livrées. Ajouter modèles Qt incrémentaux, reprise d’import et progression de migration. Le benchmark livré n’est pas cette qualification.
4. **Analyse** : branches complètes, file de tâches persistante, budgets adaptatifs, tablebases locales dédiées. Les options UCI issues du handshake sont désormais exposées et mémorisées par profil. Préparer l’adversaire par couleur et position ; distinguer absence dans la base et vraie nouveauté théorique.
5. **Outils avancés** : rapport PDF de faiblesses/répertoire, cours/annotations pédagogiques, reconnaissance de diagramme par modèle local correctement licencié avec validation manuelle de la FEN. Aucun bouton OCR factice n’est présent.
6. **Fiabilité** : sauvegarde pré-mise à jour hors thread UI, restauration guidée bibliothèque/session/préférences/pièces personnelles, tests d’installation sur le PC Voyager en X11/Wayland. Ajouter une signature de release si une clé de maintenance est gérée explicitement.

## Vérifier avant de publier

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Stockfish doit être installé pour ne pas ignorer la suite moteur. Tests sans interface physique : `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`. Le pipeline teste Debian 12/13, les paquets installés, l’AppImage et les sanitizers avant publication. Un tag stable reste une décision explicite. Après une version stable, augmenter la version de base dans CMake pour que les préversions suivantes soient sémantiquement plus récentes. À base égale, SemVer classe `beta` avant `dev` : on ne rétrograde pas automatiquement de dev vers beta.

Les outils AppImage sont épinglés par empreinte. Si une release continue amont remplace ses assets, actualiser les identifiants et SHA-256 après vérification des sources ; ne jamais désactiver le contrôle pour faire passer la CI. Le téléchargement des sources LGPL/GPL embarquées fait partie de la publication.

Pour les 0.1/0.2 déjà installées, amorçage manuel obligatoire : elles ne possèdent pas le module de mise à jour. Le dépôt privé nécessite le compte GitHub de l’utilisateur ; ne jamais intégrer de jeton au programme.

## Point de reprise — 12 octobre 2026

Lot achevé et testé localement : profils UCI indépendants, catalogue moteurs/bases, migration 4, filtres combinés, recherche annulable et pagination par curseur. Branche `feat/engine-catalog-databases` ; `main` conserve les modifications de présentation ajoutées séparément par l’utilisateur. Ne pas repartir de l’ancienne copie locale 759c3dc.

Fichiers : `libs/content/{content,package}.*`, `assets/content/`, `app/content.cpp`, `ui/Downloads.qml`, `libs/database/sources.cpp`, contrôleur et Main. Les sept suites locales passent (361 vérifications d’interaction). Vérification supplémentaire des trois paquets moteurs officiels par empreinte, extraction, handshake et coup légal ; deux PGN réels parsés ; téléchargement HTTPS réel PGN Mentor via Qt et suppression locale réussis. Aucun de ces binaires n’est incorporé au dépôt.

Tâche suivante : import reprenable, puis véritables répertoires nommés (camp, branches, transpositions) et entraînement par position. La recherche utilise un worker Qt, une connexion SQL indépendante, une annulation SQLite et des pages de 200 résultats. Les générations empêchent les réponses périmées de remplacer le filtre courant. L’export, certaines analyses de joueurs et les migrations restent synchrones ; la pagination de l’interface n’utilise plus OFFSET. Le benchmark synthétique d’un million de lignes a mis en évidence une recherche d’index insuffisante pour les dates identiques ; la correction par deux plages bornées est mesurée dans PERFORMANCE. Les futurs modèles Qt incrémentaux et les benchmarks sur 1/2/5/10 millions de parties réelles restent à faire. Le tableau détaillé de toutes les demandes est dans ROADMAP.

Précautions : PlentyChess 8 exige glibc 2.38 (donc téléchargement désactivé sur Debian 12) ; Stockfish 19 et Arasan 26 ont d’autres minima explicités au catalogue. Le catalogue est livré avec l’application, pas remplacé depuis une URL non vérifiée. Les anciennes versions et profils sont conservés lors d’une mise à jour de moteur ; la gestion de toutes les versions historiques pourra être étendue. Les cartes SRS actuelles et les bilans ne sont pas des répertoires complets et ne fournissent aucune estimation Elo artificielle.

Tests à poursuivre : CI Debian 12/13 après ajout de libarchive-dev/libsqlite3-dev et de la recherche asynchrone ; restauration depuis v3 réelle, navigation et annulation sur corpus volumineux, réseau interrompu. Ne pas modifier les sources C++/CMake pendant une compilation locale ; utiliser un seul processus compilateur dans cet environnement.
