#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Run inside a clean Debian container. /source is read-only, /packages is shared.
set -euo pipefail
trait64_debian="${1:?Debian version required}"
trait64_version="${2:?Release version required}"
[[ "$trait64_debian" == 12 || "$trait64_debian" == 13 ]]
[[ "$trait64_version" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-(beta|rc|dev)\.(0|[1-9][0-9]*))?$ ]]
apt-get update
apt-get install -y --no-install-recommends ca-certificates git file curl python3 build-essential cmake ninja-build libarchive-dev libsqlite3-dev qt6-base-dev qt6-declarative-dev qt6-multimedia-dev qml6-module-qtqml qml6-module-qtqml-models qml6-module-qtqml-workerscript qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-dialogs qml6-module-qtquick-templates qml6-module-qtquick-window libqt6sql6-sqlite libqt6svg6 qt6-wayland fonts-dejavu-core stockfish patchelf desktop-file-utils
if apt-cache show qt6-svg-plugins >/dev/null 2>&1; then apt-get install -y --no-install-recommends qt6-svg-plugins; fi
cmake -S /source -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DT64_BENCHMARKS=ON -DT64_RELEASE_VERSION="$trait64_version"
cmake --build /build --parallel 2
export T64_STOCKFISH=/usr/games/stockfish
# Monté seulement quand on veut regarder le résultat : les tests écrivent alors une image
# de l'écran d'analyse, qu'aucune relecture de QML ne remplace. Volume distinct de
# /packages, dont le script de publication refuse tout fichier inattendu.
if [[ -d /captures ]]; then
    export T64_BOARD_SCREENSHOT=/captures/analyse-debian${trait64_debian}.png
    export T64_ANALYSIS_SCREENSHOT=/captures/analyses-en-cours-debian${trait64_debian}.png
fi
ctest --test-dir /build --output-on-failure
cd /build
cpack -G DEB
trait64_deb=(trait64_*_amd64.deb)
[[ "${#trait64_deb[@]}" == 1 && -f "${trait64_deb[0]}" ]]
apt-get install -y "./${trait64_deb[0]}"
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software trait64 --smoke-test
install -m 0644 "${trait64_deb[0]}" "/packages/trait64_${trait64_version}_debian${trait64_debian}_amd64.deb"
if [[ "$trait64_debian" == 12 ]]; then
    bash /source/packaging/appimage.sh /build "$trait64_version"
    bash /source/packaging/source-bundle.sh /build
    install -m 0644 "/build/Trait64-${trait64_version}-linux-x86_64.AppImage" /packages/
    install -m 0644 /build/Trait64-runtime-sources.tar /packages/
fi
