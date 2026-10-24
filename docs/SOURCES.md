# Sources techniques examinées — 11 octobre 2026

Cette lecture identifie des pistes, pas une supériorité de Trait64. Aucun code concurrent n’a été copié dans ce lot. Avant toute incorporation future : examiner la licence du fichier et ses dépendances, conserver les attributions et documenter la révision.

| Source primaire | Observation / piste pour Trait64 | Suite nécessaire |
|---|---|---|
| [Pragma Chess](https://github.com/francescobianco/pragma-chess) | README consulté : explication de coups, sources de parties, livres Polyglot, préparation et projets de travail. Le jeu en ligne et les estimations Elo décrits par ce projet sont hors de notre périmètre. | Examiner l’implémentation Explain et ses cas limites ; rechercher le protocole exact des benchmarks publiés. Aucun benchmark reproductible n’a été identifié dans le README consulté. |
| [En Croissant](https://github.com/franciscoBSalgueiro/en-croissant) | Répertoires avec répétition espacée, multi-moteur, installations et recherche de positions. | Étudier le stockage et la reprise des imports ; GPL-3.0, vérifier chaque portion avant réutilisation. |
| [Lila opening explorer](https://github.com/lichess-org/lila-openingexplorer) | Explorateur spécialisé pour de nombreux jeux et positions. | Étudier index, agrégation et stockage sans remplacer l’architecture locale à l’aveugle. |
| [Chess openings](https://github.com/lichess-org/chess-openings) | Jeu de données CC0 déjà incorporé et attribué ; réutilisé pour résoudre noms d’ouvertures vers codes ECO. | Traductions françaises plus complètes ; un code ECO ne définit pas une variante exacte. |
| [Lucas Chess R2](https://github.com/lukasmonk/lucaschessR2) | Référence pour la diversité de l’entraînement. Dépôt indiqué archivé lors de la consultation. | Identifier le successeur et étudier séparément les licences des exercices et moteurs distribués. |
| [ChessX](https://github.com/Isarhamster/chessx) | Référence d’application Qt d’échecs. | Étudier navigation, PGN et modèles incrémentaux ; vérifier les licences par fichier. |
| [Maia-3](https://github.com/CSSLab/maia3) | Modèle de prédiction de coups humains, candidat facultatif pour l’entraînement. | Vérifier distinctement code, poids, ressources matérielles et interface ; aucune IA obligatoire ou Elo inventé. |
| [Cute Chess](https://github.com/cutechess/cutechess) | Interface et outils de matchs de moteurs. | Étudier arbitrage, protocoles et futures compétitions moteur contre moteur. |
| [SCID — recherche](https://sourceforge.net/p/scid/wiki/HowToSearchDatabases/) | Référence pour les recherches de parties et les filtres. | Étudier SI4/SI5 dans le code autorisé, tester les variantes et commentaires conservés. |
| [ChessBase — préparation](https://help.chessbase.com/CBase/18/Eng/preparingplayer.htm) | Documentation fonctionnelle de préparation d’adversaire. | Inspiration fonctionnelle uniquement ; pas de copie de code ou bases propriétaires, ni contournement de protection. |
| [ChessTempo — manuel](https://chesstempo.com/manual/en/manual.html) | Documentation des répertoires et modes de révision. | Concevoir nos critères de répétition et solutions acceptées ; ne pas recopier leur corpus d’exercices. |

Les choix livrés dans ce lot utilisent surtout les dépendances déjà présentes, les options réellement déclarées via UCI, le catalogue CC0 d’ouvertures et libarchive. Les corpus réels de 1/2/5/10 millions de parties et une comparaison reproductible avec Pragma restent à réaliser ; voir [PERFORMANCE.md](PERFORMANCE.md).

## Relecture des mesures Pragma — 24 octobre 2026

Révision observée : [`08c9d3c`](https://github.com/francescobianco/pragma-chess/commit/08c9d3c9ea2e91dd32ac710ad639d1ff24aaff47).
Le [README](https://github.com/francescobianco/pragma-chess/blob/08c9d3c9ea2e91dd32ac710ad639d1ff24aaff47/README.md)
décrit des recherches de positions rapides sur de gros volumes. La relecture
du README, des guides bases/liste/conversion et de DESIGN.md n'a pas permis
d'identifier un protocole chiffré reproductible précisant ensemble corpus,
matériel, version, index construits et mesures brutes. Ce constat se limite
aux documents examinés ; il ne dit pas qu'aucune publication n'existe ailleurs.

Aucune mesure de Pragma n'a été exécutée sur cet hôte. Le gain de 54,4 % mesuré
pour Trait64 compare ses propres versions. Il ne classe pas les deux logiciels.
Une comparaison ultérieure doit importer le même PGN, vérifier les mêmes parties
et positions, décrire les index et variantes conservés, puis mesurer séparément
import, ouverture, recherches, mémoire et disque avec les mêmes ressources.
Références de cette vérification : `benchmarks/2026-10-24/pragma-comparison.json`.
