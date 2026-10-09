"""Exercise real CLI search success/empty/SQL-failure and existing pattern paths."""
import argparse
from contextlib import closing
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sqlite3
import subprocess
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def exercise(executable, source, output):
    report = {"status": "FAIL", "executable_sha256": digest(executable), "cases": []}
    environment = os.environ.copy()
    environment.update(QT_QPA_PLATFORM="offscreen", MKL_NUM_THREADS="1",
                       QUALPF_SEARCH_BACKEND="cpp", QUALPF_PREPROCESSING_BACKEND="cpp")
    output.parent.mkdir(parents=True, exist_ok=True)

    def run(name, settings, arguments):
        command = [str(executable), "--nogui", "--settings-dir", str(settings), *arguments]
        result = subprocess.run(command, env=environment, capture_output=True, timeout=30)
        record = {"case": name, "command": command, "exit_code": result.returncode,
                  "stdout": result.stdout.decode("utf-8", errors="replace"),
                  "stderr": result.stderr.decode("utf-8", errors="replace"),
                  "stdout_sha256": hashlib.sha256(result.stdout).hexdigest(),
                  "stderr_sha256": hashlib.sha256(result.stderr).hexdigest()}
        report["cases"].append(record)
        return record

    def settings(directory, database):
        ini = directory / "settings/IC/qualpf.ini"
        ini.parent.mkdir(parents=True)
        ini.write_text("[databases]\ncount=1\n0\\inUse=true\n0\\name=cod_inorg\n0\\path="
                       + database.as_posix() + "\ndefaultDir=" + database.parent.as_posix()
                       + "\n[maintenance]\nsettingsMigrationV1=true\n", encoding="utf-8")
        return ini.parent.parent

    def successful(record, count):
        check(record["exit_code"] == 0, record)
        check("Search failed:" not in record["stderr"] and "Error:" not in record["stderr"], record)
        check(f"Found {count} card(s):" in record["stdout"], record)
        check("Search failed:" not in record["stdout"], record)

    def failed(record, diagnostic):
        check(record["exit_code"] != 0 and diagnostic in record["stderr"], record)
        check("Found " not in record["stdout"], record)

    try:
        # Only this newly created directory is removed. Actual observations remain
        # in the output report even when an assertion fails.
        with tempfile.TemporaryDirectory(prefix="cli-search-", dir=output.parent) as temporary:
            base = Path(temporary).resolve()
            valid = base / "valid"
            (valid / "cif").mkdir(parents=True)
            (valid / "database").mkdir()
            fixture = source / "tests/data/cif/cod-2300112.cif"
            shutil.copyfile(fixture, valid / "cif/cod-2300112.cif")
            report["cif_input_sha256"] = digest(fixture)
            database = valid / "database/cards"
            good_settings = settings(valid, database)
            created = run("create_valid_database", good_settings,
                          ["--createdb", "--cifdir", str(valid / "cif"), "--dbout", str(database)])
            check(created["exit_code"] == 0 and "Done: 1 ok, 0 errors." in created["stdout"], created)
            for suffix in (".sq", ".sq.info", ".sq.infostat", ".sq.search"):
                check(Path(str(database) + suffix).is_file(), "Missing created database role")

            positive = run("metadata_positive", good_settings, ["--search", "--composition", "Zn"])
            successful(positive, 1)
            check(re.search(r"\[0\]\s+zinc_oxide\s+\|\s+O Zn\s+\|\s+P 63 m c", positive["stdout"]), positive)
            successful(run("metadata_empty", good_settings, ["--search", "--composition", "Xe"]), 0)

            broken = base / "broken"
            (broken / "database").mkdir(parents=True)
            bad_database = broken / "database/cards"
            for suffix in (".sq", ".sq.info", ".sq.infostat", ".sq.search"):
                with closing(sqlite3.connect(str(bad_database) + suffix)) as connection:
                    connection.execute("CREATE TABLE placeholder(value INTEGER)")
                    if suffix == ".sq":
                        connection.execute("CREATE TABLE infodb(ncard INTEGER, type TEXT)")
                        connection.execute("INSERT INTO infodb VALUES(0, 'cod_inorg')")
                        try:
                            connection.execute("SELECT id FROM chemical")
                        except sqlite3.OperationalError as error:
                            report["broken_schema_control"] = str(error)
                        else:
                            raise AssertionError("Broken recipe must lack its candidate table")
                    connection.commit()
            bad_settings = settings(broken, bad_database)
            # The same four-file recipe as original Milestone A finding A-01.
            failure = run("metadata_query_failure", bad_settings, ["--search", "--composition", "Zn"])
            failed(failure, "Search failed:")
            check(re.search(r"Search failed: \S.+", failure["stderr"]), failure)

            # Copy existing finite input samples, without generating expected
            # numerical values or changing the approved preprocessing fixture.
            pool = source / "tests/data/current-preprocessing-pool.qxp"
            data = json.loads(pool.read_bytes())["diffraction_data"]
            check(len(data["x"]) == len(data["original_observed"]), "Aligned finite input columns required")
            report["pattern_input_source_sha256"] = digest(pool)
            pattern = base / "finite-pool.xy"
            pattern.write_text("".join(f"{x:.17g} {y:.17g}\n" for x, y in
                                       zip(data["x"], data["original_observed"])), encoding="utf-8")
            report["pattern_input_sha256"] = digest(pattern)
            matched = run("pattern_match_success", good_settings, ["--search", str(pattern)])
            successful(matched, 0)
            check("Searching database..." in matched["stdout"], matched)
            failed(run("pattern_match_query_failure", bad_settings, ["--search", str(pattern)]), "Search failed:")

            flat = base / "flat.xy"
            flat.write_text("".join(f"{10+i/100:.17g} 4\n" for i in range(601)), encoding="utf-8")
            failed(run("pattern_no_peaks", good_settings, ["--search", str(flat)]), "Error: no peaks found.")
            failed(run("pattern_read_failure", good_settings, ["--search", str(base / "absent.xy")]), "Cannot open pattern:")
            report["status"] = "PASS"
    except Exception as error:
        report["failure"] = str(error)
        raise
    finally:
        output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("PASS: actual CLI positive/empty/SQL failure; pattern match success/query/no-peak/read failures")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    exercise(args.executable.resolve(), args.source.resolve(), args.output.resolve())
