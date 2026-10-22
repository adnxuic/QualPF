#!/usr/bin/env python3
"""Check current product names without rewriting original historical names."""
import argparse
import json
from pathlib import Path
import re
import shutil
import tempfile
import unittest
import xml.etree.ElementTree as ET

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
    if not re.search(r"project\(QualPF\s+VERSION\s+1\.0\.0\s+LANGUAGES\s+CXX\)", cmake):
        errors.append("unexpected product project/version/language")
    if 'set(EXECUTABLE_NAME "qualpf")' not in cmake:
        errors.append("unexpected executable identity")
    source = (root / "src/infrastructure/settings/settings_migration.cpp").read_text(encoding="utf-8")
    if sum(bool(OLD_NAME.search(line)) for line in source.splitlines()) != 1:
        errors.append("legacy identity must have exactly one production settings-import use")
    gui = (root / "src/gui/mainwindow.cpp").read_text(encoding="utf-8")
    if re.search(r'setUrl\([^;]*qualx3/updates', gui):
        errors.append("current product update entry points to an upstream product feed")
    identity = json.loads((root / "deploy/distribution.json").read_bytes())["product"]
    if identity["publisher"] != "adnxuic" or identity["license"] != "GPL-3.0-only":
        errors.append("unexpected independent publisher or application license")
    readme = (root / "README.md").read_text(encoding="utf-8-sig")
    if identity["description"] not in readme:
        errors.append("README description contradicts distribution identity")
    if "GPL-3.0-only" not in readme or "License-LGPL" in readme or "GNU Lesser General Public License" in readme:
        errors.append("README application license contradicts distribution identity")
    packaging = (root / "cmake/packaging/CPack.cmake").read_text(encoding="utf-8-sig")
    if 'set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")' not in packaging:
        errors.append("installer must use the canonical GPL license")
    for line in packaging.splitlines():
        if re.search(r'CPACK_(?:PACKAGE_VENDOR|IFW_PACKAGE_PUBLISHER|DEBIAN_PACKAGE_MAINTAINER)', line) and re.search(r'Corrado|Institute of Crystallography|cnr\.it', line, re.IGNORECASE):
            errors.append("upstream author/institution incorrectly identified as publisher")
    qrc = ET.parse(root / "src/gui/resources/resource.qrc")
    excluded = {"images/qualx.png", "images/about.png", "images/logo_CNR_compatto.png", "images/logo_IC.png"}
    if any((entry.text or "").strip() in excluded for entry in qrc.iter("file")):
        errors.append("legacy brand image remains in active GUI resources")
    appimage = (root / "scripts/package/build_appimage.sh").read_text(encoding="utf-8-sig")
    if "icons/256x256/qualx.png" in appimage or "icons/256x256/qualpf.png" not in appimage:
        errors.append("AppImage staging must use the independent QualPF icon")
    if "https://ccorrado71.github.io/qualx3/" in gui or "softwareic/expo/contact-us" in gui:
        errors.append("product help/support points at another product")
    help_ui = ET.parse(root / "src/gui/mainwindow.ui")
    help_action = help_ui.find(".//action[@name='actionDocumentation_HTML']/property[@name='text']/string")
    if help_action is None or help_action.text != "User Manual (HTML)":
        errors.append("HTML Help must identify the current user manual")
    if build is not None:
        cache = (build / "CMakeCache.txt").read_text(encoding="utf-8-sig")
        if "CMAKE_PROJECT_NAME:STATIC=QualPF" not in cache:
            errors.append("build cache has obsolete project identity")
        config = (build / "configured_files/include/config.h").read_text(encoding="utf-8-sig")
        for value in ('APP_NAME = "qualpf"', 'APP_DISPLAY_NAME = "QualPF"'):
            if value not in config:
                errors.append("generated product identity missing: " + value)
        for key, name in (("publisher", "PUBLISHER"), ("license", "LICENSE"), ("support", "SUPPORT"), ("documentation", "DOCUMENTATION")):
            if f'APP_{name} = "{identity[key]}"' not in config:
                errors.append("generated distribution identity missing: " + key)
        if not any((build / "src" / name).is_file() for name in ("qualpf", "qualpf.exe")):
            errors.append("current executable missing from build")
    return errors, len(files)


def self_test():
    class Controls(unittest.TestCase):
        def test_previous_store_and_rejection(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                names = {"CMakeLists.txt", "CMakePresets.json", "README.md", ".gitignore",
                         "docs/mkdocs.yml", "docs/make_pdf.sh", "src/gui/mainwindow.cpp", "src/gui/mainwindow.ui",
                         "tools/validation/product-identity.json", "deploy/distribution.json",
                         "cmake/packaging/CPack.cmake", "src/gui/resources/resource.qrc",
                         "scripts/package/build_appimage.sh"}
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
                readme.write_text("QualPF uses the GNU Lesser General Public License\n", encoding="utf-8")
                self.assertTrue(any("README application license" in error for error in check(root)[0]))
                readme.write_bytes(original)
                description = json.loads((root / "deploy/distribution.json").read_bytes())["product"]["description"]
                readme.write_text(original.decode().replace(description, "Crystal structure solution"), encoding="utf-8")
                self.assertTrue(any("README description" in error for error in check(root)[0]))
                readme.write_bytes(original)
                cpack = root / "cmake/packaging/CPack.cmake"
                original_cpack = cpack.read_bytes()
                cpack.write_text(original_cpack.decode().replace('${QUALPF_PUBLISHER}', 'Corrado Cuocci'), encoding="utf-8")
                self.assertTrue(any("incorrectly identified as publisher" in error for error in check(root)[0]))
                cpack.write_bytes(original_cpack)
                resource = root / "src/gui/resources/resource.qrc"
                original_qrc = resource.read_bytes()
                resource.write_text(original_qrc.decode().replace("</qresource>", "<file>images/qualx.png</file></qresource>", 1), encoding="utf-8")
                self.assertTrue(any("legacy brand image" in error for error in check(root)[0]))
                resource.write_bytes(original_qrc)
                ui = root / "src/gui/mainwindow.ui"
                original_ui = ui.read_bytes()
                ui.write_text(original_ui.decode().replace("User Manual (HTML)", "QualX Reference Documentation (HTML)"), encoding="utf-8")
                self.assertTrue(any("HTML Help" in error for error in check(root)[0]))
                ui.write_bytes(original_ui)
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
