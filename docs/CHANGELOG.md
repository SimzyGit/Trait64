# Changements

## L'échiquier fluide : pièces qui glissent, moteur trouvé tout seul — 12 novembre 2026

- **Stockfish déjà installé est trouvé.** Trait64 ne regardait que le `PATH`, où `/usr/games` — l'endroit où Debian et Ubuntu installent Stockfish — ne figure pas pour une application lancée depuis le bureau. Il fouille maintenant les dossiers du système, `~/.local/bin`, Snap, Flatpak, Téléchargements, le dossier personnel et les moteurs du catalogue. Un bouton **Détecter** relance la recherche, et un chemin devenu invalide est remplacé au démarrage.
- **On peut glisser les pièces.** Le panneau défilant qui porte l'échiquier reprenait la souris au bout de quelques pixels : tout glisser était annulé avant d'avoir commencé, seul le clic-clic marchait.
- **Les pièces glissent vers leur case** quand un coup est joué, repris ou parcouru au clavier, roque compris. Une pièce posée à la main ne rejoue pas son trajet ; « Réduire les animations » coupe l'effet.
- **Un coup s'affiche en 7 ms au lieu de 55.** Chaque coup — et chaque seconde de pendule — faisait relire tout l'état de l'application à chaque élément de l'interface, redessiner les 64 cases et rebâtir la notation. Seules les cases et les valeurs qui changent sont désormais annoncées, et la notation n'est plus refaite quand on ne fait que parcourir la partie.
- **Flèches du moteur** sur l'échiquier pendant l'analyse : le meilleur coup, et plus discrètement les deux suivants (Apparence → Flèches du moteur).
- **Lecture automatique** de la partie : bouton ▶▶ ou barre d'espace.
- **Haut et bas passent d'une variante à l'autre**, comme gauche et droite passent d'un coup à l'autre.
- Le glisser, le lâcher interdit, le clic-clic, la lecture et le temps d'affichage d'un coup sont rejoués à vraie souris à chaque version.

## Le moteur sans bride, et les études réparées — 12 novembre 2026

- **L'analyse n'a plus de plafond de profondeur.** Le moteur s'arrêtait d'office à la profondeur 24, qu'une machine d'aujourd'hui atteint en quelques secondes — puis il ne faisait plus rien, quoi qu'on attende. Il creuse désormais tant que la position reste affichée, et cède la place dès qu'elle change. Une profondeur fixe reste possible dans les réglages.
- **Toute la machine est mise à sa disposition** : plus de plafond à 32 fils de calcul, et la table de transposition passe d'un huitième de la mémoire plafonné à 4 Go à un quart, jusqu'à 32 Go. C'est elle qui permet d'aller profond sans recalculer. Les fils sont bornés au nombre réel de cœurs et la table aux trois quarts de la mémoire : au-delà, le moteur ralentit au lieu d'accélérer.
- **Les anciens réglages par défaut sont relevés une fois** au premier lancement, y compris dans les profils de moteur enregistrés. Un réglage que vous aviez choisi vous-même n'est pas touché.
- **Dans un recueil, la liste des parties n'avait plus un pixel de large** : l'aperçu prenait toute la place, et il n'y avait rien à cliquer — donc jamais de position affichée. Corrigé.
- **« Lire ce fichier » montre le recueil une fois l'import fini.** La page restait vide, et il fallait deviner qu'un second clic ferait apparaître les parties.
- Un recueil ouvert pendant un import se met à jour au lieu de rester figé ; créer un fichier se valide à la touche Entrée.
- Tout ce parcours — choisir un dossier, déplier, lire, parcourir, créer, renommer, corbeille, recherche — est maintenant rejoué à vrais clics à chaque version.

## Les mises à jour se proposent d'elles-mêmes, y compris en cours de route — 12 novembre 2026

- **Une version publiée pendant que Trait64 tourne est proposée sans redémarrer.** La vérification n'avait lieu qu'au lancement : un logiciel laissé ouvert plusieurs jours ne l'apprenait qu'au suivant. Elle est refaite toutes les six heures.
- **Une mise à jour en attente se voit dans la barre du bas** : le bouton de version passe en couleur et la nomme, même après avoir fermé la proposition par « Plus tard ».
- Vérifié sur les versions réellement publiées : AppImage, paquets Debian 12 et 13, Windows et macOS reçoivent bien la dernière.

## Le livre de Lichess dans l'analyse, les finales en ligne — 11 novembre 2026

