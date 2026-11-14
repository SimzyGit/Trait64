#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
import subprocess,sys,shutil
from pathlib import Path
appdir=Path(sys.argv[1]);out=appdir/'usr/share/doc/trait64';licenses=out/'runtime-licenses';licenses.mkdir(exist_ok=True,parents=True)
packages=set()
for name in sorted({p.name for p in appdir.rglob('*') if p.is_file() and '.so' in p.name}):
    if name=='t64sound.so':continue  # le module sonore de Trait64 lui-même, pas une bibliothèque du système
    query=subprocess.run(['dpkg-query','-S','*/'+name],capture_output=True,text=True)
    if query.returncode:raise SystemExit('Cannot identify bundled library source: '+name)
    for line in query.stdout.splitlines():
        package=line.split(': /',1)[0]
        if not package.startswith('diversion '):packages.add(package)
rows=[]
for package in sorted(packages):
    row=subprocess.check_output(['dpkg-query','-W','-f=${binary:Package}\t${source:Package}\t${source:Version}',package],text=True)
    rows.append(row)
    doc=Path('/usr/share/doc')/package.split(':')[0]/'copyright'
    if doc.exists():shutil.copyfile(doc,licenses/(package.replace(':','_')+'.copyright'))
(out/'runtime-sources.tsv').write_text('\n'.join(rows)+'\n')
