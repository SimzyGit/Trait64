# Périmètre livré et travaux restants

## Ce que font les autres, relevé du 12 novembre 2026

Passage en revue de ChessBase, Scid vs. PC, En Croissant, Lichess (analyse) et Arena, côté échiquier et analyse.

| Option | Vue chez | État |
|---|---|---|
| Glisser les pièces | tous | ✅ réparé |
| Pièces qui glissent vers leur case | ChessBase, Lichess, En Croissant | ✅ ajouté |
| Flèches du meilleur coup du moteur | Lichess, En Croissant, ChessBase | ✅ ajouté |
| Lecture automatique d’une partie | Scid vs. PC, ChessBase, Arena | ✅ ajouté |
| Haut/bas pour changer de variante | Lichess, Scid vs. PC | ✅ ajouté |
| Détection des moteurs installés | Arena, En Croissant (téléchargement) | ✅ ajouté |
| Affichage d’un coup sous une image (16 ms) | — | ✅ 55 → 7 ms |
| Sons des coups | Lichess, ChessBase, En Croissant | ✅ ajouté : .deb, AppImage, Windows, macOS |
| Afficher la menace (coup nul) | ChessBase, Scid vs. PC | ✅ ajouté |
| Pièces prises et écart de matériel près de l’échiquier | Lichess, En Croissant | ✅ ajouté |
| Molette pour parcourir les coups | ChessBase, Scid vs. PC | ✅ ajouté, en option |
| Deviner le coup, jouer à l’aveugle | ChessBase, Scid vs. PC | ✅ ajoutés |
| Recherche par placement partiel de pièces | ChessBase, Scid vs. PC | ✅ ajouté, sans index : rejoue toutes les parties |
| Import léger des très grosses bases (positions des 30–40 premiers demi-coups) | Scid vs. PC, En Croissant | à décider |

## Interface native — dev.64, 28 octobre 2026

Navigation latérale, grand échiquier, panneaux adaptables, commandes clavier, police et tailles réglables, contraste renforcé et libellés français/anglais. Débordements, palette des dialogues et profil Joueurs vide corrigés. Treize suites en CI et 1 180 contrôles locaux (captures comprises), schéma 7 conservé. [Périmètre et limites](UI-REDESIGN.md) · [Publication](VALIDATION.md).

## Performances et intégrité — dev.55 / dev.57, 25 octobre 2026