- **Un onglet « Lichess » à côté de Notation, Arbre et Livre** : ce qui se joue dans la position affichée, lu en direct dans les parties classées de Lichess ou dans celles des maîtres. Il suit l'échiquier coup après coup, un clic sur une ligne joue le coup, et les parties en vue se rapatrient d'un clic. Les positions déjà demandées sont gardées en mémoire : parcourir une partie ne redemande pas les mêmes à chaque passage.
- **Les tables de finales à sept pièces, sans rien télécharger** : dans le même onglet, le verdict exact de la position et de chaque coup — gagne, nulle, perd, avec la distance au mat. Les tables Syzygy locales pèsent des dizaines de gigaoctets à six pièces et ne vont pas à sept.
- **Un lien de partie Lichess se colle là où l'on colle un PGN** : la partie est rapatriée avec ses pendules et ses évaluations.
- **L'AppImage dit pourquoi elle ne démarre pas.** Sur un système trop ancien ou privé d'une bibliothèque graphique, elle nomme ce qui manque et le paquet à installer selon la distribution, dans une fenêtre si elle est lancée depuis le menu. Avant, c'était une ligne de l'éditeur de liens, ou rien.
- **La compatibilité est mesurée, plus supposée** : l'AppImage demande la glibc 2.36, et chaque version publiée est relancée sur Debian, Ubuntu, Mint, Fedora, Arch, Manjaro et openSUSE. Ubuntu 22.04 et les RHEL 9 sont trop anciens, et la documentation le dit.

## Un vrai gestionnaire de fichiers d'études — 11 novembre 2026

- **L'explorateur est devenu un arbre** : les dossiers se déplient sur place, avec leurs traits de niveau et leurs icônes, et ceux que vous avez ouverts le restent d'une session à l'autre. Avant, on entrait dans un dossier à la fois et on perdait de vue le reste.
- **Recherche dans toute l'arborescence** : elle descend partout, ne garde que ce qui correspond et montre les dossiers qui y mènent.
- **Créer un fichier PGN** — vide, avec une partie vierge prête à étudier, ou à partir d'un PGN collé — et **créer un dossier**, là où l'on se trouve.
- **Renommer, déplacer, mettre à la corbeille**, par les boutons qui apparaissent au survol d'une ligne ou depuis la fiche de l'entrée choisie. La corbeille est celle du système : un clic de trop se rattrape. Là où elle n'existe pas, rien n'est supprimé.
- **Le recueil suit son fichier** : renommé ou déplacé — lui ou le dossier qui le contient —, le fichier rouvre son recueil au lieu d'être relu en double.
- **Une fiche pour l'entrée choisie** : ce qu'elle est, sa taille, sa date, si elle est dans la base, et ce qu'on peut en faire. Un clic choisit, un double-clic ouvre.
- **Les bases `.db3` d'En Croissant se lisent.** Elles notent chaque coup par son rang dans la liste des coups légaux de la bibliothèque Shakmaty ; cet ordre a été reconstruit et éprouvé contre Shakmaty lui-même sur 750 000 positions, sans un écart. Au-delà d'un million de parties l'import est refusé : chaque position étant indexée, il faudrait des heures et des dizaines de gigaoctets.

## L'explorateur Lichess en ligne — 7 novembre 2026

- **Le dossier d'une position se lit maintenant dans trois bases** : la vôtre, les parties classées de Lichess — elles se comptent en milliards — et celles des maîtres sur l'échiquier. Les deux dernières sont interrogées en direct sur l'explorateur d'ouvertures de Lichess : rien n'est téléchargé, rien n'est stocké, et la réponse arrive en une fraction de seconde quelle que soit la taille de la base.
- **Les mêmes colonnes qu'avant**, plus le classement moyen des joueurs qui jouent ce coup. Pour Lichess, deux filtres : la cadence et le niveau.
- **Les parties en vue se rapatrient d'un clic** dans votre bibliothèque, avec leurs pendules et leurs évaluations.
- Les données viennent de l'explorateur d'ouvertures de Lichess, publiées en CC0.

## Explorateur d'études — 7 novembre 2026

