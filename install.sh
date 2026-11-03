#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Installs the latest published Trait64 for the current user, without sudo and without compiling:
#   curl -fsSL https://raw.githubusercontent.com/SimzyGit/Trait64/main/install.sh | bash
# Options: --deb (Debian 12/13 package, asks for sudo), --uninstall.
set -euo pipefail

repo="SimzyGit/Trait64"
api="${T64_API_URL:-https://api.github.com/repos/${repo}/releases?per_page=20}"
bin_dir="${HOME}/.local/bin"
app_dir="${HOME}/.local/share/applications"
icon_dir="${HOME}/.local/share/icons/hicolor/scalable/apps"
mode="appimage"

for argument in "$@"; do
    case "${argument}" in
        --deb) mode="deb" ;;
        --uninstall) mode="uninstall" ;;
        -h|--help) sed -n '3,6p' "$0" 2>/dev/null || true; exit 0 ;;
        *) echo "Option inconnue : ${argument}" >&2; exit 2 ;;
    esac
done

if [[ "${mode}" == "uninstall" ]]; then
    rm -f "${bin_dir}/Trait64.AppImage" "${bin_dir}/trait64" "${app_dir}/trait64.desktop" "${icon_dir}/trait64.svg"
    echo "Trait64 est désinstallé. Votre bibliothèque reste dans ~/.local/share/Trait64."
    exit 0
fi
if [[ "$(uname -s)" != "Linux" || "$(uname -m)" != "x86_64" ]]; then
    echo "Trait64 est publié pour Linux x86_64. Windows et macOS : voir le README." >&2
    exit 1
fi
for tool in curl python3 sha256sum; do
    command -v "${tool}" >/dev/null || { echo "Outil manquant : ${tool}" >&2; exit 1; }
done

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

echo "Recherche de la dernière version de Trait64…"
curl -fsSL -H "Accept: application/vnd.github+json" "${api}" -o "${work}/releases.json"
debian=""
if [[ "${mode}" == "deb" ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    [[ "${ID:-}" == "debian" || "${ID_LIKE:-}" == *debian* ]] || { echo "--deb vise Debian 12 ou 13." >&2; exit 1; }
    case "${VERSION_ID:-}" in 12|13) debian="${VERSION_ID}" ;; *) echo "Paquet prévu pour Debian 12 ou 13 ; utilisez l'AppImage." >&2; exit 1 ;; esac
fi
# Newest non-draft release (development builds included), then the file wanted and its checksums.
read -r version asset_url sums_url asset_name < <(python3 - "${work}/releases.json" "${mode}" "${debian}" <<'PY'
import json, sys
releases, mode, debian = json.load(open(sys.argv[1])), sys.argv[2], sys.argv[3]
for release in releases:
    if release.get("draft"):
        continue
    assets = {a["name"]: a["browser_download_url"] for a in release.get("assets", [])}
    wanted = [n for n in assets if (n.endswith(f"_debian{debian}_amd64.deb") if mode == "deb" else n.endswith("-linux-x86_64.AppImage"))]
    if wanted and "SHA256SUMS" in assets:
        print(release["tag_name"], assets[wanted[0]], assets["SHA256SUMS"], wanted[0])
        break
else:
    sys.exit("Aucune version publiée ne contient ce fichier.")
PY
)
echo "Téléchargement de Trait64 ${version} (${asset_name})…"
curl -fL --progress-bar "${asset_url}" -o "${work}/${asset_name}"
curl -fsSL "${sums_url}" -o "${work}/SHA256SUMS"
( cd "${work}" && grep -F "  ${asset_name}" SHA256SUMS | sha256sum -c --status ) \
    || { echo "Empreinte SHA-256 incorrecte : fichier refusé." >&2; exit 1; }
echo "Empreinte SHA-256 vérifiée."

if [[ "${mode}" == "deb" ]]; then
    sudo apt-get install -y "${work}/${asset_name}"
    echo "Trait64 ${version} est installé. Ouvrez « Trait64 » dans le menu des applications."
    exit 0
fi

mkdir -p "${bin_dir}" "${app_dir}" "${icon_dir}"
install -m 0755 "${work}/${asset_name}" "${bin_dir}/Trait64.AppImage"
ln -sf "${bin_dir}/Trait64.AppImage" "${bin_dir}/trait64"
icon_url="${T64_ICON_URL:-https://raw.githubusercontent.com/${repo}/main/assets/trait64.svg}"
curl -fsSL "${icon_url}" -o "${icon_dir}/trait64.svg" || true
# Without FUSE (common on recent distributions) an AppImage still runs by extracting itself.
launcher="\"${bin_dir}/Trait64.AppImage\""
if ! command -v fusermount >/dev/null && ! command -v fusermount3 >/dev/null; then
    launcher="env APPIMAGE_EXTRACT_AND_RUN=1 ${launcher}"
fi
cat > "${app_dir}/trait64.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Trait64
Comment=Bases de parties, analyse, préparation et entraînement aux échecs
Exec=${launcher} %F
Icon=trait64
Terminal=false
Categories=Game;BoardGame;
MimeType=application/x-chess-pgn;
DESKTOP
command -v update-desktop-database >/dev/null && update-desktop-database "${app_dir}" >/dev/null 2>&1 || true
echo
echo "Trait64 ${version} est installé. Ouvrez « Trait64 » dans le menu des applications,"
echo "ou lancez : ${bin_dir}/trait64"
echo "Les mises à jour suivantes se font depuis l'application (Aide → Mises à jour)."
