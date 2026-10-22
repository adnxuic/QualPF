#!/usr/bin/env python3
"""Prepare/verify local dependency sources and notices without network access.

This is evidence collection, not a declaration that vendor source gaps are closed.
No archive code is executed, no scientific input is rewritten, and no upload occurs.
"""
import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import shutil
import tarfile
import tempfile
import unittest

from check_distribution import ROOT, digest, read_json, relative, write_json

CATALOGUE = "deploy/dependency-sources.json"
INDEX = "dependency-bundle.json"
LEGAL = re.compile(r"^(?:licen[cs]e[^/]*|copying[^/]*|copyright[^/]*|authors|qt_attribution\.json)$", re.I)


def archive_name(name):
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or "\\" in name or re.match(r"^[A-Za-z]:", name):
        raise ValueError("unsafe dependency archive member: " + name)
    return path


def validate_sources(cache, catalogue):
    if catalogue.get("schema") != "qualpf.dependency_sources.v1":
        raise ValueError("unsupported dependency source catalogue")
    seen = set()
    for entry in catalogue["sources"]:
        if entry["id"] in seen or Path(entry["file"]).name != entry["file"]:
            raise ValueError("duplicate/unsafe dependency source")
        seen.add(entry["id"])
        source = relative(cache, entry["file"])
        if not source.is_file() or digest(source) != entry["sha256"]:
            raise ValueError("missing/modified dependency source: " + entry["file"])
        if entry.get("bytes") != source.stat().st_size:
            raise ValueError("dependency source size differs: " + entry["file"])


def copy_legal(source, destination):
    """Keep full upstream terms and attribution-referenced files, including code headers."""
    gaps = []
    with tarfile.open(source) as archive:
        members = {member.name: member for member in archive.getmembers()}
        for name in members:
            archive_name(name)
        selected = {name for name, member in members.items() if member.isfile()
                    and LEGAL.fullmatch(PurePosixPath(name).name)}
        for name in sorted(selected):
            if not name.endswith("/qt_attribution.json"):
                continue
            # Qt's original attribution metadata contains literal newlines/tabs
            # in strings. Preserve its bytes and accept those strings.
            value = json.loads(archive.extractfile(members[name]).read(), strict=False)
            entries = value if isinstance(value, list) else [value]
            for entry in entries:
                names = entry.get("LicenseFile", entry.get("LicenseFiles", []))
                names = [names] if isinstance(names, str) else names
                for license_name in names:
                    candidate = posixpath.normpath(posixpath.join(str(PurePosixPath(name).parent), license_name))
                    archive_name(candidate)
                    if candidate.split("/", 1)[0] != name.split("/", 1)[0]:
                        raise ValueError("attribution license escapes source root")
                    if candidate not in members or not members[candidate].isfile():
                        gaps.append(name + ": unresolved LicenseFile " + license_name)
                    else:
                        selected.add(candidate)
        for name in sorted(selected, key=lambda value: members[value].offset_data):
            member = members[name]
            if member.size > 8 * 1024 * 1024:
                raise ValueError("unexpectedly large legal material: " + name)
            # Drop the archive's single top-level directory; never extract links.
            suffix = PurePosixPath(name).parts[1:]
            if not suffix:
                raise ValueError("dependency archive requires a source root directory")
            target = relative(destination, PurePosixPath(*suffix).as_posix())
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(archive.extractfile(member).read())
    return len(selected), gaps


def qt_metadata(sdk, destination):
    sboms = sorted((sdk / "sbom").glob("*.spdx"))
    if not sboms:
        raise ValueError("selected Qt SDK has no original SBOM: " + str(sdk))
    for source in sboms:
        target = destination / source.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    # Preserve vendor configure arguments (SBOM) and installed feature definitions.
    for pattern in ("mkspecs/qconfig.pri", "mkspecs/qmodule.pri", "include/Qt*/qt*-config.h",
                    "include/QtCore/qconfig.h"):
        for source in sorted(sdk.glob(pattern)):
            target = destination / source.relative_to(sdk)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)


