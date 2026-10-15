# Point de reprise — répertoires et publication, 15 octobre 2026

Objectif conservé : poursuivre la feuille de route sans recréer Trait64. C++23, Qt6/QML et SQLite ; fonctions locales hors ligne, esthétique en pause, aucun bouton factice ni Elo inventé.

## État faisant autorité

La PR #6 est intégrée à `main` au commit `31ac8803cb7d6e7e0f98ca0c3cf7eeea1b6e5eaa`. Elle apporte la file d’analyses persistante, les variantes, le cache avec provenance et les scores UCI typés. La CI de cette PR a réussi les 11 suites sur Debian 12 et 13, le cœur ASan/UBSan et les essais des paquets installés.

Le nouveau lot est sauvegardé sur **feat/async-repertoires**, [PR #7](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/7). Le premier commit `a619bbc26c13844fffb83dd8812cbd210b2f8f72` contient la publication sans archives Actions ; les opérations de répertoire sont ajoutées au même lot. Schéma toujours **7**, aucune migration existante réécrite. Le prochain changement structurel nécessite une migration **8** sauvegardée.

## Code du lot

- `app/repertoire_tasks.cpp` : création/fusion/import/export et préparation d’une analyse via QtConcurrent. Chaque worker possède sa connexion SQLite. Progression en positions, annulation, attente des workers à la fermeture. Pas de capture du contrôleur dans les workers.
- Statistiques, réponses adverses manquantes et rappels manqués : instantané cohérent en arrière-plan, interruption SQLite, générations contre les résultats périmés. Un résultat annulé n’est pas présenté comme une couverture vide. Changer de bibliothèque invalide les anciennes lectures ; le changement est refusé pendant un exercice.
- Import PGN borné à 16 Mio/200 parties, une partie analysée à la fois, transaction unique. Une erreur ou une annulation retire tout cet import sans modifier les notes/révisions antérieures. Création et lien de partie modèle dans la même transaction. Export via QSaveFile : l’ancien fichier reste intact jusqu’à la validation finale.
- Analyse d’un répertoire : copie de toutes les variantes, profil moteur et budget par position, transmission à la file persistante. Les notes, la progression et l’étude restent conservées. Les transpositions sont développées une seule fois et annotées ; ce n’est pas l’énumération de tous les historiques possibles.
- Interface : progression/annulation, rechargement des statistiques, nom du répertoire sélectionné fiable, identifiant capturé avant les sélecteurs de fichiers. Correction du positionnement de la notification dans les petites fenêtres.
- Publication : Debian 12 puis 13 dans deux conteneurs propres sur un runner ; tests, DEB installés, AppImage et sources correspondantes. Envoi direct à Releases, aucun upload/download-artifact. Manifeste exact de cinq fichiers, tailles et SHA-256 distants contrôlés avant publication. Refus d’écraser une version publiée ou un brouillon étranger ; reprise possible de son propre brouillon au même commit. Les commits ordinaires sur main restent des versions de développement.

## Vérifications en cours

Les 13 tests Python de publication, 9 tests de nettoyage, la syntaxe Bash et la structure YAML passent localement. Compilation C++ complète et tests d’intégration du lot répertoire en cours ; **ne pas annoncer leur réussite avant leurs résultats**. La première CI de la PR #7 valide déjà le cœur ASan/UBSan ; les conteneurs Debian restent à contrôler. Les tests couvrent notamment l’annulation réelle après progression, la conservation des octets de l’export, les changements de bibliothèque et une analyse avec Stockfish réel.

## Publication et stockage

Au dernier contrôle, version distribuée : **v0.3.0-dev.33**. Les exécutions main 36 et 38 ont échoué uniquement sur le quota d’archives Actions, malgré les tests et paquets réussis. Ne pas relancer ces anciennes versions : le nouveau pipeline supprime leur dépendance au stockage d’archives. Après fusion de la PR #7, vérifier la nouvelle release, ses cinq fichiers, leurs digests, le commit cible et l’absence d’artefacts Actions avant de la déclarer disponible.

Nettoyage effectué le 15 octobre à 05:59 UTC : **58 archives / 8 371 207 728 octets** supprimés ; zéro archive et zéro cache après contrôle. Les dix releases antérieures et leurs cinquante fichiers sont conservés. La purge quotidienne reste présente pour les archives anciennes ; elle ne touche ni au code ni aux fichiers distribués.

## Reprise suivante

1. Terminer les tests du commit exact, corriger toute régression, puis CI Debian 12/13 avant fusion et publication de développement. Vérifier les cinq assets de la release réellement créée.
2. Explications tactiques vérifiables, corpus tactique sous licence ouverte et progression par thème ; ne pas inventer d’équivalences ni de niveau Elo.
3. Couverture probabiliste et comparaison de répertoires. L’édition de coups, le retrait de branches, le renommage, la suppression et les révisions restent synchrones ; le lookup des coups de la position actuelle est indexé et reste sur le thread UI.
4. Qualification de 1/2/5/10 millions de parties **réelles**. Seules 10 000 parties / 661 314 positions sont mesurées dans `performance-pgn-2026-10-15.json` ; le million de métadonnées synthétiques ne valide pas un million de PGN complets.
5. Module Syzygy local, OCR licencié avec validation de FEN, rapports pédagogiques/répertoires, restauration guidée et essais Voyager X11/Wayland/Polkit. Le backlog complet reste dans ROADMAP ; il n’est pas terminé.

Compilation locale : `source ../qt-env.sh`, build `--parallel 1`, aucun changement C++/CMake pendant la compilation. Un binaire généré corrompu après restauration doit être régénéré. Conserver l’ancien checkout non commité intact ; GitHub contient la base de reprise et les étapes de ce lot.
