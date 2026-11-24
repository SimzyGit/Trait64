## 0.5.0-beta.4 — novembre 2026

**Une interface à la manière de ChessBase.** Un ruban d'onglets (Fichier, Accueil, Base, Partie, Moteur, Entraînement, Affichage, Aide) remplace la barre de menus ; les fenêtres de travail passent en onglets dessous. *Affichage → Interface classique* rend l'ancienne présentation.

**La fenêtre des bases** ouvre le logiciel : vos bases en icônes ou en liste, rangées par catégorie (mes bases, comptes, préparations, référence, études, étiquettes) ; un double-clic ouvre leurs parties.

**La fenêtre de partie** : la notation à droite de l'échiquier, le moteur et la référence dessous. *Affichage → Disposition d'origine* y ramène une disposition modifiée.

**Plus sobre.** Thème clair de bureau, gris bleuté et accent bleu, échiquier gris-bleu (l'ancien vert reste dans les palettes, « Feutre »).

**Lieu et ronde** dans la liste des parties, triables.

La bibliothèque passe au schéma 18 (deux colonnes ajoutées, remplies depuis vos parties) : une version antérieure de Trait64 ne l'ouvrira plus.

## 0.5.0-beta.3 — novembre 2026

**Préparer n'importe quel joueur.** Par pseudo Lichess ou Chess.com, par identifiant FIDE ou par nom de la bibliothèque : son arbre de coups avec chaque couleur, l'évolution de ses choix, ses faiblesses observées, là où il sort de votre répertoire, et un dossier PDF (*Base → Préparer un joueur…*).

**Une base de référence à jour.** Les rondes terminées des tournois retransmis sur Lichess s'ajoutent sans doublon, à la demande ou au démarrage ; la base élite de Lichess et TWIC s'importent depuis leur fichier téléchargé (*Base → Base de référence…*).

**Deux échiquiers côte à côte** (*Affichage*), le moteur et l'arbre suivant celui qu'on touche.

**Statistiques personnelles** dans la page Profils : force par cadence, performance par ouverture, régularité semaine par semaine.

**Synchronisation entre appareils** par un dossier partagé (Syncthing, Nextcloud, clé USB…), dans un paquet chiffré en AES-256 par une phrase de passe (*Fichier → Synchroniser entre appareils…*).

**Corrigé.** Avec beaucoup de lignes, le moteur pouvait montrer deux fois le même coup quand il réordonnait ses variantes, et l'analyse de fond rejetait parfois une position pour la même raison.

Pas de changement de schéma depuis la beta.2.

## 0.5.0-beta.2 — novembre 2026

**La synchronisation des comptes est réparée (Lichess comme Chess.com).** Après le premier compte, le classement ECO qui suit l'import bloquait la file : avec « Tout synchroniser », seul le premier compte se téléchargeait. La file repart désormais d'elle-même. Une base de compte supprimée se retélécharge entièrement au lieu d'être déclarée à jour.

**Les parties d'une position, comme dans ChessBase.** Le panneau *Parties* de l'analyse liste les parties de la bibliothèque passées par la position affichée, les mieux classées d'abord ; un double-clic ouvre la partie sur le coup où elle l'atteint.

**Jusqu'à 100 lignes du moteur** (5 auparavant), réglables en direct par − et + dans le panneau Moteur.

**Beaucoup plus fluide.** Parcourir une longue partie annotée coûte 40 fois moins (15,3 ms → 0,36 ms par coup sur 400 coups et 199 variantes), et les mises à jour du moteur sont regroupées à dix par seconde au lieu de saturer l'interface.

**Étiquettes** sur les bases, recueils et répertoires, avec un filtre par étiquette dans la page Études.

La bibliothèque passe au schéma 17 (une colonne ajoutée) : une version antérieure de Trait64 ne l'ouvrira plus.

## 0.5.0-beta.1 — novembre 2026

**L'analyse se range comme on veut.** Notation, arbre, livre, Lichess, notes, moteur et courbe sont des panneaux à onglets : on les glisse d'une pile à l'autre ou sur un bord pour ouvrir une colonne, à la manière de Scid vs. PC. Les dispositions s'enregistrent sous un nom ; échiquier, notation et courbe s'ouvrent aussi dans une fenêtre à part.

**Préparer un adversaire.** Sa fiche montre ses lignes régulières avec chaque couleur, l'évolution de ses choix année par année, et s'exporte en PDF.

**Les grandes bases.** Fusion des doublons avec leurs annotations, réparation d'une bibliothèque abîmée (l'original est gardé), comparaison de deux bases, tableau de tournoi avec Buchholz, variation d'Elo et catégorie FIDE ; la position de départ d'une base d'un million de parties s'ouvre aussitôt.

