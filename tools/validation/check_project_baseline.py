#!/usr/bin/env python3
"""Validate recorded project acceptance independently of mutable Agent task state."""
import argparse
from copy import deepcopy
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "docs/developer/project-baseline.json"
COMPLETE = "CPP_ARCHITECTURE_REFACTOR_COMPLETE"
CRITERIA = {f"A{i:02d}" for i in range(1, 23)}
RECOVERY_FIELDS = ("inventory", "recovery_index", "batch_report", "closure_report", "validation_index")


def identity(value):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{40}", value) is not None


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON field: {key}")
        result[key] = value
    return result


def read_reference(root, name, label, errors, *, as_json=False):
    if (not isinstance(name, str) or not name or any(character in name for character in "\\:\x00\r\n")
            or PureWindowsPath(name).drive or PurePosixPath(name).is_absolute()
            or ".." in PurePosixPath(name).parts):
        errors.append(f"{label}: expected a safe repository-relative file")
        return None
    path = root / name
    if not path.resolve().is_relative_to(root.resolve()) or not path.is_file():
        errors.append(f"{label}: missing/unsafe reference: {name}")
        return None
    if not as_json:
        return path
    try:
        document = json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_object)
        if not isinstance(document, dict):
            errors.append(f"{name}: expected a JSON object")
            return None
        return document
    except (OSError, UnicodeError, ValueError) as error:
        errors.append(f"{name}: {error}")
        return None


def validate(state, gate=None, refinement_receipt=None):
    errors = []
    if not isinstance(state, dict):
        return [f"{BASELINE}: expected an object"]
    if state.get("schema") != "qualpf.project_baseline.v1":
        errors.append("baseline.schema: unsupported project baseline schema")
    sections = ("source_record", "milestone_f", "milestone_a", "repository_consolidation",
                "recovery", "subsequent_refinements")
    for name in sections:
        if not isinstance(state.get(name), dict):
            errors.append(f"baseline.{name}: expected an object")
    if errors:
        return errors
    if not identity(state["source_record"].get("commit")):
        errors.append("baseline.source_record.commit: expected a complete Git identity")
    if state["milestone_f"] != {"outcome": "FORTRAN_MIGRATION_COMPLETE", "preservation": "MILESTONE_F_PRESERVED"}:
        errors.append("baseline.milestone_f: accepted Milestone F is missing")
    accepted = state["milestone_a"]
    if (accepted.get("outcome") != COMPLETE or accepted.get("final_acceptance") != "ACCEPTED"
            or accepted.get("next_action") != "STOP" or accepted.get("blockers") != []
            or accepted.get("PFSM_started") is not False):
        errors.append("baseline.milestone_a: acceptance must be complete, blocker-free and stopped; PFSM remains unstarted")
    candidate = accepted.get("candidate_commit")
    if not identity(candidate):
        errors.append("baseline.milestone_a.candidate_commit: expected a complete Git identity")
    if not isinstance(gate, dict):
        errors.append("baseline.milestone_a: independent gate is required")
    else:
        rows = gate.get("mandatory_criteria")
        if (not isinstance(rows, list) or len(rows) != 22 or not all(isinstance(row, dict) for row in rows)
                or {row.get("id") for row in rows if isinstance(row.get("id"), str)} != CRITERIA
                or any(row.get("mandatory") is not True or row.get("result") != "PASS" for row in rows)):
            errors.append("baseline.milestone_a: all 22 independent mandatory criteria must pass")
        if (gate.get("candidate_commit") != candidate or gate.get("outcome") != COMPLETE
                or gate.get("milestone_A_accepted") is not True
                or gate.get("milestone_A_final_acceptance") != "ACCEPTED"
                or gate.get("milestone_F") != "MILESTONE_F_PRESERVED"
                or gate.get("PFSM_started") is not False or gate.get("next_action") != "STOP"
                or gate.get("blockers") != []):
            errors.append("baseline.milestone_a: independent gate disagrees with the recorded acceptance")
    consolidation = state["repository_consolidation"]
    for field in ("entry_commit", "implementation_commit", "closure_commit", "validation_checkpoint_commit"):
        if not identity(consolidation.get(field)):
            errors.append(f"baseline.repository_consolidation.{field}: expected a complete Git identity")
    if (consolidation.get("objective") != "CLEAN_CPP_PROJECT_BASELINE"
            or consolidation.get("baseline_status") != "CLEAN_CPP_PROJECT_BASELINE"
            or consolidation.get("outcome") != "READY_FOR_MILESTONE_A_ACCEPTANCE"):
        errors.append("baseline.repository_consolidation: original readiness scope changed")
    refinement = state["subsequent_refinements"].get("strict_layer_convergence")
    if not isinstance(refinement, dict):
        errors.append("baseline.subsequent_refinements.strict_layer_convergence: missing recorded refinement")
    elif not isinstance(refinement_receipt, dict):
        errors.append("baseline.strict_layer_convergence: validation receipt is required")
    else:
        if (not identity(refinement.get("implementation_commit")) or not identity(refinement.get("baseline_commit"))
                or refinement.get("implementation_commit") != refinement_receipt.get("implementation_commit")
                or refinement.get("PFSM_started") is not False):
            errors.append("baseline.strict_layer_convergence: receipt/implementation identity mismatch")
        if refinement.get("status") == "ACCEPTED":
            if (refinement.get("outcome") != "STRICT_LAYER_CONVERGENCE_3_PLATFORM_PASS"
                    or refinement_receipt.get("outcome") != refinement["outcome"]
                    or refinement.get("next_action") != "STOP"
                    or refinement_receipt.get("PFSM_started") is not False):
                errors.append("baseline.strict_layer_convergence: accepted refinement disagrees with its closure")
            for field in ("normal_tests", "architecture", "reference_integrity", "zero_fortran"):
                section = refinement_receipt.get(field)
                if not isinstance(section, dict) or section.get("status") != "PASS":
                    errors.append(f"baseline.strict_layer_convergence: closure {field} must pass")
            normal = refinement_receipt.get("normal_tests", {})
            if isinstance(normal, dict) and (normal.get("failed") != 0 or normal.get("skipped") != 0
                                           or normal.get("executed") != normal.get("registered")):
                errors.append("baseline.strict_layer_convergence: normal identities must be fully executed without failures/skips")
        elif (refinement.get("status") != "IMPLEMENTED_WITH_LINUX_VALIDATION_GAP"
              or refinement_receipt.get("status") != refinement["status"]):
            errors.append("baseline.strict_layer_convergence: unsupported or unproved refinement status")
    return errors


