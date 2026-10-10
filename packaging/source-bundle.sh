#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
set -euo pipefail
trait64_build="${1:?Build directory required}"
trait64_dir="${trait64_build}/runtime-sources"
mkdir -p "${trait64_dir}"
# Debian containers expose deb822 sources. Add matching source-package indexes.
python3 - <<'PY'
from pathlib import Path
p=Path('/etc/apt/sources.list.d/debian.sources')
s=p.read_text().replace('Types: deb\n','Types: deb deb-src\n')
p.write_text(s)
PY
apt-get update
cd "${trait64_dir}"
cp "${trait64_build}/appimage/AppDir/usr/share/doc/trait64/runtime-sources.tsv" packages.tsv
python3 - <<'PY'
import subprocess
from pathlib import Path
pairs=set()
for line in Path('packages.tsv').read_text().splitlines():
    binary,source,version=line.split('\t')
    pairs.add((source or binary.split(':')[0],version))
for package,version in sorted(pairs):
    subprocess.run(['apt-get','source','--download-only','--yes',package+'='+version],check=True)
PY
tar -cf "${trait64_build}/Trait64-runtime-sources.tar" --exclude='./Trait64-runtime-sources.tar' .
