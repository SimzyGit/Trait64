#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
set -euo pipefail
trait64_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
trait64_build="${1:-${trait64_root}/build}"
trait64_version="${2:?Semantic version required}"
trait64_stage="${trait64_build}/appimage"
mkdir -p "${trait64_stage}/tools" "${trait64_stage}/AppDir"
cd "${trait64_stage}/tools"
# API asset IDs and hashes pin the exact reviewed build tools, even though upstream uses continuous tags.
# Amont republie « continuous » de temps à autre : l'ancien identifiant disparaît (404) et le repli
# rapporte un autre binaire, que l'empreinte ci-dessous rejette. Quand cela arrive, relever le nouvel
# identifiant et son empreinte sur la version publiée, puis les reporter ici après relecture.
download_tool() {
    local repository="$1" asset="$2" name="$3"
    # Shared Actions IPs may exhaust the unauthenticated REST rate limit.
    # The public download fallback is still required to match the pinned digest below.
    curl --fail --location --retry 3 --header 'Accept: application/octet-stream' \
        "https://api.github.com/repos/${repository}/releases/assets/${asset}" -o "${name}" || \
    curl --fail --location --retry 3 \
        "https://github.com/${repository}/releases/download/continuous/${name}" -o "${name}"
}
download_tool linuxdeploy/linuxdeploy 699514588 linuxdeploy-x86_64.AppImage
download_tool linuxdeploy/linuxdeploy-plugin-qt 525032210 linuxdeploy-plugin-qt-x86_64.AppImage
cat > SHA256SUMS <<'HASHES'
df908b19f1ed1cdafa79508d415aa12d6c56ce4d56d4149aa8c09286718706e5  linuxdeploy-x86_64.AppImage
cfc1055b2b9dbc08412b579f20990b7b41a17b61beaa5847dc9477c96c9e9617  linuxdeploy-plugin-qt-x86_64.AppImage
HASHES
sha256sum --check SHA256SUMS
chmod +x ./*.AppImage
DESTDIR="${trait64_stage}/AppDir" cmake --install "${trait64_build}" --prefix /usr
export QMAKE=/usr/bin/qmake6
export QML_SOURCES_PATHS="${trait64_root}/ui"
export EXTRA_QT_MODULES='svg;sql'
export EXTRA_PLATFORM_PLUGINS='libqoffscreen.so;libqwayland-egl.so;libqwayland-generic.so'
export APPIMAGE_EXTRACT_AND_RUN=1
export VERSION="${trait64_version}"
export OUTPUT="${trait64_build}/Trait64-${trait64_version}-linux-x86_64.AppImage"
# Preserve distribution copyright notices and precise package/source versions for the bundled shared libraries.
mkdir -p "${trait64_stage}/AppDir/usr/share/doc/trait64/runtime-licenses"
cp "${trait64_root}/packaging/APPIMAGE-NOTICE.md" "${trait64_stage}/AppDir/usr/share/doc/trait64/"
./linuxdeploy-x86_64.AppImage --appdir "${trait64_stage}/AppDir" --desktop-file "${trait64_root}/assets/trait64.desktop" --icon-file "${trait64_root}/assets/trait64.svg" --plugin qt
python3 "${trait64_root}/packaging/runtime-sources.py" "${trait64_stage}/AppDir"
./linuxdeploy-x86_64.AppImage --appdir "${trait64_stage}/AppDir" --output appimage
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software "${OUTPUT}" --smoke-test
