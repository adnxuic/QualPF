from reference_paths import reference_path, provenance_path
"""Prior reference assertions and exact Cell successor falsification."""
import math,struct,unittest
from copy import deepcopy
from unittest.mock import patch
import current_cell_reference as current
import current_lp_reference_test as lp_tests
import current_phase_reference_test as phase_tests
import current_qualified_reference_test as original
lp_tests.current=current
phase_tests.current=current
original.current=current

class CurrentCellReferenceContract(lp_tests.CurrentLPReferenceContract):
    def test_reflection_successor_decision_is_bound(self):
        decision=current.verify_reflection_successor()
        self.assertEqual(decision['selected_option'],'B')
        binding=current.read(current.CELL_SUCCESSOR)['reflection_boundary_decision']
        path=reference_path(current.ROOT/binding['path']);ordinary=current.sha_file
        with patch.object(current,'sha_file',side_effect=lambda p:'0'*64 if p==path else ordinary(p)):
            with self.assertRaises(ValueError):current.verify_reflection_successor()

    def test_Cell_provider_archive_decision_and_references_are_bound(self):
        successor,_=current.verify_cell_successor()
        paths=[reference_path(current.ROOT/current.CELL_SOURCE),current.CELL_ARCHIVE,
               reference_path(current.ROOT/successor['decision']['path'])]+[
               reference_path(current.ROOT/successor[k]['path']) for k in ('value_successor','normal_cell_reference','historical_scattering_reference','scattering_environment_reference')]
        ordinary=current.sha_file
        for path in paths:
            with self.subTest(path=str(path)),patch.object(current,'sha_file',
                 side_effect=lambda p:'0'*64 if p==path else ordinary(p)):
                with self.assertRaises(ValueError):current.verify_bundle('card')

    def test_exact_Cell_projection_retains_raw_and_rejects_ULP_and_unowned_change(self):
        _,records=current.verify_cell_successor()
        for domain in ('card','scattering'):
            for compiler in ('GNU','MSVC'):
                old,_=current.original_cell_output(domain,compiler);actual=deepcopy(old)
                selected=[r for r in records if (r['domain'],r['compiler'])==(domain,compiler)]
                for record in selected:
                    parent,key=current.point(actual,record['field'])
                    value=struct.unpack('>d',bytes.fromhex(record['expected_bits']))[0]
                    parent[key]=str(value) if isinstance(parent[key],str) else value
                preserved=deepcopy(actual)
                projected,mapped=current.project_cell_output(actual,domain,compiler,old)
                self.assertEqual(projected,old);self.assertEqual(actual,preserved)
                self.assertEqual(len(mapped),len(selected))
                if selected:
                    wrong=deepcopy(actual);parent,key=current.point(wrong,selected[0]['field'])
                    value=math.nextafter(float(parent[key]),math.inf)
                    parent[key]=str(value) if isinstance(parent[key],str) else value
                    with self.assertRaises(ValueError):current.project_cell_output(wrong,domain,compiler,old)
                wrong=deepcopy(actual)
                if domain=='card':wrong['0']['composition']['cell_mass']=math.nextafter(wrong['0']['composition']['cell_mass'],math.inf)
                else:
                    row=next(v for v in wrong['cases'].values() if v['mode']=='SCAT')
                    row['values'][-1]=str(math.nextafter(float(row['values'][-1]),math.inf))
                with self.assertRaises(ValueError):current.project_cell_output(wrong,domain,compiler,old)

    def test_whole_CRT_profile_is_exact_and_indivisible_after_Cell_proof(self):
        old,_=current.original_cell_output('scattering','MSVC');actual=deepcopy(old)
        _,records=current.verify_cell_successor()
        for record in records:
            if record['domain']=='scattering':
                parent,key=current.point(actual,record['field'])
                parent[key]=str(struct.unpack('>d',bytes.fromhex(record['expected_bits']))[0])
        environment=current.verify_scattering_environments()
        for record in environment['records']:
            parent,key=current.point(actual,record['field']);parent[key]=record['guest_value']
        untouched=deepcopy(actual)
        projected,_=current.project_cell_output(actual,'scattering','MSVC',old)
        self.assertEqual(projected,old);self.assertEqual(actual,untouched)
        for record in environment['records']:
            wrong=deepcopy(actual);parent,key=current.point(wrong,record['field']);parent[key]=record['host_value']
            with self.assertRaises(ValueError):current.project_cell_output(wrong,'scattering','MSVC',old)
        for field in ('scattering.cases.75.values.3',next(r['field'] for r in records if r['domain']=='scattering')):
            wrong=deepcopy(actual);parent,key=current.point(wrong,field)
            parent[key]=str(math.nextafter(float(parent[key]),math.inf))
            with self.assertRaises(ValueError):current.project_cell_output(wrong,'scattering','MSVC',old)
        wrong=deepcopy(actual);wrong['cases']['75']['values'][2]='0.05'
        with self.assertRaises(ValueError):current.project_cell_output(wrong,'scattering','MSVC',old)

    def test_CRT_profile_source_and_decision_tampering_is_refused(self):
        decision=current.verify_scattering_environments();ordinary=current.sha_file
        path=reference_path(current.ROOT/current.read(current.CELL_SUCCESSOR)['scattering_environment_reference']['path'])
        for p in [path,*[reference_path(current.ROOT/p) for p in decision['source_fingerprints']]]:
            with self.subTest(path=str(p)),patch.object(current,'sha_file',side_effect=lambda q:'0'*64 if q==p else ordinary(q)):
                with self.assertRaises(ValueError):current.verify_scattering_environments()
        with patch.object(current,'read',side_effect=lambda p:dict(decision,whole_profile_required=False) if p==path else current.json.loads(p.read_text())):
            with self.assertRaises(ValueError):current.verify_scattering_environments()

if __name__=='__main__':unittest.main()
