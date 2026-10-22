# Point de reprise — audit global vérifié le 22 octobre 2026

**Audit intégré et publié dans [v0.3.0-dev.52](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.52).** La [PR #9](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/9) corrige dix groupes de défauts sans ajout fonctionnel ni changement du schéma 7. Fusion `e301ddb694562cead329e6b6f3ff3099c27f9ec3`, arbre `40e388e8ea15b53a85004357267338a7b41e94e8` identique au commit testé `8eada72f4e94a15f4334d970d4b4a1972a46c538`.

Corrections : récupération des sessions, annulation finale de réindexation, compteurs FEN, réponses UCI réentrantes, destruction de l’updater pendant l’authentification, nulles réclamables, cache terminal hérité, cadence PGN et options UCI. Stockfish pouvait s’arrêter lorsqu’une chaîne de journal vide était envoyée comme le nom de fichier `<empty>` ; les anciens profils contenant ce marqueur sont également corrigés.

Validation locale : 12/12 suites avec Stockfish réel, 80 études et 3 000 mutations FEN/PGN. [CI de la PR 39564062699](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/39564062699) réussie : cœur ASan/UBSan 2/2, Qt ASan/UBSan 12/12, Debian 12 12/12 (58,70 s), Debian 13 12/12 (73,99 s), DEB installés, AppImage et manifeste.

**Publication confirmée le 21 octobre à 10 h 24, heure de Paris ; vérifiée le 22 octobre.** [CI de main 39566807355](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/39566807355) réussie : cœur ASan/UBSan 94,35 s, Qt 12/12 en 104,70 s, Debian 12 12/12 en 60,21 s, Debian 13 12/12 en 75,45 s ; paquets installés et AppImage démarrés. Le tag vise la fusion testée. Les cinq fichiers correspondent exactement au manifeste en noms/tailles/SHA-256 ; zéro archive Actions. Le sélecteur réel reconnaît les trois paquets depuis dev.46 sur Développement, avec exclusions de canaux et versions vérifiées.

Rapport : [DEBUG-AUDIT.md](DEBUG-AUDIT.md). Mesures : [verification-debug-2026-10-22.json](verification-debug-2026-10-22.json). Les anciens bilans restent historiques ; lancer une nouvelle analyse applique la réparation du cache. Aucun essai sur le PC Voyager n’est revendiqué. Repartir de main pour le prochain lot ; préserver les anciens checkouts non committés. Les sections suivantes conservent l’historique et la feuille de route.

## État précédent conservé

Objectif conservé : poursuivre la feuille de route sans recréer Trait64. C++23, Qt6/QML et SQLite ; fonctions locales hors ligne, esthétique en pause, aucun bouton factice ni Elo inventé.

## Lot tactique intégré — PR #8

La [PR #8](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/8) est fusionnée
au commit `1c86d538dba4b0c65020efcd1f0dff99bc329031`. Son arbre est identique au
commit testé `ced24282a2b27bb68b914e315775407d228d6c1f`.

Implémenté : attaques multiples, clouages au roi, enfilades du roi, échecs à la
découverte et doubles échecs ; captures exposant le roi exclues des cibles de
fourchette ; copie annotée avec flèches d’attaque et marques d’alignement PGN.
Guide : [TACTICAL-MOTIFS.md](TACTICAL-MOTIFS.md). Aucun changement de schéma (7).

Validation locale : 3 258 contrôles du cœur, 11 suites avec Stockfish 16, clics
sur deux cas tactiques, export PGN et étude d’origine conservés ; contrôle visuel
à 960 × 640. La [CI de la PR](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/39127471728)
réussit : 11/11 Debian 12 (49,37 s), 11/11 Debian 13 (60,14 s), deux DEB
installés/démarrés, AppImage, cœur ASan/UBSan et manifeste des cinq fichiers.

**[dev.46 publiée et vérifiée](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.46)**
le 19 octobre à 05:56 UTC. [Exécution 39206008438](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/39206008438)
réussie : 11/11 Debian 12 (50,19 s), 11/11 Debian 13 (60,78 s), paquets installés,
AppImage et contrôles mémoire. Le tag vise `1c86d53` ; les cinq fichiers ont les
noms, tailles et SHA-256 exacts du manifeste. Aucune archive Actions créée.
Le sélecteur de mises à jour compilé depuis le code réel reconnaît AppImage,
Debian 12/13 sur Développement ; Stable/Bêta, version ignorée, identique et
rétrogradation sont exclus. Aucun essai sur le PC Voyager n’est revendiqué.
Rapport : `verification-tactics-2026-10-19.json`.

Les fichiers du lot sont sur GitHub. Si le checkout isolé `Trait64-tactics`
n’a pas survécu à la restauration de l’environnement, repartir de main ou de la
branche `feat/tactical-explanations`, sans reconstituer le code depuis le chat.
Ne pas modifier les anciens checkouts non committés.

## Historique des PR #6 et #7

La PR #6 est intégrée à `main` au commit `31ac8803cb7d6e7e0f98ca0c3cf7eeea1b6e5eaa`. Elle apporte la file d’analyses persistante, les variantes, le cache avec provenance et les scores UCI typés. La CI de cette PR a réussi les 11 suites sur Debian 12 et 13, le cœur ASan/UBSan et les essais des paquets installés.

La [PR #7](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/7), commit testé `e2191937041460508507ffff4ab5c0a20afe9a70`, est **fusionnée** dans main le 16 octobre au commit `d8c49243f961ae20dfb33e11231a69cbc898f428`. Son arbre de fichiers est identique à celui validé en PR. Schéma toujours **7**, aucune migration existante réécrite. Le prochain changement structurel nécessite une migration **8** sauvegardée.

## Historique : explications du bilan

Après dev.42, ajout de `libs/core/explanation.*` : relecture légale bornée à 128
demi-coups, faits de captures/promotions/roques/échecs/mats/pats et matériel
Blancs moins Noirs. `explainReview` construit les explications à partir des suites
avant/après réellement calculées. `openReviewLine` ouvre une copie annotée depuis
la FEN ; aucune répétition antérieure n’est revendiquée. `ui/Review.qml` propose
les deux suites, la provenance moteur et le bouton d’ouverture testé par un clic.

Les 11 suites sont validées localement, dont Stockfish réel et interaction QML ;
2 553 contrôles du cœur. Deux exécutables restaurés avaient perdu leur bit
exécutable : permissions corrigées, puis suites concernées repassées. Le test
d’interaction final passe en 17,31 s ; capture à 960 × 640 inspectée sans alerte.
Aucune migration. Code intégré au commit `3a6f5d4fbdeee87d466fe31fa034eb5f3fb05d55`,
publié dans **[v0.3.0-dev.43](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.43)**
le 16 octobre à 17:45 UTC. La [CI 38940592180](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/38940592180)
réussit : **11/11 Debian 12 (49,00 s), 11/11 Debian 13 (60,91 s)**, deux DEB
installés/démarrés, AppImage démarrée, cœur ASan/UBSan (14,23 s), 17 tests du
publieur et 9 de nettoyage. Les cinq fichiers publiés correspondent exactement
au manifeste (noms, tailles, SHA-256), le tag pointe sur le commit testé et aucune
archive Actions n’a été créée. Le sélecteur de mises à jour réel propose les
trois paquets sur Développement ; Stable/Bêta, version ignorée, identique et
rétrogradation restent exclus. Aucun essai sur le PC Voyager n’est revendiqué.
Mesures : `verification-explanations-2026-10-16.json`. Guide : MOVE-EXPLANATIONS.md.

## Code du lot

- `app/repertoire_tasks.cpp` : création/fusion/import/export et préparation d’une analyse via QtConcurrent. Chaque worker possède sa connexion SQLite. Progression en positions, annulation, attente des workers à la fermeture. Pas de capture du contrôleur dans les workers.
- Statistiques, réponses adverses manquantes et rappels manqués : instantané cohérent en arrière-plan, interruption SQLite, générations contre les résultats périmés. Un résultat annulé n’est pas présenté comme une couverture vide. Changer de bibliothèque invalide les anciennes lectures ; le changement est refusé pendant un exercice.
- Import PGN borné à 16 Mio/200 parties, une partie analysée à la fois, transaction unique. Une erreur ou une annulation retire tout cet import sans modifier les notes/révisions antérieures. Création et lien de partie modèle dans la même transaction. Export via QSaveFile : l’ancien fichier reste intact jusqu’à la validation finale.
- Analyse d’un répertoire : copie de toutes les variantes, profil moteur et budget par position, transmission à la file persistante. Les notes, la progression et l’étude restent conservées. Les transpositions sont développées une seule fois et annotées ; ce n’est pas l’énumération de tous les historiques possibles.
- Interface : progression/annulation, rechargement des statistiques, nom du répertoire sélectionné fiable, identifiant capturé avant les sélecteurs de fichiers. Correction du positionnement de la notification dans les petites fenêtres.
- Publication : Debian 12 puis 13 dans deux conteneurs propres sur un runner ; tests, DEB installés, AppImage et sources correspondantes. Envoi direct à Releases, aucun upload/download-artifact. Manifeste exact de cinq fichiers, tailles et SHA-256 distants contrôlés avant publication. Refus d’écraser une version publiée ou un brouillon étranger ; reprise possible de son propre brouillon au même commit. Les commits ordinaires sur main restent des versions de développement.

## Vérifications réussies

- Localement, commit `e219193` : **11/11 suites en 66,42 s**, GCC 13.3, Qt 6.4.2, Stockfish 16 réel. 51 contrôles de répertoires, 95 d’analyse persistante, 635 contrôles d’interface/fixtures. Dialogue inspecté à 960 × 640, sans avertissement QML.
- [CI de la PR #7](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/38797743659) : **11/11 Debian 12 en 48,75 s**, **11/11 Debian 13 en 59,03 s**, DEB installés et démarrés, AppImage démarrée, sources correspondantes produites et manifeste des cinq fichiers validé. **Aucune archive Actions créée.**
- Cœur ASan/UBSan, 13 tests de publication et 9 tests de nettoyage réussis ; syntaxe Bash et structure YAML vérifiées. L’annulation en cours d’import, les révisions conservées, l’export intact, les lectures obsolètes et l’analyse réelle de répertoire sont testés.
- Les mesures et références sont enregistrées dans `verification-repertoires-2026-10-16.json`. Aucun essai sur le PC Voyager de l’utilisateur n’est revendiqué.

## Publication et stockage

Publication précédente : **[v0.3.0-dev.42](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.42)**, publiée le 16 octobre. [L’exécution 42](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/38861352628) est réussie sur le commit `fc941c17c53d6c5aa639433c6563f73469971ae9`, également cible du tag. **11/11 suites sur Debian 12 en 47,44 s et Debian 13 en 58,41 s**, deux DEB installés/démarrés, AppImage démarrée et sources correspondantes distribuées. Les cinq fichiers distants ont exactement les noms, tailles et SHA-256 du manifeste validé sur le runner. **Zéro archive Actions** pour cette exécution.

Le correctif du publieur utilise la réponse REST de création, des lectures/publication par identifiant exact et une attente bornée avant la résolution du tag par `gh release upload`. **17 tests de publication, 9 tests de nettoyage et le cœur ASan/UBSan réussissent dans cette CI.** Le code réel `selectRelease` a relu les métadonnées publiées : AppImage, Debian 12 et Debian 13 sont proposés depuis dev.33 sur Développement, exclus de Stable/Bêta, et une version ignorée, identique ou antérieure n’est pas proposée. Cela ne remplace pas un essai d’installation sur le PC Voyager.

Historique : les exécutions 36/38 ont échoué sur le quota d’archives Actions ; ne pas les relancer, car elles emploient l’ancien workflow. L’exécution 41 a réussi les tests/paquets mais échoué sur la relecture immédiate de son nouveau brouillon. Le brouillon vide **v0.3.0-dev.41** (id `414615852`, aucun fichier) est conservé ; il n’est pas distribué. La version 42 est la première publication effectivement validée sans archives Actions. Les versions antérieures et leurs sources sont préservées.

Nettoyage effectué le 15 octobre à 05:59 UTC : **58 archives / 8 371 207 728 octets** supprimés ; zéro archive et zéro cache après contrôle. Les dix releases antérieures et leurs cinquante fichiers sont conservés. La purge quotidienne reste présente pour les archives anciennes ; elle ne touche ni au code ni aux fichiers distribués.

## Reprise suivante

1. Audit global terminé et publié dans dev.52. La suite de la feuille de route peut reprendre depuis main, avec un corpus tactique sous licence ouverte et une progression par thème ; distinguer attaque observée et gain prouvé, sans inventer d’équivalences ni de niveau Elo.
2. Couverture probabiliste et comparaison de répertoires. L’édition de coups, le retrait de branches, le renommage, la suppression et les révisions restent synchrones ; le lookup des coups de la position actuelle est indexé et reste sur le thread UI.
3. Qualification de 1/2/5/10 millions de parties **réelles**. Seules 10 000 parties / 661 314 positions sont mesurées dans `performance-pgn-2026-10-15.json` ; le million de métadonnées synthétiques ne valide pas un million de PGN complets.
4. Module Syzygy local, OCR licencié avec validation de FEN, rapports pédagogiques/répertoires, restauration guidée et essais Voyager X11/Wayland/Polkit. Le backlog complet reste dans ROADMAP ; il n’est pas terminé.

Compilation locale : `source ../qt-env.sh`, build `--parallel 1`, aucun changement C++/CMake pendant la compilation. Un binaire généré corrompu après restauration doit être régénéré. Conserver l’ancien checkout non commité intact ; GitHub contient la base de reprise et les étapes de ce lot.