**Les études.** Sous-chapitres, liens `[[Titre]]` entre chapitres, recherche dans les commentaires, rapport PDF à composer, et diagramme lu dans une capture d'écran.

**Diffusion.** Paquet Flatpak, fichiers signés (vérifiables par `gh attestation verify <fichier> --repo SimzyGit/Trait64`), essais sous X11 et Wayland, échiquier jouable au clavier et annoncé aux lecteurs d'écran.

**Limites connues.** La lecture d'image ne lit que des diagrammes numériques nets, pas une photo prise de biais ni une feuille de notation manuscrite. Sous Wayland, hors GNOME et KDE, Qt 6.4 peut planter au démarrage : lancer avec `QT_IM_MODULE=compose`. L'import n'est mesuré que jusqu'à un million de parties.

Détail complet dans le [journal des changements](CHANGELOG.md).

## 0.4.0-beta.1 — 20 novembre 2026

**Première bêta.** Trait64 couvre désormais l'essentiel de ce que font ChessBase, Scid et En Croissant, et la comparaison s'étend à une quinzaine d'autres outils ([docs/COMPARISON.md](COMPARISON.md)).

**Une interface tenue d'un bout à l'autre.** Champs, listes, compteurs, cases à cocher et boutons ont la même hauteur, le même filet et le même focus. Les dialogues ont leurs boutons en bas à droite, à taille normale. Les onglets sont soulignés, les études ouvertes sont des onglets de document et le coup courant de la notation se lit à son fond teinté. La barre d'évaluation est blanche et noire comme l'échiquier. Les nombres des compteurs s'écrivent à la française.

**Des tournois de moteurs qu'on peut lire.** Le classement donne l'écart d'Elo avec sa marge d'erreur à 95 % et la probabilité que chaque moteur soit réellement plus fort (LOS), calculées comme dans Cute Chess et fastchess.

**Des bilans plus profonds et plus rapides.** Le bilan d'une partie vise une profondeur (18, 22 comme Lichess, 26 ou 30) et affiche celle qu'il atteint vraiment ; il prend tous les cœurs de la machine sauf un. Le bilan d'un profil garde le moteur ouvert d'une partie à l'autre, se met à jour à mesure que la file avance et dit sur combien de parties il porte.

**Un plantage en moins.** Ctrl + molette sur la bibliothèque pouvait fermer le programme ; c'est corrigé.

Détail complet dans le [journal des changements](CHANGELOG.md).

## Ouvertures, analyse et mise à jour — 6 novembre 2026

**Vos fichiers PGN se parcourent enfin partie par partie.** Une étude n'est pas
forcément un répertoire : un fichier tient souvent des parties commentées, qu'on
veut lire l'une après l'autre. Jusqu'ici elles étaient toutes fondues dans un
seul arbre de positions. À l'import, Trait64 demande désormais ce qu'est le
fichier — un recueil à parcourir, ou un répertoire à réviser. Un recueil liste
ses parties comme des chapitres, et un clic en ouvre une dans Analyse. Il est
aussi une base de la bibliothèque, nommée d'après le fichier : recherche,
filtres et bilan s'y appliquent comme au reste.