def runtime_evidence(inventory, sdks, mingw):
    data = read_json(inventory)
    if data.get("schema") != "qualpf.distribution_inventory.v1":
        raise ValueError("unsupported actual runtime inventory")
    stage = next((parent for parent in inventory.parents
                  if all(relative(parent, row["path"]).is_file() for row in data["files"])), None)
    if stage is None:
        raise ValueError("actual staged payload is unavailable")
    compiler = data["build"]["compiler"].split()[0]
    sdk = sdks.get(compiler)
    rows = []
    for row in data["files"]:
        path = relative(stage, row["path"])
        if digest(path) != row["sha256"]:
            raise ValueError("staged payload changed since inventory: " + row["path"])
        if path.suffix.lower() not in {".exe", ".dll"} or path.name.lower() == "qualpf.exe":
            continue
        suffix = row["path"].split("/data/", 1)[-1]
        if row["component"] == "qt" and sdk:
            origin = relative(sdk, suffix)
            identified = origin.is_file() and digest(origin) == row["sha256"]
            status = "sdk-byte-match" if identified else "UNRESOLVED-sdk-byte-mismatch"
            source = "qtbase-" + data["build"]["qt"]
            if path.name.lower() in {"qt6svg.dll", "qsvg.dll", "qsvgicon.dll"}:
                source = "qtsvg-" + data["build"]["qt"]
            gap = "Bind the matching module source, embedded notices and original SDK configuration in the final package review."
        elif path.name.lower().startswith(("libgcc", "libgomp", "libstdc++", "libwinpthread")) and mingw:
            origin = mingw / "bin" / path.name
            identified = origin.is_file() and digest(origin) == row["sha256"]
            status = "sdk-byte-match" if identified else "UNRESOLVED-sdk-byte-mismatch"
            source = "mingw-w64-v11" if "winpthread" in path.name else "gcc-13.1.0"
            gap = ("MIT/BSD notices identified; preserve both notices in the final payload."
                   if "winpthread" in path.name else
                   "SDK lists ten patches; the acquired release-date build-script snapshot is a candidate, not vendor attestation.")
        else:
            identified, source = False, "unresolved"
            status = "UNRESOLVED-runtime-terms/source"
            gap = "Identify the exact runtime's own redistribution terms and source obligations; Qt deployment is not its license."
        rows.append(dict(path=row["path"], sha256=row["sha256"], component=row["component"],
                         origin_status=status, source_candidate=source, remaining_obligation=gap))
    return dict(schema="qualpf.runtime_source_evidence.v1", release_clearance=False,
                inventory_sha256=digest(inventory), build=data["build"], files=rows)


def header_paths(folder):
    if folder.is_file():
        yield folder
    else:
        for parent, directories, names in os.walk(folder):
            directories.sort()
            for name in sorted(names):
                yield Path(parent) / name


def header_sources(output, name, version, sources):
    """Keep the actual SDK headers as preferred modification source, with notices."""
    target = output / "SOURCES" / f"{name}-{version}-sdk-headers.tar.gz"
    files = {}
    with target.open("wb") as raw, gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode="w|") as archive:
            for folder, prefix in sources:
                if not folder.exists():
                    raise ValueError("missing SDK header/notice source: " + str(folder))
                # os.walk uses directory-entry metadata on Windows; avoid a
                # separate stat for every header while enumerating large SDKs.
                paths = header_paths(folder)
                for path in paths:
                    if path.is_symlink():
                        raise ValueError("SDK header symlink requires explicit source review: " + str(path))
                    name_in_archive = prefix if folder.is_file() else prefix + "/" + path.relative_to(folder).as_posix()
                    archive_name(name_in_archive)
                    info = tarfile.TarInfo(name_in_archive)
                    info.size, info.mode, info.mtime = path.stat().st_size, 0o644, 0
                    with path.open("rb") as stream:
                        archive.addfile(info, stream)
                    files[name_in_archive] = digest(path)
    write_json(output / "SDK-CONFIG" / f"{name}-source-inputs.json", dict(
        version=version, archive=target.name, files=files,
        scope="Exact installed SDK header source and original notices; not an assertion of upstream byte identity."))
    return dict(id=name, version=version, path=target.relative_to(output).as_posix(), sha256=digest(target), files=len(files))


