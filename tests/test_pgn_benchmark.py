# SPDX-License-Identifier: AGPL-3.0-or-later
"""Exercise the optional benchmark as a process, using a tiny legal PGN fixture."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())
PGN = b'1.e4 e5 2.Ng1f3 Nc6 *\n\n1.d4 d5 2.c4 *\n'


class BenchmarkTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.input = self.root / 'games.pgn'
        self.input.write_bytes(PGN)
        self.database = self.root / 'games.sqlite'

    def run_benchmark(self, *args, success=True):
        result = subprocess.run([BINARY, '--input', str(self.input), *map(str, args)],
                                capture_output=True, text=True, timeout=30)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertEqual(self.input.read_bytes(), PGN)
        return result

    def test_report_cannot_replace_database_or_sidecars(self):
        for suffix in ('', '-wal', '-shm', '-journal', '.import.lock', '.backup', '.backup-wal'):
            with self.subTest(suffix=suffix):
                self.run_benchmark('--database', self.database, '--backup', '--report',
                                   str(self.database) + suffix, success=False)
                self.assertFalse(self.database.exists(), 'Rejected invocation must create no database')

    def test_existing_sidecars_and_backups_are_preserved(self):
        for suffix in ('-wal', '-shm', '-journal', '.import.lock', '.backup', '.backup-journal'):
            with self.subTest(suffix=suffix):
                path = Path(str(self.database) + suffix)
                path.write_bytes(b'preserve existing data')
                self.run_benchmark('--database', self.database, '--backup', success=False)
                self.assertEqual(path.read_bytes(), b'preserve existing data')
                self.assertFalse(self.database.exists())
                path.unlink()

    def test_parent_directory_alias_does_not_bypass_collision_check(self):
        alias = self.root / 'alias'
        alias.symlink_to(self.root, target_is_directory=True)
        self.run_benchmark('--database', self.database, '--report', alias / self.database.name,
                           success=False)
        self.assertFalse(self.database.exists())

    def test_existing_and_dangling_report_paths_are_preserved(self):
        self.run_benchmark('--report', self.input, success=False)
        missing = self.root / 'missing'
        link = self.root / 'link'
        link.symlink_to(missing)
        self.run_benchmark('--report', link, success=False)
        self.assertTrue(link.is_symlink())
        self.assertFalse(missing.exists())

    def test_parse_workers_produce_identical_complete_games(self):
        one = json.loads(self.run_benchmark('--threads', 1).stdout)
        report = self.root / 'report.json'
        self.run_benchmark('--threads', 4, '--report', report)
        four = json.loads(report.read_text())
        self.assertEqual(one['games'], 2)
        self.assertEqual(four['games'], 2)
        self.assertEqual(one['canonical_sha256'], four['canonical_sha256'])
        self.assertEqual(one['input_sha256'], four['input_sha256'])
        self.assertEqual(one['nodes'], four['nodes'])

    def test_import_and_backup_are_real_sqlite_files(self):
        result = self.run_benchmark('--database', self.database, '--threads', 4, '--backup')
        report = json.loads(result.stdout)
        self.assertEqual(report['games'], 2)
        self.assertEqual(report['integrity'], 'ok')
        self.assertTrue(report['backup_verified'])
        for path in (self.database, Path(str(self.database) + '.backup')):
            self.assertTrue(path.read_bytes().startswith(b'SQLite format 3\x00'))


if __name__ == '__main__':
    unittest.main()