**La mise à jour depuis une version précédente fonctionne enfin.** Le canal par
défaut valait « stable » alors que toutes les versions publiées sont des
préversions de développement : aucune mise à jour ne pouvait donc être proposée
à personne, sur aucun système. Le canal suit désormais la version installée, et
le réglage que vous avez choisi garde la main. Windows et macOS, à qui aucun
paquet n'était reconnu, voient maintenant les nouvelles versions : Trait64 les
télécharge et en vérifie l'empreinte ; l'installation reste à faire à la main
sur ces deux systèmes.

**Vos parties sont classées par ouverture, même sans balise ECO.** La plupart
des fichiers PGN n'en portent aucune, et la recherche par ouverture les ignorait
en silence. Trait64 lit désormais les coups joués et attribue le code
correspondant, à l'import comme sur une base déjà constituée.

**Dossier d'une position.** Sur n'importe quelle position : les coups qui s'y
jouent, leur score, le même découpage selon la force des joueurs — un coup qui
réussit au club et disparaît au-dessus de 2300 ne se voit pas autrement — et les
parties les mieux classées qui y sont passées. Aucune partie n'y est passée ?
C'est une nouveauté pour votre bibliothèque, et le dossier le dit ainsi.

**Votre répertoire face à vos propres parties.** À quel coup vous quittez votre
préparation, dans quelle proportion, avec quel coup, et ce qui était prévu à la
place. Votre répertoire et vos parties sont au même endroit, sur votre machine :
c'est ce qui rend la question calculable.

**Un point coloré par coup** dans le carnet, à la manière de ChessBase : vert
pour un bon coup, blanc pour un coup correct, jaune pour une imprécision, orange
pour une erreur, rouge pour une gaffe, violet quand un mat entre dans le compte.
Il apparaît dès qu'un bilan a été passé.

**La barre d'évaluation ne retombe plus à zéro à chaque coup joué.** Le moteur
vide ses lignes dès qu'il change de position ; la dernière évaluation est
maintenant conservée le temps que la suivante arrive.

**Import d'études réparé.** Les dossiers exportés par En Croissant ou Pawn
Appétit contiennent souvent des PGN compressés : Trait64 les déplie, en
reconnaissant le format aux premiers octets plutôt qu'à l'extension. Un dossier
qui ne contient que des bases `.db3` le dit en les nommant, au lieu d'annoncer
qu'il n'a rien trouvé. Et importer un PGN n'exige plus d'avoir créé une étude :
le fichier en crée une, nommée d'après lui.

**Désinstallation.** `install.sh --uninstall` ne retirait que l'AppImage et
annonçait tout de même « Trait64 est désinstallé » à qui avait posé le paquet
Debian. Il passe désormais par apt-get, dit quand il ne trouve rien, et rappelle
où reste votre bibliothèque.

Également : seize palettes d'échiquier au lieu de neuf, plusieurs jeux de pièces
installables côte à côte, un portrait par profil, et l'interface anglaise qui ne
laisse plus échapper de phrases françaises — un test refuse désormais toute
phrase sans traduction.

## Précision, profils et trois systèmes — 5 novembre 2026

Trait64 s'installe désormais sur **Linux, Windows et macOS**. Sous Linux, une
seule commande suffit, sans compilation ni mot de passe :

```bash
curl -fsSL https://raw.githubusercontent.com/SimzyGit/Trait64/main/install.sh | bash
```

Le paquet Debian s'installe par `sudo apt install ./fichier.deb`, l'archive
Windows se décompresse, l'image macOS se glisse dans Applications. Les paquets
ne sont pas signés : SmartScreen et Gatekeeper demandent une confirmation.

**Précision à la manière de Lichess.** Le bilan d'une partie donne un
pourcentage de précision pour l'ensemble, puis séparément pour l'ouverture, le
milieu de jeu et la finale. Gaffes, erreurs et imprécisions sont situées et
comptées. Le calcul part des chances de gain, pas des centipions bruts.

**Bilan de toute une base.** Après une synchronisation ou un import, un bouton
passe en revue le lot entier à la profondeur choisie — Court, Moyen,
Approfondi, ou une valeur libre — sans refaire ce qui a déjà été vu aussi loin.