- **Vos fichiers d'études se parcourent sur place**, comme dans En Croissant : un onglet *Explorateur* montre le dossier du disque où ils vivent — sous-dossiers, taille, et la marque de ceux qui sont déjà dans la base avec leur nombre de parties. Jusqu'ici, un fichier existait pour Trait64 une fois importé, et pas avant.
- **Un clic lit le fichier, les suivants rouvrent son recueil.** Relire doublait les parties ; le chemin du fichier est maintenant retenu avec le recueil, et c'est lui qui les relie.
- **Les bases des autres logiciels sont montrées et nommées** (`.si4`, `.cbh`, `.db3`…) au lieu d'être tues : on sait pourquoi le dossier paraissait vide, et quoi faire.
- **Le dossier d'études est retenu d'une session à l'autre**, et l'explorateur s'ouvre dessus.
- **Créer un dossier et y enregistrer l'étude ouverte en PGN**, sans quitter l'onglet : les fichiers restent la référence, lisibles par n'importe quel logiciel.
- **Deux fichiers du même nom dans deux dossiers font deux recueils.** Le second se versait silencieusement dans le premier, parce que l'identifiant ne retenait que le nom du fichier.
- Un chemin Windows `C:/Études` était refusé comme une adresse Internet : sa lettre de lecteur passait pour un schéma d'URL.
- **Le test de mise à jour ne cherchait qu'un fichier Linux** : depuis que Windows et macOS reçoivent eux aussi un paquet, il échouait chez eux faute de trouver la pièce jointe à son nom, et l'intégration continue restait rouge. Il éprouve maintenant la remise du fichier là où Trait64 n'installe pas lui-même.

## Parcourir sans quitter des yeux — 6 novembre 2026

- **Un échiquier d'aperçu dans la bibliothèque et dans les recueils.** Un clic sur une ligne montre la partie à côté de la liste : on fait défiler ses coups à la molette, aux flèches ou aux boutons, sans changer d'onglet. Jusqu'ici, regarder une partie coûtait l'étude ouverte dans Analyse.
- **Un double-clic ouvre dans Analyse**, comme avant le simple clic : regarder et travailler ne demandent plus le même geste.
- **La base ou le recueil se parcourt depuis l'analyse** : `◀ 3 / 48 ▶` dans la barre d'outils, ou `Alt+←` et `Alt+→`. La partie voisine remplace celle de l'onglet courant au lieu d'en ouvrir un dix-septième, et la liste parcourue est celle de la bibliothèque — filtres et tri compris.

## Recueils de parties — 6 novembre 2026

- **Une étude n'est plus forcément un répertoire.** Un fichier PGN tient souvent des parties commentées, qu'on veut lire l'une après l'autre : jusqu'ici elles étaient toutes fondues dans un seul arbre de positions, et aucune ne se parcourait. À l'import, Trait64 demande maintenant ce qu'est le fichier — un **recueil de parties** à parcourir, ou un **répertoire** à réviser.
- **Un recueil liste ses parties comme des chapitres** : joueurs, événement, résultat et date ; un clic en ouvre une dans Analyse, avec ses variantes et ses commentaires.
- **Un recueil est aussi une base de la bibliothèque**, nommée d'après le fichier : recherche, filtres, classement par ouverture et bilan s'y appliquent comme au reste.
- L'import d'un dossier propose le même choix, et crée un recueil par fichier, sous-dossiers compris.

## Ouvertures — 6 novembre 2026

- **Les parties sans balise ECO sont classées** d'après les coups joués, à l'import comme sur une base déjà constituée. Sans cela, `eco` ne valait que ce que le PGN portait — et la plupart n'en portent rien : la recherche par ouverture les ignorait en silence.
- **Dossier d'une position** : les coups joués ici, leur score, le même découpage selon la force des joueurs, et les parties les mieux classées qui y sont passées. Zéro partie se lit comme une nouveauté pour votre bibliothèque, pas comme une panne.
- **Le classement des ouvertures ne fige plus la fenêtre** : mesuré à deux secondes sur cent mille parties, il tourne désormais en arrière-plan, et n'interrompt plus une recherche de préparation en cours.
- **Comparer un répertoire à ses propres parties** : où vous quittez votre préparation, à quel coup, avec quel coup, et ce qui était prévu à la place.

## Analyse et études — 6 novembre 2026

- **Désinstallation** : `--uninstall` ne retirait que l'AppImage et annonçait quand même « Trait64 est désinstallé » à qui avait posé le paquet Debian. Il retire maintenant le paquet par apt-get, dit quoi faire s'il ne trouve rien, et rappelle où est restée la bibliothèque.
- **Le logo ne se répète plus dans la fenêtre** : il est celui du bureau et de la barre des tâches, le remettre sur l'accueil et dans la barre de bas n'ajoutait rien.
- **La mise à jour depuis une version précédente fonctionne.** Le canal par défaut était « stable » alors que toutes les versions publiées sont des préversions de développement : aucune mise à jour ne pouvait donc jamais être proposée. Le canal suit désormais la version installée, et le réglage choisi garde la main.
- **Windows et macOS voient enfin les nouvelles versions.** Aucun paquet ne leur était reconnu, et la vérification échouait avant même de chercher. Trait64 télécharge et vérifie maintenant le fichier ; son installation reste à faire à la main sur ces systèmes.
- **Point de qualité de coup**, à la manière de ChessBase : vert pour un bon coup, blanc pour un coup correct, jaune pour une imprécision, orange pour une erreur, rouge pour une gaffe, violet quand un mat entre dans le compte. Il apparaît dans le carnet de coups dès qu'un bilan a été passé.
- **La barre d'évaluation ne retombe plus à zéro à chaque coup.** Le moteur vide ses lignes dès qu'il change de position ; la dernière évaluation est maintenant gardée le temps que la suivante arrive, et la barre glisse de l'une à l'autre.
- **Import d'un dossier d'études réparé.** Les études exportées par En Croissant ou Pawn Appétit arrivent souvent compressées : Trait64 les déplie désormais, en reconnaissant le format aux premiers octets et non à l'extension. Un dossier qui ne contient que des bases `.db3` le dit, au lieu d'annoncer qu'il n'a rien trouvé.
- **Importer un PGN n'exige plus d'avoir créé une étude** : le fichier en crée une, nommée d'après lui.

