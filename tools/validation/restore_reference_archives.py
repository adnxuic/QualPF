#!/usr/bin/env python3
"""Restore sealed reference ZIPs from an existing Git commit, without network I/O."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def destination(root, name):
    path = PurePosixPath(name)
    if (not name.startswith("tests/") or not name.endswith(".zip") or
            path.is_absolute() or ".." in path.parts or "\\" in name or ":" in name):
        raise ValueError("Unsafe reference archive path: " + name)
    result = root / path
    if not result.resolve().is_relative_to(root.resolve()):
        raise ValueError("Reference archive escapes checkout: " + name)
    return result


def archive_entries(root):
    manifest = json.loads((root / "tests/reference/archives.json").read_bytes())
    if (manifest.get("schema") != "qualpf.reference_archives.v1" or
            manifest.get("expected_from_product") is not False or
            not re.fullmatch(r"[0-9a-f]{40}", manifest.get("source_commit", ""))):
        raise ValueError("Invalid independent archive manifest")
    seen = set()
    for entry in manifest["files"]:
        destination(root, entry["path"])
        if (entry["path"] in seen or type(entry["bytes"]) is not int or
                entry["bytes"] <= 0 or
                not re.fullmatch(r"[0-9a-f]{64}", entry["sha256"])):
            raise ValueError("Invalid or duplicate archive entry")
        seen.add(entry["path"])
    if not seen:
        raise ValueError("Empty archive manifest")
    # Preserve the original normal-reference authority where the sets overlap.
    normal = json.loads((root / "tests/reference/manifest.json").read_bytes())
    indexed = {entry["path"]: entry for entry in manifest["files"]}
    for entry in normal["files"]:
        if entry["path"].endswith(".zip"):
            other = indexed.get(entry["path"], {})
            if any(other.get(key) != entry[key] for key in ("bytes", "sha256")):
                raise ValueError("Archive disagrees with frozen normal reference")
    return manifest


def checked(data, entry):
    if len(data) != entry["bytes"] or hashlib.sha256(data).hexdigest() != entry["sha256"]:
        raise ValueError("Frozen archive changed: " + entry["path"])
    return data


def restore(root, manifest, read_blob=None, verify_only=False):
    pending = []
    for entry in manifest["files"]:
        path = destination(root, entry["path"])
        if path.exists():
            checked(path.read_bytes(), entry)
        elif verify_only:
            raise ValueError("Missing reference archive: " + entry["path"] +
                             "; run python -B tools/validation/restore_reference_archives.py")
        else:
            pending.append((path, checked(read_blob(entry["path"]), entry)))
    # Check every existing/source byte before writing any missing file.
    for path, data in pending:
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
                temporary = Path(stream.name)
                stream.write(data)
            os.replace(temporary, path)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
    return len(pending)


class RestoreTests(unittest.TestCase):
    def test_restore_and_idempotence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = b"independent test input\x00\r\n"
            entry = dict(path="tests/data/example.zip", bytes=len(data),
                         sha256=hashlib.sha256(data).hexdigest())
            manifest = dict(files=[entry])
            self.assertEqual(restore(root, manifest, lambda _: data), 1)
            self.assertEqual((root / entry["path"]).read_bytes(), data)
            self.assertEqual(restore(root, manifest, verify_only=True), 0)
            self.assertEqual(restore(root, manifest), 0)

    def test_missing_corrupt_and_no_partial_write(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            entry = dict(path="tests/data/a.zip", bytes=1,
                         sha256=hashlib.sha256(b"a").hexdigest())
            other = dict(entry, path="tests/data/b.zip")
            manifest = dict(files=[entry, other])
            with self.assertRaisesRegex(ValueError, "Missing reference"):
                restore(root, manifest, verify_only=True)
            with self.assertRaisesRegex(ValueError, "Frozen archive changed"):
                restore(root, manifest, lambda name: b"a" if name.endswith("a.zip") else b"b")
            self.assertFalse((root / entry["path"]).exists())
            target = root / entry["path"]
            target.parent.mkdir(parents=True)
            target.write_bytes(b"b")
            with self.assertRaisesRegex(ValueError, "Frozen archive changed"):
                restore(root, manifest, lambda _: b"a")
            self.assertEqual(target.read_bytes(), b"b")

    def test_safe_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ("../tests/a.zip", "tests/../../a.zip", "/tests/a.zip",
                         "tests\\a.zip", "tests/a.zip:stream", "src/a.zip", "tests/a.cpp"):
                with self.assertRaises(ValueError, msg=name):
                    destination(root, name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", help="Existing local protection tag or commit; default: pinned main ancestor")
    parser.add_argument("--verify-only", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(RestoreTests)
        return not unittest.TextTestRunner().run(suite).wasSuccessful()
    manifest = archive_entries(ROOT)
    commit = None

    def read_blob(name):
        nonlocal commit
        if commit is None:
            source = args.ref or manifest["source_commit"]
            try:
                commit = subprocess.check_output(
                    ["git", "rev-parse", "--verify", "--end-of-options", source + "^{commit}"],
                    cwd=ROOT, stderr=subprocess.PIPE, text=True).strip()
            except subprocess.CalledProcessError as error:
                raise ValueError("Reference source is unavailable locally: " + source +
                                 "; see tests/README.md#independent-references") from error
        return subprocess.check_output(["git", "cat-file", "blob", commit + ":" + name], cwd=ROOT)

    count = restore(ROOT, manifest, read_blob, args.verify_only)
    print(f"PASS: {len(manifest['files'])} sealed reference archives verified; {count} restored")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
