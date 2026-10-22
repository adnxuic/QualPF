#!/usr/bin/env python3
"""Check documentation ownership, local navigation and preserved historical bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "agent"))
from check_current_context import check_links, relative, unique_object

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = "history/relocations.json"
REQUIRED = ("docs/README.md", "docs/user/index.md", "docs/developer/README.md",
            "docs/developer/building.md", "docs/developer/repository-layout.md",
            "history/README.md", "docs/mkdocs.yml", "docs/make_pdf.sh", MANIFEST)
AGENT_FILES = {"README.md", "POLICY.md", "WORKFLOW.md", "CONTEXT_MAP.md", "TASK_STATE.json", ".gitattributes"}
RETIRED = re.compile(r"(?<![\w.-])docs/(?:architecture|numerical|development|refactoring|validation)(?:/|\b)|docs/migration-history\.md")


def current_markdown(root):
    names = {root / name for name in ("README.md", "AGENTS.md", "docs/README.md", "history/README.md", "tests/data-guide.md")}
    for area in (".agent", "docs/user", "docs/developer"):
        names.update((root / area).rglob("*.md"))
    for area in ("apps", "src"):
        names.update((root / area).rglob("README.md"))
    # Local measured-data collections are intentionally not published. Do not
    # inspect their private README files or require them in source packages.
    names.update(root / name for name in ("tests/README.md", "tests/compatibility/README.md",
                 "tests/integration/README.md", "tests/unit/README.md", "tests/data/README.md",
                 "tests/data/pfsm-quantification/README.md"))
    return sorted(p for p in names if p.is_file())


def historical_files(root, errors):
    document = json.loads((root / MANIFEST).read_text(encoding="utf-8"), object_pairs_hook=unique_object)
    if (not isinstance(document, dict) or document.get("schema") != "qualpf.history_relocations.v1"
            or not re.fullmatch(r"[0-9a-f]{40}", str(document.get("source_commit", "")))
            or not isinstance(document.get("files"), list) or not document["files"]):
        errors.append("invalid history relocation manifest")
        return
    originals, destinations = set(), set()
    for row in document["files"]:
        if not isinstance(row, dict):
            errors.append("invalid history relocation row")
            continue
        old, new = row.get("original_path"), row.get("path")
        if (not relative(old) or not old.startswith("docs/") or not relative(new)
                or not new.startswith("history/") or new in {MANIFEST, "history/README.md"}
                or not re.fullmatch(r"[0-9a-f]{64}", str(row.get("sha256", "")))
                or not re.fullmatch(r"[0-9a-f]{40}", str(row.get("git_blob", "")))
                or type(row.get("size")) is not int or row["size"] < 0):
            errors.append("invalid/unsafe historical identity: " + str(new))
            continue
        if old in originals or new in destinations:
            errors.append("duplicate historical relocation: " + new)
        originals.add(old)
        destinations.add(new)
        path = root / new
        if not path.resolve().is_relative_to(root) or not path.is_file():
            errors.append("missing/unsafe historical file: " + new)
            continue
        data = path.read_bytes()
        if len(data) != row["size"] or hashlib.sha256(data).hexdigest() != row["sha256"]:
            errors.append("changed historical bytes: " + new)
    for path in (root / "history").rglob("*"):
        if path.is_file():
            name = path.relative_to(root).as_posix()
            if name not in destinations | {MANIFEST, "history/README.md"}:
                errors.append("unindexed historical file: " + name)


def check(root=ROOT):
    root = root.resolve()
    errors = []
    for name in REQUIRED:
        path = root / name
        if not path.resolve().is_relative_to(root) or not path.is_file():
            errors.append("missing/unsafe documentation input: " + name)
    if errors:
        return errors
    for path in (root / "docs").iterdir():
        if path.is_dir() and path.name not in {"user", "developer", "site", ".venv"}:
            if any(p.is_file() for p in path.rglob("*")):
                errors.append("misplaced documentation directory: " + path.name)
        elif path.is_file() and path.name not in {"README.md", "mkdocs.yml", "make_pdf.sh", "requirements.txt", "qualpf_manual.pdf", "qualx_manual.pdf"}:
            errors.append("misplaced documentation file: " + path.name)
    for path in (root / ".agent").rglob("*"):
        if path.is_file() and path.relative_to(root / ".agent").as_posix() not in AGENT_FILES:
            errors.append("misplaced Agent content: " + path.relative_to(root).as_posix())
    for path in current_markdown(root):
        content = path.read_text(encoding="utf-8-sig")
        check_links(root, path, content, errors)
        if path.relative_to(root).as_posix() == "history/README.md":
            continue  # The recovery index intentionally names original paths.
        for number, line in enumerate(content.splitlines(), 1):
            if RETIRED.search(line) and not re.search(r"(?:phase[\w-]+|[0-9a-f]{7,40}):docs/", line):
                errors.append(f"{path.relative_to(root)}:{number}: retired current documentation path")
    config = (root / "docs/mkdocs.yml").read_text(encoding="utf-8")
    if not re.search(r"^docs_dir: user\s*$", config, re.MULTILINE):
        errors.append("MkDocs must publish only docs/user")
    if not re.search(r"^use_directory_urls: false\s*$", config, re.MULTILINE):
        errors.append("existing user page URL format must be retained")
    nav = re.findall(r"^\s+- [^:\n]+:\s+([\w./-]+\.md)\s*$", config, re.MULTILINE)
    pages = {p.relative_to(root / "docs/user").as_posix() for p in (root / "docs/user").rglob("*.md")}
    if len(nav) != len(set(nav)) or set(nav) != pages:
        errors.append("user navigation must list every manual page exactly once")
    pdf = (root / "docs/make_pdf.sh").read_text(encoding="utf-8")
    inputs = re.findall(r'"\$\{SCRIPT_DIR\}/user/([^"\n]+\.md)"', pdf)
    if inputs != nav:
        errors.append("PDF inputs must match user navigation in order without duplicates")
    try:
        historical_files(root, errors)
    except (OSError, UnicodeError, ValueError, TypeError, KeyError) as error:
        errors.append("invalid historical manifest: " + str(error))
    return errors


def self_test():
    class Controls(unittest.TestCase):
        def setUp(self):
            self.directory = tempfile.TemporaryDirectory()
            self.addCleanup(self.directory.cleanup)
            self.root = Path(self.directory.name)
            for name in REQUIRED:
                self.put(name, "# Guide\n")
            self.put("docs/mkdocs.yml", "docs_dir: user\nuse_directory_urls: false\nnav:\n  - Home: index.md\n")
            self.put("docs/make_pdf.sh", '"${SCRIPT_DIR}/user/index.md"\n')
            content = b"original failure\r\n"
            self.put("history/development/original.md", content)
            self.manifest = dict(schema="qualpf.history_relocations.v1", source_commit="a" * 40,
                                 files=[dict(original_path="docs/development/original.md", path="history/development/original.md",
                                             size=len(content), sha256=hashlib.sha256(content).hexdigest(), git_blob="b" * 40)])
            self.put(MANIFEST, json.dumps(self.manifest))

        def put(self, name, content):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content if isinstance(content, bytes) else content.encode())

        def test_portable_and_links(self):
            self.assertEqual(check(self.root), [])
            self.put("docs/README.md", "[Developer](developer/README.md#guide)\n")
            self.assertEqual(check(self.root), [])
            self.put("docs/README.md", "[Bad](developer/README.md#missing)\n")
            self.assertTrue(any("anchor" in e for e in check(self.root)))
            (self.root / "docs/developer/README.md").unlink()
            self.assertTrue(any("missing/unsafe" in e for e in check(self.root)))

        def test_ownership_and_old_paths(self):
            self.put("docs/development/old.md", "old")
            self.put(".agent/architecture.md", "misplaced")
            self.put("docs/README.md", "Use docs/architecture/overview.md\n")
            errors = check(self.root)
            for token in ("misplaced documentation", "misplaced Agent", "retired current"):
                self.assertTrue(any(token in e for e in errors), token)

        def test_navigation_and_pdf(self):
            self.put("docs/user/settings.md", "# Settings\n")
            errors = check(self.root)
            self.assertTrue(any("navigation must" in e for e in errors))
            self.put("docs/make_pdf.sh", '"${SCRIPT_DIR}/user/settings.md"\n')
            self.assertTrue(any("PDF inputs" in e for e in check(self.root)))

        def test_frozen_bytes_and_safe_mapping(self):
            self.put("history/development/original.md", b"original failure\n")
            self.assertTrue(any("changed historical bytes" in e for e in check(self.root)))
            self.manifest["files"][0]["path"] = "history/../../outside.md"
            self.put(MANIFEST, json.dumps(self.manifest))
            self.assertTrue(any("unsafe historical" in e for e in check(self.root)))

        def test_missing_duplicate_and_unindexed_evidence(self):
            (self.root / "history/development/original.md").unlink()
            self.manifest["files"].append(self.manifest["files"][0].copy())
            self.put(MANIFEST, json.dumps(self.manifest))
            self.put("history/extra.md", "unindexed")
            errors = check(self.root)
            for token in ("missing/unsafe historical", "duplicate historical", "unindexed historical"):
                self.assertTrue(any(token in e for e in errors), token)

    return unittest.TextTestRunner(verbosity=0).run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls)).wasSuccessful()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test and not self_test():
        raise SystemExit(1)
    try:
        errors = check(args.root)
    except (OSError, UnicodeError, ValueError) as error:
        errors = [str(error)]
    for error in errors:
        print("FAIL:", error)
    if not errors:
        print("PASS: documentation ownership, navigation, PDF inputs and preserved history")
    raise SystemExit(bool(errors))
