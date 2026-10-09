"""Integrity and fail-closed tests for the actual S6 capture bundle and verifier."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
TESTS = Path(__file__).resolve().parents[1]
RUNNER = Path(sys.argv.pop(1)).resolve()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    out = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(out)
    return out


ref = module("reference", TESTS / "support/classic_reference.py")
capture = module("capture", TESTS / "reference_checks/fortran_capture/capture.py")
BUNDLE = TESTS / "data/classic-fortran"


class Phase6Evidence(unittest.TestCase):
    def manifest(self):
        result = ref.load_json(BUNDLE / "manifest.json")
        # Test copies reference the same immutable artifact files.
        for item in result["evidence"] + [result["tolerance_policy"]]:
            item["path"] = str(BUNDLE / item["path"])
        for case in result["cases"]:
            for key in ("input", "expected"):
                case[key]["path"] = str(BUNDLE / case[key]["path"])
        return result

    def run_copy(self, data, directory, report=False):
        path = Path(directory) / "manifest.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        return ref.run_manifest(RUNNER, path, report=Path(directory) / "report.json" if report else None)

    def test_expected_is_directly_traceable_to_legacy_transcript(self):
        plain = capture.records((BUNDLE / "original.txt").read_text())
        traced = capture.records((BUNDLE / "traced.txt").read_text())
        catalog = ref.load_json(BUNDLE / "input-catalog.json")["cases"]
        manifest = ref.load_json(BUNDLE / "manifest.json")
        by_id = {c["id"]: c for c in manifest["cases"]}
        self.assertEqual(len(plain), len(traced))
        self.assertEqual(len(plain), len(catalog))
        self.assertEqual(len(by_id), 26)
        self.assertEqual(sum(c.get("classification") == "legacy_undefined" for c in catalog), 4)
        all_inputs = []
        for item, a, b in zip(catalog, plain, traced):
            input_path = BUNDLE / (item["id"] + (".json" if item.get("classification") == "legacy_undefined" else "-input.json"))
            self.assertEqual(capture.sha(input_path), item["input"]["sha256"])
            all_inputs.append(ref.load_json(input_path))
            if item.get("classification") == "legacy_undefined":
                self.assertNotIn(item["id"], by_id)
                continue
            self.assertEqual(list(map(float, a["SCORE"])), list(map(float, b["SCORE"])))
            self.assertEqual(capture.observation(b), ref.artifact(BUNDLE, by_id[item["id"]]["expected"]))
        self.assertEqual(capture.wire(all_inputs), (BUNDLE / "input.wire").read_text())
        diff = (BUNDLE / "instrumentation.diff").read_text().splitlines()
        self.assertFalse(any(line.startswith("-") and not line.startswith("---") for line in diff))

    def test_present_unreviewed_capture_fails_and_cannot_self_approve(self):
        self.assertEqual(ref.run_manifest(RUNNER, BUNDLE / "captured-manifest.json"), 1)

    def test_tolerance_widening_is_rejected(self):
        m = self.manifest()
        m["cases"][0]["tolerances"]["total"]["absolute"] = 1.0
        with tempfile.TemporaryDirectory() as directory, self.assertRaisesRegex(ValueError, "frozen policy"):
            self.run_copy(m, directory)

    def test_duplicate_identity_and_operation_mismatch_are_rejected(self):
        m = self.manifest()
        m["cases"].append(copy.deepcopy(m["cases"][0]))
        with tempfile.TemporaryDirectory() as directory, self.assertRaisesRegex(ValueError, "unique"):
            self.run_copy(m, directory)
        m = self.manifest()
        m["cases"][0]["operation"] = "residual"
        with tempfile.TemporaryDirectory() as directory, self.assertRaisesRegex(ValueError, "operation mismatch"):
            self.run_copy(m, directory)

    def test_tampered_raw_evidence_is_rejected(self):
        m = self.manifest()
        m["evidence"][0]["sha256"] = "0" * 64
        with tempfile.TemporaryDirectory() as directory, self.assertRaisesRegex(ValueError, "hash mismatch"):
            self.run_copy(m, directory)

    def test_report_cannot_overwrite_reference(self):
        manifest = BUNDLE / "manifest.json"
        for target in (manifest, BUNDLE / "perfect-expected.json", BUNDLE / "original.txt"):
            with self.assertRaisesRegex(ValueError, "must not overwrite"):
                ref.run_manifest(RUNNER, manifest, report=target)

    def test_empty_database_batch_and_malformed_wire(self):
        data = ref.load_json(BUNDLE / "perfect-input.json")
        data["cards"] = []
        for operation in ("initial", "rescore"):
            data["operation"] = operation
            out = ref.replay(RUNNER, data)
            self.assertIsNone(out["error"])
            self.assertEqual(out["discrete"], {"prefilter": [], "retained": [], "evaluated": []})
            self.assertEqual(out["fields"], {})
        import subprocess
        for wire in ("", "garbage", "1 score\n"):
            result = subprocess.run([str(RUNNER)], input=wire, text=True, capture_output=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)

    def test_failure_dominates_pending_and_records_field_diagnostics(self):
        m = self.manifest()
        m["cases"] = [m["cases"][0], dict(m["cases"][1], status="captured")]
        # In-memory faulty runner injection; reference bytes/hashes remain intact.
        original_replay = ref.replay
        def faulty(*args):
            result = original_replay(*args)
            result["fields"]["total"][0] += 0.1
            return result
        ref.replay = faulty
        try:
            with tempfile.TemporaryDirectory() as directory:
                self.assertEqual(self.run_copy(m, directory, report=True), 1)
                report = ref.load_json(Path(directory) / "report.json")
                self.assertEqual((report["failed"], report["pending"]), (1, 1))
                self.assertIn("total[0]", report["cases"][0]["message"])
                self.assertFalse(report["product_switch_authorized"])
        finally:
            ref.replay = original_replay


if __name__ == "__main__":
    unittest.main()