**Profils de joueur.** Un seul profil rassemble vos comptes Lichess et
Chess.com et votre fiche FIDE, dont les classements sont lus dans les listes
mensuelles officielles de la fédération.

**Tableau de bord des compétences.** Précision par phase, gaffes pour cent
coups, réussite tactique et progression mois par mois, agrégés sur vos parties.
La comparaison se fait avec vous-même, pas avec une moyenne inventée.

Corrections notables : l'import ne plante plus sous Windows, l'icône s'affiche
sur les trois systèmes, et le paquet Debian ne déclare plus deux dépendances
recommandées introuvables qui le faisaient refuser à l'installation.

## Carnet de coups côte à côte — 28 octobre 2026

Le lecteur de l'espace Analyse présente désormais une ligne par numéro de coup,
avec le demi-coup blanc et le demi-coup noir côte à côte. Chaque cellule reste
cliquable, la sélection suit la bonne position et les variantes restent séparées.
Les départs FEN avec les Noirs et les lignes incomplètes conservent leur numéro
et leur colonne. Les commentaires longs et annotations restent accessibles.

Le SAN français est une transformation d'affichage : PGN, études et coups
internes gardent leur format standard. Les exports existants, les pièces et le
schéma SQLite 7 sont préservés. Le défilement suit le coup après le calcul de
mise en page, y compris dans la page entière aux tailles maximales.

Validation locale : 14 suites sans suite ignorée, Stockfish réel, 207 contrôles
du carnet incluant six captures ; matrice existante de neuf configurations et
164 captures. Trois passages des interactions ne reproduisent pas l'ancien
échec isolé des filtres ; sa cause reste inconnue. Rapports et limites :
https://github.com/SimzyGit/Trait64/blob/main/docs/MOVE-NOTEBOOK.md

À 960 × 640 avec texte 150 % / interface 125 %, la page reste défilante.
Les essais logiciels hors écran ne valident pas Voyager/Wayland/GPU.
Aucun benchmark 100k supplémentaire et aucune migration de base.

## Refonte native — dev.64, 28 octobre 2026

Navigation latérale compacte ou développée, grand échiquier à l’accueil, panneaux adaptables et palette de commandes au clavier. Police, texte de 90 à 150 %, interface de 90 à 125 %, contraste renforcé, animations réduites et libellés français/anglais sont persistés.

Corrections : commandes coupées à petite taille ou en grandes polices, blocage du profil Joueurs vide, titre d’aide tronqué, choix de promotion et couleurs des dialogues. Les valeurs des listes restent stables après changement de langue. Board.qml, Piece.qml et le schéma SQLite 7 sont conservés.

Les 13 suites passent sous Debian 12/13 et Qt ASan/UBSan ; le cœur passe également sous sanitizers. Paquets Debian installés et AppImage démarrés. Localement : 1 180 contrôles incluant ceux des captures, 9 configurations, 164 images et zéro avertissement QML. Tag, cinq fichiers publiés, tailles, SHA-256 et sélection de la mise à jour depuis dev.57 vérifiés ; aucune archive Actions. [Rapport](VALIDATION.md).

Essais en rendu logiciel hors écran ; aucun essai matériel Voyager/Wayland/120 Hz. Certains diagnostics C++ et descriptifs de catalogue restent français. Un échec local isolé de préparation est documenté dans [local.json](VALIDATION.md), non reproduit lors des relances ni dans les CI finales ; diagnostic ajouté, assertion inchangée. Aucun nouveau benchmark de grandes bases.

## Vérification de la base en arrière-plan — 25 octobre 2026

