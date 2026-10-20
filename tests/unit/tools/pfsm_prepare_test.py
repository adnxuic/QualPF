"""Input adapters and structural truth checks, independent of product output."""
import importlib.util
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('prepare', ROOT/'tools/validation/pfsm_prepare_benchmark.py')
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class Inputs(unittest.TestCase):
    def test_chemical_evidence_is_explicit_and_frozen(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root/'weighing.txt'
            source.write_text('Independently specified sample constituents: silicon and alumina\n')
            output = root/'frozen'
            output.mkdir()
            value = dict(status='supported', kind='allowed_elements', elements=['Si', 'Al', 'O'],
                         basis='Nominal constituent inventory from the weighing record',
                         evidence=[dict(path=source.name, locator='Constituent inventory, line 1')])
            frozen = prepare.freeze_chemical_constraints(value, root, output)
            self.assertEqual(frozen['elements'], ['Al', 'O', 'Si'])
            self.assertEqual(value['elements'], ['Si', 'Al', 'O'])
            self.assertEqual(frozen['evidence'][0]['sha256'], prepare.sha(source))
            self.assertEqual((output/frozen['evidence'][0]['path']).resolve(), source)
            source.write_text('Changed source\n')
            self.assertNotEqual(frozen['evidence'][0]['sha256'], prepare.sha(source))
            for invalid in [dict(value, elements=['Si', 'Si']), dict(value, elements=['Xx']),
                            dict(value, elements=[]), dict(value, evidence=[]),
                            dict(value, evidence=[dict(path=source.name, locator='')]),
                            dict(value, evidence=[dict(path=source.name, locator='Line 1', sha256='0'*64)]),
                            dict(value, basis=''), dict(value, kind='truth_cards')]:
                with self.assertRaises(ValueError):
                    prepare.freeze_chemical_constraints(invalid, root, output)
            missing = prepare.freeze_chemical_constraints(None, root, output)
            self.assertEqual(missing['status'], 'evidence_pending')
            self.assertNotIn('elements', missing)
            partial = dict(status='partial', reason='Only a positive element identification is available')
            self.assertEqual(prepare.freeze_chemical_constraints(partial, root, output)['status'], 'evidence_pending')

    def test_holdout_keeps_repeat_scans_together(self):
        cases = [dict(id='sample_2min', validation_split='holdout', evaluation_family='sample'),
                 dict(id='sample_8min', validation_split='holdout', evaluation_family='sample')]
        prepare.validate_cases(cases)
        cases[1]['validation_split'] = 'development'
        with self.assertRaises(ValueError):
            prepare.validate_cases(cases)

    def test_decimal_preservation(self):
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)/'raw'
            p.write_text('10.000 .025 10.050\n-1.2300 0.0000 8.1200\n')
            rows, _ = prepare.read_rows(p, 'packed_start_step_end')
            self.assertEqual(rows, [['10.000', '-1.2300'], ['10.025', '0.0000'], ['10.050', '8.1200']])
            p.write_text('10.000 .025 10.075\n1 2 3\n')
            with self.assertRaises(ValueError):
                prepare.read_rows(p, 'packed_start_step_end')

    def test_nist_measured_only(self):
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)/'raw'
            text = '_pd_proc_number_of_points 3\nloop_\n_pd_proc_2theta_corrected\n_pd_proc_intensity_total\n_pd_proc_ls_weight\n10 2.00 .25\n11 -1.00 .04\n12 4.00 .01\ndata_calculated\n100 999 999\n'
            p.write_text(text)
            rows, _ = prepare.read_rows(p, 'nist_measured_cif')
            self.assertEqual(rows[1], ['11', '-1.00', '.04'])
            self.assertEqual(len(rows), 3)
            p.write_text(text.replace('points 3', 'points 4'))
            with self.assertRaises(ValueError):
                prepare.read_rows(p, 'nist_measured_cif')

    def test_csv_header_is_not_a_reconstructed_grid(self):
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)/'raw'
            p.write_text('# No. steps = 2\n# Calibrated wavelength = 0.413671\n# Normalization factor = 8.1812\n2.000000, 3.400, .230\n2.000999, -1.00, .120\n2.001999, 2.00, .110\n')
            rows, metadata = prepare.read_rows(p, 'csv_sigma')
            self.assertEqual(rows[1], ['2.000999', '-1.00', '.120'])
            self.assertEqual(len(rows), 3)
            self.assertEqual(metadata['format_diagnostics']['header_steps'], 2)
            self.assertEqual(metadata['format_diagnostics']['tabulated_rows'], 3)
            self.assertFalse(metadata['format_diagnostics']['header_steps_equal_rows'])
            self.assertTrue(metadata['intensity_scaled'])
            self.assertEqual(metadata['format_diagnostics']['normalization_factor'], '8.1812')

    def test_xrdml_units(self):
        text = '''<xrdMeasurements xmlns="urn:test"><usedWavelength><kAlpha1>1.2</kAlpha1><kAlpha2>1.3</kAlpha2><ratioKAlpha2KAlpha1>0.5</ratioKAlpha2KAlpha1></usedWavelength><scan><dataPoints><positions axis="2Theta" unit="deg"><startPosition>10</startPosition><endPosition>11</endPosition></positions><counts unit="counts">10 20 30</counts></dataPoints></scan></xrdMeasurements>'''
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)/'raw'
            p.write_text(text)
            rows, metadata = prepare.read_rows(p, 'xrdml')
            self.assertEqual(rows[1], ['10.5', '20'])
            self.assertEqual(metadata['radiation'][1], dict(wavelength_angstrom=1.3, weight=.5))
            for invalid in [text.replace('unit="counts"', 'unit="cps"'), text.replace('unit="deg"', 'unit="rad"')]:
                p.write_text(invalid)
                with self.assertRaises(ValueError):
                    prepare.read_rows(p, 'xrdml')

    def test_reaction_count_source_contract(self):
        # This checked-in source inventory is independent of fitted results.
        # Do not open the large optional real-data files in the normal suite.
        evidence_path = 'tests/data/pfsm-real/reaction-count-evidence.json'
        evidence = json.loads((ROOT/evidence_path).read_text(encoding='utf-8'))
        recipe = json.loads((ROOT/'tests/data/pfsm-real/recipes.json').read_text(encoding='utf-8'))
        self.assertEqual(evidence['schema'], 'qualpf.pfsm.reaction-count-evidence.v1')
        self.assertFalse(evidence['expected_from_product'])
        pairs = evidence['pairs']
        cases = {c['id']: c for c in recipe['cases'] if c['id'].startswith('Dara_reaction_')}
        self.assertEqual(len(pairs), 20)
        self.assertEqual({p['case_id'] for p in pairs}, set(cases))
        self.assertEqual(len(cases), 20)
        common = evidence['common_original_metadata']
        self.assertEqual(common['counts']['unit'], 'counts')
        self.assertEqual(common['counting_time']['unit'], 'seconds')
        self.assertEqual(common['coordinates']['unit'], 'deg')
        for pair in pairs:
            with self.subTest(case=pair['case_id']):
                raw, exported = pair['xrdml'], pair['exported_xy']
                self.assertEqual(Path(raw['path']).suffix, '.xrdml')
                self.assertEqual(Path(raw['path']).with_suffix('.xy').as_posix(), exported['path'])
                for source in (raw, exported):
                    self.assertRegex(source['sha256'], r'^[0-9a-f]{64}$')
                self.assertEqual(pair['point_count'], 8301)
                self.assertTrue(pair['all_exported_intensities_equal_original_counts'])
                self.assertTrue(pair['all_original_counts_integer'])
                self.assertTrue(pair['all_exported_coordinates_equal_native_grid_rounded_to_five_decimals'])
                case = cases[pair['case_id']]
                # Apply the source contract even to unresolved cases; truth,
                # chemistry and validation split cannot select a noise policy.
                self.assertEqual((case['raw_file'], case['signal_source'], case['format']),
                                 (raw['path'], raw['path'], 'xrdml'))
                self.assertEqual((case['noise'], case['intensity_units']), ('raw_counts', 'counts'))
                self.assertTrue(case['unclipped_signal'])
                self.assertFalse(case.get('intensity_scaled', False))
                retained = {exported['path'], evidence_path, *evidence['retained_evidence_files']}
                self.assertTrue(retained.issubset(case['evidence_files']))
                radiation_source = case['radiation_source'].lower()
                for field in ('xrdml', 'usedwavelength', 'kalpha1', 'kalpha2', 'ratiokalpha2kalpha1'):
                    self.assertIn(field, radiation_source)

    def test_xrdml_native_grid_counts_and_radiation_are_preserved(self):
        # Prescribed decimal endpoints and counts: neither a rounded XY grid
        # nor a count-rate conversion is an admissible substitute.
        text = '''<xrdMeasurements xmlns="urn:test">
<usedWavelength><kAlpha1>1.21</kAlpha1><kAlpha2>1.27</kAlpha2>
<ratioKAlpha2KAlpha1>0.375</ratioKAlpha2KAlpha1></usedWavelength>
<scan><dataPoints><positions axis="2Theta" unit="deg">
<startPosition>9.90343322</startPosition><endPosition>9.94689898</endPosition>
</positions><commonCountingTime unit="seconds">13.770</commonCountingTime>
<counts unit="counts">0 13 1377 27 4131</counts></dataPoints></scan></xrdMeasurements>'''
        expected = [['9.90343322', '0'], ['9.91429966', '13'], ['9.92516610', '1377'],
                    ['9.93603254', '27'], ['9.94689898', '4131']]
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)/'raw.xrdml'
            for seconds in ('13.770', '27.540'):
                with self.subTest(counting_time=seconds):
                    p.write_text(text.replace('13.770', seconds), encoding='utf-8')
                    rows, metadata = prepare.read_rows(p, 'xrdml')
                    self.assertEqual(rows, expected)
                    self.assertEqual(metadata['radiation'], [dict(wavelength_angstrom=1.21, weight=1),
                                                            dict(wavelength_angstrom=1.27, weight=.375)])

    def test_polymorph_setting_and_explicit_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            db = Path(directory)/'test.sq'
            with sqlite3.connect(db) as c:
                c.execute('create table id(id,chemical_formula,spacegroup,name,mineralname)')
                c.executemany('insert into id values(?,?,?,?,?)', [
                    (1,'H3 La O3','P 63/m','correct',None),
                    (2,'La O3','P 63/m','verified hydrogen omission',None),
                    (3,'La O3','P 63/m','unverified omission',None),
                    (4,'H3 La O3','P 63/m m c','wrong polymorph',None),
                    (5,'H3 La O3','P 63/m','wrong cell',None),
                    (6,'H3 La O3','P 63/m','missing cell',None)])
            c.close()
            with sqlite3.connect(str(db)+'.info') as c:
                c.execute('create table info(id,a,b,c,alpha,beta,gamma)')
                c.executemany('insert into info values(?,?,?,?,?,?,?)',
                              [(k, 6.5 if k!=5 else 20, 6.5, 3.8 if k!=6 else None,90,90,120) for k in range(1,7)])
            c.close()
            rule = dict(formulas=['H3 La O3'],spacegroups=['P 63/m'],cell=[6.5,6.5,3.8,90,90,120],
                        relative_cell_tolerance=.03,angle_tolerance_degrees=1,
                        formula_aliases=[dict(id='2',formula='La O3')])
            result = prepare.structural_map(db, {'phase': rule})['phase']['cards']
            self.assertEqual([r['id'] for r in result], ['1','2'])
            self.assertFalse(result[0]['formula_alias'])
            self.assertTrue(result[1]['formula_alias'])
            rule['variants'] = [dict(spacegroups=['P 63/m'],cell=[20,6.5,3.8,90,90,120],setting='explicit test cell')]
            result = prepare.structural_map(db, {'phase': rule})['phase']['cards']
            self.assertEqual([r['id'] for r in result], ['1','2','5'])
            self.assertEqual(result[-1]['matched_setting'], 'explicit test cell')

    def test_v2o5_exchanged_ab_setting_preserves_phase_guards(self):
        recipe = json.loads((ROOT/'tests/data/pfsm-real/recipes.json').read_text(encoding='utf-8'))
        rule = recipe['phase_rules']['V2O5_shcherbinaite']
        setting = 'Pmmn exchanged a/b axes'
        self.assertEqual(rule['formulas'], ['O5 V2'])
        self.assertEqual(rule['relative_cell_tolerance'], .03)
        self.assertEqual(rule['angle_tolerance_degrees'], 1)
        self.assertFalse(rule.get('formula_aliases'))
        with tempfile.TemporaryDirectory() as directory:
            db = Path(directory)/'test.sq'
            cards = [
                (1,'O5 V2','P m m n :2','primary setting',None),
                (2,'O5 V2','P m m n :2','exchanged a/b setting',None),
                (3,'O5 V2','P m n m','existing b/c setting',None),
                (4,'O2.6 V','P m m n :2','V2O5',None),
                (5,'O9.65 V3.52','P m m n :2','V2O5',None),
                (6,'O5 V2','P m n 21','Shcherbinaite',None),
                (7,'O5 V2','P m m n :2','unproved permutation',None),
                (8,'O5 V2','P m m n :2','length outside tolerance',None),
                (9,'O5 V2','P m m n :2','angle outside tolerance',None),
                (10,'O10.04 V4.01','P m m n :2','source formula is not an SQL alias',None),
                (11,'O5 V2','P m m n :2','length inside tolerance',None),
                (12,'O5 V2','P m m n :2','angle inside tolerance',None)]
            with sqlite3.connect(db) as c:
                c.execute('create table id(id,chemical_formula,spacegroup,name,mineralname)')
                c.executemany('insert into id values(?,?,?,?,?)', cards)
            c.close()
            cells = []
            for ident, *_ in cards:
                cell = [3.564,11.512,4.368,90,90,90]
                if ident == 1:
                    cell = [11.512,3.564,4.368,90,90,90]
                elif ident == 3:
                    cell = [11.512,4.368,3.564,90,90,90]
                elif ident == 7:
                    cell = [4.368,3.564,11.512,90,90,90]
                elif ident in (8, 11):
                    cell[1] *= 1.0301 if ident == 8 else 1.0299
                elif ident in (9, 12):
                    cell[5] += 1.001 if ident == 9 else .999
                cells.append([ident]+cell)
            with sqlite3.connect(str(db)+'.info') as c:
                c.execute('create table info(id,a,b,c,alpha,beta,gamma)')
                c.executemany('insert into info values(?,?,?,?,?,?,?)', cells)
            c.close()
            without_setting = dict(rule, variants=[v for v in rule['variants'] if v['setting'] != setting])
            before = prepare.structural_map(db, {'phase': without_setting})['phase']['cards']
            after = prepare.structural_map(db, {'phase': rule})['phase']['cards']
            self.assertEqual([r['id'] for r in before], ['1','3'])
            self.assertEqual([r['id'] for r in after], ['1','2','3','11','12'])
            self.assertTrue(all(not r['formula_alias'] for r in after))
            self.assertEqual([r['id'] for r in after if r['matched_setting'] == setting], ['2','11','12'])


if __name__ == '__main__':
    unittest.main()
