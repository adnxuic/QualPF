#!/usr/bin/env python3
"""Check retained history or explicitly verify/restore an independent evidence ZIP."""
import argparse
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parent.parent.parent
CATALOG = "history/relocations.json"
SCHEMA = "qualpf.history_relocations.v2"


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON field: " + key)
        result[key] = value
    return result


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_object)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def safe_name(name):
    if (not isinstance(name, str) or not name or any(c in name for c in '\\:<>"|?*')
            or any(ord(c) < 32 for c in name)
            or PurePosixPath(name).is_absolute() or PurePosixPath(name).as_posix() != name):
        return False
    for part in name.split("/"):
        if (part in {"", ".", ".."} or part.endswith((".", " "))
                or re.fullmatch(r"(?i:con|prn|aux|nul|com[1-9]|lpt[1-9])", part.split(".")[0])):
            return False
    return True


def identity(value, length):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{%d}" % length, value) is not None


def byte_identity(row):
    return (isinstance(row, dict) and type(row.get("size")) is int and row["size"] >= 0
            and identity(row.get("sha256"), 64))


def validate_catalog(catalog):
    errors = []
    if (not isinstance(catalog, dict) or catalog.get("schema") != SCHEMA
            or not identity(catalog.get("source_commit"), 40)):
        return ["invalid history catalog schema/source identity"]
    archive = catalog.get("archive")
    if (not byte_identity(archive) or not safe_name(archive.get("filename"))
            or "/" in archive["filename"] or not archive["filename"].endswith(".zip")
            or not identity(archive.get("source_commit"), 40)
            or not isinstance(archive.get("location_hints"), list)
            or not all(isinstance(x, str) and x for x in archive["location_hints"])):
        errors.append("invalid independent archive identity")
    rows = catalog.get("files")
    extras = catalog.get("snapshot_files")
    if not isinstance(rows, list) or not rows or not isinstance(extras, list):
        return errors + ["missing archive member inventory"]
    old_names, names = set(), set()
    for row in rows:
        if (not byte_identity(row) or not safe_name(row.get("path"))
                or not row["path"].startswith("history/")
                or row["path"] in {CATALOG, "history/README.md"}
                or not safe_name(row.get("original_path")) or not row["original_path"].startswith("docs/")
                or not identity(row.get("git_blob"), 40)
                or row.get("storage") not in {"repository", "archive"}
                or (row.get("storage") == "repository" and not row.get("retention_reason"))):
            errors.append("invalid/unsafe history member: " + str(row))
            continue
        old, name = row["original_path"].casefold(), row["path"].casefold()
        if old in old_names or name in names:
            errors.append("duplicate historical relocation: " + row["path"])
        old_names.add(old)
        names.add(name)
    for row in extras:
        if not byte_identity(row) or row.get("path") not in {CATALOG, "history/README.md"}:
            errors.append("invalid snapshot metadata member")
            continue
        if row["path"].casefold() in names:
            errors.append("duplicate snapshot metadata member")
        names.add(row["path"].casefold())
    if {r.get("path") for r in extras if isinstance(r, dict)} != {CATALOG, "history/README.md"}:
        errors.append("original snapshot metadata must be preserved")
    inventory = catalog.get("recovery_inventory")
    if (not isinstance(inventory, dict) or not identity(inventory.get("entry_commit"), 40)
            or not isinstance(inventory.get("protection_tag"), str) or not inventory["protection_tag"]
            or not any(isinstance(r, dict) and r.get("path") == inventory.get("source_path")
                       and r.get("storage") == "archive" for r in rows)):
        errors.append("invalid compact recovery inventory binding")
    return errors


def check_local(root, catalog):
    root = root.resolve()
    errors = validate_catalog(catalog)
    if errors:
        return errors
    retained = {r["path"]: r for r in catalog["files"] if r["storage"] == "repository"}
    for name, row in retained.items():
        path = root / name
        if not path.resolve().is_relative_to(root) or not path.is_file():
            errors.append("missing/unsafe retained history: " + name)
        else:
            data = path.read_bytes()
            if len(data) != row["size"] or digest(data) != row["sha256"]:
                errors.append("changed retained history bytes: " + name)
    allowed = set(retained) | {CATALOG, "history/README.md"}
    for path in (root / "history").rglob("*"):
        if path.is_file() and path.relative_to(root).as_posix() not in allowed:
            errors.append("unindexed or archived-only file in history: " + path.relative_to(root).as_posix())
    return errors


