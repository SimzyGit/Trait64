#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Installe des paquets sur un exécuteur Ubuntu de GitHub sans dépendre de son miroir.
# Ce miroir (azure.archive.ubuntu.com hier, us-east-2.ec2.archive.ubuntu.com
# aujourd'hui, désigné directement ou par la liste /etc/apt/apt-mirrors.txt) tombe de
# temps à autre sans échouer : les paquets arrivent au compte-gouttes et le travail meurt
# sans avoir rien compilé. On lui laisse cinq minutes, puis on passe au miroir principal
# d'Ubuntu, quel que soit le nom du miroir de l'exécuteur.
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
    sudo sed -i -E -e 's#mirror\+file:/etc/apt/apt-mirrors\.txt#http://archive.ubuntu.com/ubuntu#g' \
        -e 's#https?://[a-z0-9.-]+\.archive\.ubuntu\.com/ubuntu#http://archive.ubuntu.com/ubuntu#g' \
        /etc/apt/sources.list /etc/apt/sources.list.d/* 2>/dev/null || true
    install "$@"
fi