- « Vérifier la base » utilise désormais une connexion SQLite dédiée en arrière-plan. L'état est affiché et le bouton « Interrompre » permet d'annuler, en conservant l'étude ouverte.
- Les tâches qui se chevauchent sont refusées. Une interruption ou une erreur ne peut pas être présentée comme une vérification réussie. La relance immédiate depuis une notification de fin conserve le bon résultat.
- Douze suites locales et CI Debian 12/13 réussissent, avec contrôles ASan/UBSan et démarrage des paquets. Schéma 7 inchangé.
- La qualification séparée de dev.55 a importé 100 000 parties avec Qt sans avertissement QML, réindexé 10 000 parties et vérifié tous les PGN de la sauvegarde et de la restauration. Résultats et limites dans `docs/VALIDATION.md` ; aucun essai Voyager ni de plusieurs millions de parties n'est revendiqué.

La sauvegarde manuelle et le contrôle préalable à l'ouverture d'une bibliothèque restent des opérations synchrones distinctes. Détails : `docs/ASYNC-INTEGRITY.md`.

## Import PGN accéléré — dev.55, 24 octobre 2026

- Recherche des doublons par l'index des empreintes existant, et calcul du matériel sans expression régulière recompilée pour chaque position. Schéma 7, clés persistées et transactions conservés.
- Sur les mêmes 10 000 parties réelles, l'essai conservé passe de 93,114 à 42,498 secondes (−54,4 %). Les données échiquéennes et agrégats comparés restent identiques ; ce gain ne prédit pas chaque machine ou corpus.
- 100 000 parties importées localement avec pause/reprise, intégrité, sauvegarde et restauration contrôlées. Un pic de lecture concurrente de 4,031 secondes reste à caractériser. L'interface fait l'objet d'une mesure Qt distincte.
- Douze suites passent sous Debian 12/13 et avec Qt ASan/UBSan ; paquets installés et AppImage démarrés. Fichiers de release et sélection de la mise à jour depuis dev.52 vérifiés. Aucun essai Voyager/Wayland matériel ni de plusieurs millions de parties n'est revendiqué.

Détails et limites : [PERFORMANCE.md](PERFORMANCE.md), [TESTING.md](TESTING.md).

## Audit global de fiabilité — dev.52, 21 octobre 2026

- Récupération des sessions endommagées sans perte de l’original ; chemins restaurés uniquement depuis les variantes enregistrées.
- Annulation de réindexation atomique, même au dernier signal de progression.
- Compteurs FEN protégés ; refus d’un dépassement avant modification de l’étude.
- Anciennes réponses moteur exclues après une nouvelle recherche ou un arrêt réentrant ; options automatiques adaptées aux bornes du moteur.
- Chaînes UCI vides corrigées : désactiver le journal ne tente plus d’ouvrir un fichier `<empty>` et ne fait plus quitter Stockfish dans un dossier protégé.
- Fermeture sûre pendant une connexion GitHub ; correction d’un accès à de la mémoire libérée.
- Distinction entre fin de partie et nulle réclamable dans les analyses ; priorité correcte du pat.
- Anciennes entrées de cache marquées à tort comme fin de partie recalculées puis remplacées. Relancer une analyse pour actualiser un bilan historique.
- Cadence PGN cohérente avec l’horloge et protégée des dépassements entiers.
- Stress FEN/PGN reproductible et contrôles ASan/UBSan étendus à tous les modules dans la CI.

Publié dans [v0.3.0-dev.52](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.52). Douze suites réussies sur Debian 12 et 13, contrôles ASan/UBSan, paquets installés et AppImage validés. Rapport et limites : [DEBUG-AUDIT.md](DEBUG-AUDIT.md). Schéma 7 conservé.

## Motifs tactiques et repères sur l’échiquier

- Fourchettes/attaques multiples, clouages au roi, enfilades du roi, échecs à la découverte et doubles échecs détectés dans les suites légales du bilan.
- Pièces et cases explicites ; captures qui exposeraient le roi de l’attaquant exclues des cibles de fourchette. Un motif observé n’est pas présenté comme un gain forcé.
- Copie d’étude annotée : flèches vertes pour les attaques, marques jaunes pour les alignements, conservation dans le PGN exporté et étude d’origine préservée.
- Tests des faux positifs, sous-promotions, prise en passant, Chess960 et comparaison au générateur de coups légaux. Aucun changement de schéma.

