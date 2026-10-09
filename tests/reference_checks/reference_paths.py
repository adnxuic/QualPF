"""Resolve independent regression resources by their unchanged logical identities.

Only the small current reference set is addressed here. No history restoration,
archive download, native execution or generated expected values participates.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
_manifest = json.loads((ROOT / "tests/reference/manifest.json").read_bytes())
_files = {r["original_path"]: ROOT / r["path"] for r in _manifest["files"]}
_old_locations = {"tests/reference/history/" + n: p for n, p in _files.items()
                  if p.is_relative_to(ROOT / "tests/regression/references")}
_prefixes = {'docs/refactoring/phase-9.3': 'tests/regression/references/preprocessing', 'docs/refactoring/phase-14': 'tests/regression/references/symmetry', 'docs/refactoring/phase-15': 'tests/regression/references/reflection', 'docs/refactoring/phase-16': 'tests/regression/references/scattering', 'docs/refactoring/phase-17': 'tests/regression/references/powder', 'docs/refactoring/phase-18': 'tests/regression/references/card', 'docs/refactoring/phase-19': 'tests/regression/references/cif', 'docs/refactoring/phase-34': 'tests/regression/references/qualified-numerics', 'docs/refactoring/phase-36': 'tests/regression/references/provider-successors', 'archive/legacy/native-provider': 'tests/regression/references/provenance/resource-inputs', 'archive/legacy/phase36d-test-reference': 'tests/regression/references/provenance/previous-providers', 'tests/compatibility': 'tests/regression/references/provenance/comparison-sources', 'tools/crystallography': 'tests/regression/references/provenance/table-generators'}

def reference_path(path):
    path = Path(path)
    try:
        name = path.relative_to(ROOT).as_posix()
    except ValueError:
        return path
    if name in _old_locations:
        return _old_locations[name]
    if name.startswith(("docs/refactoring/", "archive/legacy/", "tests/phase")):
        if name in _files:
            return _files[name]
        for old, new in _prefixes.items():
            if name == old or name.startswith(old + "/"):
                return ROOT / (new + name[len(old):])
    return path

def provenance_path(path):
    """Hash sealed comparison source for provenance, live providers separately."""
    path = reference_path(path)
    try:
        name = path.relative_to(ROOT).as_posix()
    except ValueError:
        return path
    if ((name.startswith("tests/compatibility/") and name.endswith(".py")) or
            name in ("tests/CMakeLists.txt", "tools/crystallography/generate_chemistry.py",
                     "tools/crystallography/generate_scattering.py")):
        return _files[name]
    if name == "tests/unit/card_properties.cpp":
        return ROOT / "tests/unit/core/card_properties.cpp"
    return path
