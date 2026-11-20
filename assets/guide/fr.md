## Premiers pas

Trait64 travaille sur votre ordinateur : vos parties, vos études et vos analyses restent chez vous, sans compte ni connexion obligatoire.

1. **Ouvrez une partie.** Depuis l'accueil, *Exemples* charge quelques parties ; *Fichier → Importer des parties* (Ctrl+O) lit un fichier PGN ou un texte collé. Un lien de partie ou d'étude Lichess se colle au même endroit.
2. **Installez un moteur.** Stockfish déjà installé est trouvé tout seul. Sinon, *Moteur → Moteurs et bases à télécharger* en installe un vérifié, et le bouton *Détecter* des paramètres relance la recherche.
3. **Analysez.** Dans l'onglet *Analyse*, *Analyser* lance le moteur sur la position affichée ; *Bilan* passe toute la partie en revue.

Les pages se choisissent dans la barre de gauche : Accueil, Analyse, Bibliothèque, Ouvertures, Entraînement, Joueurs, Profils, Études et Outils. Elle se replie en icônes avec « (ou d'elle-même si la fenêtre est étroite). *Ctrl+K* ouvre la palette de commandes, qui retrouve n'importe quelle action par son nom.

## L'échiquier

- **Jouer un coup** : glissez la pièce, ou cliquez-la puis cliquez sa case d'arrivée. Les coups possibles sont marqués.
- **Parcourir** : flèches gauche et droite, Début et Fin ; haut et bas passent d'une variante à l'autre. *Espace* lance la lecture automatique. La molette peut aussi parcourir les coups (réglage *Molette : parcourir les coups*).
- **Retourner l'échiquier** : *F*.
- **Annoter** : un clic droit glissé trace une flèche, un clic droit sur une case pose un cercle ; Maj, Ctrl ou Alt changent la couleur. Un double clic, gauche ou droit, efface toutes les flèches et tous les cercles.
- **À côté des noms** des joueurs s'affichent les pièces prises et l'avance matérielle.

*Affichage → Pièces, échiquier et couleurs* règle les pièces, les couleurs, la taille, les sons, les flèches du moteur et le mode *À l'aveugle*. *Affichage → Raccourcis clavier* change les touches. *Affichage → Pièces en prise* entoure de rouge ce qu'une prise gagnerait.

## La notation et les variantes

Jouer un autre coup que celui de la partie crée une variante. La notation se lit en tableau ou en texte continu.

- **Supprimer un coup et sa suite** : clic droit sur le coup puis *Supprimer à partir d'ici*, ou touche *Suppr*. *Ctrl+Z* annule.
- **Promouvoir une variante** : clic droit sur l'un de ses coups, *Promouvoir la variante*.
- **Commentaires et symboles** (!, ?, !?, …) s'ajoutent au coup courant sous la notation.

Chaque étude ouverte a son onglet, jusqu'à seize ; ils sont retrouvés au redémarrage.

## Analyser avec un moteur

- *Analyser* lance le moteur sans limite de profondeur ; plusieurs lignes s'affichent (MultiPV). Un clic sur une ligne l'insère dans l'étude.
- Les flèches bleues montrent les meilleurs coups ; *Menace* (touche *M*) montre en rouge ce que jouerait l'adversaire si vous passiez votre tour.
- Le nombre de fils et la mémoire du moteur sont réglés d'après votre machine ; *Paramètres* permet de les changer.
- Plusieurs moteurs peuvent analyser côte à côte, et des tournois entre moteurs se jouent depuis *Moteur → Tournoi de moteurs*. Le classement donne l'écart d'Elo avec sa marge d'erreur à 95 % et la LOS, la probabilité que le moteur soit réellement plus fort : en dessous de 95 %, le résultat peut encore tenir du hasard.

## Comprendre ses erreurs

*Bilan* analyse la partie et classe chaque coup. Choisissez d'abord la **profondeur** : 18 pour un aperçu rapide, 22 pour un bilan comparable à celui de Lichess, 26 ou 30 pour aller plus loin. Le moteur prend tous les cœurs de la machine sauf un ; avec *Deux passages*, il parcourt la partie du début à la fin, puis de la fin vers le début en profitant de ce qu'il a appris. La profondeur réellement atteinte s'affiche sous la barre de progression, et les trois moments clés de la partie sont numérotés sur la courbe.

 En ouvrant l'explication d'un coup, un encadré dit **pourquoi** il est moins bon que celui du moteur : mat permis ou manqué, matériel perdu ou manqué (avec la pièce, la case et le coup qui le montre), pièce laissée en prise, sinon écart positionnel. Les motifs (fourchette, clouage, attaque à la découverte, menace de mat…) et ce que le coup change aux pions et à l'abri du roi sont donnés à côté.

Ces explications viennent des suites calculées par le moteur : un calcul plus long peut dire autre chose.

## La bibliothèque

Toutes vos parties enregistrées (*Ctrl+S*) vivent dans la bibliothèque.

- **Rechercher** : par joueur, tournoi, date, résultat, Elo, ouverture ; par position exacte ; par matériel ; par structure de pions ; par pièces sur des cases (*Ke1 Pd4 pd5*, majuscule pour les Blancs — cette dernière recherche rejoue toutes les parties et peut être lente).
- **Importer** de grosses bases : l'import est parallèle, reprenable, et vérifie l'intégrité. *Base → Tournois en direct* montre les retransmissions de Lichess : les échiquiers d'une ronde, et *Suivre* ouvre une partie dans l'analyse, où ses coups arrivent au fil du jeu. Le menu ⋮ d'une partie de la liste ouvre le *Tableau du tournoi* : classement, départages, performance, et un clic sur un résultat ouvre la partie. Les bases `.db3` d'En Croissant et les bases SCID au format 4 (`.si4`, avec leurs `.sn4` et `.sg4` à côté) se lisent aussi.
- **Doublons** : *Base → Doublons de coups* repère les parties en double.
- **Dossier de position** : pour la position affichée, les coups joués, les résultats par niveau et les meilleures parties.

## Études et fichiers

La page *Études* montre l'arborescence de votre dossier de PGN : dossiers à déplier, recherche, création, renommage et déplacement de fichiers. Supprimer met à la corbeille du système, jamais définitivement.

*Exporter cette étude* (dans le menu ⋮ de l'analyse ou par Ctrl+K) écrit l'étude en PGN, la position en EPD, ou l'étude en **page HTML autonome** qui se consulte dans n'importe quel navigateur, sans Trait64.

## Ouvertures et répertoires

- La page *Ouvertures* montre l'arbre des coups de vos parties, les livres Polyglot et le nom ECO de chaque position.
- Un **répertoire** rassemble les lignes que vous jouez. Son onglet des lacunes donne la **couverture estimée** — la part des parties de votre bibliothèque qui restent dans le répertoire — et les réponses adverses non préparées, de la plus fréquente à la plus rare.
- *Comparer…* met deux répertoires côte à côte.
- L'onglet *Lichess* de l'analyse montre ce qui se joue sur Lichess ou chez les maîtres dans la position affichée, et les tables de finales à sept pièces (connexion nécessaire). Si vous avez des tables Syzygy sur l'ordinateur, indiquez leur dossier dans les paramètres : les finales qu'elles couvrent se lisent alors sans réseau. Les paramètres savent aussi les télécharger : 3 à 5 pièces (939 Mio) suffisent à la plupart des finales.

## S'entraîner

- **Puzzles** : classés selon votre niveau, par thème ; vos résultats par thème s'affichent, et un puzzle manqué revient le lendemain, puis trois et sept jours après chaque réussite.
- **Cartes** : une ligne ou une erreur à retrouver, répétées à intervalles.
- **Répertoire** : retrouver vos coups préparés.
- **Deviner le coup** : rejouer une partie de maître en cherchant chaque coup d'un camp.
- **Finales à jouer** : les finales de base et les positions de Lucena et de Philidor, contre le moteur.
- **Coordonnées** : trente secondes pour cliquer les cases nommées.
- **À l'aveugle** : les pièces disparaissent, les coups se jouent toujours.

## Vos progrès

La page *Profils* réunit sous un seul nom vos comptes Lichess et Chess.com (*Fichier → Comptes Lichess et Chess.com*) et votre fiche FIDE. Une synchronisation récupère jusqu'à 5 000 parties par compte, puis seulement les nouvelles.

*Analyser le profil* envoie toutes les parties du profil dans la file d'analyse, à la profondeur choisie (12, 18, 22 comme Lichess, ou 24). Le rapport se complète à mesure : il dit combien de parties sont déjà passées en revue sur le total, et combien attendent encore. Une partie déjà vue aussi loin n'est pas refaite. Précision par phase, fautes pour cent coups, force estimée et progression mois par mois se lisent ensuite au même endroit.

## Jouer

*Jouer une partie* oppose à un moteur à la force choisie (en Elo pour les moteurs qui le permettent), ou deux personnes sur le même ordinateur, avec pendule et incrément.

## Sauvegardes et mises à jour

- La bibliothèque est sauvegardée toutes les six heures (les quatorze dernières sont gardées) et avant chaque mise à jour.
- *Fichier → Restaurer une sauvegarde* liste ces sauvegardes ; restaurer remplace la bibliothèque après l'avoir elle-même sauvegardée.
- Quand une nouvelle version paraît, Trait64 la propose ; le bouton de version, en bas à droite, se colore.

Vos données sont dans le dossier indiqué par *Paramètres* ; la bibliothèque est `library.sqlite`.

## Raccourcis par défaut

| Touche | Action |
|---|---|
| ← / → | Coup précédent / suivant |
| Début / Fin | Début / fin de la ligne |
| ↑ / ↓ | Variante précédente / suivante |
| Alt+← / Alt+→ | Partie précédente / suivante de la liste |
| Espace | Lecture automatique |
| F | Retourner l'échiquier |
| M | Menace |
| Ctrl+B / F11 | Échiquier seul / plein écran |
| Ctrl+L | Taper un coup (« Cf3 », « e4 », « O-O », ou « e2e4 ») puis Entrée |
| Suppr | Supprimer le coup et sa suite |
| Ctrl+Z / Ctrl+Maj+Z | Annuler / rétablir |
| Ctrl+N / Ctrl+O / Ctrl+S | Nouvelle étude / importer / enregistrer |
| Ctrl+K | Palette de commandes |

Tous se changent dans *Affichage → Raccourcis clavier*.