Guide : `docs/TACTICAL-MOTIFS.md`. Les attaques à la découverte sans échec, plans stratégiques et sacrifices justifiés restent à développer.

## Comprendre les coups du bilan

- Deux suites consultables pour chaque coup analysé : coup joué suivi de la réponse moteur, et proposition moteur avant le coup.
- Captures et cases réelles, prise en passant, promotions, roques classiques/Chess960, échecs, mats et pats déterminés en rejouant uniquement des coups légaux.
- Comparaison du matériel présent, distincte de l’évaluation moteur ; nom du moteur, budget et profondeur affichés. Aucun niveau Elo estimé.
- Ouverture de la suite annotée dans une étude indépendante ; retour à l’étude d’origine avec ses résultats conservés.
- Limites explicites : suite moteur, pas preuve d’une défense forcée ; 128 demi-coups maximum, départ depuis une FEN sans historique des répétitions. Les thèmes tactiques et plans stratégiques restent à approfondir.

Guide : `docs/MOVE-EXPLANATIONS.md`.

## Nouveautés du lot répertoires

- Création, fusion, import/export des répertoires en arrière-plan, progression et annulation sans ajout partiel. Notes et révisions préservées.
- Statistiques et réponses adverses manquantes calculées sans bloquer l’interface ; résultats périmés ignorés.
- Analyse de toutes les variantes d’un répertoire par profil moteur, dans la file persistante.
- Correction du nom sélectionné à l’ouverture du dialogue et des notifications en petite fenêtre ; changement de bibliothèque refusé pendant un exercice.
- Tests Debian 12/13, DEB installés et AppImage puis publication directe dans GitHub Releases, sans archives Actions. Manifeste et SHA-256 vérifiés ; une version publiée n’est pas écrasée.
- Publication des brouillons : utilisation de la réponse de création et de l’identifiant exact, attente bornée si l’index GitHub tarde à refléter la création. Aucun brouillon dupliqué et aucun assouplissement des vérifications.

## Analyses en arrière-plan et reprise

- File persistante accessible dans le menu de l’espace Analyse : moteur courant ou profil enregistré, MultiPV, temps par position et toutes les variantes.
- Pause/reprise, récupération après interruption, résultats conservés à chaque position et étude courante indépendante.
- Évaluations, WDL fournis par le moteur et variantes légales ; cache avec provenance et export PGN annoté, sans modifier la ligne réellement jouée.
- Les mats restent des scores de mat et ne gonflent plus les pertes en centipions du bilan.
- Migration 7 avec sauvegarde préalable. Tests Stockfish réels, arrêt brutal, changement de moteur, Chess960, pagination et édition simultanée.

Guide : `docs/ANALYSIS-QUEUE.md`. Les réseaux NNUE ou autres ressources non identifiables désactivent seulement le cache ; ils n’empêchent pas l’analyse.

## Import PGN parallèle et compatibilité des corpus anciens

- **Lecteurs parallèles réglables de 1 à 8** dans les imports reprenables, avec conservation de l'ordre des parties, de la pause/reprise et du point de sauvegarde. Deux lecteurs sont proposés par défaut.
- **PGN anciens** : acceptation d'une origine de pièce redondante, telle que `Rd1d2`, uniquement lorsqu'elle identifie un seul coup légal. Les coups ambigus, fausses captures et origines incorrectes sont refusés.
- **Validation sur 10 000 parties réelles** : résultats identiques à 1 et 4 lecteurs, pause à 5 000 puis reprise, intégrité et sauvegarde vérifiées. Les corpus de plusieurs millions de parties restent à qualifier.
- **Dix suites automatisées** dans la CI Linux, avec contrôles des sorties du benchmark pour protéger les bases, journaux et sauvegardes existants. Aucun changement de schéma de données dans ce lot.

## Imports reprenables, répertoires et préparation