def prepare(root, cache, output, sdks, mingw, inventories, eigen=None, boost=None):
    catalogue = read_json(root / CATALOGUE)
    validate_sources(cache, catalogue)
    if output.exists():
        raise ValueError("dependency bundle destination already exists; preserve it and choose a new directory")
    if not output.resolve().is_relative_to((root / "build").resolve()):
        raise ValueError("create dependency bundles only below this workspace's build directory")
    output.mkdir(parents=True)
    sources, gaps = [], []
    for entry in catalogue["sources"]:
        source = cache / entry["file"]
        target = output / "SOURCES" / source.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        count, missing = copy_legal(source, output / "NOTICES" / entry["id"])
        gaps.extend(missing)
        sources.append(dict(entry, path=target.relative_to(output).as_posix(), legal_files=count))
        print("Prepared", entry["id"], "with", count, "upstream legal/attribution files", flush=True)
    for label, sdk in sdks.items():
        qt_metadata(sdk, output / "SDK-CONFIG" / label)
    if mingw:
        shutil.copytree(mingw / "licenses", output / "NOTICES/MinGW-SDK")
        (output / "SDK-CONFIG").mkdir(exist_ok=True)
        shutil.copyfile(mingw / "build-info.txt", output / "SDK-CONFIG/mingw-build-info.txt")
    headers = []
    if eigen:
        macros = (eigen / "include/eigen3/Eigen/src/Core/util/Macros.h").read_text()
        version = ".".join(re.search(r"#define EIGEN_" + part + r"_VERSION\s+(\d+)", macros)[1]
                           for part in ("WORLD", "MAJOR", "MINOR"))
        shutil.copytree(eigen / "share/licenses/Eigen", output / "NOTICES/Eigen-SDK")
        headers.append(header_sources(output, "eigen", version, [
            (eigen / "include/eigen3", "include/eigen3"),
            (eigen / "share/licenses/Eigen", "LICENSES")]))
    if boost:
        number = int(re.search(r'#define BOOST_VERSION\s+(\d+)', (boost / "boost/version.hpp").read_text())[1])
        version = f"{number // 100000}.{number // 100 % 1000}.{number % 100}"
        headers.append(header_sources(output, "boost", version, [
            (boost / "boost", "include/boost"), (root / "LICENSES/BSL-1.0.txt", "LICENSE_1_0.txt")]))
    for index, inventory in enumerate(inventories):
        write_json(output / "RUNTIME-EVIDENCE" / f"payload-{index + 1}.json", runtime_evidence(inventory, sdks, mingw))
    files = {path.relative_to(output).as_posix(): digest(path)
             for path in sorted(output.rglob("*")) if path.is_file()}
    write_json(output / INDEX, dict(schema="qualpf.dependency_bundle.v1", release_clearance=False,
               source_catalogue_sha256=digest(root / CATALOGUE), sources=sources,
               header_sources=headers, attribution_gaps=gaps, files=files))
    return gaps


def verify(root, bundle):
    info = read_json(bundle / INDEX)
    if info.get("schema") != "qualpf.dependency_bundle.v1" or info.get("release_clearance") is not False:
        raise ValueError("dependency evidence cannot declare release clearance")
    if info.get("source_catalogue_sha256") != digest(root / CATALOGUE):
        raise ValueError("dependency source catalogue changed after bundle preparation")
    actual = {path.relative_to(bundle).as_posix() for path in bundle.rglob("*") if path.is_file()}
    if actual != set(info["files"]) | {INDEX}:
        raise ValueError("dependency bundle file list differs")
    for name, expected in info["files"].items():
        if digest(relative(bundle, name)) != expected:
            raise ValueError("dependency source/notice changed: " + name)
    # Bind archives independently to the committed catalogue, not merely to an editable index.
    for entry in read_json(root / CATALOGUE)["sources"]:
        if digest(relative(bundle, "SOURCES/" + entry["file"])) != entry["sha256"]:
            raise ValueError("bundled archive differs from source catalogue: " + entry["file"])
    return info


