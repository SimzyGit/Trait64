# SPDX-License-Identifier: AGPL-3.0-or-later
"""Explicit qualification on the pinned CC0 corpus; reports go to logs, not artifacts."""
import argparse
import ctypes
import hashlib
import json
import os
import platform
import shutil
import sqlite3
import subprocess
import time
from pathlib import Path

CORPUS_SHA = '8963b6a1620a0e9c77e5515a0744ec133e86869487188af047bb0a74400dee37'
CANONICAL_SHA = 'eb0826cb0ba0f70ee446c1bf706224031a500c6ec5bd7e6028845bc1bf97ca1f'
SOURCE = 'https://database.lichess.org/standard/lichess_db_standard_rated_2013-01.pgn.zst'


def emit(name, value):
    print('TRAIT64_REPORT_BEGIN ' + name, flush=True)
    print(json.dumps(value, indent=2, ensure_ascii=False), flush=True)
    print('TRAIT64_REPORT_END ' + name, flush=True)


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--scenario', required=True, choices=['gui', 'storage'])
    args = parser.parse_args()
    corpus = args.input.resolve(strict=True)
    if corpus.stat().st_size != 92811021 or sha256(corpus) != CORPUS_SHA:
        raise RuntimeError('Corpus does not match the pinned, real PGN')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    runtime = out / 'runtime'
    runtime.mkdir(mode=0o700)
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
               XDG_RUNTIME_DIR=str(runtime))
    bench = Path('build-perf/pgn_benchmark').resolve(strict=True)
    gui = Path('build-perf/desktop_tests').resolve(strict=True)
    library = ctypes.CDLL('libsqlite3.so.0')
    library.sqlite3_libversion.restype = ctypes.c_char_p
    method = {
        'host': platform.platform(), 'logical_cpus': os.cpu_count(),
        'sqlite': library.sqlite3_libversion().decode(),
        'source_url': SOURCE, 'source_sha256': CORPUS_SHA, 'source_bytes': corpus.stat().st_size,
        'tested_runtime_commit': '550f28f51f174b8d3b1d5f896928c2cfa09e04d4',
        'measurement_commit': os.environ.get('GITHUB_SHA', ''),
        'workflow_run': os.environ.get('GITHUB_RUN_ID', ''),
        'scenario': args.scenario,
        'compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0],
        'build': 'Release -O3 -DNDEBUG',
        'free_disk_bytes_before_runs': shutil.disk_usage(out).free,
        'binary_sha256': {p.name: sha256(p) for p in [bench, gui]},
        'conditions': 'Separate GitHub ubuntu-24.04 runners for GUI and storage; scenarios within one runner are sequential after compilation. Caches not purged. GUI is offscreen software. RSS excludes kernel file cache. No Voyager hardware claim.',
    }
    for line in Path('/proc/cpuinfo').read_text().splitlines():
        if line.startswith('model name'):
            method['cpu_model'] = line.split(':', 1)[1].strip()
            break
    for name in ['cpu.max', 'memory.max']:
        p = Path('/sys/fs/cgroup') / name
        if p.exists():
            method[name] = p.read_text().strip()
    emit('method', method)

    def run(name, command, stdout_json=False):
        available = shutil.disk_usage(out).free
        if available < 14 * 1024**3:
            raise RuntimeError(f'Insufficient free disk: {available} bytes; need 14 GiB')
        started = time.monotonic()
        started_utc = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
        print('START ' + name, flush=True)
        stdout_path = out / (name + '.stdout')
        stderr_path = out / (name + '.stderr')
        with stdout_path.open('x') as stdout, stderr_path.open('x') as stderr:
            child = subprocess.Popen(command, stdout=stdout, stderr=stderr, env=env)
            _, status, usage = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
        resources = {
            'name': name, 'argv': command, 'started_utc': started_utc,
            'completed_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
            'process_seconds': time.monotonic() - started, 'maximum_rss_kib': usage.ru_maxrss,
            'user_seconds': usage.ru_utime, 'system_seconds': usage.ru_stime,
            'resource_method': 'Linux os.wait4 child rusage', 'returncode': child.returncode,
        }
        emit(name + '-process', resources)
        path = stdout_path if stdout_json else out / (name + '.json')
        report = None
        if path.exists() and path.stat().st_size:
            report = json.loads(path.read_text())
            emit(name, report)
        if child.returncode:
            print(stderr_path.read_text()[-10000:], flush=True)
            raise RuntimeError(name + ' failed')
        if report is None:
            raise RuntimeError(name + ' produced no report')
        return report

    if args.scenario == 'gui':
        gui_report = run('gui-100k', [str(gui), '--benchmark-import', str(corpus),
                                    '--benchmark-games', '100000',
                                    '--benchmark-timeout-seconds', '5400'], stdout_json=True)
        if gui_report['stored_games'] < 100000 or gui_report['qml_warnings']:
            raise RuntimeError('GUI qualification did not meet its requested corpus size')
        return
    reindex = run('reindex-10k', [str(bench), '--input', str(corpus), '--threads', '4',
                               '--limit', '10000', '--database', str(out / 'reindex.sqlite'),
                               '--reindex', '--source-url', SOURCE,
                               '--report', str(out / 'reindex-10k.json')])
    if reindex['reindexed_games'] != 10000 or reindex['integrity'] != 'ok':
        raise RuntimeError('Reindex validation failed')
    imported = run('material-100k', [str(bench), '--input', str(corpus), '--threads', '4',
                                    '--limit', '100000', '--pause-at', '50000', '--probe-reads',
                                    '--database', str(out / 'material-100k.sqlite'), '--backup',
                                    '--source-url', SOURCE, '--report', str(out / 'material-100k.json')])
    if imported['games'] != 100000 or imported['integrity'] != 'ok' or not (
            imported['backup_verified'] and imported['restore_verified']):
        raise RuntimeError('Import and restore validation failed')
    content = {'method': 'SHA-256 of every canonical PGN ordered by game id; counts of both position tables. Not a byte comparison of every index row.', 'databases': {}}
    for suffix in ['', '.backup', '.restored']:
        path = out / ('material-100k.sqlite' + suffix)
        with sqlite3.connect('file:' + str(path) + '?mode=ro', uri=True) as db:
            digest = hashlib.sha256()
            count = 0
            for (pgn,) in db.execute('SELECT pgn FROM games ORDER BY id'):
                digest.update(pgn.encode('utf-8'))
                count += 1
            positions = db.execute('SELECT COUNT(*) FROM positions').fetchone()[0]
            # Index allégé : 40 demi-coups au plus par partie, et des signatures pour le reste.
            expected = db.execute('SELECT SUM(MIN(plies+1,40)) FROM games').fetchone()[0]
            signatures = db.execute('SELECT COUNT(*) FROM game_signatures').fetchone()[0]
            if count != 100000 or positions != expected or signatures < count or digest.hexdigest() != CANONICAL_SHA:
                raise RuntimeError('Canonical content mismatch in ' + path.name)
            content['databases'][path.name] = {'games': count, 'positions': positions,
                'signatures': signatures, 'canonical_sha256': digest.hexdigest(),
                'closed_database_bytes': path.stat().st_size}
    emit('restored-content-100k', content)
    with sqlite3.connect('file:' + str(out / 'material-100k.sqlite') + '?mode=ro', uri=True) as db:
        rows = db.execute('SELECT name,SUM(pgsize),SUM(payload),SUM(unused),COUNT(*) FROM dbstat GROUP BY name ORDER BY SUM(pgsize) DESC').fetchall()
    emit('storage-profile-100k', {'method': 'SQLite dbstat after all timed runs; excludes WAL/SHM',
        'objects': [dict(zip(['name', 'bytes', 'payload_bytes', 'unused_bytes', 'pages'], row)) for row in rows]})


if __name__ == '__main__':
    main()