def check(root=ROOT):
    root = root.resolve()
    errors = []
    state = read_reference(root, BASELINE, "baseline", errors, as_json=True)
    if not isinstance(state, dict):
        return errors or [f"{BASELINE}: expected an object"]
    accepted = state.get("milestone_a", {})
    accepted = accepted if isinstance(accepted, dict) else {}
    gate = read_reference(root, accepted.get("gate"), "baseline.milestone_a.gate", errors, as_json=True)
    for field in ("review", "matrix"):
        read_reference(root, accepted.get(field), f"baseline.milestone_a.{field}", errors)
    refinements = state.get("subsequent_refinements", {})
    refinement = refinements.get("strict_layer_convergence", {}) if isinstance(refinements, dict) else {}
    refinement = refinement if isinstance(refinement, dict) else {}
    receipt = read_reference(root, refinement.get("validation_receipt"), "baseline.strict_layer_convergence.validation_receipt", errors, as_json=True)
    for field in ("report", "windows_validation_receipt", "linux_closure_report"):
        if field in refinement:
            read_reference(root, refinement[field], f"baseline.strict_layer_convergence.{field}", errors)
    errors.extend(validate(state, gate, receipt))
    recovery = state.get("recovery", {})
    recovery = recovery if isinstance(recovery, dict) else {}
    documents = {}
    for field in RECOVERY_FIELDS:
        documents[field] = read_reference(root, recovery.get(field), f"baseline.recovery.{field}", errors,
                                          as_json=field in ("inventory", "recovery_index", "validation_index"))
    consolidation = state.get("repository_consolidation", {})
    consolidation = consolidation if isinstance(consolidation, dict) else {}
    for field in ("inventory", "recovery_index"):
        document = documents[field]
        if isinstance(document, dict) and any(document.get(key) != consolidation.get(key) for key in ("entry_commit", "protection_tag")):
            errors.append(f"baseline.recovery.{field}: protected snapshot identity mismatch")
    document = documents["validation_index"]
    if isinstance(document, dict) and (document.get("implementation_commit") != consolidation.get("implementation_commit")
                                      or document.get("full_suite_checkpoint") != consolidation.get("validation_checkpoint_commit")
                                      or document.get("outcome") != consolidation.get("outcome")):
        errors.append("baseline.recovery.validation_index: readiness implementation/checkpoint mismatch")
    return errors


