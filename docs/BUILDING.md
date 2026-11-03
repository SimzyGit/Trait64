# Installer et compiler

## Installation : détails

L’installation rapide est décrite dans [Télécharger](../README.md#télécharger). Les fichiers publiés : une **AppImage x86_64** (glibc ≥ 2.36) et deux paquets **.deb**, pour Debian 12 et Debian 13, accompagnés de leurs empreintes `SHA256SUMS`. Sans FUSE, l’AppImage se lance avec `APPIMAGE_EXTRACT_AND_RUN=1 ./Trait64-…AppImage` ; l’installateur le règle tout seul. Sur Debian, `install.sh --deb` installe le paquet correspondant à votre version.

Après l’installation, **Aide → Mises à jour** vérifie les nouvelles versions (canaux Stable, Bêta, Développement), télécharge le fichier, vérifie son empreinte et installe après une sauvegarde. Voir [UPDATES.md](UPDATES.md).

## Compiler et installer depuis les sources

Debian 12 ou 13, Ubuntu 24.04 et leurs dérivés avec Qt ≥ 6.4 sont les cibles de l’installateur. La compilation utilise les bibliothèques de **votre** système ; aucun binaire Ubuntu n’est présenté comme universel pour Debian.

1. Sur GitHub : **Code → Download ZIP / Télécharger ZIP**. Décompressez l’archive.
2. Ouvrez un terminal dans le dossier `Trait64-main`.
3. Lancez :

```bash
bash scripts/install-from-source.sh
```

Le script demande votre mot de passe uniquement pour installer les paquets Debian, compile, exécute les tests, puis installe Trait64 dans `~/.local`. Deux compilations simultanées par défaut pour limiter la mémoire utilisée. La première installation nécessite Internet et peut prendre plusieurs minutes. **L’utilisation du logiciel est ensuite hors ligne**, Stockfish compris.

Ouvrez **Trait64** dans le menu des applications, ou :

```bash
~/.local/bin/trait64
```

Pour mettre à jour une compilation locale, vous pouvez relancer le script ou utiliser le module de mise à jour qui installera l’AppImage dans `~/.local/bin/trait64`. Les données sont stockées hors du dossier source. N’exécutez pas tout le script avec `sudo`.

## Compiler sans l’installateur

Avec les dépendances Qt6, CMake, `libarchive-dev` et `libsqlite3-dev` installées :

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/trait64
```

Tester seulement le cœur sans Qt :

```bash
cmake -S . -B build-core -DT64_BUILD_GUI=OFF
cmake --build build-core --parallel 2
ctest --test-dir build-core --output-on-failure
```

`T64_SANITIZERS=ON` active ASan/UBSan avec GCC/Clang. `cpack --config build/CPackConfig.cmake -G DEB` produit un paquet **pour la distribution de compilation** après une compilation sur un système normalement installé. La CI construit et teste les cibles Linux. Windows/macOS ne sont pas annoncés comme validés dans cette version.
