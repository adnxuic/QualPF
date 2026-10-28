#!/usr/bin/env python3
"""Exercise Linux staging and source-package wrappers with isolated inputs."""
import os
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(sys.platform == "linux", "native Linux install semantics")
class LinuxPackagingTests(unittest.TestCase):
    def test_qt_minimum_is_enforced_at_configuration(self):
        for version in ("6.6.3", "6.7.0"):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                qt = root / "qt"
                qt.mkdir()
                (qt / "Qt6ConfigVersion.cmake").write_text(
                    f'set(PACKAGE_VERSION "{version}")\n'
                    'if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)\n'
                    'set(PACKAGE_VERSION_COMPATIBLE FALSE)\nelse()\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\nendif()\n')
                (qt / "Qt6Config.cmake").write_text(
                    f'set(Qt6_FOUND TRUE)\nset(Qt6_VERSION "{version}")\n'
                    'function(qt_standard_project_setup)\nendfunction()\n')
                (root / "CMakeLists.txt").write_text(
                    'cmake_minimum_required(VERSION 3.20)\nset(CMAKE_SYSTEM_NAME Generic)\n'
                    'project(QtMinimumFixture LANGUAGES NONE)\n'
                    # An installed SDK must not satisfy the deliberately old
                    # fixture after CMake rejects its Qt6_DIR version.
                    'set(CMAKE_FIND_USE_PACKAGE_ROOT_PATH FALSE)\n'
                    'set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH FALSE)\n'
                    'set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)\n'
                    'set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)\n'
                    'set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)\n'
                    'set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY FALSE)\n'
                    f'include("{(ROOT / "cmake/modules/QualpfQtDependencies.cmake").as_posix()}")\n')
                result = subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build"), f"-DQt6_DIR={qt}"],
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                self.assertEqual(result.returncode == 0, version == "6.7.0", result.stdout)
                if result.returncode:
                    self.assertIn("6.7", result.stdout)

    def test_installer_overrides_stale_prefix_consistently(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo, tools = root / "repo", root / "tools"
            (repo / "scripts").mkdir(parents=True)
            tools.mkdir()
            shutil.copy2(ROOT / "scripts/install.sh", repo / "scripts/install.sh")
            (repo / "CMakeLists.txt").write_text('set(EXECUTABLE_NAME "qualpf")\nproject(QualPF VERSION 1.0.0)\n')
            build = repo / "build/linux-gnu"
            build.mkdir(parents=True)
            (build / "CMakeCache.txt").write_text("CMAKE_INSTALL_PREFIX:PATH=/stale/prefix\n")
            cmake = tools / "cmake"
            cmake.write_text('''#!/usr/bin/python3
import json, os, pathlib, sys
args = sys.argv[1:]
with open(os.environ["QUALPF_TEST_COMMAND_LOG"], "a") as log:
    log.write(json.dumps(args) + "\\n")
if "--install" in args:
    prefix = pathlib.Path(args[args.index("--prefix") + 1])
    assert str(prefix) == os.environ["QUALPF_INSTALL_PREFIX"]
    target = prefix / "bin/qualpf"
    target.parent.mkdir(parents=True)
    target.write_bytes(b"installed fixture")
elif "--build" not in args:
    prefix = next(arg.split("=", 1)[1] for arg in args if arg.startswith("-DCMAKE_INSTALL_PREFIX="))
    assert prefix == os.environ["QUALPF_INSTALL_PREFIX"]
    build = pathlib.Path(args[args.index("-B") + 1])
    (build / "CMakeCache.txt").write_text("CMAKE_INSTALL_PREFIX:PATH=" + prefix + "\\n")
''', encoding="utf-8")
            cmake.chmod(0o755)
            sudo = tools / "sudo"
            sudo.write_text('#!/bin/bash\nexec "$@"\n')
            sudo.chmod(0o755)
            prefix = root / "chosen prefix"
            log = root / "commands.jsonl"
            env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"],
                       QUALPF_INSTALL_PREFIX=str(prefix), QUALPF_TEST_COMMAND_LOG=str(log))
            result = subprocess.run(["bash", str(repo / "scripts/install.sh")], env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(result.returncode, 0, result.stdout)
            calls = [json.loads(line) for line in log.read_text().splitlines()]
            self.assertIn("-DCMAKE_INSTALL_PREFIX=" + str(prefix), calls[0])
            self.assertEqual(calls[2], ["--install", str(build), "--prefix", str(prefix)])
            self.assertEqual((prefix / "bin/qualpf").read_bytes(), b"installed fixture")
            self.assertIn("Program installed in : " + str(prefix), result.stdout)

    def test_desktop_registration_only_for_direct_install(self):
        for mode in ("direct", "deb", "appimage", "destdir", "cpack"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source, build, home = root / "source", root / "build", root / "home"
                home.mkdir()
                shutil.copytree(ROOT / "deploy/linux", source / "deploy/linux")
                (source / "CMakeLists.txt").write_text(
                    'cmake_minimum_required(VERSION 3.20)\nproject(DesktopFixture VERSION 1.0.0 LANGUAGES NONE)\n'
                    'set(EXECUTABLE_NAME qualpf)\nset(COMPONENT_NAME_MAIN main)\n'
                    f'include("{(ROOT / "cmake/packaging/Linux.cmake").as_posix()}")\n', encoding="utf-8")
                subprocess.run(["cmake", "-S", str(source), "-B", str(build),
                                f"-DBUILD_DEB_PACKAGE={'ON' if mode == 'deb' else 'OFF'}",
                                f"-DBUILD_APPIMAGE={'ON' if mode == 'appimage' else 'OFF'}"], check=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                prefix = root / ("_CPack_Packages/test/usr" if mode == "cpack" else "prefix with spaces")
                env = dict(os.environ, HOME=str(home), DESTDIR=str(root / "stage") if mode == "destdir" else "")
                subprocess.run(["cmake", "--install", str(build), "--prefix", str(prefix)], env=env,
                               check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                installed = root / "stage" / prefix.relative_to("/") if mode == "destdir" else prefix
                desktop = installed / "share/applications/qualpf.desktop"
                self.assertTrue(desktop.is_file())
                self.assertIn(f'Exec="{prefix}/bin/qualpf" %f', desktop.read_text())
                for size in (16, 32, 48, 256):
                    self.assertTrue((installed / f"share/icons/hicolor/{size}x{size}/apps/qualpf.png").is_file())
                self.assertEqual((home / ".local/share/applications/qualpf.desktop").exists(), mode == "direct")
                if mode != "direct":
                    self.assertEqual(list(home.iterdir()), [])

    def test_source_deb_wrapper_keeps_release_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            wrapper = root / "repo/scripts/package/make_debian.sh"
            wrapper.parent.mkdir(parents=True)
            shutil.copy2(ROOT / "scripts/package/make_debian.sh", wrapper)
            source = root / "source/qualpf"
            helper = source / "scripts/package/build_deb.sh"
            helper.parent.mkdir(parents=True)
            (source / "CMakeLists.txt").write_text("project(QualPF VERSION 1.0.0)\n")
            helper.write_text(
                '#!/bin/bash\nset -e\ncd "$(dirname "$0")/../.."\n'
                'mkdir -p build/linux-deb/packages releases/1.0.0/linux-deb\n'
                "printf package > build/linux-deb/packages/qualpf.deb\n"
                'cp build/linux-deb/packages/qualpf.deb releases/1.0.0/linux-deb/\n'
                "printf checksum > releases/1.0.0/linux-deb/qualpf.deb.sha256\n"
                "printf cpack-checksum > build/linux-deb/packages/qualpf.deb.sha256\n"
                "printf cpack-record > build/linux-deb/packages/package-artifacts.json\n"
                "printf inventory > build/linux-deb/packages/distribution-inventory.json\n"
                "printf build-record > releases/1.0.0/linux-deb/artifacts.json\n", encoding="utf-8")
            archive = root / "source.tar.gz"
            with tarfile.open(archive, "w:gz") as stream:
                stream.add(source, arcname="qualpf")
            subprocess.run(["bash", str(wrapper), str(archive)], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            release = root / "repo/releases/1.0.0/linux-deb"
            self.assertEqual({p.name: p.read_bytes() for p in release.iterdir()},
                             {"qualpf.deb": b"package", "qualpf.deb.sha256": b"cpack-checksum", "artifacts.json": b"build-record",
                              "package-artifacts.json": b"cpack-record", "distribution-inventory.json": b"inventory"})
            self.assertEqual(list((root / "repo/build").iterdir()), [])


if __name__ == "__main__":
    unittest.main()
