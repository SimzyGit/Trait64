# SPDX-License-Identifier: AGPL-3.0-or-later
import copy
import datetime as dt
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('cleanup_actions', Path(__file__).parents[1] / 'scripts' / 'cleanup-actions.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
NOW = dt.datetime(2026, 10, 15, 6, tzinfo=dt.timezone.utc)


def artifact(number, run=1, name='Trait64-debian12-amd64', age=48):
    return {'id': number, 'name': name, 'size_in_bytes': 1000 + number,
            'workflow_run': {'id': run}, 'created_at': (NOW - dt.timedelta(hours=age)).isoformat()}


class FakeAPI:
    def __init__(self, artifacts=()):
        self.artifacts = copy.deepcopy(list(artifacts))
        self.caches = [{'id': 70, 'size_in_bytes': 5000}]
        self.runs = {1: {'id': 1, 'status': 'completed', 'conclusion': 'success'},
                     2: {'id': 2, 'status': 'in_progress'},
                     3: {'id': 3, 'status': 'completed', 'conclusion': 'failure'}}
        self.releases = [{'id': 10, 'draft': False, 'tag_name': 'v0.3.0-dev.33'}]
        self.assets = [{'id': i, 'name': name, 'size': 200, 'state': 'uploaded', 'digest': 'sha256:example'}
                       for i, name in enumerate(['app.AppImage', 'a.deb', 'SHA256SUMS', 'Trait64-runtime-sources.tar'])]
        self.deleted = []
        self.active = []

    def pages(self, path, key=None):
        if path == '/releases': return copy.deepcopy(self.releases)
        if path == '/releases/10/assets': return copy.deepcopy(self.assets)
        if path == '/actions/artifacts': return copy.deepcopy(self.artifacts)
        if path == '/actions/caches': return copy.deepcopy(self.caches)
        if path.startswith('/actions/runs?'): return copy.deepcopy(self.active)
        raise AssertionError(path)

    def request(self, method, path):
        if method == 'GET' and path.startswith('/actions/runs/'):
            return copy.deepcopy(self.runs[int(path.rsplit('/', 1)[1])])
        if method == 'DELETE':
            self.deleted.append(path)
            number = int(path.rsplit('/', 1)[1])
            if path.startswith('/actions/artifacts/'):
                self.artifacts = [a for a in self.artifacts if a['id'] != number]
            elif path.startswith('/actions/caches/'):
                self.caches = [a for a in self.caches if a['id'] != number]
            else: raise AssertionError('Deletion outside Actions artifacts/caches')
            return None
        raise AssertionError((method, path))


class CleanupTests(unittest.TestCase):
    def run_cleanup(self, api, **options):
        return module.cleanup(api, now=NOW, log=lambda _: None, **options)

    def test_default_dry_run_preserves_all_data(self):
        api = FakeAPI([artifact(1)])
        report = self.run_cleanup(api, all_completed=True, clear_caches=True)
        self.assertEqual(api.deleted, [])
        self.assertEqual(report['remaining_artifacts'], 1)

    def test_full_cleanup_keeps_active_unknown_and_all_releases(self):
        api = FakeAPI([artifact(1), artifact(2, run=2), artifact(3, name='personal-backup'), artifact(4, run=3)])
        before = copy.deepcopy(api.assets)
        report = self.run_cleanup(api, apply=True, all_completed=True)
        self.assertEqual(api.deleted, ['/actions/artifacts/1', '/actions/artifacts/4'])
        self.assertEqual(report['deleted_artifact_bytes'], 2005)
        self.assertEqual(api.assets, before)
        self.assertEqual(report['releases_preserved'], 1)

    def test_only_successful_named_run_can_be_cleaned_immediately(self):
        api = FakeAPI([artifact(1), artifact(2, run=3)])
        self.run_cleanup(api, apply=True, run_id=3)
        self.assertEqual(api.deleted, [])
        self.run_cleanup(api, apply=True, run_id=1)
        self.assertEqual(api.deleted, ['/actions/artifacts/1'])

    def test_scheduled_cleanup_preserves_recent_failures(self):
        api = FakeAPI([artifact(1, run=3, age=2), artifact(2, run=3, age=25)])
        self.run_cleanup(api, apply=True)
        self.assertEqual(api.deleted, ['/actions/artifacts/2'])

    def test_sources_must_remain_available_before_any_deletion(self):
        api = FakeAPI([artifact(1)])
        api.assets = [a for a in api.assets if a['name'] != 'Trait64-runtime-sources.tar']
        with self.assertRaisesRegex(RuntimeError, 'sources'):
            self.run_cleanup(api, apply=True, all_completed=True)
        self.assertEqual(api.deleted, [])

    def test_completed_snapshot_is_checked_again_at_deletion_time(self):
        api = FakeAPI([artifact(1)])
        api.runs[1]['status'] = 'queued'
        self.run_cleanup(api, apply=True, all_completed=True)
        self.assertEqual(api.deleted, [])

    def test_cache_cleanup_does_not_interrupt_other_runs(self):
        api = FakeAPI()
        api.active = [{'id': 99}, {'id': 123}]
        self.run_cleanup(api, apply=True, clear_caches=True, current_run_id=99)
        self.assertEqual(api.deleted, [])
        api.active = [{'id': 99}]
        report = self.run_cleanup(api, apply=True, clear_caches=True, current_run_id=99)
        self.assertEqual(api.deleted, ['/actions/caches/70'])
        self.assertEqual(report['deleted_cache_bytes'], 5000)

    def test_pagination_collects_all_pages_before_deleting(self):
        class Paged(module.GitHub):
            def __init__(self): self.calls = []
            def request(self, method, path):
                self.calls.append((method, path))
                page = int(path.rsplit('=', 1)[1])
                return {'artifacts': list(range(100)) if page == 1 else [100]}
        api = Paged()
        self.assertEqual(len(api.pages('/actions/artifacts', 'artifacts')), 101)
        self.assertEqual(len(api.calls), 2)
        self.assertTrue(all(method == 'GET' for method, _ in api.calls))

    def test_repository_scope_cannot_expand(self):
        with self.assertRaises(ValueError): module.GitHub('other/repo', 'test-token')
        with self.assertRaises(ValueError): module.GitHub(module.REPOSITORY, '')


if __name__ == '__main__': unittest.main()
