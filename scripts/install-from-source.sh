#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
set -euo pipefail
trait64_source="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "${EUID}" -eq 0 ]]; then
    echo 'Lance ce script avec ton compte habituel, sans sudo. Il demandera sudo pour installer les dépendances.' >&2
    exit 1
fi
if ! command -v apt-get >/dev/null; then
    echo 'Cet installateur vise Debian / Ubuntu / Voyager. Voir README.md pour une compilation manuelle.' >&2
    exit 1
fi
trait64_prefix="${HOME}/.local"
trait64_jobs="${T64_BUILD_JOBS:-2}"
if ! [[ "${trait64_jobs}" =~ ^[1-9][0-9]*$ ]]; then
    echo 'T64_BUILD_JOBS doit être un entier positif.' >&2
    exit 1
fi
echo 'Installation des dépendances de compilation, de Qt6 et de Stockfish…'
sudo apt-get update
sudo apt-get install --no-install-recommends -y \
    build-essential cmake ninja-build python3 file libarchive-dev libsqlite3-dev qt6-base-dev qt6-declarative-dev \
    qml6-module-qtqml qml6-module-qtqml-models qml6-module-qtqml-workerscript \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
    qml6-module-qtquick-dialogs qml6-module-qtquick-templates qml6-module-qtquick-window \
    libqt6sql6-sqlite libqt6svg6 qt6-wayland fonts-dejavu-core stockfish
# Debian 13 splits image-format plugins out of libqt6svg6.
if apt-cache show qt6-svg-plugins >/dev/null 2>&1; then
    sudo apt-get install --no-install-recommends -y qt6-svg-plugins
fi
echo 'Compilation de Trait64 pour cet ordinateur…'
cmake -S "${trait64_source}" -B "${trait64_source}/build-linux" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${trait64_prefix}"
cmake --build "${trait64_source}/build-linux" --parallel "${trait64_jobs}"
ctest --test-dir "${trait64_source}/build-linux" --output-on-failure
cmake --install "${trait64_source}/build-linux"
# Le menu doit fonctionner même si ~/.local/bin n’est pas encore dans PATH.
trait64_desktop="${trait64_prefix}/share/applications/trait64.desktop"
python3 - "${trait64_desktop}" "${trait64_prefix}/bin/trait64" <<'PY'
import pathlib, sys
p = pathlib.Path(sys.argv[1])
# Desktop Entry Exec quoting (not shell evaluation).
exe = sys.argv[2].replace('\\', '\\\\').replace('"', '\\"').replace('`', '\\`').replace('$', '\\$').replace('%', '%%')
p.write_text(p.read_text().replace('Exec=trait64', 'Exec="' + exe + '"'))
PY
if command -v update-desktop-database >/dev/null; then
    update-desktop-database "${trait64_prefix}/share/applications" || true
fi
echo
echo 'Trait64 est installé. Ouvre « Trait64 » dans le menu des applications.'
echo "Tu peux aussi lancer : ${trait64_prefix}/bin/trait64"