def self_test():
    state = json.loads((ROOT / BASELINE).read_bytes())
    gate = json.loads((ROOT / state["milestone_a"]["gate"]).read_bytes())
    refinement = state["subsequent_refinements"]["strict_layer_convergence"]
    receipt = json.loads((ROOT / refinement["validation_receipt"]).read_bytes())

    class Controls(unittest.TestCase):
        def test_acceptance(self):
            self.assertEqual(validate(state, gate, receipt), [])
            for field, value in (("PFSM_started", True), ("outcome", "IN_PROGRESS"),
                                 ("final_acceptance", "PENDING"), ("next_action", "PFSM"),
                                 ("candidate_commit", "a" * 40), ("blockers", ["unresolved"])):
                changed = deepcopy(state)
                changed["milestone_a"][field] = value
                with self.subTest(field=field):
                    self.assertTrue(validate(changed, gate, receipt))
            self.assertTrue(validate(state, None, receipt))
            changed = deepcopy(state)
            changed["milestone_f"]["outcome"] = "PENDING"
            self.assertTrue(validate(changed, gate, receipt))

        def test_all_mandatory_rows(self):
            for index in range(22):
                for result in ("FAIL", "NOT_VERIFIED"):
                    changed = deepcopy(gate)
                    changed["mandatory_criteria"][index]["result"] = result
                    with self.subTest(index=index, result=result):
                        self.assertTrue(validate(state, changed, receipt))
            for rows in (None, [], [1] * 22, gate["mandatory_criteria"][:-1], [gate["mandatory_criteria"][0]] * 22):
                self.assertTrue(validate(state, dict(gate, mandatory_criteria=rows), receipt))
            for field, value in (("candidate_commit", "b" * 40), ("milestone_A_accepted", False),
                                 ("milestone_A_final_acceptance", "PENDING"), ("milestone_F", "PENDING"),
                                 ("PFSM_started", True), ("next_action", "PFSM"), ("blockers", ["failure"])):
                self.assertTrue(validate(state, dict(gate, **{field: value}), receipt))

        def test_shapes_and_refinement(self):
            for value in (None, [], True, "baseline"):
                self.assertTrue(validate(value))
            for field in state:
                changed = deepcopy(state)
                changed[field] = None
                self.assertTrue(validate(changed, gate, receipt))
            self.assertTrue(validate(state, gate, dict(receipt, implementation_commit="b" * 40)))
            self.assertTrue(validate(state, gate, dict(receipt, outcome="NOT_VERIFIED")))
            changed = deepcopy(receipt)
            changed["normal_tests"]["status"] = "FAIL"
            self.assertTrue(validate(state, gate, changed))

        def test_files_and_recovery(self):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                names = {BASELINE, *[state["milestone_a"][key] for key in ("gate", "review", "matrix")],
                         *state["recovery"].values(),
                         *[refinement[key] for key in ("report", "validation_receipt", "windows_validation_receipt", "linux_closure_report") if key in refinement]}
                for name in names:
                    target = root / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(ROOT / name, target)
                self.assertEqual(check(root), [])
                gate_path = root / state["milestone_a"]["gate"]
                content = gate_path.read_bytes()
                gate_path.unlink()
                self.assertTrue(check(root))
                gate_path.write_bytes(b"{invalid")
                self.assertTrue(check(root))
                gate_path.write_bytes(b'{"outcome":"FAIL","outcome":"PASS"}')
                self.assertTrue(check(root))
                gate_path.write_bytes(content)
                path = root / state["recovery"]["recovery_index"]
                document = json.loads(path.read_bytes())
                document["entry_commit"] = "a" * 40
                path.write_text(json.dumps(document), encoding="utf-8")
                self.assertTrue(check(root))
                path.write_text("null", encoding="utf-8")
                self.assertTrue(check(root))
                for unsafe in ("../outside.json", "/outside.json", "C:/outside.json", "folder\\outside.json"):
                    errors = []
                    self.assertIsNone(read_reference(root, unsafe, "control", errors))
                    self.assertTrue(errors)

    result = unittest.TextTestRunner(verbosity=0).run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls))
    return result.wasSuccessful()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args()
    if args.self_test and not self_test():
        return 1
    errors = check(args.root)
    for error in errors:
        print("FAIL:", error)
    if not errors:
        print("PASS: recorded project baseline; historical authority is snapshot-bound")
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
