#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Publish a complete, verified release without intermediate Actions artifacts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

VERSION = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-(beta|rc|dev)\.(0|[1-9][0-9]*))?")


def file_info(path):
    if path.is_symlink() or not path.is_file() or not 0 < path.stat().st_size < 2**31:
        raise ValueError(f"Invalid package: {path.name}")
    with path.open("rb") as source:
        digest = hashlib.file_digest(source, "sha256").hexdigest()
    return {"size": path.stat().st_size, "digest": "sha256:" + digest}


def package_manifest(directory, version):
    if not VERSION.fullmatch(version):
        raise ValueError("Invalid release version")
    names = {f"trait64_{version}_debian12_amd64.deb", f"trait64_{version}_debian13_amd64.deb",
             f"Trait64-{version}-linux-x86_64.AppImage", "Trait64-runtime-sources.tar",
             f"Trait64-{version}-windows-x86_64.zip", f"Trait64-{version}-macos-arm64.dmg"}
    actual = {p.name for p in directory.iterdir()} - {"SHA256SUMS"}
    if actual != names:
        raise ValueError(f"Incomplete package set: missing={names - actual}, unexpected={actual - names}")
    manifest = {name: file_info(directory / name) for name in sorted(names)}
    checksums = directory / "SHA256SUMS"
    if checksums.is_symlink():
        raise ValueError("SHA256SUMS must not be a symbolic link")
    checksums.write_text("".join(info["digest"][7:] + "  " + name + "\n"
                                 for name, info in manifest.items()), encoding="utf-8")
    manifest[checksums.name] = file_info(checksums)
    return manifest


def validate_draft(release, tag, commit, marker):
    if release is None:
        raise ValueError("The expected draft is not visible in the GitHub response")
    if (not release.get("draft") or release.get("tag_name") != tag
            or release.get("target_commitish") != commit or marker not in (release.get("body") or "")):
        raise ValueError("Refusing to modify a published release or an unrelated draft")


def verify_assets(assets, manifest):
    if len(assets) != len(manifest) or {a["name"] for a in assets} != set(manifest):
        raise ValueError("Remote asset set does not match the tested packages")
    for asset in assets:
        expected = manifest[asset["name"]]
        if (asset.get("state") != "uploaded" or asset.get("size") != expected["size"]
                or asset.get("digest") != expected["digest"]):
            raise ValueError(f"Remote checksum or size mismatch: {asset['name']}")


def publish(api, directory, version, commit, notes):
    manifest = package_manifest(directory, version)
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("An exact tested commit SHA is required")
    tag, prerelease = "v" + version, "-" in version
    marker = f"<!-- Trait64 tested commit: {commit} -->"
    api.validate_tag(tag, commit)
    release = api.find_release(tag)
    if release is None:
        release = api.create(tag, commit, prerelease, notes + "\n\n" + marker)
    validate_draft(release, tag, commit, marker)
    if any(a["name"] not in manifest for a in api.assets(release["id"])):
        raise ValueError("Refusing to replace a draft containing unrelated assets")
    api.upload(release, [directory / name for name in manifest])
    validate_draft(api.get_release(release["id"]), tag, commit, marker)
    verify_assets(api.assets(release["id"]), manifest)
    # Check the tag again immediately before exposing the release to updaters.
    api.validate_tag(tag, commit)
    published = api.publish(release["id"], prerelease)
    if (not published or published["id"] != release["id"] or published["tag_name"] != tag
            or published["draft"] or published["target_commitish"] != commit
            or published["prerelease"] != prerelease):
        raise ValueError("Published release metadata does not match the tested commit")
    verify_assets(api.assets(published["id"]), manifest)
    return published


class GitHub:
    def __init__(self, repository):
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
            raise ValueError("Invalid repository")
        self.repository = repository

    def api(self, path, pages=False, *, method="GET", payload=None):
        command = ["gh", "api", f"repos/{self.repository}/{path}"]
        if method != "GET":
            command += ["--method", method]
        if payload is not None:
            command += ["--input", "-"]
        if pages:
            command += ["--paginate", "--slurp"]
        value = json.loads(subprocess.check_output(command, text=True,
                           input=json.dumps(payload) if payload is not None else None))
        return [item for page in value for item in page] if pages else value

    def release_command(self, *args):
        subprocess.run(["gh", "release", *args, "--repo", self.repository], check=True)

    def validate_tag(self, tag, commit):
        refs = self.api("git/matching-refs/tags/" + tag)
        if any(r["ref"] == "refs/tags/" + tag for r in refs):
            if self.api("commits/" + tag)["sha"] != commit:
                raise ValueError("Existing tag points to another commit")

    def find_release(self, tag):
        matches = [r for r in self.api("releases?per_page=100", pages=True) if r["tag_name"] == tag]
        if len(matches) > 1:
            raise ValueError("Ambiguous release tag")
        return matches[0] if matches else None

    def create(self, tag, commit, prerelease, notes):
        # Use the creation response, not the release listing: the latter may
        # briefly omit a draft that GitHub has already created successfully.
        return self.api("releases", method="POST", payload={
            "tag_name": tag, "target_commitish": commit, "draft": True,
            "name": "Trait64 " + tag[1:], "prerelease": prerelease, "body": notes})

    def get_release(self, release_id):
        return self.api(f"releases/{release_id}")

    def assets(self, release_id):
        return self.api(f"releases/{release_id}/assets?per_page=100", pages=True)

    def upload(self, release, paths):
        # Only an owned, unpublished draft can reach this operation.
        # gh release upload resolves its tag through the release listing. Wait
        # for that index without retrying creation or weakening draft checks.
        tag = release["tag_name"]
        marker = f"<!-- Trait64 tested commit: {release['target_commitish']} -->"
        for delay in (0, 1, 2, 4, 8):
            if delay:
                time.sleep(delay)
            visible = self.find_release(tag)
            if visible is None:
                continue
            validate_draft(visible, tag, release["target_commitish"], marker)
            if visible["id"] != release["id"]:
                raise ValueError("Release ID changed before uploading")
            break
        else:
            raise ValueError("Draft is not indexed yet; retry this tested version later")
        self.release_command("upload", tag, *map(str, paths), "--clobber")

    def publish(self, release_id, prerelease):
        # The PATCH response describes this precise release even while the
        # listing still shows its earlier draft state.
        return self.api(f"releases/{release_id}", method="PATCH", payload={
            "draft": False, "prerelease": prerelease,
            "make_latest": str(not prerelease).lower()})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("version")
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    if args.check_only:
        print(json.dumps(package_manifest(args.directory, args.version), indent=2))
        return
    event, ref = os.environ.get("GITHUB_EVENT_NAME"), os.environ.get("GITHUB_REF")
    if event not in {"push", "workflow_dispatch"} or ref not in {"refs/heads/main", "refs/tags/v" + args.version}:
        raise ValueError("Publication requires main or the matching release tag")
    if event == "push" and ref == "refs/heads/main" and "-dev." not in args.version:
        raise ValueError("An ordinary main commit may only publish a development version")
    release = publish(GitHub(os.environ["GITHUB_REPOSITORY"]), args.directory, args.version,
                      os.environ["GITHUB_SHA"], "Trait64 " + args.version + "\n\n" +
                      Path("docs/RELEASE-NOTES.md").read_text(encoding="utf-8"))
    print("Published verified release:", release["html_url"])


if __name__ == "__main__":
    main()
