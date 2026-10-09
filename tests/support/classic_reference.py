"""Offline replay/differential verifier. Never creates or updates expected outputs.

Exit 77 means reference evidence is missing, NOT equivalence passed. Synthetic
fixtures require --synthetic and can never pass the Legacy evidence gate.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys


def load_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"),
                      parse_constant=lambda x: (_ for _ in ()).throw(ValueError("Non-finite JSON: " + x)))


def optional(value):
    return "0 0" if value is None else f"1 {value:.17g}"


def quote(value):
    # std::quoted wire format is not JSON escaping. No control characters allowed.
    if any(ord(c) < 32 for c in value):
        raise ValueError("Control characters in identity")
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def encode(data):
    operation = data["operation"]
    if operation not in ("score", "residual", "initial", "rescore"):
        raise ValueError("Unknown replay operation")
    if operation in ("score", "residual") and len(data["cards"]) != 1:
        raise ValueError("Single-card operation requires exactly one card")
    low, high = data["range"]
    rows = [f"1 {operation}", f"{low:.17g} {high:.17g}", str(len(data["pattern"]))]
    rows += [f"{x:.17g} {y:.17g}" for x, y in data["pattern"]]
    rows.append(str(len(data["peaks"])))
    for p in data["peaks"]:
        rows.append(f'{p["id"]} {p["two_theta"]:.17g} {p["intensity"]:.17g} ' + " ".join(
            optional(p.get(field)) for field in ("original_intensity", "d_spacing", "delta_d", "fwhm", "integrated_intensity")))
    p = data["parameters"]
    rows.append(" ".join(format(p[k], ".17g") for k in
                ("delta", "weight_position", "weight_intensity", "weight_phases", "minimum_score")) +
                f' {p["maximum_entries"]} {int(p["include_deleted"])} {int(p["filter_strongest"])} {p["strongest_count"]}')
    rows += [format(data.get("residual_scale", 1), ".17g"), str(len(data["cards"]))]
    for card in data["cards"]:
        rows.append(" ".join(quote(card[k]) for k in ("database_id", "card_id", "quality")))
        rows.append(optional(card.get("previous_scale")))
        top = card.get("stored_top")
        rows.append("0" if top is None else "1 " + str(len(top)) + " " + " ".join(format(x, ".17g") for x in top))
        rows.append(str(len(card["reflections"])))
        rows += [f'{r["id"]} {r["two_theta"]:.17g} {r["intensity"]:.17g}' for r in card["reflections"]]
    return "\n".join(rows) + "\n"


def replay(executable, data):
    result = subprocess.run([str(executable)], input=encode(data), text=True, encoding="utf-8",
                            capture_output=True, check=True, timeout=60)
    return json.loads(result.stdout, parse_constant=lambda x: (_ for _ in ()).throw(ValueError(x)))


def compare(expected, actual, tolerances):
    if set(expected) != {"error", "fields", "discrete"} or set(actual) != set(expected):
        raise ValueError("Invalid observation schema")
    if expected["error"] != actual["error"]:
        raise ValueError("Error classification differs")
    for observation in (expected, actual):
        if observation["error"] is not None and type(observation["error"]) is not int:
            raise ValueError("Invalid error classification")
        if not isinstance(observation["discrete"], dict) or any(
                not isinstance(v, list) or any(not isinstance(x, str) for x in v)
                for v in observation["discrete"].values()):
            raise ValueError("Invalid discrete observation")
    if expected["discrete"] != actual["discrete"]:
        different = sorted(k for k in expected["discrete"].keys() | actual["discrete"].keys()
                           if expected["discrete"].get(k) != actual["discrete"].get(k))
        raise ValueError("Discrete decisions, identities or order differ: " + ", ".join(different))
    if expected["fields"].keys() != actual["fields"].keys():
        raise ValueError("Numeric field set differs")
    if expected["error"] is not None and expected["fields"]:
        raise ValueError("Failed observations must not contain success quantities")
    statistics = {}
    failures = []
    for field, values in expected["fields"].items():
        if field not in tolerances:
            raise ValueError("Missing frozen tolerance for " + field)
        t = tolerances[field]
        a, r = t["absolute"], t["relative"]
        if (not all(type(x) in (int, float) and math.isfinite(x) and x >= 0 for x in (a, r))
                or not isinstance(t["unit"], str) or not t["unit"].strip()
                or not isinstance(t["approval_evidence"], str) or not t["approval_evidence"].strip()):
            raise ValueError("Invalid tolerance for " + field)
        other = actual["fields"][field]
        if len(values) != len(other):
            raise ValueError("Numeric field length differs: " + field)
        statistics[field] = {"values": len(values), "max_absolute_error": 0.0, "max_bound_fraction": 0.0}
        for index, (old, new) in enumerate(zip(values, other)):
            if type(old) not in (int, float) or type(new) not in (int, float):
                raise ValueError("Non-numeric value: " + field)
            bound = a + r * abs(old)
            if not all(math.isfinite(x) for x in (old, new, bound)):
                raise ValueError("Non-finite value/bound: " + field)
            error = abs(new-old)
            statistics[field]["max_absolute_error"] = max(statistics[field]["max_absolute_error"], error)
            if bound:
                statistics[field]["max_bound_fraction"] = max(statistics[field]["max_bound_fraction"], error / bound)
            if error > bound:
                failures.append(f"{field}[{index}]: expected {old}, actual {new}, bound {bound}")
    if failures:
        raise ValueError("; ".join(failures))
    return statistics


def checked_path(base, descriptor):
    path = (base / descriptor["path"]).resolve()
    if not re.fullmatch(r"[0-9a-f]{64}", descriptor["sha256"]):
        raise ValueError("Invalid SHA-256")
    if hashlib.sha256(path.read_bytes()).hexdigest() != descriptor["sha256"]:
        raise ValueError("Artifact hash mismatch: " + str(path))
    return path


def artifact(base, descriptor):
    return load_json(checked_path(base, descriptor))


def run_manifest(executable, path, synthetic=False, report=None):
    if not Path(path).exists():
        print("REFERENCE_UNAVAILABLE:", path)
        return 77
    manifest = load_json(path)
    if manifest["schema_version"] != 1:
        raise ValueError("Unknown manifest schema")
    wanted = "hand_derived_synthetic" if synthetic else "legacy_capture"
    if manifest["kind"] != wanted:
        raise ValueError("Synthetic data cannot serve as a Legacy reference")
    cases = manifest.get("cases")
    base = Path(path).resolve().parent
    inherited_evidence = []
    if "source_manifests" in manifest:
        imported = []
        for descriptor in manifest["source_manifests"]:
            child_path = checked_path(base, descriptor)
            child = load_json(child_path)
            if child["kind"] != wanted or child["schema_version"] != 1:
                raise ValueError("Invalid child reference manifest")
            child_evidence = list(child.get("evidence", []))
            child_policy = None
            if "tolerance_policy" in child:
                child_evidence.append(child["tolerance_policy"])
                child_policy = artifact(child_path.parent, child["tolerance_policy"])
            for item in child_evidence:
                inherited_evidence.append(dict(item, path=(Path(descriptor["path"]).parent / item["path"]).as_posix()))
            for original in child["cases"]:
                if child_policy and original.get("expected"):
                    expected = artifact(child_path.parent, original["expected"])
                    for field in expected["fields"]:
                        t = original["tolerances"][field]
                        if [t["absolute"], t["relative"], t["unit"]] != child_policy["rules"][field]:
                            raise ValueError("Tolerance differs from frozen policy: " + field)
                case = dict(original)
                for key in ("input", "expected", "raw"):
                    if key in case:
                        case[key] = dict(case[key], path=(Path(descriptor["path"]).parent / case[key]["path"]).as_posix())
                imported.append(case)
        if cases is not None and cases != imported:
            raise ValueError("Aggregate reference differs from its reviewed source manifests")
        cases = imported
    if not cases:
        raise ValueError("Empty reference collection")
    if any(not isinstance(c.get("id"), str) or not c["id"].strip() for c in cases) or len({c["id"] for c in cases}) != len(cases):
        raise ValueError("Case identities must be nonempty and unique")
    for item in list(manifest.get("evidence", [])) + inherited_evidence:
        checked_path(base, item)
    policy = artifact(base, manifest["tolerance_policy"]) if "tolerance_policy" in manifest else None
    if report is not None:
        protected = {Path(path).resolve(), Path(executable).resolve()}
        descriptors = list(manifest.get("evidence", [])) + inherited_evidence + list(manifest.get("source_manifests", []))
        if "tolerance_policy" in manifest:
            descriptors.append(manifest["tolerance_policy"])
        descriptors += [c[k] for c in cases for k in ("input", "expected", "raw") if c.get(k)]
        protected.update((base / d["path"]).resolve() for d in descriptors)
        if Path(report).resolve() in protected:
            raise ValueError("Report must not overwrite reference evidence or executable")
    outcomes = []
    pending = 0
    failed = 0
    for case in cases:
        if not synthetic and (case["status"] != "reviewed" or case.get("expected") is None):
            pending += 1
            print("UNREVIEWED_REFERENCE:", case["id"])
            outcomes.append({"id": case["id"], "status": "pending"})
            continue
        if not synthetic:
            env = case["environment"]
            for key in ("source_commit", "compiler", "flags", "platform", "real_kind", "thread_configuration", "capture_procedure", "review_evidence"):
                if not isinstance(env.get(key), str) or not env[key].strip():
                    raise ValueError("Missing reference environment: " + key)
            if not re.fullmatch(r"[0-9a-f]{40}", env["source_commit"]):
                raise ValueError("Invalid reference source commit")
            if not case["legacy_tolerances_frozen"]:
                raise ValueError("Legacy tolerances not frozen")
            if case["reference_origin"] not in ("fortran_capture", "legacy_cpp_capture"):
                raise ValueError("Untrusted reference origin")
        data = artifact(base, case["input"])
        expected = artifact(base, case["expected"])
        if case.get("operation", data["operation"]) != data["operation"]:
            raise ValueError("Manifest/input operation mismatch: " + case["id"])
        if not synthetic and data["operation"] in ("score", "initial", "rescore") and case["reference_origin"] != "fortran_capture":
            raise ValueError("FOM verification requires Fortran capture evidence")
        if policy:
            for key in expected["fields"]:
                rule = policy["rules"][key]
                t = case["tolerances"][key]
                if [t["absolute"], t["relative"], t["unit"]] != rule:
                    raise ValueError("Tolerance differs from frozen policy: " + key)
        try:
            actual = replay(executable, data)
            statistics = compare(expected, actual, case["tolerances"])
            outcomes.append({"id": case["id"], "status": "pass", "fields": statistics})
            print("SYNTHETIC_PASS:" if synthetic else "REFERENCE_PASS:", case["id"])
        except (ValueError, subprocess.SubprocessError) as error:
            failed += 1
            outcomes.append({"id": case["id"], "status": "fail", "message": str(error)})
            print("DIFFERENTIAL_FAIL:", case["id"], error)
    # Present but unreviewed/incomplete evidence is a failed gate, never Skip.
    code = 1 if failed or pending else 0
    if report is not None:
        Path(report).write_text(json.dumps({"schema_version": 1, "kind": wanted,
            "manifest_sha256": hashlib.sha256(Path(path).read_bytes()).hexdigest(),
            "runner_sha256": hashlib.sha256(Path(executable).read_bytes()).hexdigest(),
            "passed": len(cases)-pending-failed, "pending": pending, "failed": failed,
            "exit_code": code, "product_switch_authorized": False, "cases": outcomes}, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--synthetic", action="store_true")
    parser.add_argument("--report", type=Path, help="Write observations/statistics, never reference outputs")
    args = parser.parse_args()
    try:
        return run_manifest(args.runner.resolve(), args.manifest, args.synthetic, args.report)
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError) as error:
        print("FAIL:", error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
