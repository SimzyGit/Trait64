# Périmètre livré et travaux restants

La version 0.3 est une nouvelle étape de l’application locale. Elle n’achève pas le catalogue d’environ 245 fonctions envisagé pour Trait64. Les fonctions ci-dessous sont celles réellement présentes dans le code.

| Domaine | Fonctions présentes après 0.3 | Travaux restants |
|---|---|---|
| Apparence | 4 jeux SVG + Unicode, import SVG personnel, 9 palettes, couleurs des cases/fond/repères, préférences persistantes | Textures, animations avancées, logo final |
| Échiquier | Chess960, 960 départs numérotés, éditeur graphique FEN, flèches/cercles persistants | Autres variantes |
| Études | 16 onglets restaurables, annuler/rétablir, duplication, fusion d’arbres, diagramme PNG, étude PDF | Chapitres, références et documents attachés, mise en page avancée |
| Fichiers | EPD classique, import PGN et export de bibliothèque par flux | Polyglot, SCID, ChessBase, T64 compact Move16/TLV/BLAKE3 |
| Bibliothèque | Schéma 7, import PGN parallèle (1–8 lecteurs) par lots reprenable et vérifié, bases nommées, historique PGN, FTS5, index variantes/matériel/Elo/joueurs, compteurs et arbres incrémentaux, filtres combinés, recherche annulable en arrière-plan, pagination par curseur, doublons, réindexation annulable | Qualification sur des corpus PGN réels de plusieurs millions de parties, modèles Qt incrémentaux, dédoublonnage avec choix de fusion, recherche structurelle fine |
| Analyse | Bilan principal, graphe, pertes en centipions, annotations, profils avec options UCI validées, comparaison simultanée de 2 à 4 UCI, MultiPV, profondeur, Syzygy via UCI, file persistante avec variantes/pause/reprise, cache avec provenance et export annoté | Budgets adaptatifs, interface de tablebases dédiée |
| Ouvertures | ECO CC0, répertoires nommés Blancs/Noirs, transpositions, annotations/priorités, création/fusion/import/export PGN annulables en arrière-plan, parties modèles, couverture locale asynchrone, analyse moteur de toutes les variantes via copie persistante | Couverture probabiliste globale, comparaison de répertoires, nouveautés théoriques, Polyglot |
| Entraînement | Cartes depuis erreurs, activité quotidienne/hebdomadaire, rappels espacés par position de répertoire, réponses préparées multiples et suivi des rappels manqués | Équivalences validées par moteur, cours de finales, calcul/visualisation, agenda adaptatif, certification tactique |
| Jeu local | Deux humains ou moteur, pendule avec incrément, abandon/réclamation de nulle | Moteur-moteur, périodes multiples, handicap et arbitrage complet |
| Préparation | Fiches joueurs ; statistiques par position, source, nom exact/couleur, période et Elo ; requête annulable, parties exemples et export CSV | Styles d’adversaires, tendances par période, préparation et gestion de tournois |
| Interface | 7 espaces, personnalisation, petites fenêtres à partir de 960 × 640 | Panneaux détachables, raccourcis personnalisables, traductions complètes |
| Plateforme | Paquets Debian 12/13 amd64, AppImage, installateur source, tests CI et Releases automatisées sans archives Actions | Qualification sur le PC Voyager cible, Flatpak, Windows/macOS, API/plugins |
| Fiabilité | Sauvegardes périodiques, avant mise à jour/migration, corbeille, session atomique et préservation des sessions illisibles | Restauration guidée de tout le dossier utilisateur, installation et sauvegarde pré-mise à jour entièrement asynchrones |
| Réseau | Vérification facultative GitHub Releases, trois canaux, SHA-256, notification, téléchargement et installation | Signature indépendante du mainteneur, import distant volontaire, partage ; jeu en ligne exclu du périmètre |
| Fonctions avancées | Études PDF/PNG, bilan moteur, exercices issus des erreurs | Reconnaissance de diagrammes depuis images, rapports de répertoire, détection de faiblesses, cours pédagogiques complets |

