#!/usr/bin/env python3
"""Fetch pinned AppImage tools; reject changed downloads and cached executables."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
import unittest
from unittest.mock import patch
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[2]
CATALOGUE = ROOT / "deploy/appimage-tools.json"
REPOSITORIES = {"linuxdeploy": "linuxdeploy/linuxdeploy", "appimagetool": "AppImage/appimagetool"}


def load_tools(path=CATALOGUE):
    catalogue = json.loads(path.read_text(encoding="utf-8"))
    tools = catalogue["tools"]
    if catalogue.get("schema") != "qualpf.appimage_tools.v1" or sorted(t["id"] for t in tools) != sorted(REPOSITORIES):
        raise ValueError("invalid AppImage tool catalogue")
    for tool in tools:
        repository = re.escape(REPOSITORIES[tool["id"]])
        if (tool["file"] != tool["id"] + "-x86_64.AppImage" or
                not re.fullmatch(r"https://api\.github\.com/repos/" + repository + r"/releases/assets/[0-9]+", tool["url"]) or
                not re.fullmatch(r"[0-9a-f]{64}", tool["sha256"]) or
                not re.fullmatch(r"[0-9a-f]{40}", tool["source_revision"]) or tool["bytes"] <= 0):
            raise ValueError("invalid pinned AppImage tool: " + tool["id"])
    return tools


def verify(path, tool):
    with path.open("rb") as stream:
        actual = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            actual.update(block)
    if path.stat().st_size != tool["bytes"] or actual.hexdigest() != tool["sha256"]:
        raise ValueError("AppImage tool size/SHA-256 mismatch: " + str(path))


def prepare_tool(directory, tool):
    directory.mkdir(parents=True, exist_ok=True)
    target = directory / tool["file"]
    if target.is_symlink():
        raise ValueError("AppImage tool cache must not be a symlink: " + str(target))
    if not target.exists():
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(prefix=tool["file"] + ".", suffix=".download", dir=directory, delete=False) as stream:
                temporary = Path(stream.name)
                request = Request(tool["url"], headers={"Accept": "application/octet-stream", "User-Agent": "QualPF-packaging"})
                with urlopen(request, timeout=60) as response:
                    shutil.copyfileobj(response, stream)
            verify(temporary, tool)
            os.replace(temporary, target)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
    # Check every reuse before permitting execution; a mismatch never redownloads
    # over the suspect cache or changes its permissions.
    verify(target, tool)
    target.chmod(0o755)
    return target


def self_test():
    class ToolTests(unittest.TestCase):
        def test_catalogue(self):
            self.assertEqual(len(load_tools()), 2)

        def test_download_cache_and_rejection(self):
            raw = b"independent tool fixture\n"
            tool = dict(file="tool.AppImage", url="https://example.invalid/immutable-asset", bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())
            with tempfile.TemporaryDirectory() as directory:
                directory = Path(directory)
                with patch(__name__ + ".urlopen", return_value=io.BytesIO(raw)) as download:
                    target = prepare_tool(directory, tool)
                    download.assert_called_once()
                self.assertEqual(target.read_bytes(), raw)
                if os.name != "nt":
                    self.assertEqual(target.stat().st_mode & 0o777, 0o755)
                with patch(__name__ + ".urlopen") as download:
                    self.assertEqual(prepare_tool(directory, tool), target)
                    download.assert_not_called()
                    tampered = b"x" * len(raw)
                    target.write_bytes(tampered)
                    with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                        prepare_tool(directory, tool)
                    download.assert_not_called()
                    self.assertEqual(target.read_bytes(), tampered)
            for payload in (b"y" * len(raw), raw[:-1]):
                with tempfile.TemporaryDirectory() as directory, patch(__name__ + ".urlopen", return_value=io.BytesIO(payload)):
                    with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                        prepare_tool(Path(directory), tool)
                    self.assertEqual(list(Path(directory).iterdir()), [])

    return unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(ToolTests)).wasSuccessful()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools-dir", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test and not self_test():
        return 1
    tools = load_tools()
    if args.tools_dir:
        for tool in tools:
            print("Verified:", prepare_tool(args.tools_dir, tool), tool["sha256"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