def compare_source_sbom(source, sbom):
    expected = {}
    for block in sbom.read_text(encoding="utf-8").split("FileName: ")[1:]:
        name = block.splitlines()[0].replace("\\", "/").removeprefix("./")
        archive_name(name)
        checksum = re.search(r"FileChecksum: SHA1: ([a-f0-9]{40})", block)
        if checksum:
            expected[name] = checksum[1]
    if not expected:
        raise ValueError("source SBOM has no file checksums")
    raw_matches, line_ending_only, seen, mismatches = 0, 0, set(), []
    with tarfile.open(source, mode="r|*") as archive:
        for member in archive:
            archive_name(member.name)
            name = member.name.split("/", 1)[-1]
            if not member.isfile() or name not in expected:
                continue
            raw = archive.extractfile(member).read()
            seen.add(name)
            if hashlib.sha1(raw).hexdigest() == expected[name]:
                raw_matches += 1
            elif b"\0" not in raw and expected[name] in {
                    hashlib.sha1(raw.replace(b"\r\n", b"\n")).hexdigest(),
                    hashlib.sha1(raw.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")).hexdigest()}:
                line_ending_only += 1
            else:
                mismatches.append(name)
    missing = sorted(set(expected) - seen)
    bookkeeping = {".tag", ".gitignore", ".gitattributes"}
    return dict(schema="qualpf.qt_source_comparison.v1", release_clearance=False,
                source_sha256=digest(source), sbom_sha256=digest(sbom),
                source_files=len(expected), raw_matches=raw_matches, line_ending_only=line_ending_only,
                mismatches=mismatches, missing=missing,
                non_bookkeeping_differences=[name for name in mismatches + missing
                                            if PurePosixPath(name).name not in bookkeeping])


def self_test():
    class Controls(unittest.TestCase):
        def test_archive_escape(self):
            for name in ("../secret", "/secret", "C:/secret", "root/../../secret", "root\\secret"):
                with self.assertRaises(ValueError):
                    archive_name(name)

        def test_source_and_bundle_tampering(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / "source.tar.gz"
                source.write_bytes(b"independent source")
                entry = dict(id="source", file=source.name, sha256=digest(source), bytes=source.stat().st_size)
                catalogue = dict(schema="qualpf.dependency_sources.v1", sources=[entry])
                validate_sources(root, catalogue)
                source.write_bytes(b"modified source")
                with self.assertRaisesRegex(ValueError, "modified"):
                    validate_sources(root, catalogue)
                (root / "deploy").mkdir()
                write_json(root / CATALOGUE, catalogue)
                bundle = root / "bundle"
                (bundle / "SOURCES").mkdir(parents=True)
                source.write_bytes(b"independent source")
                shutil.copyfile(source, bundle / "SOURCES" / source.name)
                info = dict(schema="qualpf.dependency_bundle.v1", release_clearance=False,
                            source_catalogue_sha256=digest(root / CATALOGUE), files={"SOURCES/" + source.name: digest(source)})
                write_json(bundle / INDEX, info)
                verify(root, bundle)
                (bundle / "extra.dll").write_bytes(b"unlisted runtime")
                with self.assertRaisesRegex(ValueError, "file list"):
                    verify(root, bundle)
                (bundle / "extra.dll").unlink()
                (bundle / "SOURCES" / source.name).write_bytes(b"modified source")
                with self.assertRaisesRegex(ValueError, "changed"):
                    verify(root, bundle)
                info["release_clearance"] = True
                write_json(bundle / INDEX, info)
                with self.assertRaisesRegex(ValueError, "clearance"):
                    verify(root, bundle)

        def test_attribution_header_and_original_whitespace(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / "source.tar.gz"
                raw = b'{"Copyright":"Author\tName", "LicenseFile":"notice.cpp"}'
                notice = b"/* Original copyright and full permission. */\n"
                with tarfile.open(source, "w:gz") as archive:
                    for name, value in (("source/thirdparty/qt_attribution.json", raw),
                                        ("source/thirdparty/notice.cpp", notice)):
                        entry = tarfile.TarInfo(name)
                        entry.size = len(value)
                        archive.addfile(entry, io.BytesIO(value))
                count, gaps = copy_legal(source, root / "notices")
                self.assertEqual((count, gaps), (2, []))
                self.assertEqual((root / "notices/thirdparty/qt_attribution.json").read_bytes(), raw)
                self.assertEqual((root / "notices/thirdparty/notice.cpp").read_bytes(), notice)

        def test_staged_runtime_identity(self):
            with tempfile.TemporaryDirectory() as directory:
                stage = Path(directory)
                runtime = stage / "bin/Qt6Core.dll"
                runtime.parent.mkdir()
                runtime.write_bytes(b"matching runtime")
                inventory = stage / "inventory.json"
                write_json(inventory, dict(schema="qualpf.distribution_inventory.v1",
                    build=dict(compiler="GNU 13.1.0", qt="6.8.3"),
                    files=[dict(path="bin/Qt6Core.dll", sha256=digest(runtime), component="qt")]))
                proof = runtime_evidence(inventory, {"GNU": stage}, None)
                self.assertEqual(proof["files"][0]["origin_status"], "sdk-byte-match")
                runtime.write_bytes(b"different runtime")
                with self.assertRaisesRegex(ValueError, "payload changed"):
                    runtime_evidence(inventory, {"GNU": stage}, None)

        def test_source_comparison_retains_differences(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / "source.tar.gz"
                with tarfile.open(source, "w:gz") as archive:
                    for name, value in (("source/code.cpp", b"original source\n"), ("source/.tag", b"public tag")):
                        member = tarfile.TarInfo(name)
                        member.size = len(value)
                        archive.addfile(member, io.BytesIO(value))
                sbom = root / "source.spdx"
                sbom.write_text("FileName: ./code.cpp\nFileChecksum: SHA1: " + hashlib.sha1(b"original source\r\n").hexdigest()
                                + "\nFileName: ./.tag\nFileChecksum: SHA1: " + "a" * 40
                                + "\nFileName: ./missing.cpp\nFileChecksum: SHA1: " + "b" * 40, encoding="utf-8")
                result = compare_source_sbom(source, sbom)
                self.assertEqual(result["line_ending_only"], 1)
                self.assertEqual(result["mismatches"], [".tag"])
                self.assertEqual(result["non_bookkeeping_differences"], ["missing.cpp"])
                self.assertFalse(result["release_clearance"])
    return unittest.TextTestRunner(verbosity=1).run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls)).wasSuccessful()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--source-cache", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--qt-sdk", action="append", default=[], help="GNU=path or MSVC=path; read-only SDK")
    parser.add_argument("--mingw-prefix", type=Path)
    parser.add_argument("--eigen-prefix", type=Path, help="SDK prefix containing include/eigen3 and share/licenses/Eigen")
    parser.add_argument("--boost-include", type=Path, help="SDK include directory containing boost/version.hpp")
    parser.add_argument("--inventory", action="append", type=Path, default=[])
    parser.add_argument("--verify-bundle", type=Path)
    parser.add_argument("--source-sbom", type=Path, help="Compare a vendor Qt source SBOM without erasing any differences")
    parser.add_argument("--source-archive", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test and not self_test():
        return 1
    if args.source_sbom:
        if not args.source_archive or not args.report:
            raise ValueError("source SBOM comparison requires --source-archive and --report")
        result = compare_source_sbom(args.source_archive, args.source_sbom)
        write_json(args.report, result)
        print("Source comparison:", result["raw_matches"], "raw matches,", result["line_ending_only"],
              "line-ending-only matches,", len(result["mismatches"]), "mismatches,", len(result["missing"]), "missing")
        if result["non_bookkeeping_differences"]:
            return 1
    elif args.verify_bundle:
        info = verify(args.root, args.verify_bundle.resolve())
        print("PASS: dependency bundle integrity;", len(info["sources"]), "sources; release clearance remains false")
    elif args.output:
        if not args.source_cache:
            raise ValueError("bundle preparation requires --source-cache")
        sdks = {name: Path(path) for name, path in (value.split("=", 1) for value in args.qt_sdk)}
        gaps = prepare(args.root.resolve(), args.source_cache.resolve(), args.output.resolve(), sdks,
                       args.mingw_prefix, args.inventory, args.eigen_prefix, args.boost_include)
        print("PASS: local dependency evidence prepared; unresolved attribution references:", len(gaps))
    elif not args.self_test:
        parser.error("select --output, --verify-bundle or --self-test")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, TypeError, tarfile.TarError) as error:
        print("FAIL:", error)
        raise SystemExit(1)
