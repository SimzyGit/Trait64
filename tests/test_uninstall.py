#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""`install.sh --uninstall` doit retirer ce qui est installé, et le dire honnêtement.

Son premier état ne retirait que l'AppImage : qui avait posé le paquet Debian
s'entendait répondre « Trait64 est désinstallé » avec le logiciel toujours en place.
Rien ici ne touche à la machine : un faux HOME, et des `dpkg-query`, `sudo`, `apt-get`
de substitution posés dans le PATH.
"""
import os
import pathlib
import subprocess
import sys
import tempfile

script = pathlib.Path(__file__).resolve().parent.parent / "install.sh"
checks = 0


def check(ok, what):
    global checks
    checks += 1
    if not ok:
        print(f"ÉCHEC : {what}", file=sys.stderr)
        raise SystemExit(1)


def run(home, stubs=None):
    """Lance la désinstallation dans un HOME isolé, avec un PATH sans outils système."""
    binaries = pathlib.Path(home) / "stubs"
    binaries.mkdir(exist_ok=True)
    for name, body in (stubs or {}).items():
        target = binaries / name
        target.write_text("#!/usr/bin/env bash\n" + body)
        target.chmod(0o755)
    environment = dict(os.environ, HOME=str(home), PATH=f"{binaries}:/usr/bin:/bin")
    return subprocess.run(["bash", str(script), "--uninstall"], capture_output=True, text=True,
                          env=environment, timeout=60)


def installed_files(home):
    return [pathlib.Path(home) / p for p in (
        ".local/bin/Trait64.AppImage", ".local/bin/trait64", ".local/bin/trait64.previous",
        ".local/share/applications/trait64.desktop",
        ".local/share/icons/hicolor/scalable/apps/trait64.svg")]


with tempfile.TemporaryDirectory() as home:
    # Rien d'installé : il ne faut pas annoncer une désinstallation qui n'a pas eu lieu.
    empty = run(home)
    check(empty.returncode == 0, "An empty home is not an error")
    check("Aucune installation" in empty.stdout,
          "With nothing installed, the script says so instead of claiming success")

    # L'AppImage, son raccourci, son icône, et la copie gardée avant mise à jour.
    for path in installed_files(home):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("x")
    appimage = run(home)
    check(appimage.returncode == 0, "Removing an AppImage install succeeds")
    check("désinstallé" in appimage.stdout, "Removing an AppImage install says so")
    check(not any(p.exists() for p in installed_files(home)),
          "Every file the install left behind is removed, the update backup included")
    check("~/.local/share/Trait64" in appimage.stdout,
          "The user is told where the library stayed, and how to erase it")

with tempfile.TemporaryDirectory() as home:
    # Paquet Debian présent, et rien dans le HOME : c'est le cas qui était manqué.
    log = pathlib.Path(home) / "sudo.log"
    deb = run(home, {
        "dpkg-query": 'echo "install ok installed"\n',
        "sudo": f'echo "$@" >> {log}\n',
    })
    check(deb.returncode == 0, "Removing a Debian package succeeds")
    check(log.exists() and "apt-get remove --assume-yes trait64" in log.read_text(),
          "The Debian package is handed to apt-get rather than deleted file by file")
    check("désinstallé" in deb.stdout, "Removing a Debian package says so")

with tempfile.TemporaryDirectory() as home:
    # Paquet connu de dpkg mais déjà retiré : il ne reste rien à faire.
    absent = run(home, {"dpkg-query": 'echo "deinstall ok config-files"\n',
                        "sudo": 'echo "$@"\n'})
    check("Aucune installation" in absent.stdout,
          "A package already removed is not mistaken for one still installed")

print(f"{checks} uninstall checks passed")