Deux corrections mesurées de l'import (choix de l'index des doublons, calcul du
matériel), sans migration. Sur 10 000 parties réelles : 93,114 → 42,498 s dans
la comparaison conservée. Corrections intégrées et publiées dans dev.55 ;
voir PERFORMANCE et VALIDATION. La qualification Qt séparée a réussi sur 100 000 parties, avec 1 842 navigations
et zéro avertissement QML ; tous les PGN restaurés et la réindexation 10k sont
contrôlés. La vérification manuelle d'intégrité est asynchrone et annulable dans
dev.57 (PR #12), sans changement du schéma.

Paliers conservés : **10 000, 100 000, 500 000, 1, 2, 5 et 10 millions**. Les deux
premiers possèdent des rapports CLI terminés ; Qt est également mesuré sur 100k,
en rendu logiciel sans écran. Les modèles
incrémentaux, l'export asynchrone, les migrations volumineuses, la fusion et la
réparation restent des travaux distincts. Les autres objectifs restent prévus.

La version 0.3 est une nouvelle étape de l’application locale. Elle n’achève pas le catalogue d’environ 245 fonctions envisagé pour Trait64. Les fonctions ci-dessous sont celles réellement présentes dans le code.

| Domaine | Fonctions présentes après 0.3 | Travaux restants |
|---|---|---|
| Apparence | 4 jeux SVG + Unicode, import SVG personnel, 9 palettes, couleurs des cases/fond/repères, préférences persistantes | Textures, animations avancées, logo final |
| Échiquier | Chess960, 960 départs numérotés, éditeur graphique FEN, flèches/cercles persistants | Autres variantes |
| Études | 16 onglets restaurables, annuler/rétablir, duplication, fusion d’arbres, diagramme PNG, étude PDF | Chapitres, références et documents attachés, mise en page avancée |
| Fichiers | EPD classique, import PGN et export de bibliothèque par flux | Polyglot, SCID, ChessBase, T64 compact Move16/TLV/BLAKE3 |
| Bibliothèque | Schéma 7, import PGN parallèle (1–8 lecteurs) par lots reprenable et vérifié, bases nommées, historique PGN, FTS5, index variantes/matériel/Elo/joueurs, compteurs et arbres incrémentaux, filtres combinés, recherche annulable en arrière-plan, pagination par curseur, doublons, réindexation annulable | Qualification sur des corpus PGN réels de plusieurs millions de parties, modèles Qt incrémentaux, dédoublonnage avec choix de fusion, recherche structurelle fine |
| Analyse | Bilan principal, graphe, pertes en centipions, annotations, profils avec options UCI validées, comparaison simultanée de 2 à 4 UCI, MultiPV, profondeur, Syzygy via UCI, file persistante avec variantes/pause/reprise, cache avec provenance et export annoté | Budgets adaptatifs, interface de tablebases dédiée |
| Ouvertures | ECO CC0, classement automatique des parties sans balise, dossier d'une position (coups, score, tranches de force, parties modèles), écart entre un répertoire et les parties jouées, répertoires nommés Blancs/Noirs, transpositions, annotations/priorités, création/fusion/import PGN (compressés compris) et export annulables en arrière-plan, parties modèles, couverture locale asynchrone, analyse moteur de toutes les variantes via copie persistante | Couverture probabiliste globale, comparaison de deux répertoires entre eux, nouveautés théoriques, Polyglot |
| Entraînement | Cartes depuis erreurs, activité quotidienne/hebdomadaire, rappels espacés par position de répertoire, réponses préparées multiples et suivi des rappels manqués | Équivalences validées par moteur, cours de finales, calcul/visualisation, agenda adaptatif, certification tactique |
| Jeu local | Deux humains ou moteur, pendule avec incrément, abandon/réclamation de nulle | Moteur-moteur, périodes multiples, handicap et arbitrage complet |
| Préparation | Fiches joueurs ; statistiques par position, source, nom exact/couleur, période et Elo ; requête annulable, parties exemples et export CSV | Styles d’adversaires, tendances par période, préparation et gestion de tournois |
| Interface | 7 espaces, navigation latérale compacte/développée, commandes clavier, police et tailles réglables, contraste renforcé, libellés français/anglais, fenêtres à partir de 960 × 640 | Panneaux détachables, raccourcis personnalisables, traduction des diagnostics C++ et descriptifs de catalogue |
| Plateforme | Paquets Debian 12/13 amd64, AppImage, installateur source, tests CI et Releases automatisées sans archives Actions | Qualification sur le PC Voyager cible, Flatpak, Windows/macOS, API/plugins |
| Fiabilité | Vérification manuelle d'intégrité asynchrone et annulable, sauvegardes périodiques, avant mise à jour/migration, corbeille, session atomique et préservation des sessions illisibles | Restauration guidée de tout le dossier utilisateur, installation et sauvegarde pré-mise à jour entièrement asynchrones |
| Réseau | Vérification facultative GitHub Releases, trois canaux, SHA-256, notification, téléchargement et installation | Signature indépendante du mainteneur, import distant volontaire, partage ; jeu en ligne exclu du périmètre |
| Fonctions avancées | Études PDF/PNG, bilan moteur expliqué par suites légales et événements matériels, copies annotées, exercices issus des erreurs | Reconnaissance de diagrammes depuis images, rapports de répertoire, détection de faiblesses, cours pédagogiques complets |

Priorité suivante : qualifier les gros corpus et développer les explications vérifiables, puis approfondir les répertoires et l’entraînement. La refonte demandée le 26 octobre est livrée ; préserver ses contrôles visuels. Aucune estimation de niveau Elo à partir d’une partie ne sera développée. Voir [PROJECT-STATUS.md](PROJECT-STATUS.md) pour reprendre sans recréer l’application. Conserver PGN et les sauvegardes SQLite comme sorties récupérables pendant l’évolution du format T64 expérimental.

## Lot explications — 16 octobre 2026

Le bilan immédiat propose « Comprendre ce coup » : coup joué et réponse moteur,
proposition moteur avant le coup, faits de captures/promotions/échecs/mats/pats,
compte matériel conventionnel et ouverture d’une copie navigable. Les lignes
sont légalement rejouées, avec moteur/budget/profondeur visibles. Ce premier lot
est complété par les motifs tactiques de la PR #8 : fourchettes, clouages au roi,
enfilades du roi, échecs à la découverte et doubles échecs, avec repères PGN.
Les plans, réfutations démontrées et équivalences restent ouverts. Guide : [MOVE-EXPLANATIONS.md](MOVE-EXPLANATIONS.md).

## Lot répertoires — 15 octobre 2026

- [x] Création, fusion, import et export annulables en arrière-plan, progression et transactions atomiques.
- [x] Statistiques et couverture locale asynchrones, annulation et rejet des résultats d’une ancienne bibliothèque.
- [x] Analyse moteur de la copie complète d’un répertoire, profils et budget par position, sans modifier les notes ni les révisions.

Le lot est distribué dans v0.3.0-dev.42 : 11/11 suites sur Debian 12 et 13, paquets et empreintes distantes vérifiés, zéro archive Actions. Les preuves et le commit exact figurent dans VALIDATION.json. Les éditions ponctuelles restent synchrones, les transpositions sont développées une seule fois, et la couverture probabiliste/comparaison des répertoires restent ouvertes.

## Catalogue validé localement — 12 octobre 2026

Trois moteurs amont avec versions, tailles et SHA-256 épinglés ; options et profils indépendants ; téléchargement/extraction asynchrones sans exécution automatique. Bibliothèque de sources PGN : 18 champions classiques, quatre champions FIDE de la période du titre scindé, huit championnes disponibles chez le fournisseur, six matchs historiques et six sélections d’ouvertures. Les collections ne prétendent pas couvrir chaque partie de la carrière ni toutes les championnes. PGN Mentor autorise le téléchargement gratuit mais ne précise pas de licence ouverte de redistribution ; ses fichiers ne sont pas incorporés au dépôt.

## Cahier de continuation accepté

> **Rapproché du code le 12 novembre 2026.** Une case n'est cochée que si la fonction existe dans `main` et qu'un test ou une lecture du code le confirme. `◐` signale une fonction commencée, avec ce qui manque. Les cases vides n'ont pas été trouvées dans le code.

Les listes suivantes représentent les objectifs détaillés demandés le 11 octobre. Une case ouverte signifie que l’objectif complet reste à terminer, même lorsqu’une première capacité figure dans le tableau ci-dessus. Les fonctions déjà utilisables doivent rester opérationnelles. Les mesures de performances seront publiées avec leur protocole, sans comparaison non étayée.

### A. Performances et grandes bases de données — PRIORITÉ CRITIQUE

Je veux que Trait64 puisse travailler efficacement avec plusieurs millions de parties réelles.

Implémente :

- [ ] ◐ Un importateur PGN parallèle et performant — partiel : 1 à 8 lecteurs, qualifié à 500 000 et un million de parties réelles (voir PERFORMANCE.md) ; 2, 5 et 10 millions restent à mesurer.
- [x] Une importation reprenable après interruption.
- [x] Une progression détaillée des importations.
- [x] L'indexation progressive des positions — index allégé du schéma 12 : 20 premiers coups position par position, le reste par signature (matériel, pions).
- [x] Des index persistants réutilisables. — clés Zobrist et index SQLite conservés dans la base
- [x] Des recherches annulables en arrière-plan.
- [x] Une pagination efficace par curseur.
- [ ] ◐ Des modèles Qt incrémentaux — partiel : pages par curseur ; à qualifier au million.
- [ ] ◐ Le chargement paresseux des parties — partiel : la liste est paginée ; le reste à mesurer.
- [ ] ◐ L'optimisation des requêtes SQLite — partiel : requêtes préparées gardées, recherche de position menée par ses résultats quand elle est rare ; le dossier d'une position très courante reste lent sur une grosse base.
- [ ] La fusion intelligente des doublons.
- [ ] La conservation des annotations lors des fusions.
- [x] Une recherche structurelle par matériel et configuration des pions. — filtre matériel et « même structure de pions »
- [x] Une recherche combinant joueur, Elo, couleur, position, tournoi, date et ouverture. — recherche avancée de la bibliothèque
- [x] Une surveillance de l'intégrité des bases. — contrôle en arrière-plan, annulable
- [ ] ◐ Une procédure de restauration et de réparation — partiel : sauvegarde complète et restauration ; pas de réparation d'une base abîmée.
- [x] Une gestion fiable des migrations. — schéma 12, sauvegarde avant chaque migration

Teste le logiciel sur de véritables corpus PGN de 1, 2, 5 et 10 millions de parties.

Mesure systématiquement les temps d'importation, la consommation mémoire, les temps d'ouverture et les performances des recherches.

Compare les résultats avec les benchmarks publiés par Pragma Chess, à matériel et conditions comparables lorsqu'ils sont reproductibles.

Ne prétends jamais que Trait64 est plus rapide sans preuve mesurée.

### B. Analyse et compréhension des coups — PRIORITÉ CRITIQUE

Créer un véritable système d'explication échiquéenne inspiré des meilleurs outils existants, notamment le mode Explain de Pragma Chess.

Fonctionnalités :

- [x] Bouton « Comprendre ce coup » dans le bilan.
- [ ] Réfutations visuelles des erreurs.
- [x] Affichage de la variante moteur avant le coup et de sa réponse au coup joué.
- [x] Faits de captures/promotions et variation du compte matériel dans la suite ; gain forcé non déduit.
- [x] Détection des fourchettes/attaques multiples dans les suites du bilan (captures laissant le roi en sécurité, sans gain forcé revendiqué).
- [x] Détection des clouages absolus au roi (les clouages relatifs restent à développer).
- [x] Détection des enfilades du roi (autres enfilades à développer).
- [ ] Détection des attaques à la découverte : échecs à la découverte et doubles échecs implémentés ; attaques sans échec à développer.
- [ ] Détection des déviations et surcharges.
- [ ] Détection des sacrifices tactiques justifiés.
- [x] Reconnaissance des menaces de mat. — mat en un coup si l'adversaire passait
- [ ] Explication des séquences forcées.
- [ ] Analyse des structures de pions. — commencé : ce qu'un coup change (isolé, doublés, passé) est dit dans l'explication ; pas d'analyse d'ensemble
- [ ] Analyse de la sécurité des rois.
- [ ] Identification des colonnes ouvertes et des cases faibles.
- [ ] Explication des plans stratégiques lorsque des éléments concrets les justifient.
- [ ] Comparaison visuelle entre le coup joué et le meilleur coup.
- [x] Analyse automatique des variantes secondaires.
- [x] File de tâches d'analyse persistante.
- [ ] Budgets de calcul adaptatifs.
- [ ] ◐ Comparaison détaillée des conclusions de plusieurs moteurs — partiel : 2 à 4 profils côte à côte, sans synthèse.
- [ ] ◐ Exploitation des tables de finales Syzygy — partiel : chemin transmis au moteur et tables à 7 pièces en ligne ; pas de gestionnaire local.
- [x] Conservation et réutilisation des évaluations précédentes.
- [ ] ◐ Explications pédagogiques en français — partiel : suites légales et cinq motifs ; pas encore le « pourquoi ».

Les explications doivent s'appuyer sur de véritables calculs et des caractéristiques échiquéennes vérifiables.

Si une explication ne peut pas être établie avec suffisamment de confiance, le logiciel doit le préciser plutôt que d'inventer une justification.

Prévois une architecture permettant éventuellement l'intégration ultérieure d'une IA locale, sans la rendre obligatoire.

### C. Refonte et amélioration du bilan de partie

**Consigne importante : je ne veux aucune estimation artificielle du niveau Elo à partir d'une partie.**

Ne développe surtout pas les fonctions du type :

« Tu as joué comme un 2200 Elo. »

Je considère cette information peu utile et potentiellement trompeuse.

À la place, le bilan doit fournir :

- [x] Un graphique d'évaluation. — bilan de partie
- [ ] Les moments critiques.
- [x] Les imprécisions, erreurs et gaffes.
- [x] La perte en centipions.
- [ ] ◐ L'évolution des chances de gain si elle peut être calculée correctement — partiel : WDL affiché quand le moteur le fournit.
- [ ] Les coups ayant réellement changé le cours de la partie.
- [x] Les meilleures alternatives.
- [x] Les variantes justificatives. — suites légales rejouées
- [ ] ◐ Les erreurs tactiques — partiel : limitées aux motifs reconnus.
- [ ] Les erreurs stratégiques suffisamment identifiables.
- [ ] Les possibilités manquées.
- [ ] ◐ Les phases de la partie à retravailler — partiel : précision par phase.
- [x] Des exercices créés à partir des erreurs. — cartes et puzzles tirés du bilan
- [ ] ◐ Une comparaison avec les erreurs récurrentes des parties précédentes — partiel : tableau de bord d'un profil, sans rapprochement partie par partie.
- [ ] Des recommandations d'entraînement justifiées.

Ajouter des paramètres permettant de régler la profondeur, le moteur et le temps consacré au bilan.

Le bilan doit être sérieux, informatif et pédagogique, sans gamification artificielle ni compliments automatiques.

### D. Bases de données et interopérabilité

Ajouter :

- [x] Importation directe de parties Lichess. — compte synchronisé, lien de partie collable
- [x] Importation directe de parties Chess.com.
- [x] Synchronisation facultative de ces sources. — seules les nouvelles parties sont téléchargées
- [ ] Importation des formats ChessBase compatibles.
- [ ] Importation des formats SCID SI4/SI5.
- [x] Lecture et création de livres Polyglot. — lecture `.bin` et export depuis la bibliothèque
- [x] Gestionnaire de bases téléchargeables. — 42 collections
- [x] Gestionnaire de sources de données. — bases nommées, recueils qui suivent leur fichier
- [x] Historique des importations.
- [ ] ◐ Vérification des licences des bases — partiel : licence affichée au catalogue, non vérifiée pour un fichier importé.
- [x] Bases consacrées aux champions du monde.
- [x] Bases de parties historiques.
- [x] Bases thématiques. — ouvertures
- [x] Collections personnalisables. — collections et favoris
- [x] Recherche de nouveautés par rapport à une base de référence. — première position absente de la base locale
- [ ] Statistiques avancées de tournois.
- [x] Exportation CSV des statistiques de préparation, avec filtres et FEN.
- [ ] Comparaison de plusieurs bases.
- [x] Recherche de positions indépendamment de l'ordre des coups. — index par position, variantes comprises

Privilégie les formats ouverts et les sources légalement réutilisables.

Ne contourne aucune protection d'accès aux bases propriétaires.

### E. Répertoires d'ouvertures et préparation d'adversaire

Créer un véritable système de préparation pour les joueurs compétitifs.

Ajouter :

- [x] Répertoires séparés pour les Blancs et les Noirs.
- [x] Variantes principales et secondaires.
- [x] Gestion des transpositions.
- [ ] ◐ Couverture des réponses adverses — partiel : lacunes observées ; pas de couverture probabiliste.
- [x] Détection des trous dans les répertoires. — lacunes observées dans la bibliothèque
- [x] Statistiques par position.
- [x] Statistiques par plage de la moyenne des deux Elo connus.
- [x] Statistiques par période (années).
- [x] Références vers des parties modèles.
- [x] Identification des positions peu maîtrisées par l’historique des rappels.
- [x] Comparaison de plusieurs répertoires. — deux à la fois : positions communes, propres, et coups préparés différents
- [ ] ◐ Détection des nouveautés théoriques — partiel : nouveauté dans la base locale seulement.
- [x] Entraînement du répertoire par positions dues et réponses préparées multiples.
- [x] Répétition espacée simple par position.
- [x] Importation et exportation PGN des répertoires.

Créer un mode **Préparation d'adversaire**.

L'utilisateur doit pouvoir sélectionner un adversaire et obtenir :

- [x] Ses ouvertures principales. — fiche joueur et préparation
- [ ] Ses réponses les plus fréquentes.
- [x] Ses résultats avec chaque couleur.
- [ ] ◐ Ses tendances selon les périodes — partiel : filtre de période, pas de courbe dans le temps.
- [ ] Les variantes qu'il joue régulièrement.
- [ ] Ses choix dans des positions précises.
- [ ] Les faiblesses statistiques réellement observables.
- [ ] Des propositions de préparation.
- [ ] Des parties de référence.
- [ ] ◐ Un dossier de préparation exportable — partiel : CSV ; pas de PDF.

Distingue toujours une nouveauté théorique d'un simple coup absent de la base utilisée.

### F. Entraînement intelligent et progression

Améliore fortement le système d'entraînement.

Ajouter :

- [x] Exercices tactiques par thème et difficulté. — puzzles Lichess, classement Elo, thèmes
- [x] Entraînement aux ouvertures. — révision des positions de répertoire
- [ ] Entraînement aux finales.
- [ ] Entraînement au calcul.
- [ ] Entraînement à la visualisation.
- [x] Échecs à l'aveugle. — pièces masquées sur l'échiquier d'analyse
- [x] Exercices de coordonnées. — trente secondes, vu des Blancs ou des Noirs
- [ ] Exercices personnalisés.
- [x] Exercices extraits des parties personnelles.
- [ ] Entraînement sur les erreurs récurrentes.
- [ ] ◐ Plusieurs solutions correctes acceptées lorsqu'elles sont vérifiées — partiel : pour le répertoire seulement, pas par moteur.
- [ ] ◐ Répétition espacée avancée — partiel : répertoires (version simple) et puzzles manqués (1, 3, 7 jours) ; pas d'intervalle ajusté à la difficulté
- [x] Calendrier de révision.
- [ ] ◐ Historique des entraînements — partiel : suivi quotidien et hebdomadaire.
- [x] Statistiques de progression par thème. — réussite par thème des puzzles ; pas d'évolution dans le temps
- [x] Objectifs personnels. — objectif quotidien
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

- [ ] ◐ Organisation des études en chapitres — partiel : un recueil PGN tient lieu de chapitres.
- [ ] Sous-chapitres.
- [ ] Titres et sous-titres.
- [ ] Paragraphes pédagogiques.
- [ ] Images et diagrammes.
- [ ] Références vers des parties.
- [ ] Liens entre plusieurs études.
- [ ] Réutilisation d'analyses sans duplication inutile.
- [ ] Projets de travail enregistrables.
- [ ] ◐ Organisation des documents de préparation — partiel : explorateur d'études : dossiers, renommage, déplacement.
- [ ] ◐ Recherche globale dans les études — partiel : noms de fichiers et parties ; pas le texte des commentaires.
- [ ] ◐ Historique des modifications — partiel : annuler/rétablir et instantanés.
- [x] Instantanés récupérables. — 14 instantanés automatiques
- [x] Importation d'études Lichess. — lien d'étude ou de chapitre collable ; jamais vu répondre en vrai (pas d'accès à Lichess en test)

## Pistes retenues pour se distinguer (6 novembre 2026)

Trois ajouts qui n'existent chez aucun concurrent libre, et que l'architecture locale de Trait64 rend possibles.

- [ ] **Suivre les tournois en direct.** Lichess diffuse ses retransmissions en PGN par une API publique et gratuite. Suivre une partie coup par coup avec son propre moteur, et voir en direct quand elle entre dans son répertoire et où elle en sort. ChessBase fait payer cet usage.
- [x] **Partager sans serveur.** Exporter une étude ou un bilan en un fichier HTML autonome, échiquier et variantes compris. Cela règle la faiblesse du « tout local » sans renier la promesse : un fichier n'est pas un compte. — fait pour une étude (export HTML) ; pas pour un bilan
- [ ] **Saisir une partie depuis une feuille de notation.** Photographier la feuille et en tirer le PGN. C'est le point de douleur du joueur de club, et personne ne le fait bien. Le plus coûteux des trois.
- [ ] ◐ Rapports PDF avancés — partiel : étude en PDF avec diagramme et notation.
- [x] Exportation de diagrammes SVG.
- [ ] Modèles personnalisables de rapports.

L'objectif est de permettre à Trait64 de servir aussi bien à un joueur qu'à un entraîneur, un auteur ou un club.

### H. Gestionnaires de moteurs et de bases

Ajouter des gestionnaires permettant à l'utilisateur :

- [x] De rechercher les moteurs disponibles. — détection des moteurs installés, 12 novembre
- [x] De télécharger des moteurs open source. — Stockfish, Arasan, PlentyChess
- [x] De les installer.
- [x] De les supprimer.
- [ ] De les mettre à jour.
- [x] De créer différents profils.
- [x] De configurer indépendamment les threads, le Hash, le MultiPV et les autres options UCI.
- [x] De télécharger des bases de parties.
- [x] De consulter leur taille, source et licence.
- [ ] De gérer les versions installées.
- [ ] De télécharger des tables Syzygy adaptées à son stockage disponible.

Respecter les licences et vérifier les téléchargements.

Ne jamais exécuter automatiquement un fichier non fiable.

### I. Système de mises à jour, fiabilité et sécurité

Le système GitHub existe déjà. Il faut le conserver et le perfectionner.

Ajouter ou vérifier :

- [ ] Signatures cryptographiques indépendantes des publications.
- [x] Vérification d'intégrité des téléchargements. — SHA-256
- [ ] Restauration guidée après échec.
- [x] Sauvegardes complètes avant migration.
- [x] Installation asynchrone.
- [ ] Gestion correcte des interruptions.
- [x] Conservation des préférences.
- [x] Conservation des bases de données.
- [ ] Gestion des versions incompatibles.
- [ ] ◐ Tests d'installation sur Voyager/Debian — partiel : Debian 12/13 et 10 autres images en CI ; jamais sur la machine réelle.
- [ ] Tests Wayland et X11.
- [x] Tests de régression. — 20 suites, parcours rejoués à vrais clics
- [ ] Tests de sécurité.
- [ ] ◐ Journaux de diagnostic exportables — partiel : page Intégrité & diagnostic, sans export.
- [x] Vérification des dépendances. — lanceur de l'AppImage, 12 distributions testées à chaque version

Éviter toute télémétrie obligatoire.

### J. Plateformes, ergonomie et accessibilité

Poursuivre le support Linux, puis préparer :

- [x] Windows. — installateur publié à chaque version
- [x] macOS. — image `.dmg` puce Apple
- [ ] Flatpak.
- [ ] Distribution Flathub.
- [ ] ◐ Traductions complètes — partiel : interface français/anglais ; diagnostics techniques non traduits.
- [x] Libellés français/anglais de l’interface et changement de langue conservant les valeurs des listes.
- [ ] Compléter les traductions des diagnostics C++ et descriptifs de catalogue.
- [x] Navigation principale et palette de commandes au clavier.
- [ ] Audit exhaustif du parcours clavier et des technologies d’assistance.
- [ ] Raccourcis personnalisables.
- [ ] Améliorations d'accessibilité.
- [x] Gestion des petites résolutions. — vérifiée à 960 × 640
- [ ] Panneaux détachables si compatibles avec notre conception.
- [ ] Dispositions de travail enregistrées.
- [ ] Configuration de plusieurs espaces de travail.
- [ ] Tutoriel de première utilisation.
- [ ] ◐ Documentation utilisateur intégrée — partiel : aide et raccourcis ; pas de manuel.

Conserve l'identité visuelle actuelle. L'esthétique n'est pas notre priorité immédiate : nous y reviendrons plus tard.

### K. Fonctionnalités avancées à prévoir

Lorsque les fonctions principales seront suffisamment robustes :

- [ ] Reconnaissance de diagrammes à partir d'images.
- [ ] Validation manuelle des FEN reconnues.
- [ ] Prise en charge des échiquiers électroniques.
- [x] Tournois moteur contre moteur. — toutes rondes, 2 à 8 profils
- [ ] Benchmarks automatisés de moteurs.
- [ ] Assistant IA pédagogique facultatif.
- [ ] Modèles IA locaux.
- [ ] API publique versionnée.
- [ ] Gestionnaire d'extensions.
- [ ] Synchronisation chiffrée facultative entre appareils.
- [ ] Application compagnon mobile.
- [ ] Rapports approfondis sur les faiblesses.
- [ ] Bibliothèques de cours.
- [ ] ◐ Compatibilité avec d'autres variantes d'échecs lorsque cela ne fragilise pas les fonctions principales — partiel : Chess960.

---


Références étudiées et précautions de réutilisation : [SOURCES.md](SOURCES.md). Ordre de travail et reprise : [PROJECT-STATUS.md](PROJECT-STATUS.md).

13 octobre : répertoires par position, partage des transpositions, PGN et rappels espacés implémentés. Les réponses adverses absentes sont détectées par rapport à la bibliothèque ; couverture probabiliste globale, comparaison de répertoires et analyse moteur des branches restent à développer. Guide : [REPERTOIRES-IMPORTS.md](REPERTOIRES-IMPORTS.md).

15 octobre : stockage Actions nettoyé (8,37 Go) et purge automatique intégrée par la PR #4. La PR #5 ajoute l'import parallèle, corrige la notation SAN historique et protège les sorties du benchmark. Dix suites réussissent sur Debian 12 et 13 ; corpus réel de 10 000 parties avec pause/reprise et sauvegarde vérifiées. Voir [PERFORMANCE.md](PERFORMANCE.md). Prochain lot : file d'analyses persistante et explications vérifiables, puis entraînement et répertoires ; détails de reprise dans [PROJECT-STATUS.md](PROJECT-STATUS.md).

15 octobre, lot analyses : file Qt/SQLite persistante, copie indépendante de l’étude, toutes les branches, pause/reprise après arrêt réel, MultiPV, cache borné tenant compte de l’historique et de l’identité moteur, export PGN annoté sans prolonger la ligne jouée. Les scores de mat ne produisent plus de pertes artificielles en centipions. Guide : [ANALYSIS-QUEUE.md](ANALYSIS-QUEUE.md).
