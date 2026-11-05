# SPDX-License-Identifier: AGPL-3.0-or-later
"""Exercise the synthetic database benchmark as a process, at its smallest fixture.

The benchmark segfaulted for the whole lifetime of a schema migration: it kept writing
the pre-migration opening key, the explorer never matched those rows, and the tool read
the first element of an empty list. Nothing reported it because, unlike the PGN
benchmark, this one had no safety test. Running it once at 1,000 games costs about a
second and would have caught that on the first build.
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())
MEASUREMENTS = ('library_page', 'deep_cursor_200', 'deep_offset_200', 'trigram_substring',
                'exact_player', 'statistics', 'opening_tree', 'top_players')


class DatabaseBenchmarkTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_benchmark(self, *args, success=True):
        result = subprocess.run([BINARY, *map(str, args)], capture_output=True, text=True,
                                timeout=300, cwd=self.root)
        if success:
            # A crash reports a negative code; asserting zero covers both failure modes.
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertEqual(sorted(p.name for p in self.root.iterdir()), [],
                         'The benchmark must leave nothing behind in the working directory')
        return result

    def test_smallest_fixture_reports_every_measurement(self):
        report = json.loads(self.run_benchmark(1000).stdout)
        self.assertEqual(report['games'], 1000)
        self.assertGreater(report['database_bytes'], 0)
        self.assertGreaterEqual(report['seed_ms'], 0)
        for name in MEASUREMENTS:
            with self.subTest(measurement=name):
                sample = report[name]
                self.assertGreaterEqual(sample['max_ms'], sample['median_ms'])
                self.assertGreater(sample['median_ms'], 0)

    def test_opening_tree_is_measured_on_populated_statistics(self):
        # The regression guard: a mismatched opening key leaves the explorer empty, and the
        # benchmark then fails loudly here instead of reading past the end of a list.
        report = json.loads(self.run_benchmark(1000).stdout)
        self.assertIn('opening_tree', report)

    def test_counts_outside_the_accepted_range_are_refused(self):
        for count in (0, 999, 2_000_001):
            with self.subTest(count=count):
                result = self.run_benchmark(count, success=False)
                self.assertIn('1,000', result.stderr)

    def test_a_count_that_is_not_a_number_is_refused(self):
        self.run_benchmark('beaucoup', success=False)


if __name__ == '__main__':
    unittest.main()
