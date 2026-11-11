#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Installe des paquets sur un exécuteur Ubuntu de GitHub sans dépendre de son miroir.
# Ce miroir (azure.archive.ubuntu.com) tombe de temps à autre sans échouer : chaque
# paquet attend vingt secondes avant d'être ignoré, et le travail meurt au bout de sa
# demi-heure sans avoir rien compilé. On lui laisse cinq minutes, puis on passe au
# miroir principal d'Ubuntu.
set -euo pipefail
apt_get() {
    sudo apt-get -o Acquire::http::Timeout=15 -o Acquire::Retries=2 -o DPkg::Lock::Timeout=120 "$@"
}
install() {
    apt_get update && timeout 300 sudo apt-get -o Acquire::http::Timeout=15 -o Acquire::Retries=2 \
        install -y --no-install-recommends "$@"
}
if ! install "$@"; then
    echo "Miroir lent ou en panne : bascule sur archive.ubuntu.com." >&2
    sudo sed -i 's|http://azure.archive.ubuntu.com/ubuntu|http://archive.ubuntu.com/ubuntu|g' \
        /etc/apt/sources.list /etc/apt/sources.list.d/* 2>/dev/null || true
    install "$@"
fi