Priorité suivante : qualifier les gros corpus et développer les explications vérifiables, puis approfondir les répertoires et l’entraînement. L’esthétique est en pause. Aucune estimation de niveau Elo à partir d’une partie ne sera développée. Voir [NEXT-STEPS.md](NEXT-STEPS.md) pour reprendre sans recréer l’application. Conserver PGN et les sauvegardes SQLite comme sorties récupérables pendant l’évolution du format T64 expérimental.

## Lot répertoires — 15 octobre 2026

- [x] Création, fusion, import et export annulables en arrière-plan, progression et transactions atomiques.
- [x] Statistiques et couverture locale asynchrones, annulation et rejet des résultats d’une ancienne bibliothèque.
- [x] Analyse moteur de la copie complète d’un répertoire, profils et budget par position, sans modifier les notes ni les révisions.

La validation de publication du commit exact est suivie dans WORK-IN-PROGRESS. Les éditions ponctuelles restent synchrones, les transpositions sont développées une seule fois, et la couverture probabiliste/comparaison des répertoires restent ouvertes.

## Catalogue validé localement — 12 octobre 2026

Trois moteurs amont avec versions, tailles et SHA-256 épinglés ; options et profils indépendants ; téléchargement/extraction asynchrones sans exécution automatique. Bibliothèque de sources PGN : 18 champions classiques, quatre champions FIDE de la période du titre scindé, huit championnes disponibles chez le fournisseur, six matchs historiques et six sélections d’ouvertures. Les collections ne prétendent pas couvrir chaque partie de la carrière ni toutes les championnes. PGN Mentor autorise le téléchargement gratuit mais ne précise pas de licence ouverte de redistribution ; ses fichiers ne sont pas incorporés au dépôt.

## Cahier de continuation accepté

Les listes suivantes représentent les objectifs détaillés demandés le 11 octobre. Une case ouverte signifie que l’objectif complet reste à terminer, même lorsqu’une première capacité figure dans le tableau ci-dessus. Les fonctions déjà utilisables doivent rester opérationnelles. Les mesures de performances seront publiées avec leur protocole, sans comparaison non étayée.

### A. Performances et grandes bases de données — PRIORITÉ CRITIQUE

Je veux que Trait64 puisse travailler efficacement avec plusieurs millions de parties réelles.

Implémente :

- [ ] Un importateur PGN parallèle et performant. Parallélisme livré (1–8 lecteurs), mesuré sur 10 000 parties réelles ; qualification à plusieurs millions encore ouverte.
- [x] Une importation reprenable après interruption.
- [x] Une progression détaillée des importations.
- [ ] L'indexation progressive des positions.
- [ ] Des index persistants réutilisables.
- [x] Des recherches annulables en arrière-plan.
- [x] Une pagination efficace par curseur.
- [ ] Des modèles Qt incrémentaux.
- [ ] Le chargement paresseux des parties.
- [ ] L'optimisation des requêtes SQLite.
- [ ] La fusion intelligente des doublons.
- [ ] La conservation des annotations lors des fusions.
- [ ] Une recherche structurelle par matériel et configuration des pions.
- [ ] Une recherche combinant joueur, Elo, couleur, position, tournoi, date et ouverture.
- [ ] Une surveillance de l'intégrité des bases.
- [ ] Une procédure de restauration et de réparation.
- [ ] Une gestion fiable des migrations.

Teste le logiciel sur de véritables corpus PGN de 1, 2, 5 et 10 millions de parties.

Mesure systématiquement les temps d'importation, la consommation mémoire, les temps d'ouverture et les performances des recherches.

Compare les résultats avec les benchmarks publiés par Pragma Chess, à matériel et conditions comparables lorsqu'ils sont reproductibles.

Ne prétends jamais que Trait64 est plus rapide sans preuve mesurée.

### B. Analyse et compréhension des coups — PRIORITÉ CRITIQUE

Créer un véritable système d'explication échiquéenne inspiré des meilleurs outils existants, notamment le mode Explain de Pragma Chess.

Fonctionnalités :