- **Imports PGN reprenables** : progression, vitesse et estimation du temps restant ; pause puis reprise après fermeture ou interruption. Les parties et leur point de reprise sont validés ensemble par lots de 250. Le lot incomplet est annulé.
- **Vérification des fichiers** : SHA-256 complet avant chaque reprise, possibilité de retrouver un PGN déplacé, refus d’un fichier modifié et verrou entre instances. Retirer une tâche ne supprime pas ses parties.
- **Répertoires personnels par position** : nom, camp, classique ou Chess960, transpositions partagées, coup principal et alternatives, commentaires/NAG, parties modèles, fusion d’études et import/export PGN.
- **Répétition espacée des répertoires** : toutes les réponses préparées sont acceptées ; calendrier partagé entre transpositions, révision libre et suivi des rappels manqués. Quitter un exercice restaure l’étude.
- **Couverture locale** : réponses adverses absentes du répertoire, classées par fréquence dans la bibliothèque. Leur absence ne prouve pas une nouveauté théorique.
- **Préparation par position** : filtres combinés par base, joueur exact/couleur, période et moyenne des Elo renseignés ; calcul annulable en arrière-plan, parties exemples et export CSV avec FEN et filtres.
- **Migrations SQLite 5 et 6** avec sauvegarde des bases précédentes. Neuf suites automatisées couvrent notamment l’arrêt réel d’un processus pendant un import, les transpositions, les calendriers et les interactions QML.

Guide : `docs/REPERTOIRES-IMPORTS.md`.

## Fonctions existantes conservées

- Trois moteurs téléchargeables avec SHA-256 épinglé : Stockfish 19, Arasan 26 et PlentyChess 8 ; profils et options UCI indépendants. PlentyChess nécessite glibc 2.38 ou plus.
- 42 collections PGN optionnelles : champions classiques, titres FIDE séparés, championnes disponibles, matchs et ouvertures ; conditions du fournisseur affichées.
- Bases nommées, dédoublonnage et appartenance à plusieurs bases, historique, import/export, recherche multicritère annulable et pages de 200 parties par curseur.
- Analyse UCI, MultiPV, comparaison de 2 à 4 moteurs, graphe et bilan de la ligne principale, cartes d’entraînement existantes, annotations et personnalisation.
- Mises à jour GitHub Releases en arrière-plan : Stable/Bêta/Développement, nouveautés, Mettre à jour/Plus tard/Ignorer, contrôle SHA-256 et sauvegarde avant installation.
- Accès aux dépôts privés facultatif ; aucun jeton intégré. Remplacement atomique AppImage et installation Debian avec autorisation système.
- Sauvegardes automatiques toutes les six heures, 14 instantanés, sauvegardes pré-mise à jour séparées et préservation des sessions illisibles.
- CI Debian 12/13, tests mémoire du cœur, paquets .deb et AppImage x86_64. Une modification ordinaire de main produit uniquement une préversion Développement après validation.

## Limites de cette préversion

La série 0.3 reste en alpha. Le catalogue complet n’est pas achevé. Les imports ont une reprise fiable, mais les performances sur 1/2/5/10 millions de **PGN réels** ne sont pas encore qualifiées. L’import PGN d’un répertoire reste limité à 16 Mio et 200 parties ; l’édition des coups, le renommage, la suppression et les révisions restent synchrones. Les transpositions sont développées une seule fois lors de l’export ou de la préparation d’une analyse.

Les rappels manqués mesurent la mémorisation, pas la qualité des coups ni un niveau Elo. Les statistiques reposent sur les métadonnées connues des parties. La comparaison de répertoires, les explications tactiques approfondies, les cours de finales/calcul/visualisation, les formats supplémentaires et la reconnaissance d’images restent à développer. Un essai sur le PC Voyager cible reste nécessaire.

Les installations 0.1/0.2 nécessitent un premier passage manuel à 0.3, car elles ne disposent pas du module de mise à jour. Voir `docs/PROJECT-STATUS.md`, `docs/ROADMAP.md` et `docs/PERFORMANCE.md` pour les limites et la reprise précise.
