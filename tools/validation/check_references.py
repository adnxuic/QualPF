#!/usr/bin/env python3
"""Verify the small normal-test reference set without historical materialization."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    manifest = json.loads((ROOT / "tests/reference/manifest.json").read_bytes())
    if manifest.get("expected_from_product") is not False:
        raise ValueError("Reference provenance must be independent of product output")
    for entry in manifest["files"]:
        path = ROOT / entry["path"]
        if not path.resolve().is_relative_to(ROOT):
            raise ValueError("Reference path escapes repository")
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        if path.stat().st_size != entry["bytes"] or digest != entry["sha256"]:
            raise ValueError("Frozen reference changed: " + entry["path"])
    print("PASS: {} byte-preserved normal references".format(len(manifest["files"])))


if __name__ == "__main__":
    main()
