#!/usr/bin/env python3
"""Verify all frozen normal references, including separately restored ZIPs."""
import hashlib
import json
from pathlib import Path
from restore_reference_archives import archive_entries, restore

ROOT = Path(__file__).resolve().parents[2]


def main():
    restore(ROOT, archive_entries(ROOT), verify_only=True)
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
