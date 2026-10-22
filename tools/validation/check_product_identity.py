#!/usr/bin/env python3
"""Check current product names without rewriting original historical names."""
import argparse
import json
from pathlib import Path
import re
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
OLD_NAME = re.compile(r"qualx" + "pf", re.IGNORECASE)
AREAS = (".agent", ".github", "apps", "src", "cmake", "deploy", "scripts",
         "tools", "tests", "docs/user", "docs/developer")
TEXT_SUFFIXES = {".md", ".cpp", ".h", ".hpp", ".cmake", ".txt", ".json",
                 ".py", ".sh", ".ps1", ".cmd", ".qs", ".in", ".desktop",
                 ".yml", ".yaml", ".ui", ".qrc"}


def check(root=ROOT, build=None):
    errors = []
    inventory = json.loads((root / "tools/validation/product-identity.json").read_bytes())
    allowed = inventory["allowed_old_name_lines"]
    files = {root / name for name in ("CMakeLists.txt", "CMakePresets.json", "README.md",
                                    ".gitignore", "docs/mkdocs.yml", "docs/make_pdf.sh")}
    for area in AREAS:
        files.update(path for path in (root / area).rglob("*")
                     if path.is_file() and path.suffix in TEXT_SUFFIXES
                     and not path.is_relative_to(root / "tests/reference")
                     and not path.is_relative_to(root / "tests/regression/references")
                     and not path.is_relative_to(root / "tests/data"))
    # The reviewed whitelist itself stores the permitted previous-store tokens.
    files.discard(root / "tools/validation/product-identity.json")
    for path in sorted(files):
        name = path.relative_to(root).as_posix()
        try:
            text = path.read_text(encoding="utf-8-sig")
        except (OSError, UnicodeError) as error:
            errors.append(f"unreadable current product input {name}: {error}")
            continue
        for number, line in enumerate(text.splitlines(), 1):
            if OLD_NAME.search(line) and line.strip() not in allowed.get(name, ()):
                errors.append(f"obsolete product identity: {name}:{number}")
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8-sig")
    if not re.search(r"project\(QualPF\s+VERSION\s+1\.0\.5\s+LANGUAGES\s+CXX\)", cmake):
        errors.append("unexpected product project/version/language")
    if 'set(EXECUTABLE_NAME "qualpf")' not in cmake:
        errors.append("unexpected executable identity")
    source = (root / "src/infrastructure/settings/settings_migration.cpp").read_text(encoding="utf-8")
    if sum(bool(OLD_NAME.search(line)) for line in source.splitlines()) != 1:
        errors.append("legacy identity must have exactly one production settings-import use")
    gui = (root / "src/gui/mainwindow.cpp").read_text(encoding="utf-8")
    if re.search(r'setUrl\([^;]*qualx3/updates', gui):
        errors.append("current product update entry points to an upstream product feed")
    if build is not None:
        cache = (build / "CMakeCache.txt").read_text(encoding="utf-8-sig")
        if "CMAKE_PROJECT_NAME:STATIC=QualPF" not in cache:
            errors.append("build cache has obsolete project identity")
        config = (build / "configured_files/include/config.h").read_text(encoding="utf-8-sig")
        for value in ('APP_NAME = "qualpf"', 'APP_DISPLAY_NAME = "QualPF"'):
            if value not in config:
                errors.append("generated product identity missing: " + value)
        if not any((build / "src" / name).is_file() for name in ("qualpf", "qualpf.exe")):
            errors.append("current executable missing from build")
    return errors, len(files)


def self_test():
    class Controls(unittest.TestCase):
        def test_previous_store_and_rejection(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                names = {"CMakeLists.txt", "CMakePresets.json", "README.md", ".gitignore",
                         "docs/mkdocs.yml", "docs/make_pdf.sh", "src/gui/mainwindow.cpp",
                         "tools/validation/product-identity.json"}
                inventory = json.loads((ROOT / "tools/validation/product-identity.json").read_bytes())
                names.update(inventory["allowed_old_name_lines"])
                for name in names:
                    target = root / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(ROOT / name, target)
                self.assertEqual(check(root)[0], [])
                readme = root / "README.md"
                original = readme.read_bytes()
                readme.write_text(OLD_NAME.pattern + "\n", encoding="utf-8")
                self.assertTrue(any("obsolete product identity: README.md" in error for error in check(root)[0]))
                readme.write_bytes(original)
                inventory["allowed_old_name_lines"]["src/infrastructure/settings/settings_migration.cpp"] = []
                (root / "tools/validation/product-identity.json").write_text(json.dumps(inventory), encoding="utf-8")
                self.assertTrue(any("settings_migration.cpp" in error for error in check(root)[0]))

    result = unittest.TextTestRunner(verbosity=0).run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls))
    return result.wasSuccessful()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test and not self_test():
        raise SystemExit(1)
    try:
        errors, count = check(build=args.build.resolve() if args.build else None)
    except (OSError, ValueError, KeyError, TypeError) as error:
        errors, count = [str(error)], 0
    for error in errors:
        print("FAIL:", error)
    if not errors:
        print(f"PASS: current product identity across {count} inputs; reviewed legacy import/history retained")
    raise SystemExit(bool(errors))
