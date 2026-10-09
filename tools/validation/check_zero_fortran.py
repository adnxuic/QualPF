#!/usr/bin/env python3
"""Current source/build/binary preservation check; never executes historical gates."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FORTRAN = {".f", ".f90", ".for", ".f95", ".f03", ".f08", ".ftn", ".fpp"}
RUNTIME = re.compile(r"gfortran|quadmath|libifcore|libifport|libfortran|flang", re.I)


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--objdump", type=Path, required=True)
    parser.add_argument("--nm", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    build = args.build.resolve()
    failures = []
    sources, cmake = [], []
    for area in ("src", "legacy", "apps", "third_party", "configured", "cmake", "tests"):
        for path in (ROOT / area).rglob("*"):
            if not path.is_file() or path.is_relative_to(ROOT / "tests/reference"):
                continue
            if path.suffix.lower() in FORTRAN:
                sources.append(path.relative_to(ROOT).as_posix())
            if path.name == "CMakeLists.txt" or path.suffix == ".cmake":
                cmake.append(path)
    cmake.append(ROOT / "CMakeLists.txt")
    enabled = []
    for path in cmake:
        text = re.sub(r"#.*", "", path.read_text(encoding="utf-8-sig"))
        if re.search(r"(?:enable_language|project)\s*\([^)]*\bFortran\b", text, re.I):
            enabled.append(path.relative_to(ROOT).as_posix())
    commands = json.loads((build / "compile_commands.json").read_bytes())
    units = [c["file"] for c in commands if Path(c["file"]).suffix.lower() in FORTRAN]
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8-sig")
    required = [line for line in cache.splitlines() if re.match(r"CMAKE_Fortran_COMPILER[^:]*:", line)]
    ninja = (build / "build.ninja").read_text(encoding="utf-8-sig")
    if re.search(r"(?:Fortran_COMPILER|Fortran_MODULE|(?:-l|/)(?:gfortran|ifcore|ifport))", ninja, re.I):
        failures.append("Fortran compiler/module/runtime in actual build graph")
    binaries = []
    for path in build.rglob("*"):
        if not path.is_file() or "CMakeFiles" in path.parts:
            continue
        if path.suffix.lower() in {".exe", ".dll"}:
            binaries.append(path)
        elif not path.suffix or path.suffix == ".so":
            with path.open("rb") as stream:
                if stream.read(4) == b"\x7fELF":
                    binaries.append(path)
    binaries.sort()
    if not binaries:
        failures.append("No current product/test binaries found")
    imports, native_symbols = {}, []
    for path in binaries:
        text = subprocess.check_output([str(args.objdump), "-p", str(path)], text=True, errors="replace")
        libraries = re.findall(r"(?:DLL Name:|NEEDED)\s*(\S+)", text)
        imports[path.relative_to(build).as_posix()] = libraries
        if any(RUNTIME.search(name) for name in libraries):
            failures.append("Fortran runtime import: " + str(path))
        if args.nm:
            text = subprocess.check_output([str(args.nm), "--defined-only", str(path)], text=True, errors="replace")
            matches = re.findall(r"\b(?:_+gfortran\w*|_+\w+_MOD_\w+|for_\w+)\b", text)
            native_symbols.extend(matches)
    if sources or enabled or units or required or native_symbols:
        failures.append("Active Fortran source/language/unit/compiler/origin symbol remains")
    result = dict(status="PASS" if not failures else "FAIL", scope="CURRENT_F_PRESERVATION",
                  active_fortran_sources=sources, enabled_fortran_languages=enabled,
                  fortran_compile_units=units, fortran_compiler_requirements=required,
                  fortran_origin_symbols=sorted(set(native_symbols)), binary_imports=imports,
                  fortran_symbol_scan="FRESH_VALIDATION" if args.nm else "NOT_EXECUTED",
                  compiled_units_checked=len(commands), failures=failures,
                  evidence_fingerprints={
                      "audit": sha(Path(__file__)),
                      "compile_commands": sha(build / "compile_commands.json"),
                      "cache": sha(build / "CMakeCache.txt"),
                      "build_graph": sha(build / "build.ninja"),
                      "binaries": {p.relative_to(build).as_posix(): sha(p) for p in binaries}},
                  original_F_authority="phase37e-entry-20261009:docs/refactoring/phase-36/current-release-acceptance/zero-fortran-gate.json")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"], len(commands), "C++ compile commands,", len(binaries), "current binaries")
    if failures:
        raise SystemExit("; ".join(failures))


if __name__ == "__main__":
    main()
