#!/usr/bin/env python3
"""Remove reproducible Trait64 Actions artifacts; never touch releases or Git data."""
# SPDX-License-Identifier: AGPL-3.0-or-later
import argparse
import datetime as dt
import json
import os
import urllib.error
import urllib.request

REPOSITORY = 'SimzyGit/Trait64'
ARTIFACT_NAMES = {'Trait64-debian12-amd64', 'Trait64-debian13-amd64'}


class GitHub:
    def __init__(self, repository, token):
        if repository.lower() != REPOSITORY.lower() or not token:
            raise ValueError('An authenticated Trait64 repository context is required')
        self.prefix = f'https://api.github.com/repos/{repository}'
        self.token = token

    def request(self, method, path):
        if not path.startswith('/') or path.startswith('//'):
            raise ValueError('Repository-relative API path required')
        request = urllib.request.Request(self.prefix + path, method=method, headers={
            'Authorization': f'Bearer {self.token}',
            'Accept': 'application/vnd.github+json',
            'X-GitHub-Api-Version': '2022-11-28',
        })
        # API deletion/list endpoints must not redirect authorization to another host.
        class NoRedirect(urllib.request.HTTPRedirectHandler):
            def redirect_request(self, req, fp, code, msg, headers, newurl):
                return None
        try:
            with urllib.request.build_opener(NoRedirect).open(request, timeout=30) as response:
                data = response.read()
                return json.loads(data) if data else None
        except urllib.error.HTTPError as error:
            if method == 'DELETE' and error.code == 404:
                return {'already_absent': True}
            raise RuntimeError(f'GitHub {method} {path}: HTTP {error.code}') from None

    def pages(self, path, key=None):
        result = []
        separator = '&' if '?' in path else '?'
        for page in range(1, 1001):
            response = self.request('GET', f'{path}{separator}per_page=100&page={page}')
            values = response[key] if key else response
            result.extend(values)
            if len(values) < 100:
                return result
        raise RuntimeError('Pagination limit reached; no deletion attempted')


def release_inventory(api):
    inventory = []
    for release in api.pages('/releases'):
        if release['draft']:
            continue
        assets = api.pages(f"/releases/{release['id']}/assets")
        packages = [a for a in assets if a['name'].endswith(('.deb', '.AppImage'))]
        if packages:
            names = {a['name'] for a in assets if a.get('state') == 'uploaded' and a['size'] > 0}
            if 'SHA256SUMS' not in names or not ({'Trait64-runtime-sources.tar', 'Trait64-runtime-sources.tar.xz'} & names):
                raise RuntimeError(f"Release {release['tag_name']} lacks its published checksums/runtime sources; cleanup stopped")
            if any(a.get('state') != 'uploaded' or a['size'] <= 0 for a in packages):
                raise RuntimeError(f"Release {release['tag_name']} is incomplete; cleanup stopped")
        inventory.append((release['id'], release['tag_name'], sorted(
            (a['id'], a['name'], a['size'], a.get('digest')) for a in assets)))
    return sorted(inventory)


