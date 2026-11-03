# Contribuer à Trait64

Merci de votre intérêt ! Les contributions en français ou en anglais sont bienvenues. *English is fine everywhere.*

## Signaler un problème ou proposer une idée

Utilisez les [modèles de demande](https://github.com/SimzyGit/Trait64/issues/new/choose). Pour un bug, un petit PGN qui le reproduit fait gagner beaucoup de temps. Une faille de sécurité se signale en privé : voir [SECURITY.md](SECURITY.md).

## Préparer l'environnement

Debian 12/13 ou Ubuntu 24.04 avec Qt ≥ 6.4. Les paquets nécessaires sont listés dans [scripts/install-from-source.sh](scripts/install-from-source.sh).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

`T64_STOCKFISH=/usr/games/stockfish` active les tests avec un vrai moteur ; `-DT64_SANITIZERS=ON` active ASan/UBSan. Le détail des suites est dans [docs/TESTING.md](docs/TESTING.md), l'organisation du code dans [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Proposer une modification

1. Créez une branche depuis `main` (`fix/…`, `feat/…`, `docs/…`).
2. Gardez chaque demande de fusion centrée sur un seul sujet.
3. Ajoutez ou adaptez les tests : tout comportement corrigé doit être couvert.
4. Formatez le C++ avec `clang-format` (configuration à la racine) ; suivez le style des fichiers QML voisins.
5. Textes de l'interface : écrivez-les en français dans `qsTr()` et ajoutez la traduction dans `translations/en.json`.
6. Mettez à jour `docs/CHANGELOG.md` pour tout changement visible.
7. Messages de commit : une première ligne courte qui dit ce qui change, puis le pourquoi si utile.

La CI compile et teste sous Linux à chaque demande de fusion ; elle doit être verte avant la fusion.

## Licence

En contribuant, vous acceptez que votre contribution soit publiée sous la licence du projet, [AGPL-3.0-or-later](LICENSE).
