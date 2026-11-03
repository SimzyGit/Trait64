# Motifs tactiques du bilan

Dans **Analyse → Bilan → Comprendre ce coup**, chaque suite est rejouée légalement.
Le bilan décrit les nouveaux motifs observés après chaque coup : fourchette ou
attaque multiple, clouage au roi, enfilade avec le roi devant une pièce, échec à
la découverte et échec double. Les pièces, couleurs et cases proviennent de la
position obtenue. Un motif strictement inchangé n’est pas attribué au nouveau coup.

Une attaque multiple n’est pas une garantie de gain. Pour les cibles autres que
le roi, la capture doit laisser le roi de l’attaquant en sécurité si l’adversaire
ne répondait pas. Les réponses adverses, les échanges et la valeur stratégique
des pièces ne sont pas résolus par ce détecteur. Le roi ciblé signifie un échec,
jamais une capture du roi. Les rois ne sont pas détectés comme auteurs de fourchettes.
Un clouage décrit un alignement : la pièce peut encore bouger sur cet alignement.

**Ouvrir la suite dans une nouvelle étude** conserve les explications en commentaires
et ajoute des annotations PGN standard :

- Flèches vertes (`%cal`) : attaques réellement présentes.
- Marques jaunes (`%csl`) : cases alignées d’un clouage ou d’une enfilade.

Aucune flèche ne traverse une pièce pour représenter une attaque inexistante.
Les repères identiques sont dédoublonnés. Les commentaires et repères restent
dans le PGN exporté ; l’étude d’origine et son bilan sont conservés.

La prise en passant, la promotion vers la pièce réellement choisie et le roque
Chess960 sont traités après application du coup légal. La suite reste limitée
à 128 demi-coups et commence depuis une FEN sans l’historique des répétitions.
Les sacrifices justifiés, plans stratégiques, défenses forcées et équivalences
tactiques restent à développer. Aucun niveau Elo n’est déduit.

## Validation du 19 octobre 2026

Base : `ef383aa8aec45e771fcf31d70e065066ace300b3` (dev.43 vérifiée).
Checkout isolé : `Trait64-tactics`, sans modification de l’ancien dossier.
Fichiers du lot : `libs/core/tactics.*`, `explanation.*`, `app/features.cpp`,
`ui/Review.qml`, `tests/tactics_cases.h`, `tests/core_tests.cpp` et CMake.
Lot testé `ced2428`, intégré par la PR #8 au commit `1c86d53`. Les 11 suites
locales et Debian 12/13 passent, ainsi que le cœur ASan/UBSan et les paquets.
Le contrôle visuel à 960 × 640 confirme les repères et l’accès au bouton de copie.
Détails : `verification-tactics-2026-10-19.json`. Aucun changement de schéma.

Disponible dans [Trait64 0.3.0-dev.46](https://github.com/SimzyGit/Trait64/releases/tag/v0.3.0-dev.46), canal Développement.
