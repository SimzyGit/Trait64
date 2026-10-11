# Prochaine mise à jour — préparation du cycle 0.4

> **Statut : planification.** Rien dans ce document ne doit être présenté comme livré tant que le code, les tests et les paquets correspondants ne sont pas présents sur `main`.

## Point de départ

Trait64 0.3 alpha possède déjà l’application C++23/Qt6 locale, la bibliothèque SQLite schéma 3, les recherches et statistiques indexées, l’analyse UCI/MultiPV, la comparaison simultanée de 2 à 4 moteurs, Chess960, l’entraînement simple, les sauvegardes et les mises à jour GitHub vérifiées. La refonte « carnet d’étude » du 11 octobre 2026 est conservée ; l’esthétique n’est pas la priorité de ce cycle.

## Objectif du prochain jalon

Faire de Trait64 un environnement local capable de gérer **plusieurs moteurs et plusieurs bases de parties comme des ressources de première classe**, puis rendre la recherche d’ouverture suffisamment précise pour exploiter ces corpus.

### 1. Moteurs installables et profils indépendants

- Inventaire des moteurs disponibles sur la machine.
- Ajout/suppression manuelle d’un exécutable UCI.
- Plusieurs profils par moteur, sans écraser les réglages des autres.
- Réglages persistants : Threads, Hash, MultiPV, limites, Syzygy et options UCI découvertes au handshake.
- Catalogue de téléchargement vérifié pour des moteurs redistribuables : version, plateforme, architecture, licence, source, URL, taille et SHA-256.
- Candidats initiaux : **Stockfish, Arasan et PlentyChess**, uniquement après vérification de leurs conditions de redistribution.
- Installation atomique ; aucun binaire téléchargé n’est lancé automatiquement.
- Comparaison existante 2–4 moteurs réutilisée au lieu d’être réécrite.

### 2. Plusieurs bases et packs de parties

- Ouvrir, créer, fermer et identifier clairement plusieurs bases locales.
- Import PGN en arrière-plan, progression, annulation et reprise quand c’est possible.
- Métadonnées : nom, provenance, version/date, licence, empreinte et nombre de parties.
- Une partie stockée une seule fois peut être liée à plusieurs collections.
- Catalogue optionnel de packs téléchargeables.
- Collections dédiées aux **champions du monde**, construites seulement à partir de sources dont l’usage et la redistribution sont documentés.
- Déduplication lors de l’import et historique des mises à jour du pack.
- Ne jamais mélanger silencieusement une base téléchargée à la bibliothèque personnelle.

### 3. Recherche d’ouverture et filtres

Les filtres doivent pouvoir se combiner :

- ECO et plage ECO ;
- nom d’ouverture et variante ;
- position/FEN ;
- joueur, couleur et adversaire ;
- événement, site, date et ronde ;
- Elo minimum/maximum ;
- résultat ;
- collection/base ;
- favoris et autres attributs déjà indexés.

Prévoir une recherche annulable, des index adaptés aux gros corpus et des filtres enregistrables. Les tests doivent couvrir les combinaisons de critères, pas uniquement chaque filtre séparément.

## Après ce jalon

Reprendre le catalogue fonctionnel restant sans repartir de zéro : répertoires structurés, entraînement avancé, très gros corpus, analyse des branches, préparation d’adversaire, tablebases dédiées, rapports, import distant volontaire, plugins/API et plateformes supplémentaires. Le jeu en ligne reste hors périmètre.

## Définition de terminé pour une 0.4 alpha

- Tests unitaires et d’intégration des nouveaux schémas et migrations.
- Aucune régression sur PGN/FEN, études, analyse, entraînement, mises à jour et sauvegardes.
- Test réel de plusieurs moteurs installés et configurés différemment.
- Test de plusieurs bases et d’une partie liée à plusieurs collections sans duplication.
- Test d’un pack historique avec provenance/licence/empreinte documentées.
- Tests de filtres d’ouverture combinés sur un corpus représentatif.
- Migration sauvegardée vers un éventuel schéma 4 ; ne jamais modifier rétroactivement le schéma 3 déjà publié.
- Debian 12/13, AppImage, tests CI et contrôles d’intégrité verts avant publication.
- `THIRD_PARTY_NOTICES.md` mis à jour pour tout nouveau composant redistribué.

## Brouillon de notes de release

### Trait64 0.4 alpha — moteurs, bases et recherche

- Gestion de plusieurs moteurs UCI avec profils et réglages indépendants.
- Catalogue de moteurs téléchargeables et vérifiés lorsque leur redistribution est autorisée.
- Gestion de plusieurs bases locales et collections sans duplication des parties.
- Packs optionnels de parties avec provenance et intégrité documentées, dont des collections historiques par champion du monde lorsque les sources le permettent.
- Filtres de recherche d’ouverture combinables et adaptés aux grandes bibliothèques.
- Les fonctions de Trait64 0.3 restent conservées.

Ce texte est un **brouillon** : supprimer toute ligne non réellement livrée avant la publication.