- [ ] Bouton « Pourquoi ce coup ? ».
- [ ] Réfutations visuelles des erreurs.
- [ ] Affichage des alternatives pertinentes.
- [ ] Détection des gains et pertes de matériel.
- [ ] Détection des fourchettes.
- [ ] Détection des clouages.
- [ ] Détection des enfilades.
- [ ] Détection des attaques à la découverte.
- [ ] Détection des déviations et surcharges.
- [ ] Détection des sacrifices tactiques justifiés.
- [ ] Reconnaissance des menaces de mat.
- [ ] Explication des séquences forcées.
- [ ] Analyse des structures de pions.
- [ ] Analyse de la sécurité des rois.
- [ ] Identification des colonnes ouvertes et des cases faibles.
- [ ] Explication des plans stratégiques lorsque des éléments concrets les justifient.
- [ ] Comparaison visuelle entre le coup joué et le meilleur coup.
- [x] Analyse automatique des variantes secondaires.
- [x] File de tâches d'analyse persistante.
- [ ] Budgets de calcul adaptatifs.
- [ ] Comparaison détaillée des conclusions de plusieurs moteurs.
- [ ] Exploitation des tables de finales Syzygy.
- [x] Conservation et réutilisation des évaluations précédentes.
- [ ] Explications pédagogiques en français.

Les explications doivent s'appuyer sur de véritables calculs et des caractéristiques échiquéennes vérifiables.

Si une explication ne peut pas être établie avec suffisamment de confiance, le logiciel doit le préciser plutôt que d'inventer une justification.

Prévois une architecture permettant éventuellement l'intégration ultérieure d'une IA locale, sans la rendre obligatoire.

### C. Refonte et amélioration du bilan de partie

**Consigne importante : je ne veux aucune estimation artificielle du niveau Elo à partir d'une partie.**

Ne développe surtout pas les fonctions du type :

« Tu as joué comme un 2200 Elo. »

Je considère cette information peu utile et potentiellement trompeuse.

À la place, le bilan doit fournir :

- [ ] Un graphique d'évaluation.
- [ ] Les moments critiques.
- [ ] Les imprécisions, erreurs et gaffes.
- [ ] La perte en centipions.
- [ ] L'évolution des chances de gain si elle peut être calculée correctement.
- [ ] Les coups ayant réellement changé le cours de la partie.
- [ ] Les meilleures alternatives.
- [ ] Les variantes justificatives.
- [ ] Les erreurs tactiques.
- [ ] Les erreurs stratégiques suffisamment identifiables.
- [ ] Les possibilités manquées.
- [ ] Les phases de la partie à retravailler.
- [ ] Des exercices créés à partir des erreurs.
- [ ] Une comparaison avec les erreurs récurrentes des parties précédentes.
- [ ] Des recommandations d'entraînement justifiées.

Ajouter des paramètres permettant de régler la profondeur, le moteur et le temps consacré au bilan.

Le bilan doit être sérieux, informatif et pédagogique, sans gamification artificielle ni compliments automatiques.

### D. Bases de données et interopérabilité

Ajouter :

- [ ] Importation directe de parties Lichess.
- [ ] Importation directe de parties Chess.com.
- [ ] Synchronisation facultative de ces sources.
- [ ] Importation des formats ChessBase compatibles.
- [ ] Importation des formats SCID SI4/SI5.
- [ ] Lecture et création de livres Polyglot.
- [ ] Gestionnaire de bases téléchargeables.
- [ ] Gestionnaire de sources de données.
- [ ] Historique des importations.
- [ ] Vérification des licences des bases.
- [ ] Bases consacrées aux champions du monde.
- [ ] Bases de parties historiques.
- [ ] Bases thématiques.
- [ ] Collections personnalisables.
- [ ] Recherche de nouveautés par rapport à une base de référence.
- [ ] Statistiques avancées de tournois.
- [x] Exportation CSV des statistiques de préparation, avec filtres et FEN.
- [ ] Comparaison de plusieurs bases.
- [ ] Recherche de positions indépendamment de l'ordre des coups.

Privilégie les formats ouverts et les sources légalement réutilisables.

Ne contourne aucune protection d'accès aux bases propriétaires.

### E. Répertoires d'ouvertures et préparation d'adversaire

Créer un véritable système de préparation pour les joueurs compétitifs.

Ajouter :