def verify_archive(catalog, archive_path):
    errors = validate_catalog(catalog)
    if errors:
        raise ValueError("; ".join(errors))
    archive = catalog["archive"]
    expected = {r["path"]: r for r in catalog["files"] + catalog["snapshot_files"]}
    # One file handle binds the complete archive hash and its member reads.
    with archive_path.open("rb") as stream:
        h, size = hashlib.sha256(), 0
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
            size += len(chunk)
        if size != archive["size"] or h.hexdigest() != archive["sha256"]:
            raise ValueError("independent archive size/SHA-256 mismatch")
        stream.seek(0)
        with zipfile.ZipFile(stream) as z:
            infos = z.infolist()
            names = [i.filename for i in infos]
            if (len(names) != len(set(n.casefold() for n in names)) or set(names) != set(expected)
                    or any(not safe_name(n) for n in names)):
                raise ValueError("unsafe, duplicate, missing or unexpected archive member")
            payloads = {}
            for info in infos:
                row = expected[info.filename]
                kind = stat.S_IFMT(info.external_attr >> 16)
                if info.is_dir() or kind not in (0, stat.S_IFREG) or info.flag_bits & 1:
                    raise ValueError("non-regular or encrypted archive member: " + info.filename)
                if info.file_size != row["size"]:
                    raise ValueError("archive member length mismatch: " + info.filename)
                data = z.read(info)
                if len(data) != row["size"] or digest(data) != row["sha256"]:
                    raise ValueError("archive member SHA-256 mismatch: " + info.filename)
                payloads[info.filename] = data
    summary = catalog["recovery_inventory"]
    original = json.loads(payloads[summary["source_path"]], object_pairs_hook=unique_object)
    if any(original.get(k) != summary[k] for k in ("entry_commit", "protection_tag")):
        raise ValueError("compact recovery inventory disagrees with archived original")
    return payloads


def restore(catalog, archive_path, destination, root=ROOT, member=None):
    # Validate all input before creating any destination, even for one-member recovery.
    payloads = verify_archive(catalog, archive_path)
    if member is not None:
        if member not in payloads:
            raise ValueError("requested member is not indexed: " + member)
        payloads = {member: payloads[member]}
    destination = destination.absolute()
    resolved = destination.resolve()
    root = root.resolve()
    if os.path.lexists(destination) or root.is_relative_to(resolved) or resolved.is_relative_to(root / "history"):
        raise ValueError("restore requires a new destination outside the published history tree")
    if not destination.parent.is_dir():
        raise ValueError("restore destination parent must already exist")
    staging = Path(tempfile.mkdtemp(prefix=".history-restore-", dir=destination.parent))
    try:
        for name, data in payloads.items():
            path = staging / name
            if not path.resolve().is_relative_to(staging.resolve()):
                raise ValueError("restore path escapes staging directory")
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open("xb") as stream:
                stream.write(data)
            if path.read_bytes() != data:
                raise ValueError("restored bytes differ: " + name)
        if os.path.lexists(destination):
            raise ValueError("restore destination appeared during verification")
        staging.rename(destination)
    finally:
        if staging.exists():
            # This private directory was created here, never supplied by a user.
            if (staging.is_symlink() or staging.resolve().parent != destination.parent.resolve()
                    or not staging.name.startswith(".history-restore-")):
                raise ValueError("unsafe restore staging cleanup path")
            shutil.rmtree(staging)
    return len(payloads)


