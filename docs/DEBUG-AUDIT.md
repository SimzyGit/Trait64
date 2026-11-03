# Audit de débogage global — 20–22 octobre 2026

Demande utilisateur : suspendre les ajouts et déboguer tout le logiciel.
Base exacte : `4e4cd3d36711c98a5952d26dca4b2531bc4c6264` (code de dev.46).

**Audit intégré dans la [PR #9](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/9), publié dans [v0.3.0-dev.52](https://github.com/feuvrierbernardinisimhon-png/Trait64/releases/tag/v0.3.0-dev.52).** Publication le 21 octobre 2026, vérification finale le 22 octobre. Dix groupes de défauts corrigés. Mesures et références : [verification-debug-2026-10-22.json](VALIDATION.md).

## Corrections et preuves de régression

| Défaut | Correction et scénario vérifié |
|---|---|
| Session JSON valide mais inexploitable remplacée sans copie de secours | Validation des onglets et chemins ; seuls les coups présents dans l’étude peuvent être restaurés. Tests `{}`, tableau vide, mauvais type, chemin absent ou illégal ; original conservé octet pour octet. Un JSON malformé ne produit qu’une copie de secours. |
| Annulation à la dernière notification de réindexation ignorée | Le refus de la notification finale déclenche aussi le rollback SQLite. Export et intégrité sont contrôlés après annulation. |
| Compteur sans prise/pion remis à zéro après 255 ; continuation au-delà du coup 32000 créant un nœud illisible | Compteurs internes élargis dans la dépendance, avec patch documenté. Les limites FEN publiques restent identiques ; rejet avant mutation de l’étude ou production d’une explication. Tests du dépassement, de l’annulation du coup et de l’étude inchangée. |
| Recherche UCI relancée depuis un signal de fin : ancienne réponse associée à la nouvelle position | Copie de la FEN et génération de recherche ; un arrêt ou remplacement réentrant invalide la livraison de l’ancien coup. Deux tests avec Stockfish réel. |
| Fermeture pendant une authentification GitHub : accès à des membres déjà détruits | Déconnexion des signaux des processus et de la réponse réseau avant destruction ; arrêt du processus d’authentification. Un faux `gh` isolé reste actif pendant la destruction. AddressSanitizer avait signalé un `heap-use-after-free` dans `Updater::fail`, appelé depuis la destruction de `QProcess`. |
| Nulle réclamable traitée comme fin forcée dans les analyses ; pat masqué par les 50 coups | Résolution des mats/pats/matériel insuffisant avant les motifs réclamables. Répétition et 50 coups continuent à recevoir une évaluation moteur réelle ; tests des nœuds analysés et de la priorité du pat. |
| Cadence PGN calculée avec les valeurs non bornées | Minutes et incrément bornés avant le calcul de l’horloge et de `TimeControl`. Test avec `INT_MAX`, attendu `10800+120`, sans dépassement signé. |
| Ancien cache réintroduisant une fausse fin de partie | Les entrées marquées terminales sont ignorées si la position actuelle ne l’est pas, puis remplacées par une vraie évaluation. Test d’un cache hérité injecté après une recherche réelle : recalcul nécessaire, puis réutilisation du cache réparé. Les analyses déjà terminées restent historiques ; en lancer une nouvelle applique la correction. |
| Options automatiques UCI ignorant les bornes déclarées par un moteur | Threads/Hash/MultiPV/Skill Level adaptés aux bornes annoncées ; identité du cache fondée sur les valeurs réellement envoyées. Les options personnalisées restent validées strictement. Un moteur de test refuse explicitement les valeurs hors limites. |
| Option UCI vide envoyée comme nom de fichier `<empty>` | Une chaîne vide est envoyée réellement vide ; les anciens profils contenant ce marqueur sont également corrigés. Stockfish quittait avec `Unable to open debug log file <empty>` dans un dossier protégé, notamment pour les analyses en arrière-plan. Test avec Stockfish réel et un dossier homonyme empêchant toute création du fichier, y compris en exécution privilégiée. |

## Vérification

Référence avant modification : **11/11 suites, 55,76 s**, GCC 13.3 / Qt 6.4.2, Stockfish 16 réel. Après correction : **12/12 suites locales validées** le 21 octobre, onze dans la passe de 84,12 s, puis le démarrage en 2,96 s après régénération d’un exécutable local incomplet restauré par l’environnement. Ce dernier incident ne nécessitait pas de modification du code.

Contrôles locaux : cœur **3 269**, robustesse **4 414**, interface **683**, analyses persistantes **121**, mises à jour **45**, stockage **158**, répertoires **51**, imports reprenables **24**. Le stress déterministe couvre **80 études classiques/Chess960 et 3 000 mutations FEN/PGN** : 330 acceptées avec round trip vérifié, 2 670 rejetées. Les parseurs en flux et parallèle vérifient aussi l’ordre des parties et leurs offsets.

| Contrôle | [PR, CI 39564062699](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/39564062699) | [Publication, CI 39566807355](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/39566807355) |
|---|---|---|
| Cœur ASan/UBSan | 2/2, 139,75 s | 2/2, 94,35 s |
| Tous les modules Qt ASan/UBSan, Stockfish réel | 12/12, 112,50 s | 12/12, 104,70 s |
| Debian 12 Release | 12/12, 58,70 s | 12/12, 60,21 s |
| Debian 13 Release | 12/12, 73,99 s | 12/12, 75,45 s |
| Deux DEB installés et démarrés, AppImage démarrée | Réussis | Réussis |
| Tests de publication / nettoyage, syntaxe Bash | 17 / 9 réussis | 17 / 9 réussis |
| Manifeste des cinq fichiers | Vérifié | Vérifié |

Le commit testé est `8eada72f4e94a15f4334d970d4b4a1972a46c538`. La fusion `e301ddb694562cead329e6b6f3ff3099c27f9ec3` possède exactement le même arbre `40e388e8ea15b53a85004357267338a7b41e94e8`. Les contrôles mémoire de tous les modules sont désormais requis avant la distribution, en complément du contrôle du cœur.

Les avertissements `-Wall -Wextra -Wpedantic -Wformat=2` sont étendus à tous les modules ; trois expressions logiques ont été parenthésées. Deux avertissements GCC 13 `-Wmaybe-uninitialized` subsistent dans les en-têtes regex/std_function de libstdc++ lors du build optimisé avec sanitizers ; aucune erreur mémoire ou de comportement indéfini n’est détectée dans les suites terminées. Le scan linuxdeployqt signale des modules Windows/macOS/iOS et QtQuick.Shapes absents ; Trait64 ne les importe pas et le démarrage de l’AppImage réussit.

### Échecs ayant conduit aux corrections

- Le 20 octobre, l’arbre `9188d119` passait 12/12 suites locales en Debug (92,78 s) et ASan/UBSan (120,18 s). La [CI 39393905485](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/39393905485) a ensuite révélé l’arrêt de Stockfish lié aux chaînes UCI vides. Le comportement a été reproduit avec le moteur réel, puis corrigé au commit `de41134a`.
- La [CI 39563408828](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/39563408828) a atteint le délai de 120 s du stress en Debug non optimisé avec sanitizers. Le délai passe à **300 s uniquement sous sanitizers**, et reste 120 s sinon. Le même corpus termine ensuite en 125,86 s dans la PR et 84,86 s dans la publication ; aucun cas, contrôle ni sanitizer retiré.
- Le test d’annulation d’export est rendu déterministe en annulant avant dispatch. Un export minuscule peut déjà être terminé à l’instruction UI suivante ; ce test ne suppose plus que l’annulation gagne cette course.

## Publication vérifiée

La préversion **dev.52**, publiée le 21 octobre à **10 h 24, heure de Paris**, vise le commit de fusion testé. Les cinq noms, tailles et SHA-256 fournis par GitHub correspondent exactement au manifeste produit par la CI. Aucune archive Actions n’a été créée.

Le 22 octobre, le sélecteur de mises à jour compilé depuis le code exact publié a relu les métadonnées réelles : AppImage, Debian 12 et Debian 13 sont proposés depuis dev.46 sur Développement ; Stable/Bêta, version ignorée, identique et rétrogradation restent exclus. Les paquets sont installés et testés dans la CI ; aucune installation sur le PC utilisateur n’a été effectuée.

## Périmètre et limites

Revue du cœur et des formats, SQLite/migrations/imports/répertoires, analyses/UCI, contrôleur/session/historique/entraînement/pendule, interface QML, téléchargements/mises à jour, scripts et distribution. Pas de modification du schéma 7 ni d’ajout de fonctionnalité.

Les mutations sont un stress déterministe, pas une preuve exhaustive de robustesse. Les tests graphiques Qt sont hors écran avec rendu logiciel : aucun essai sur le PC Voyager ou son GPU n’est revendiqué. LeakSanitizer est désactivé pour la validation locale et la tâche Qt ; cela ne prouve pas l’absence de fuites. La tâche CI du cœur garde le contrôle par défaut. Les règles automatiques des 75 coups/cinq répétitions et tous les cas complexes de position morte ne sont pas ajoutés par ce correctif. Les bibliothèques tierces ne font pas l’objet d’un audit exhaustif ligne par ligne.
