from reference_paths import reference_path, provenance_path
"""Reuse every V1 assertion and try to falsify the bounded phase successor."""
import unittest
import math,struct
from copy import deepcopy
from unittest.mock import patch
import current_phase_reference as current
import current_qualified_reference_test as original
original.current=current

class CurrentPhaseReferenceContract(original.CurrentReferenceContract):
    def test_new_phase_providers_reference_and_decision_are_bound(self):
        successor=current.read(current.SUCCESSOR)
        paths=[reference_path(current.ROOT/name) for name in successor['current_providers']]
        paths.append(reference_path(current.ROOT/successor['decision']['path']))
        paths.append(current.HISTORICAL_PHASE_PROVIDER)
        paths.append(reference_path(current.ROOT/successor['phase_value_successor']['path']))
        original_sha=current.sha_file
        for path in paths:
            with self.subTest(path=str(path)),patch.object(current,'sha_file',
                 side_effect=lambda item:'0'*64 if item==path else original_sha(item)):
                with self.assertRaises(ValueError):current.verify_bundle('scattering')
    def test_old_human_approval_is_not_extended(self):
        successor=current.verify_successor()
        self.assertTrue(successor['historical_qualifications_not_extended'])
        self.assertTrue(successor['complete_reference_assertions_retained'])
        self.assertFalse(successor['new_tolerance'])

    def test_exact_phase_field_projection_cannot_hide_another_value(self):
        successor=current.verify_successor()
        field=current.read(reference_path(current.ROOT/successor['phase_value_successor']['path']))['records'][0]
        records=current.read(reference_path(current.ROOT/'docs/refactoring/phase-16/qualification-records.json'))['records']
        failures=[dict(case=r['case'],field=r['field'],legacy=r['legacy'],cpp=r['cpp_by_compiler']['MSVC']) for r in records]
        changed=next(f for f in failures if (f['case'],f['field'])==(field['case'],field['field']))
        changed['cpp']=struct.unpack('>d',bytes.fromhex(field['expected_bits']))[0]
        raw=dict(raw_failures=failures)
        projected,mapped=current.restore_superseded_field_for_original_checks(raw,'scattering','MSVC',records)
        self.assertEqual(len(mapped),1)
        current.validate_numeric_scope(projected['raw_failures'],records,'MSVC')
        self.assertEqual(struct.pack('>d',changed['cpp']).hex(),field['expected_bits'])
        wrong=deepcopy(raw)
        point=next(f for f in wrong['raw_failures'] if (f['case'],f['field'])==(field['case'],field['field']))
        point['cpp']=math.nextafter(point['cpp'],math.inf)
        with self.assertRaises(ValueError):
            current.restore_superseded_field_for_original_checks(wrong,'scattering','MSVC',records)

if __name__=='__main__':unittest.main()
