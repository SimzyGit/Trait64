# Refonte graphique — chantier en cours, 26 octobre 2026

Autorisation explicite de l’utilisateur du 26 octobre : intégrer la refonte graphique en vérifiant notamment les bugs visuels. Base réelle : main `f123ec77f60d75d36bf06d4c7e045bdc096d6b3f` (dev.57). Branche `ui/desktop-redesign`.

Référence : paquet préparé `Trait64_refonte_interface_corrigee.zip` du 24 octobre. Navigation à gauche, grand échiquier, commandes réelles, sobriété, préférences de police/texte/échelle, français/anglais. L’HTML reste une référence, jamais embarquée. Board.qml, Piece.qml, ressources Lichess, schéma SQLite 7 et index de pages 0–6 conservés.

Ce premier point de sauvegarde contient les fondations. **Non publié, non validé comme refonte complète.** Restent : réglages et traduction complète, adaptation des pages/dialogues aux grandes polices, matrice Qt 960×640 à 2560×1600 avec captures, interactions, tests complets, CI et publication. Les 187 vérifications de la maquette HTML ne sont pas des tests Qt. Aucun essai matériel Voyager n’est revendiqué.