- [x] Répertoires séparés pour les Blancs et les Noirs.
- [x] Variantes principales et secondaires.
- [x] Gestion des transpositions.
- [ ] Couverture des réponses adverses.
- [ ] Détection des trous dans les répertoires.
- [x] Statistiques par position.
- [x] Statistiques par plage de la moyenne des deux Elo connus.
- [x] Statistiques par période (années).
- [x] Références vers des parties modèles.
- [x] Identification des positions peu maîtrisées par l’historique des rappels.
- [ ] Comparaison de plusieurs répertoires.
- [ ] Détection des nouveautés théoriques.
- [x] Entraînement du répertoire par positions dues et réponses préparées multiples.
- [x] Répétition espacée simple par position.
- [x] Importation et exportation PGN des répertoires.

Créer un mode **Préparation d'adversaire**.

L'utilisateur doit pouvoir sélectionner un adversaire et obtenir :

- [ ] Ses ouvertures principales.
- [ ] Ses réponses les plus fréquentes.
- [ ] Ses résultats avec chaque couleur.
- [ ] Ses tendances selon les périodes.
- [ ] Les variantes qu'il joue régulièrement.
- [ ] Ses choix dans des positions précises.
- [ ] Les faiblesses statistiques réellement observables.
- [ ] Des propositions de préparation.
- [ ] Des parties de référence.
- [ ] Un dossier de préparation exportable.

Distingue toujours une nouveauté théorique d'un simple coup absent de la base utilisée.

### F. Entraînement intelligent et progression

Améliore fortement le système d'entraînement.

Ajouter :

- [ ] Exercices tactiques par thème et difficulté.
- [ ] Entraînement aux ouvertures.
- [ ] Entraînement aux finales.
- [ ] Entraînement au calcul.
- [ ] Entraînement à la visualisation.
- [ ] Échecs à l'aveugle.
- [ ] Exercices de coordonnées.
- [ ] Exercices personnalisés.
- [ ] Exercices extraits des parties personnelles.
- [ ] Entraînement sur les erreurs récurrentes.
- [ ] Plusieurs solutions correctes acceptées lorsqu'elles sont vérifiées.
- [ ] Répétition espacée avancée.
- [ ] Calendrier de révision.
- [ ] Historique des entraînements.
- [ ] Statistiques de progression par thème.
- [ ] Objectifs personnels.
- [ ] Programmes d'entraînement personnalisés.
- [ ] Un mode entraîneur pour créer des exercices destinés aux élèves.

Créer également un entraîneur interactif proposant, après une erreur :

- [ ] Rejouer le coup.
- [ ] Comprendre l'erreur.
- [ ] Voir la réfutation.
- [ ] Continuer malgré l'erreur.

Étudier l'utilisation facultative de moteurs inspirés du jeu humain, tels que Maia, pour diversifier les entraînements.

### G. Études et outils pédagogiques

Ajouter :

- [ ] Organisation des études en chapitres.
- [ ] Sous-chapitres.
- [ ] Titres et sous-titres.
- [ ] Paragraphes pédagogiques.
- [ ] Images et diagrammes.
- [ ] Références vers des parties.
- [ ] Liens entre plusieurs études.
- [ ] Réutilisation d'analyses sans duplication inutile.
- [ ] Projets de travail enregistrables.
- [ ] Organisation des documents de préparation.
- [ ] Recherche globale dans les études.
- [ ] Historique des modifications.
- [ ] Instantanés récupérables.
- [ ] Importation d'études Lichess.
- [ ] Rapports PDF avancés.
- [ ] Exportation de diagrammes SVG.
- [ ] Modèles personnalisables de rapports.

L'objectif est de permettre à Trait64 de servir aussi bien à un joueur qu'à un entraîneur, un auteur ou un club.

### H. Gestionnaires de moteurs et de bases

Ajouter des gestionnaires permettant à l'utilisateur :

- [ ] De rechercher les moteurs disponibles.
- [ ] De télécharger des moteurs open source.
- [ ] De les installer.
- [ ] De les supprimer.
- [ ] De les mettre à jour.
- [ ] De créer différents profils.
- [ ] De configurer indépendamment les threads, le Hash, le MultiPV et les autres options UCI.
- [ ] De télécharger des bases de parties.
- [ ] De consulter leur taille, source et licence.
- [ ] De gérer les versions installées.
- [ ] De télécharger des tables Syzygy adaptées à son stockage disponible.

Respecter les licences et vérifier les téléchargements.

Ne jamais exécuter automatiquement un fichier non fiable.

### I. Système de mises à jour, fiabilité et sécurité

