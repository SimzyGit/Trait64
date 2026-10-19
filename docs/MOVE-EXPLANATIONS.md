# Comprendre un coup du bilan

Dans l’espace Analyse, ouvrir **Bilan**, lancer l’analyse de la partie, sélectionner
un coup puis **Comprendre ce coup**. Deux suites sont proposées :

- **Coup joué et réponse moteur** : le coup réellement joué, suivi de la variante
  calculée depuis la position obtenue.
- **Proposition moteur avant le coup** : la variante calculée depuis la position
  précédente. Elle peut commencer par le même coup si le moteur le propose aussi.

Chaque suite est rejouée avec le générateur légal avant affichage. Les faits
indiquent les captures et leur case réelle, les prises en passant, les promotions,
les roques, les échecs, les mats et les pats. Un coup illégal invalide l’explication ;
aucune continuation n’est inventée. Une suite trop longue est explicitement
limitée aux 128 premiers demi-coups.

Le bilan matériel est exprimé du point de vue des Blancs moins celui des Noirs,
avec les valeurs conventionnelles pion 1, cavalier/fou 3, tour 5, dame 9. Les rois
ne sont pas comptés. Il décrit le matériel présent, pas l’évaluation moteur : un
mat peut survenir à matériel égal, une promotion modifie le compte sans capture,
et une capture suivie d’une reprise peut laisser l’écart inchangé.

Les évaluations avant/après, le nom du moteur, la profondeur atteinte et le temps
par position sont affichés. La variante est celle du calcul effectué, pas une
preuve qu’aucune autre défense existe. Une analyse plus longue peut changer les
conclusions. Le classement actuel du bilan conserve ses seuils indicatifs ;
aucune équivalence Elo n’est calculée.

**Ouvrir la suite dans une nouvelle étude** crée une copie annotée, navigable avec
les commandes habituelles de l’échiquier. L’étude d’origine, ses variantes et son
bilan restent dans leur onglet. La copie commence à la FEN avant le coup : les
répétitions antérieures ne sont pas reproduites. Le roque Chess960 conserve sa
variante et n’est jamais décrit comme la capture de sa propre tour.

Les nouveaux motifs tactiques sont également décrits : attaques multiples,
clouages au roi, enfilades du roi, échecs à la découverte et doubles échecs.
La copie contient des flèches vertes pour les attaques et des marques jaunes
pour les alignements. Voir [TACTICAL-MOTIFS.md](TACTICAL-MOTIFS.md) pour la portée
et les limites. Les sacrifices justifiés, plans stratégiques et équivalences
restent à développer. La file d’analyses persistante conserve son fonctionnement ;
ce dialogue utilise le bilan immédiat.
