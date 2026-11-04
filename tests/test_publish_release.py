# SPDX-License-Identifier: AGPL-3.0-or-later
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/publish-release.py"
spec = importlib.util.spec_from_file_location("publisher", SCRIPT)
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)
COMMIT = "a" * 40


class FakeGitHub:
    """In-memory API test double; publication itself always uses the real gh CLI."""
    def __init__(self):
        self.release = None
        self.files = []
        self.uploads = 0
        self.corrupt = False
        self.tag_commit = None

    def validate_tag(self, tag, commit):
        if self.tag_commit and self.tag_commit != commit:
            raise ValueError("Existing tag points to another commit")

    def find_release(self, tag):
        return copy.deepcopy(self.release)

    def create(self, tag, commit, prerelease, notes):
        self.release = dict(id=1, tag_name=tag, target_commitish=commit, draft=True,
                            prerelease=prerelease, body=notes, html_url="https://example.invalid/release")
        return self.find_release(tag)

    def assets(self, release_id):
        return copy.deepcopy(self.files)

    def get_release(self, release_id):
        if self.release["id"] != release_id:
            raise ValueError("Unknown release")
        return copy.deepcopy(self.release)

    def upload(self, release, paths):
        self.uploads += 1
        self.files = [dict(name=p.name, state="uploaded", **publisher.file_info(p)) for p in paths]
        if self.corrupt:
            self.files[0]["digest"] = "sha256:" + "0" * 64

    def publish(self, release_id, prerelease):
        self.release.update(draft=False, prerelease=prerelease)
        return self.get_release(release_id)


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.version = "0.3.0-dev.999"
        self.api = FakeGitHub()
        self.make_packages()

    def make_packages(self):
        for name in [f"trait64_{self.version}_debian12_amd64.deb",
                     f"trait64_{self.version}_debian13_amd64.deb",
                     f"Trait64-{self.version}-linux-x86_64.AppImage", "Trait64-runtime-sources.tar",
                     f"Trait64-{self.version}-windows-x86_64.zip",
                     f"Trait64-{self.version}-macos-arm64.dmg"]:
            (self.directory / name).write_text("test package " + name)

    def publish(self):
        return publisher.publish(self.api, self.directory, self.version, COMMIT, "Test notes")

    def owned_draft(self):
        self.api.create("v" + self.version, COMMIT, True, f"<!-- Trait64 tested commit: {COMMIT} -->")

    def test_complete_development_release_has_verified_assets(self):
        result = self.publish()
        self.assertFalse(result["draft"])
        self.assertTrue(result["prerelease"])
        self.assertEqual(result["target_commitish"], COMMIT)
        # Comptés d'après l'ensemble attendu plutôt qu'en dur : ajouter une plateforme ne
        # doit pas rendre ce test faux, seulement l'étendre.
        expected = publisher.package_manifest(self.directory, self.version)
        self.assertEqual(len(self.api.files), len(expected))
        lines = (self.directory / "SHA256SUMS").read_text().splitlines()
        self.assertEqual(len(lines), len(expected) - 1)
        for line in lines:
            digest, name = line.split("  ", 1)
            self.assertEqual("sha256:" + digest, publisher.file_info(self.directory / name)["digest"])

    def test_missing_sources_cannot_create_a_draft(self):
        (self.directory / "Trait64-runtime-sources.tar").unlink()
        with self.assertRaises(ValueError):
            self.publish()
        self.assertIsNone(self.api.release)

    def test_unexpected_package_cannot_create_a_draft(self):
        (self.directory / "old-version.deb").write_text("stale")
        with self.assertRaises(ValueError):
            self.publish()
        self.assertIsNone(self.api.release)

    def test_empty_and_symlink_packages_rejected(self):
        path = self.directory / "Trait64-runtime-sources.tar"
        path.write_bytes(b"")
        with self.assertRaises(ValueError):
            self.publish()
        path.unlink()
        path.symlink_to(next(self.directory.glob("*.deb")))
        with self.assertRaises(ValueError):
            self.publish()
        self.assertIsNone(self.api.release)

    def test_corrupt_remote_asset_leaves_draft_and_can_resume(self):
        self.api.corrupt = True
        with self.assertRaises(ValueError):
            self.publish()
        self.assertTrue(self.api.release["draft"])
        self.api.corrupt = False
        self.assertFalse(self.publish()["draft"])
        self.assertEqual(self.api.uploads, 2)

    def test_published_release_never_overwritten(self):
        self.publish()
        before = copy.deepcopy(self.api.files)
        with self.assertRaises(ValueError):
            self.publish()
        self.assertEqual(self.api.files, before)
        self.assertEqual(self.api.uploads, 1)

    def test_draft_of_another_commit_preserved(self):
        self.owned_draft()
        self.api.release["target_commitish"] = "b" * 40
        before = copy.deepcopy(self.api.release)
        with self.assertRaises(ValueError):
            self.publish()
        self.assertEqual(self.api.release, before)
        self.assertEqual(self.api.uploads, 0)

    def test_manual_draft_preserved(self):
        self.owned_draft()
        self.api.release["body"] = "Manual draft"
        with self.assertRaises(ValueError):
            self.publish()
        self.assertEqual(self.api.uploads, 0)

    def test_existing_tag_must_match_tested_commit(self):
        self.api.tag_commit = "b" * 40
        with self.assertRaises(ValueError):
            self.publish()
        self.assertIsNone(self.api.release)

    def test_unrelated_draft_assets_preserved(self):
        self.owned_draft()
        self.api.files = [{"name": "manual.pdf"}]
        with self.assertRaises(ValueError):
            self.publish()
        self.assertEqual(self.api.files, [{"name": "manual.pdf"}])
        self.assertEqual(self.api.uploads, 0)

    def test_missing_duplicate_and_wrong_size_remote_assets_rejected(self):
        self.publish()
        manifest = publisher.package_manifest(self.directory, self.version)
        wrong_size = copy.deepcopy(self.api.files)
        wrong_size[0]["size"] += 1
        for assets in [self.api.files[:-1], self.api.files + self.api.files[:1], wrong_size]:
            with self.subTest(assets=assets), self.assertRaises(ValueError):
                publisher.verify_assets(assets, manifest)

    def test_cli_blocks_prs_unrelated_branches_and_stable_main_pushes(self):
        for event, ref, version in [("pull_request", "refs/pull/7/merge", self.version),
                                    ("push", "refs/heads/main", "0.3.0"),
                                    ("push", "refs/heads/main", "0.3.0-beta.1"),
                                    ("workflow_dispatch", "refs/heads/topic", self.version)]:
            result = subprocess.run([sys.executable, str(SCRIPT), str(self.directory), version],
                                    env={**os.environ, "GITHUB_EVENT_NAME": event, "GITHUB_REF": ref},
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("ValueError", result.stderr)
            self.assertNotIn("FileNotFoundError", result.stderr)

    def test_explicit_stable_release_has_stable_metadata(self):
        for path in self.directory.iterdir():
            path.unlink()
        self.version = "0.3.0"
        self.make_packages()
        self.assertFalse(self.publish()["prerelease"])

    def test_created_release_does_not_depend_on_a_fresh_listing(self):
        find = self.api.find_release
        with patch.object(self.api, "find_release", side_effect=[None]), \
                patch.object(self.api, "create", side_effect=lambda *args: self.create_without_list(*args)):
            result = self.publish()
        self.assertFalse(result["draft"])
        self.assertIsNotNone(find("v" + self.version))

    def create_without_list(self, tag, commit, prerelease, notes):
        self.api.release = dict(id=1, tag_name=tag, target_commitish=commit, draft=True,
                                prerelease=prerelease, body=notes)
        return self.api.get_release(1)

    def test_rest_create_and_publish_use_mutation_responses_and_exact_id(self):
        github = publisher.GitHub("owner/repo")
        notes = "Nouveautés\n\n`literal $() text`\n"
        with patch.object(publisher.subprocess, "check_output", return_value='{"id": 123}') as run:
            self.assertEqual(github.create("v" + self.version, COMMIT, True, notes), {"id": 123})
            command = run.call_args.args[0]
            self.assertEqual(command, ["gh", "api", "repos/owner/repo/releases", "--method", "POST", "--input", "-"])
            data = json.loads(run.call_args.kwargs["input"])
            self.assertEqual(data["body"], notes)
            self.assertTrue(data["draft"])
            self.assertEqual(data["target_commitish"], COMMIT)
            self.assertEqual(github.publish(123, True), {"id": 123})
            self.assertEqual(run.call_args.args[0][2], "repos/owner/repo/releases/123")
            self.assertEqual(json.loads(run.call_args.kwargs["input"]),
                             {"draft": False, "prerelease": True, "make_latest": "false"})

    def test_upload_waits_for_same_draft_without_creating_it_again(self):
        self.owned_draft()
        github = publisher.GitHub("owner/repo")
        with patch.object(github, "find_release", side_effect=[None, None, self.api.release]) as find, \
                patch.object(github, "release_command") as command, patch.object(publisher.time, "sleep") as sleep:
            github.upload(self.api.release, [self.directory / "SHA256SUMS"])
            self.assertEqual(find.call_count, 3)
            self.assertEqual([call.args[0] for call in sleep.call_args_list], [1, 2])
            self.assertEqual(command.call_count, 1)
            self.assertEqual(command.call_args.args[0], "upload")

    def test_invisible_or_changed_draft_never_uploaded(self):
        self.owned_draft()
        github = publisher.GitHub("owner/repo")
        other_id = dict(self.api.release, id=2)
        published = dict(self.api.release, draft=False)
        for response in (None, other_id, published):
            with self.subTest(response=response), \
                    patch.object(github, "find_release", return_value=response), \
                    patch.object(github, "release_command") as command, patch.object(publisher.time, "sleep"):
                with self.assertRaises(ValueError):
                    github.upload(self.api.release, [self.directory / "SHA256SUMS"])
                command.assert_not_called()


if __name__ == "__main__":
    unittest.main()