def cleanup(api, *, apply=False, all_completed=False, run_id=None,
            older_than_hours=24, clear_caches=False, current_run_id=None, now=None, log=print):
    """Snapshot pagination before mutations, then check every run again before deletion."""
    now = now or dt.datetime.now(dt.timezone.utc)
    before_releases = release_inventory(api)
    artifacts = api.pages('/actions/artifacts', 'artifacts')
    before_caches = api.pages('/actions/caches', 'actions_caches')
    summary = {'apply': apply, 'before_artifacts': len(artifacts),
               'before_artifact_bytes': sum(a['size_in_bytes'] for a in artifacts),
               'deleted_artifacts': 0, 'deleted_artifact_bytes': 0,
               'deleted_caches': 0, 'deleted_cache_bytes': 0, 'skipped': [],
               'releases_preserved': len(before_releases)}
    planned = []
    cutoff = now - dt.timedelta(hours=older_than_hours)
    for artifact in artifacts:
        origin = artifact.get('workflow_run', {}).get('id')
        if artifact['name'] not in ARTIFACT_NAMES or not origin:
            summary['skipped'].append({'id': artifact['id'], 'reason': 'unknown artifact'})
            continue
        if run_id is not None and origin != run_id:
            continue
        created = dt.datetime.fromisoformat(artifact['created_at'].replace('Z', '+00:00'))
        if not all_completed and run_id is None and created > cutoff:
            continue
        planned.append(artifact)
    summary['planned_artifacts'] = len(planned)
    summary['planned_artifact_bytes'] = sum(a['size_in_bytes'] for a in planned)
    for artifact in planned:
        origin = artifact['workflow_run']['id']
        run = api.request('GET', f'/actions/runs/{origin}')
        if run['status'] != 'completed':
            summary['skipped'].append({'id': artifact['id'], 'reason': 'run still active'})
            continue
        # Automatic immediate cleanup only follows successful delivery. Failed runs
        # remain available until the daily age-based cleanup (or an explicit full cleanup).
        if run_id is not None and run.get('conclusion') != 'success':
            summary['skipped'].append({'id': artifact['id'], 'reason': 'run not successful'})
            continue
        log(json.dumps({'action': 'delete' if apply else 'would_delete',
                        'artifact': artifact['id'], 'name': artifact['name'],
                        'bytes': artifact['size_in_bytes'], 'run': origin}))
        if apply:
            response = api.request('DELETE', f"/actions/artifacts/{artifact['id']}")
            if not response or not response.get('already_absent'):
                summary['deleted_artifacts'] += 1
                summary['deleted_artifact_bytes'] += artifact['size_in_bytes']
    if clear_caches:
        active = []
        for status in ('queued', 'in_progress', 'waiting', 'pending', 'requested'):
            active.extend(api.pages(f'/actions/runs?status={status}', 'workflow_runs'))
        active = [r for r in active if r.get("id") != current_run_id]
        if active:
            summary['skipped'].append({'reason': 'cache cleanup skipped while runs are active'})
        else:
            for cache in before_caches:
                log(json.dumps({'action': 'delete_cache' if apply else 'would_delete_cache',
                                'cache': cache['id'], 'bytes': cache['size_in_bytes']}))
                if apply:
                    response = api.request('DELETE', f"/actions/caches/{cache['id']}")
                    if not response or not response.get('already_absent'):
                        summary['deleted_caches'] += 1
                        summary['deleted_cache_bytes'] += cache['size_in_bytes']
    after_releases = release_inventory(api)
    # New releases may be published concurrently, but every pre-existing one must be intact.
    after_by_id = {r[0]: r for r in after_releases}
    if any(after_by_id.get(r[0]) != r for r in before_releases):
        raise RuntimeError('Release inventory changed during cleanup; inspect before continuing')
    remaining = api.pages('/actions/artifacts', 'artifacts')
    caches = api.pages('/actions/caches', 'actions_caches')
    summary['remaining_artifacts'] = len(remaining)
    summary['remaining_artifact_bytes'] = sum(a['size_in_bytes'] for a in remaining)
    summary['remaining_cache_bytes'] = sum(c['size_in_bytes'] for c in caches)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true', help='Actually delete; default is a dry run')
    parser.add_argument('--all-completed', action='store_true')
    parser.add_argument('--run-id', type=int)
    parser.add_argument('--older-than-hours', type=int, default=24)
    parser.add_argument('--clear-caches', action='store_true')
    args = parser.parse_args()
    if args.older_than_hours < 0 or (args.run_id is not None and args.run_id <= 0):
        parser.error('Invalid age/run ID')
    api = GitHub(os.environ.get('GITHUB_REPOSITORY', ''), os.environ.get('GH_TOKEN', ''))
    summary = cleanup(api, current_run_id=int(os.environ.get("GITHUB_RUN_ID", "0")), **vars(args))
    report = json.dumps(summary, ensure_ascii=False, indent=2)
    print('TRAIT64_STORAGE_RESULT=' + json.dumps(summary, ensure_ascii=False))
    if path := os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(path, 'a', encoding='utf-8') as output:
            output.write('# Trait64 Actions storage\n\n```json\n' + report + '\n```\n')


if __name__ == '__main__':
    main()