Le système GitHub existe déjà. Il faut le conserver et le perfectionner.

Ajouter ou vérifier :

- [ ] Signatures cryptographiques indépendantes des publications.
- [ ] Vérification d'intégrité des téléchargements.
- [ ] Restauration guidée après échec.
- [ ] Sauvegardes complètes avant migration.
- [ ] Installation asynchrone.
- [ ] Gestion correcte des interruptions.
- [ ] Conservation des préférences.
- [ ] Conservation des bases de données.
- [ ] Gestion des versions incompatibles.
- [ ] Tests d'installation sur Voyager/Debian.
- [ ] Tests Wayland et X11.
- [ ] Tests de régression.
- [ ] Tests de sécurité.
- [ ] Journaux de diagnostic exportables.
- [ ] Vérification des dépendances.

Éviter toute télémétrie obligatoire.

### J. Plateformes, ergonomie et accessibilité

Poursuivre le support Linux, puis préparer :

- [ ] Windows.
- [ ] macOS.
- [ ] Flatpak.
- [ ] Distribution Flathub.
- [ ] Traductions complètes.
- [ ] Internationalisation.
- [ ] Navigation au clavier.
- [ ] Raccourcis personnalisables.
- [ ] Améliorations d'accessibilité.
- [ ] Gestion des petites résolutions.
- [ ] Panneaux détachables si compatibles avec notre conception.
- [ ] Dispositions de travail enregistrées.
- [ ] Configuration de plusieurs espaces de travail.
- [ ] Tutoriel de première utilisation.
- [ ] Documentation utilisateur intégrée.

Conserve l'identité visuelle actuelle. L'esthétique n'est pas notre priorité immédiate : nous y reviendrons plus tard.

### K. Fonctionnalités avancées à prévoir

Lorsque les fonctions principales seront suffisamment robustes :

- [ ] Reconnaissance de diagrammes à partir d'images.
- [ ] Validation manuelle des FEN reconnues.
- [ ] Prise en charge des échiquiers électroniques.
- [ ] Tournois moteur contre moteur.
- [ ] Benchmarks automatisés de moteurs.
- [ ] Assistant IA pédagogique facultatif.
- [ ] Modèles IA locaux.
- [ ] API publique versionnée.
- [ ] Gestionnaire d'extensions.
- [ ] Synchronisation chiffrée facultative entre appareils.
- [ ] Application compagnon mobile.
- [ ] Rapports approfondis sur les faiblesses.
- [ ] Bibliothèques de cours.
- [ ] Compatibilité avec d'autres variantes d'échecs lorsque cela ne fragilise pas les fonctions principales.

---


Références étudiées et précautions de réutilisation : [SOURCES.md](SOURCES.md). Ordre de travail et reprise : [NEXT-STEPS.md](NEXT-STEPS.md).

13 octobre : répertoires par position, partage des transpositions, PGN et rappels espacés implémentés. Les réponses adverses absentes sont détectées par rapport à la bibliothèque ; couverture probabiliste globale, comparaison de répertoires et analyse moteur des branches restent à développer. Guide : [REPERTOIRES-IMPORTS.md](REPERTOIRES-IMPORTS.md).

15 octobre : stockage Actions nettoyé (8,37 Go) et purge automatique intégrée par la PR #4. La PR #5 ajoute l'import parallèle, corrige la notation SAN historique et protège les sorties du benchmark. Dix suites réussissent sur Debian 12 et 13 ; corpus réel de 10 000 parties avec pause/reprise et sauvegarde vérifiées. Voir [PERFORMANCE.md](PERFORMANCE.md). Prochain lot : file d'analyses persistante et explications vérifiables, puis entraînement et répertoires ; détails de reprise dans [WORK-IN-PROGRESS.md](WORK-IN-PROGRESS.md).

15 octobre, lot analyses : file Qt/SQLite persistante, copie indépendante de l’étude, toutes les branches, pause/reprise après arrêt réel, MultiPV, cache borné tenant compte de l’historique et de l’identité moteur, export PGN annoté sans prolonger la ligne jouée. Les scores de mat ne produisent plus de pertes artificielles en centipions. Guide : [ANALYSIS-QUEUE.md](ANALYSIS-QUEUE.md).