## Profils, études et confort — 5 novembre 2026

- **Page Profils** : chaque joueur suivi a sa page, avec « Analyser le profil » qui passe en revue les parties synchronisées encore sans bilan et alimente le tableau de bord.
- **Onglet Études**, à la manière d'En Croissant et de Pawn Appétit : import d'un dossier entier, y compris les études imbriquées les unes dans les autres.
- **Ranger par ouverture** (Bibliothèque) : chaque partie reçoit la collection de sa famille d'ouverture d'après son code ECO, sans rien télécharger. Les parties sans code ECO restent inchangées.
- **Pièces et échiquiers** : seize palettes d'échiquier au lieu de neuf, dont sept qui tiennent dans la pénombre, chacune avec un aperçu de ses deux cases. On peut désormais garder plusieurs jeux de pièces installés et passer de l'un à l'autre, au lieu qu'un import remplace le précédent.
- **Barre d'évaluation** le long de l'échiquier, et courbe de la partie au-dessus du moteur.
- **Portrait** : chaque profil peut recevoir une photo ; Trait64 en range une copie réduite dans son dossier de données, le profil ne dépend donc pas d’un fichier que l’on peut déplacer. Sans photo, les initiales tiennent la place.
- **L'interface anglaise parle anglais partout** : les cent soixante messages du logiciel étaient écrits en français dans le code et le restaient quand on choisissait l'anglais. Un test refuse désormais toute phrase sans traduction.
- **Fluidité** : les lignes de la bibliothèque et du carnet de coups sont recyclées au défilement plutôt que rebâties une à une, et les pièces ne sont plus redessinées à chaque pixel quand on tire une poignée de panneau.
- **Titre et drapeau** : le titre FIDE se détache du nom comme sur Lichess, et le drapeau de la fédération accompagne le joueur.
- Le logo sert désormais d'icône de l'application, sur les trois systèmes.
- Plus de bridage du moteur : fils d'exécution et table de hachage suivent la machine.
- Corrections : les colonnes de la bibliothèque ne tombaient pas sous leurs titres ; le survol d'un menu peignait le texte en noir sur noir ; le `.deb` était refusé par les installateurs graphiques de Debian ; Windows et macOS annonçaient une version de développement.

## Bilan, comptes en ligne et installation — 3 novembre 2026

