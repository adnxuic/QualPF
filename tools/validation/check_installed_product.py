#!/usr/bin/env python3
"""Exercise an installed product with isolated settings and an independent CIF.

This checks an existing installation/extracted package; it neither installs nor
uninstalls software, contacts a service, or modifies existing user settings.
Use a new work directory and the native Qt platform (Windows or xcb).
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def check(executable, prefix, work, qpa):
    executable, prefix, work = executable.resolve(), prefix.resolve(), work.resolve()
    if not executable.is_file():
        raise ValueError("installed executable is missing")
    if work.exists():
        raise ValueError("use a new isolated work directory; existing settings are never overwritten")
    work.mkdir(parents=True)
    settings = work / "settings"
    old = settings / "IC/qualxpf.ini"
    old.parent.mkdir(parents=True)
    cif = work / "cif"
    cif.mkdir()
    fixture = ROOT / "tests/data/cif/cod-2300112.cif"
    shutil.copyfile(fixture, cif / fixture.name)
    database = work / "database/cards"
    database.parent.mkdir()
    old.write_text('[databases]\ncount=1\n0\\inUse=true\n0\\name=local_acceptance_fixture\n'
                   '0\\path=' + database.as_posix() + '\ndefaultDir=' + database.parent.as_posix() +
                   '\n[migration-test]\nsentinel=installed-acceptance-fixture\n', encoding="utf-8")
    original = old.read_bytes()
    env = os.environ.copy()
    if sys.platform == "win32":
        system = Path(env["SYSTEMROOT"])
        env["PATH"] = str(system / "System32") + ";" + str(system)
    else:
        env["PATH"] = "/usr/bin:/bin"
        env["APPIMAGE_EXTRACT_AND_RUN"] = "1"
    for key in ("QT_PLUGIN_PATH", "QML2_IMPORT_PATH", "QTDIR", "LD_LIBRARY_PATH"):
        env.pop(key, None)
    env["QT_QPA_PLATFORM"] = qpa
    report = dict(schema="qualpf.installed_acceptance.v1", status="RUNNING",
                  executable=str(executable), executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                  platform=sys.platform, qpa=qpa, path=env["PATH"], cases=[])
    output = work / "observations.json"

    def save():
        output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    def run(name, arguments, expected):
        result = subprocess.run([str(executable), "--settings-dir", str(settings), *arguments],
                                cwd=prefix, env=env, capture_output=True, timeout=90)
        text = (result.stdout + result.stderr).decode("utf-8", errors="replace")
        report["cases"].append(dict(case=name, exit_code=result.returncode, output=text))
        save()
        if result.returncode or expected not in text:
            raise ValueError(f"installed {name} failed: {result.returncode}: {text}")

    try:
        for name in ("LICENSE", "NOTICE.md", "THIRD_PARTY_NOTICES.md"):
            if (prefix / "share/qualpf" / name).read_bytes() != (ROOT / name).read_bytes():
                raise ValueError("missing/stale offline legal material: " + name)
        run("version", ["--nogui", "--version"], "1.0.0")
        run("help", ["--nogui", "--help"], "Qualitative phase identification from powder X-ray diffraction data")
        if (settings / "IC/qualpf.ini").exists():
            raise ValueError("early CLI exit migrated settings")
        run("build_database", ["--nogui", "--createdb", "--cifdir", str(cif), "--dbout", str(database)],
            "Done: 1 ok, 0 errors.")
        if old.read_bytes() != original:
            raise ValueError("legacy settings were changed")
        if "sentinel=installed-acceptance-fixture" not in (settings / "IC/qualpf.ini").read_text(encoding="utf-8-sig"):
            raise ValueError("legacy settings sentinel did not migrate")
        run("database_search", ["--nogui", "--search", "--composition", "Zn"], "Found 1 card(s):")
        run("native_automatic_startup", ["--auto"], "")
        if old.read_bytes() != original:
            raise ValueError("legacy settings changed after startup")
        report.update(status="PASS", offline_notices=True, legacy_settings_preserved=True)
    except Exception as error:
        report.update(status="FAIL", error=str(error))
        raise
    finally:
        save()
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--qpa", default="windows" if sys.platform == "win32" else "xcb")
    args = parser.parse_args()
    try:
        print("PASS: installed product acceptance; evidence:", check(args.executable, args.prefix, args.work_dir, args.qpa))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print("FAIL:", error)
        raise SystemExit(1)
