#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
set -euo pipefail
trait64_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
trait64_build="${1:-${trait64_root}/build}"
trait64_version="${2:?Semantic version required}"
trait64_stage="${trait64_build}/appimage"
mkdir -p "${trait64_stage}/tools" "${trait64_stage}/AppDir"
cd "${trait64_stage}/tools"
# Les empreintes ci-dessous fixent les binaires exacts qui ont été relus : c'est le seul
# garde-fou, et il suffit. Amont ne publie que l'étiquette « continuous » et remplace ses
# actifs à chaque reconstruction, en supprimant les précédents ; épingler l'identifiant
# d'actif n'apportait donc aucune reproductibilité, mais faisait échouer la construction
# par un 404 à chaque republication. En cas de republication, relire les binaires publiés
# puis reporter les empreintes que le message d'erreur affiche.
download_tool() {
    local repository="$1" name="$2"
    curl --fail --location --retry 3 \
        "https://github.com/${repository}/releases/download/continuous/${name}" -o "${name}"
}
download_tool linuxdeploy/linuxdeploy linuxdeploy-x86_64.AppImage
download_tool linuxdeploy/linuxdeploy-plugin-qt linuxdeploy-plugin-qt-x86_64.AppImage
cat > SHA256SUMS <<'HASHES'
df908b19f1ed1cdafa79508d415aa12d6c56ce4d56d4149aa8c09286718706e5  linuxdeploy-x86_64.AppImage
cfc1055b2b9dbc08412b579f20990b7b41a17b61beaa5847dc9477c96c9e9617  linuxdeploy-plugin-qt-x86_64.AppImage
HASHES
if ! sha256sum --check SHA256SUMS; then
    echo "Les outils publiés ne correspondent plus aux empreintes relues." >&2
    echo "Empreintes observées, à reporter ci-dessus après relecture d'amont :" >&2
    sha256sum ./*.AppImage >&2
    exit 1
fi
chmod +x ./*.AppImage
DESTDIR="${trait64_stage}/AppDir" cmake --install "${trait64_build}" --prefix /usr
# Le module sonore reste hors de l'AppImage : il y ferait entrer Qt Multimedia, GStreamer
# et PulseAudio, soit une pile audio entière à faire tourner sur douze distributions.
# Les sons sont livrés par le paquet .deb, qui prend ces bibliothèques au système.
rm -rf "${trait64_stage}/AppDir/usr/lib/trait64"
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
# Le lanceur vérifie la glibc et les bibliothèques du système avant de démarrer, et dit
# en clair ce qui manque : voir packaging/AppRun.
./linuxdeploy-x86_64.AppImage --appdir "${trait64_stage}/AppDir" --desktop-file "${trait64_root}/assets/trait64.desktop" --icon-file "${trait64_root}/assets/trait64.svg" --custom-apprun "${trait64_root}/packaging/AppRun" --plugin qt
python3 "${trait64_root}/packaging/runtime-sources.py" "${trait64_stage}/AppDir"
./linuxdeploy-x86_64.AppImage --appdir "${trait64_stage}/AppDir" --output appimage
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software "${OUTPUT}" --smoke-test
