"""Exercise the acceptance gate, including failures that must never count as passes."""
import math
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.dont_write_bytecode = True
from preprocessing_compare import compare, decode
import preprocessing_source_snapshot as snapshots


class AcceptanceGate(unittest.TestCase):
    def test_nonfinite_reference_is_rejected(self):
        for value in ("nan", "inf", "-inf"):
            with self.assertRaises(ValueError):
                decode("1 " + value, 0)

    def test_truncation_and_trailing_data_are_rejected(self):
        for value in ("2 1", "1 1 2", "1.5 1"):
            with self.assertRaises(ValueError):
                decode(value, 0)

    def test_discrete_count_cannot_be_softened(self):
        result = compare(decode("1 1", 0), decode("2 1 1", 0), {"signal": [100, 100]})
        self.assertEqual(result["status"], "failed")

    def test_field_threshold_is_enforced(self):
        ref = decode("2 1 2", 0)
        policy = {"signal": [1e-5, 1e-5]}
        self.assertEqual(compare(decode("2 1.00001 2.00002", 0), ref, policy)["status"], "passed")
        self.assertEqual(compare(decode("2 1.001 2", 0), ref, policy)["status"], "failed")
        actual = {"discrete": ref["discrete"], "fields": {"signal": [math.nan, 2]}}
        self.assertEqual(compare(actual, ref, policy)["status"], "failed")

    def test_export_whitespace_is_significant(self):
        self.assertEqual(compare(decode(" x\r\n", 3), decode(" x\n", 3), {})["status"], "passed")
        self.assertEqual(compare(decode("x\n", 3), decode(" x\n", 3), {})["status"], "failed")


class SourceSnapshotProvenance(unittest.TestCase):
    def test_original_payloads_match_both_frozen_manifests(self):
        import json
        for source, expected in snapshots.SOURCE_SNAPSHOTS.items():
            manifest = json.loads((source.parent / "manifest.json").read_bytes())
            self.assertEqual(snapshots.provenance_sha(source), expected)
            self.assertEqual(expected, manifest["provenance"]["wrapper_sha256"])

    def test_corruption_missing_payload_and_ambiguous_source_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "capture-wrapper.f90"
            snapshot = source.with_suffix(".f90.txt")
            payload = b"original captured source\r\n"
            expected = hashlib.sha256(payload).hexdigest()
            snapshot.write_bytes(payload)
            self.assertEqual(snapshots.snapshot_digest(source, snapshot, expected), expected)
            snapshot.write_bytes(payload + b"changed")
            with self.assertRaises(ValueError):
                snapshots.snapshot_digest(source, snapshot, expected)
            snapshot.unlink()
            with self.assertRaises(FileNotFoundError):
                snapshots.snapshot_digest(source, snapshot, expected)
            snapshot.write_bytes(payload)
            source.write_bytes(payload)
            with self.assertRaises(ValueError):
                snapshots.snapshot_digest(source, snapshot, expected)

    def test_unapproved_source_fails_and_ordinary_hash_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture-wrapper.f90"
            path.write_bytes(b"unapproved")
            with self.assertRaises(ValueError):
                snapshots.provenance_sha(path)
            ordinary = path.with_name("input.wire")
            ordinary.write_bytes(b"1 2 3\n")
            self.assertEqual(snapshots.provenance_sha(ordinary), snapshots.original_sha(ordinary))

    def test_original_gate_return_and_failure_restore_hash_function(self):
        previous = snapshots.original.sha
        with patch.object(snapshots.original, "main", return_value=7) as gate:
            self.assertEqual(snapshots.main(), 7)
            gate.assert_called_once_with()
        self.assertIs(snapshots.original.sha, previous)
        with patch.object(snapshots.original, "main", side_effect=ValueError("original rejection")):
            with self.assertRaisesRegex(ValueError, "original rejection"):
                snapshots.main()
        self.assertIs(snapshots.original.sha, previous)


if __name__ == "__main__":
    unittest.main()