def self_test():
    class Controls(unittest.TestCase):
        def setUp(self):
            self.temp = tempfile.TemporaryDirectory()
            self.addCleanup(self.temp.cleanup)
            self.root = Path(self.temp.name)
            self.archive = self.root / "external.zip"
            self.inventory = dict(entry_commit="a" * 40, protection_tag="original-tag")
            self.data = {"history/acceptance.json": b'{"outcome":"PASS"}\r\n',
                         "history/inventory.json": json.dumps(self.inventory).encode(),
                         CATALOG: b'{"schema":"original"}\r\n', "history/README.md": b"original report\r\n"}
            rows = [dict(path=p, original_path=p.replace("history/", "docs/", 1), size=len(b), sha256=digest(b),
                         git_blob="b" * 40, storage="repository" if "acceptance" in p else "archive",
                         **({"retention_reason": "acceptance"} if "acceptance" in p else {}))
                    for p, b in self.data.items() if p not in {CATALOG, "history/README.md"}]
            self.catalog = dict(schema=SCHEMA, source_commit="b" * 40, files=rows,
                                snapshot_files=[dict(path=p, size=len(self.data[p]), sha256=digest(self.data[p])) for p in (CATALOG, "history/README.md")],
                                recovery_inventory=dict(source_path="history/inventory.json", **self.inventory))
            self.write_archive()
            path = self.root / "history/acceptance.json"
            path.parent.mkdir()
            path.write_bytes(self.data["history/acceptance.json"])

        def write_archive(self, data=None):
            with zipfile.ZipFile(self.archive, "w") as z:
                for name, value in (self.data if data is None else data).items():
                    z.writestr(name, value)
            b = self.archive.read_bytes()
            self.catalog["archive"] = dict(filename="external.zip", size=len(b), sha256=digest(b), source_commit="c" * 40, location_hints=[])

        def test_offline_checkout_and_missing_retained(self):
            self.archive.unlink()
            self.assertEqual(check_local(self.root, self.catalog), [])
            (self.root / "history/acceptance.json").unlink()
            self.assertTrue(check_local(self.root, self.catalog))

        def test_full_and_selected_restore(self):
            self.assertEqual(verify_archive(self.catalog, self.archive), self.data)
            destination = self.root / "restored"
            self.assertEqual(restore(self.catalog, self.archive, destination, self.root), 4)
            for name, value in self.data.items():
                self.assertEqual((destination / name).read_bytes(), value)
            with self.assertRaises(ValueError):
                restore(self.catalog, self.archive, destination, self.root)
            selected = self.root / "selected"
            self.assertEqual(restore(self.catalog, self.archive, selected, self.root, "history/acceptance.json"), 1)
            self.assertFalse((selected / CATALOG).exists())

        def test_corruption_and_no_partial_restore(self):
            self.archive.write_bytes(self.archive.read_bytes() + b"corruption")
            destination = self.root / "missing"
            with self.assertRaisesRegex(ValueError, "archive size/SHA"):
                restore(self.catalog, self.archive, destination, self.root)
            self.assertFalse(destination.exists())
            changed = dict(self.data, **{"history/acceptance.json": b'changed'})
            self.write_archive(changed)
            with self.assertRaises(ValueError):
                restore(self.catalog, self.archive, destination, self.root)
            self.assertFalse(destination.exists())
            changed["history/acceptance.json"] = b"x" * len(self.data["history/acceptance.json"])
            self.write_archive(changed)
            with self.assertRaisesRegex(ValueError, "member SHA-256"):
                restore(self.catalog, self.archive, destination, self.root)
            self.assertFalse(destination.exists())
            self.write_archive()
            self.catalog["recovery_inventory"]["entry_commit"] = "d" * 40
            with self.assertRaisesRegex(ValueError, "compact recovery"):
                verify_archive(self.catalog, self.archive)

        def test_unsafe_duplicate_missing_and_symlink(self):
            for name in ("../outside", "/absolute", "C:/outside", "history/../outside", "history\\file", "history/NUL", "history/a:", "history/a."):
                self.assertFalse(safe_name(name), name)
            changed = deepcopy(self.catalog)
            changed["files"].append(changed["files"][0].copy())
            self.assertTrue(validate_catalog(changed))
            self.write_archive()
            with zipfile.ZipFile(self.archive, "a") as z:
                with self.assertWarns(UserWarning):
                    z.writestr("history/acceptance.json", self.data["history/acceptance.json"])
            b = self.archive.read_bytes()
            self.catalog["archive"].update(size=len(b), sha256=digest(b))
            with self.assertRaisesRegex(ValueError, "duplicate"):
                verify_archive(self.catalog, self.archive)
            for data in ({k: v for k, v in self.data.items() if k != CATALOG}, dict(self.data, **{"../outside": b"x"})):
                self.write_archive(data)
                with self.assertRaisesRegex(ValueError, "archive member"):
                    verify_archive(self.catalog, self.archive)
            self.write_archive()
            with zipfile.ZipFile(self.archive, "a") as z:
                info = zipfile.ZipInfo("history/link")
                info.create_system = 3
                info.external_attr = (stat.S_IFLNK | 0o777) << 16
                z.writestr(info, "../../outside")
            b = self.archive.read_bytes()
            self.catalog["archive"].update(size=len(b), sha256=digest(b))
            self.catalog["files"].append(dict(path="history/link",original_path="docs/link",size=13,sha256=digest(b"../../outside"),git_blob="a"*40,storage="archive"))
            with self.assertRaisesRegex(ValueError, "non-regular"):
                verify_archive(self.catalog, self.archive)

    return unittest.TextTestRunner(verbosity=0).run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls)).wasSuccessful()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--catalog", type=Path, help="Explicit catalog; default: <root>/history/relocations.json")
    parser.add_argument("--archive", type=Path, help="Explicit ZIP to verify; location hints are never auto-executed")
    parser.add_argument("--restore-to", type=Path, help="New output directory; requires --archive")
    parser.add_argument("--member", help="Restore just this indexed archive member")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.restore_to and not args.archive or args.member and not args.restore_to:
        parser.error("--restore-to requires --archive; --member requires --restore-to")
    if args.self_test and not self_test():
        return 1
    try:
        catalog = read_json(args.catalog or args.root / CATALOG)
        if args.restore_to:
            count = restore(catalog, args.archive, args.restore_to, args.root, args.member)
            print(f"PASS: restored and byte-verified {count} historical members to {args.restore_to}")
        elif args.archive:
            count = len(verify_archive(catalog, args.archive))
            print(f"PASS: independent archive and all {count} member identities")
        else:
            errors = check_local(args.root, catalog)
            if errors:
                raise ValueError("; ".join(errors))
            print("PASS: retained acceptance and archive index; external payload availability not checked")
    except (OSError, ValueError, TypeError, KeyError, zipfile.BadZipFile) as error:
        print("FAIL:", error)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