- **Bilan en deux passages** : un premier passage rapide va du début vers la fin, puis un passage complet remonte de la fin vers le début. Le moteur garde sa table de hachage d'une position à l'autre : en remontant, il connaît déjà la suite de la partie, ce qui rend les évaluations des positions précédentes plus fiables (c'est ce que font les analyses « backward » de ChessBase et SCID). Un seul passage reste possible.
- **Comptes Lichess et Chess.com** (Fichier → Comptes) : enregistrez vos pseudos, Trait64 ne récupère à chaque synchronisation que les parties jouées depuis la précédente, à la demande ou au démarrage. Retirer un compte garde ses parties.
- **Installation en une commande** : `install.sh` télécharge la dernière version publiée, vérifie son empreinte SHA-256 et l'ajoute au menu des applications, sans sudo. README refait avec des captures à jour.
- Carnet en tableau : l'étiquette « Variante » n'apparaît plus qu'au début de chaque variante.

## Interface classique — 3 novembre 2026

L'interface suit désormais la disposition que connaissent la plupart des joueurs (ChessBase, SCID, analyse Lichess), modernisée : barre de menus, onglets d'espaces de travail, grand échiquier encadré des joueurs (couleur, nom, Elo ou pendule), notation, arbre et livre à droite, moteur dessous, bibliothèque en tableau dense. Thème clair par défaut, gris neutres et un seul accent ; le thème sombre et les tailles restent réglables. L'arbre d'ouvertures montre une barre de résultats Blancs / nulles / Noirs par coup. Le carnet garde des colonnes compactes sur grand écran.

Corrections : changer de langue renvoyait sur l'Accueil ; sur une petite fenêtre, la rangée d'onglets poussait des boutons hors de l'écran.

## Petits ajouts — 3 novembre 2026

- **Écriture de livres Polyglot** : Bibliothèque → « Exporter un livre Polyglot » écrit un `.bin` des coups joués au moins deux fois, pondérés par le nombre de parties. Les clés du schéma 8 sont des clés Polyglot ; un aller-retour écriture/lecture est testé, roques compris.
- **Diagrammes SVG** : l'export de diagramme accepte `.svg` en plus de `.png`, avec la même apparence, les flèches et les cercles. Les pièces sont des groupes transformés, lisibles par les moteurs SVG Tiny comme par les navigateurs.
- **Horloges et évaluations PGN** : `[%clk]` et `[%eval]` (parties Lichess) s'affichent à côté du coup ; `[%cal]`/`[%csl]` ne polluent plus les commentaires. Le PGN enregistré reste inchangé.

## Analyse en lot et nouveautés — 3 novembre 2026

- **Analyse des parties affichées** : la file d'analyse reçoit en une fois jusqu'à 200 parties correspondant aux filtres de la bibliothèque (lignes principales), avec le budget et le profil choisis.
- **Chercher la nouveauté** (menu de l'étude) : premier coup de la ligne principale qu'aucune autre partie de la bibliothèque n'a joué depuis cette position, avec le nombre de parties qui l'atteignent et les suites connues. Les coups de la partie elle-même ne comptent pas.

## Dossier adversaire — 3 novembre 2026

La fiche joueur devient un dossier de préparation : lignes réellement jouées sur les six premiers demi-coups (Blancs et Noirs, avec fréquence et score), résultats selon la longueur de partie, ouvertures à cibler (plus faibles scores sur au moins trois parties terminées) et courbe d'Elo. Un bouton importe directement les parties publiques d'un pseudo Lichess ou Chess.com.

## Puzzles tactiques — 3 novembre 2026

Banque de puzzles locale (Entraînement → Puzzles) : import direct de l'export Lichess (CSV, compressé `.zst` ou non, licence CC0) avec plafond et plage de classement, et puzzles tirés de vos erreurs analysées (Bilan → « Créer des puzzles des erreurs »). Chaque puzzle est choisi près de votre classement (Elo, K = 32), parmi ceux non résolus, par thème (mat en 1/2/3, fourchette, clouage…) et origine. Au dernier coup, tout mat est accepté ; un indice ou la solution comptent comme un échec. Chrono et historique des tentatives conservés.

## Schéma 8 : positions en clés Zobrist — 3 novembre 2026

Les quatre tables de positions (`positions`, `all_positions`, `opening_edges`, `opening_stats`) utilisent une clé entière de 64 bits au lieu de la FEN texte, en tables sans `rowid`. Sur 20 000 parties générées (≈ 1,9 million de positions, mêmes fichier et machine) : **1 010 Mo → 413 Mo (−59 %)**, import 192 s → 189 s, recherche par position 2,79 → 1,74 ms, arbre d'ouvertures 0,05 → 0,03 ms. Ces mesures ne remplacent pas la qualification sur plusieurs millions de parties réelles.

**Recherche par structure de pions** : dans la bibliothèque, « Même structure de pions » retrouve les parties qui atteignent la structure de l'échiquier, quelles que soient les autres pièces (clé indexée, calculée aussi pour les bases migrées).

La migration sauvegarde d'abord la base (`.before-v8-…`), convertit les clés existantes sans relire les PGN, puis compacte le fichier. Deux positions ne diffèrent plus par une case en passant impossible à jouer, comme le veut la règle de répétition. Une collision de clés 64 bits est théoriquement possible mais négligeable (de l'ordre d'une chance sur cinquante pour un milliard de positions distinctes) ; elle ne pourrait qu'ajouter une partie à un résultat de recherche.

## Gains rapides — 3 novembre 2026

- **Livres Polyglot** : onglet Livre à côté de l'Explorateur, part de chaque coup ; chemin du `.bin` dans Paramètres. Recherche dichotomique dans le fichier, clés vérifiées contre les valeurs de référence du format.
- **Import Lichess / Chess.com** par pseudo, sans compte, depuis la fenêtre des imports. Lichess ne récupère ensuite que les nouvelles parties ; les parties sont rangées sous une source par joueur.
- **Force du moteur en Elo** dans la partie locale (`UCI_LimitStrength` + `UCI_Elo`, bornés à la plage du moteur) ; repli sur le niveau 0–20 si le moteur ne le propose pas.
- **Carnet de coups continu** en option (ordre PGN, variantes entre parenthèses) et **notation figurine** (♘ ♗ ♖ ♕ ♔), affichage seulement.
- **Glisser-déposer** d'un PGN, FEN, EPD ou T64 dans la fenêtre.
- **Tournois de moteurs** toutes rondes entre 2 et 8 profils : couleurs alternées sur la même ouverture (liste intégrée ou livre Polyglot), arbitrage complet (mat, pat, répétition, 50 coups, matériel insuffisant, coup illégal, moteur muet), classement avec écart Elo estimé ; les parties rejoignent la bibliothèque sous leur propre source.
- **Correction** : fermer l'application pendant qu'un moteur tarde à quitter pouvait la faire planter.
- **CI Windows et macOS** (portabilité, sans publication). Schéma SQLite 7 inchangé.

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


## Vérification d'intégrité — dev.57, 25 octobre 2026

- Contrôle manuel déplacé dans un worker avec sa connexion SQLite et annulation pendant la requête ; état de tâche visible et étude non enregistrée conservée.
- Refus du chevauchement, attente à la fermeture, absence de faux succès après erreur/interruption ; résultat capturé avant le signal de fin pour préserver les relances réentrantes.
- Tests déterministes SQL/Qt et 12 suites locales/CI, contrôles ASan/UBSan, DEB installés et AppImage démarrés ; schéma 7 inchangé.
- Qualification séparée de dev.55 terminée : Qt 100k, 1 842 navigations sans avertissement QML, contenu de tous les PGN sauvegardés/restaurés identique, réindexation 10k et profil SQLite.
- Pic SQL local de 4,031 s non reproduit sur un autre hôte ; cause inconnue. Pas de qualification Voyager, 500k ou plusieurs millions. Sauvegarde manuelle et contrôle préalable d'ouverture encore synchrones.
- PR #12 fusionnée ; release, tag, cinq fichiers, SHA-256 et sélecteur réel depuis dev.55 vérifiés. Aucune archive Actions.

## Optimisation des imports — dev.55, 24 octobre 2026

- Requête de doublons bornée par l'index des empreintes existant, sans changement du schéma 7.
- Clés matérielles identiques aux anciennes, calculées sans compiler une expression régulière à chaque position ; même normalisation dans la recherche.
- Tests du travail SQLite et des clés persistées, captures, variantes, promotions, prise en passant, Chess960 et annulation finale de réindexation.
- Benchmark enrichi : restauration réelle, réindexation, lectures concurrentes, distributions et mémoire. Mode Qt opt-in pour mesurer les événements/navigation pendant un import réel.
- Comparaison complète conservée sur 10 000 parties ; import local de 100 000 avec pause/reprise, intégrité, sauvegarde et restauration validés. Pic de lecture concurrente de 4,031 s documenté, qualification Qt séparée.
- PR #10 fusionnée, dev.55 publiée : 12 suites Debian 12/13 et Qt ASan/UBSan, paquets et AppImage, cinq fichiers/SHA-256 et sélecteur de mises à jour vérifiés. Aucun corpus, SQLite ou archive Actions ajouté.

## Explications des suites du bilan — 16 octobre 2026

- Relecture légale des suites avant/après le coup joué, avec captures, promotions, roques, échecs, mats, pats et compte matériel conventionnel.
- Dialogue « Comprendre ce coup », comparaison des deux suites, provenance moteur et ouverture d’une copie annotée sans modifier l’étude d’origine.
- 15 contrôles du cœur ajoutés, tests Stockfish et clic réel sur le bouton d’ouverture, capture à 960 × 640.
- Aucun changement de schéma ; limites et guide dans MOVE-EXPLANATIONS.md.

## Publication des brouillons — 16 octobre 2026

- La création utilise directement la réponse REST de GitHub ; les contrôles et la publication ciblent l’identifiant exact de la release.
- Attente bornée si le nouveau brouillon tarde à apparaître dans la liste utilisée par le téléversement. Aucun nouvel essai de création, aucun écrasement d’une version publiée ou d’un brouillon étranger.
- Quatre tests de régression supplémentaires, soit 17 tests de publication ; correction des descriptions périmées du schéma et des fonctions livrées.

## Répertoires asynchrones et distribution

- Création, fusion, import/export des répertoires en arrière-plan, progression et annulation sans ajout partiel. Notes et révisions préservées.
- Statistiques et réponses adverses manquantes calculées sans bloquer l’interface ; résultats périmés ignorés.
- Analyse de toutes les variantes d’un répertoire par profil moteur, dans la file persistante.
- Correction du nom sélectionné à l’ouverture du dialogue et des notifications en petite fenêtre ; changement de bibliothèque refusé pendant un exercice.
- Tests Debian 12/13, DEB installés et AppImage puis publication directe dans GitHub Releases, sans archives Actions. Manifeste et SHA-256 vérifiés ; une version publiée n’est pas écrasée.

## File d’analyses persistante — 15 octobre 2026

- Analyse hors ligne en arrière-plan, toutes les branches ou ligne principale, profils et MultiPV, pause/reprise et récupération après plantage. Copie PGN indépendante de l’étude ouverte.
- Migration 7 sauvegardée, résultats et point de reprise atomiques, verrou par bibliothèque, pages de 200 résultats et cache borné à 10 000 positions. Identité SHA-256/options/ressources et historique complet pour la réutilisation ; ressources non identifiées exclues du cache.
- Export et ouverture d’une copie annotée, variantes légales et provenance, conservation des commentaires et de la fin réellement jouée.
- Scores UCI typés, WDL du moteur et compteurs ; scores malformés rejetés et suppression des fausses pertes en centipions lors des mats.
- Tests avec vrai Stockfish, moteur modifié, pause/reprise, processus tué, Chess960, pagination et étude éditée pendant l’analyse.
- Correction du plantage au redimensionnement après une comparaison UCI sur Qt 6.4 ; dialogue de file vérifié à 960 × 720 et exports empêchés d’écraser les verrous, même via un lien de bibliothèque.

## Import parallèle et stockage GitHub — 15 octobre 2026

- Parsing PGN avec 1 à 8 lecteurs, file bornée, restitution ordonnée sur le thread du consommateur et un seul écrivain SQLite. Préférence persistante accessible dans les imports reprenables.
- Notation SAN historique surspécifiée acceptée seulement pour un unique coup légal ; export canonique et refus des ambiguïtés/fausses captures.
- Benchmark sur de vrais PGN, contrôles des chemins de sortie, import interrompu/repris et sauvegarde vérifiée. Mesures de 10 000 parties complètes enregistrées dans PERFORMANCE, sans qualification abusive de millions de parties.
- 10 suites locales réussies ; test du benchmark activé en CI Debian 12/13.
- Nettoyage effectif de 58 archives Actions (8 371 207 728 octets), dix releases préservées ; suppression des archives des PR, conservation d'un jour pour les publications, purge automatique après succès et quotidienne.

## Préparation par position — 13 octobre 2026

- Statistiques locales combinant position, base, adversaire exact/couleur, période et Elo moyen connu des deux joueurs. Les répétitions ne gonflent pas les comptes.
- Calcul Qt en arrière-plan, annulation SQLite et remplacement des demandes périmées. Invalidation lorsque les données changent ; rejet d’une suite appliquée depuis une autre position.
- Parties d’exemple réelles, résultats Blancs/Nulles/Noirs et Elo issus des métadonnées, export CSV avec FEN et filtres.

## Répertoires par position — 13 octobre 2026

- Migration 6 sauvegardée : répertoires nommés Blancs/Noirs, classique/Chess960, variantes et transpositions partagées.
- Fusion des études et import PGN atomique, annotations/NAG, priorité des coups, liens vers les parties modèles et export PGN borné avec transpositions signalées.
- Suppression de branches sans perdre les positions encore accessibles ni leurs révisions.
- Répétition espacée par position acceptant les différentes réponses préparées, révision libre, suivi des rappels manqués et restauration de l’étude après entraînement. Les cartes existantes sont conservées.
- Réponses adverses manquantes calculées depuis la bibliothèque locale ; aucune nouveauté théorique déduite d’une simple absence.

## Imports reprenables — 13 octobre 2026

- Mode PGN par lots de 250, avec pause et reprise après redémarrage ou plantage ; conservation atomique des parties, liens de source et point de reprise. L’import classique reste entièrement annulable.
- SHA-256 du fichier complet vérifié avant chaque reprise ; fichier original déplaçable, fichier modifié refusé. Verrou interprocessus récupérable après arrêt brutal.
- Progression en octets et parties validées, débit et estimation du temps restant pendant l’import. Gestionnaire accessible depuis la bibliothèque et le menu.
- Migration 5 sauvegardée, tâches persistantes et historique unique à la fin ; retirer une tâche conserve les parties importées.

## Recherche asynchrone — 12 octobre 2026

- Recherches SQL en arrière-plan, annulables au niveau SQLite ; seule la dernière demande peut alimenter la liste.
- Pages de 200 résultats par curseur, retour à la page précédente, absence de page suivante fictive ; ordre stable avec égalités et valeurs absentes.
- Les familles d’ouvertures ne capturent plus les variantes homonymes d’une autre famille (ex. Franco-Sicilienne). Les filtres de base et d’ouverture restent actifs pendant la navigation. Les erreurs de recherche sont affichées sans conserver des résultats trompeurs.
- Dépendance de compilation `libsqlite3-dev` ajoutée à l’installation et à la CI Debian.

## En développement — moteurs et bases

- Options UCI découvertes au handshake et contrôlées selon leurs types et limites.
- Réglages propres à chaque profil, y compris les options du moteur ; les analyses désactivent la réduction de force.
- Répertoire de travail du moteur adapté aux réseaux NNUE et livres locaux.
- Catalogue de trois moteurs externes vérifiés (Stockfish, Arasan, PlentyChess), compatible avec le système détecté, progression/annulation et extraction protégée. Pas de lancement automatique.
- 42 collections PGN à la demande : champions, championnes disponibles, matchs historiques et ouvertures. Source et conditions explicites.
- Migration 4 sauvegardée : bases nommées, appartenance multiple sans doublons exacts, compteurs transactionnels, historique des imports et export PGN d’une base.
- Filtres combinant base, ouverture par nom/ECO, plage ECO, joueur/couleur, date, tournoi, Elo, matériel et position. Annuler une recherche avancée ne modifie plus les filtres actifs.
- Interface existante conservée.

## 0.3 — préversions de développement

Refonte de l’interface : navigation supérieure numérotée, accueil « carnet » avec position réelle, identité encre/papier/cuivre, typographie serif et notation monospace. Palette Trait64 ajoutée sans écraser les choix personnels. Commandes de l’échiquier regroupées et clavier désactivé pendant les menus ; focus visible des boutons. Géométrie et navigation testées à 960 × 640. Aucun changement de schéma ou de données pour cette refonte.

Mises à jour GitHub vérifiées (trois canaux), accès au dépôt privé sans clé intégrée, installation .deb/AppImage et sauvegarde préalable ; automatisation des tests et Releases. Sauvegardes périodiques en arrière-plan et rotation de 14 instantanés.

SQLite schéma 3 : recherche FTS5 trigramme avec repli LIKE, filtres exacts, compteurs et statistiques d’ouvertures incrémentaux, classement des joueurs indexé. Migration sauvegardée et transactionnelle.

Comparaison simultanée de 2 à 4 moteurs UCI, MultiPV indépendant, affichage des hits Syzygy, navigation sans variantes périmées, insertion de PV et budgets de mémoire/threads partagés.

Corrections : tri sémantique strict des versions, annulation de connexion GitHub CLI, propagation des statistiques après corbeille/restauration/réindexation, panneau de paramètres défilant et raccourcis bloqués pendant les dialogues.

## 0.2.0 alpha

Personnalisation des pièces/échiquier/fond ; Chess960 et éditeur de position ; annotations persistantes ; onglets et annuler/rétablir ; fusion/duplication ; bilan automatique et graphe ; cartes depuis erreurs ; profils UCI et configuration Syzygy ; pendules locales ; ECO ; fiches joueurs et recherche avancée ; imports/exports PGN en flux ; EPD, PNG et PDF.

Corrections ciblées pendant la vérification :

- Chemins des ressources SVG corrigés et chargement des 48 images testé.
- Dépendance explicite au greffon SVG séparé de Debian 13, y compris avec une installation sans paquets recommandés.
- Contraste des aperçus, du texte de fond et des boutons ajusté ; panneaux vérifiés sur petite fenêtre et navigation latérale défilante.
- Plantage de redimensionnement après actualisation des statistiques corrigé en conservant les composants des tableaux de bord.
- Résultats exacts du moteur conservés face à ses bornes intermédiaires ; réponses de bilan périmées ignorées après annulation.
- Restauration de la bonne position de variante et des annotations entre onglets/redémarrages ; orientation préservée après entraînement.
- Migration SQLite et réindexation dans une transaction, avec sauvegarde avant version 2 ; import annulé entièrement restauré.
- Écritures concurrentes vers la base empêchées pendant un import/réindexage.
- Export et sauvegarde protégés contre la base active, ses alias symboliques et ses journaux.
- Validation renforcée des FEN (matériel et droits de roque) et des opérations EPD.

Le format `.t64` reste expérimental et inchangé. Les détails du périmètre restant figurent dans ROADMAP.md ; cette version ne représente pas l’ensemble du catalogue initial.

## 0.1.0 alpha

Première application locale : règles classiques, variantes PGN, bibliothèque SQLite, analyse/jeu UCI, explorateur, cartes d’entraînement, sauvegardes et récupération de session. Compatibilité GCC 12 et paquets Debian 12/13.
