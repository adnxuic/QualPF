#!/usr/bin/env python3
"""Causal controls for exact full/subset and two-stage recall evidence checks."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest

module = Path(__file__).resolve().parents[3]/'tools/validation/pfsm_candidate_recall_compare.py'
spec = importlib.util.spec_from_file_location('fast_compare', module)
comparator = importlib.util.module_from_spec(spec); spec.loader.exec_module(comparator)


class ComparatorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.left = Path(self.tmp.name)/'left'; self.left.mkdir()
        self.right = Path(self.tmp.name)/'right'; self.right.mkdir()
        self.full = [dict(id=str(i), original_index=i, rank=i+1, Q=float(i), delta_Rwp_net=1/(i+1),
                          parameters=dict(scales=[0.0], shifts=[.125])) for i in range(25)]
        self.a = dict(status='performance_not_acceptance', points=5, candidate_count=25,
                      rounds=[dict(baseline_Q=25., baseline_parameters=dict(scales=[]))])
        self.b = copy.deepcopy(self.a)
        self.evidence = [dict(attempt=0, original_index=i, recalled=True, retained=i<20,
                              protection_reason='') for i in range(25)]
        self.b['rounds'][0]['fast_pfsm'] = dict(mode='enabled', recall_budget=500, fit_budget=20,
            normalization_scope='formal_subset', normalization_maximum=1., attempts=[dict(attempt=0,
                discarded=False, full_fallback=False, recalled_indices=list(range(25)), retained_indices=list(range(20)))])
        for folder in (self.left, self.right):
            (folder/'candidate-order.txt').write_text('\n'.join(str(i) for i in range(25)))
            (folder/'baseline-0.tsv').write_text('independent comparator fixture\n')
        self.write(self.left/'round-0.jsonl', self.full)
        self.fitted = copy.deepcopy(self.full[:20]); self.flush()

    @staticmethod
    def write(path, records):
        path.write_text(''.join(json.dumps(r)+'\n' for r in records))

    def flush(self):
        self.write(self.right/'round-0.jsonl', self.fitted)
        self.write(self.right/'fast-0.jsonl', self.evidence)

    def result(self):
        return comparator.compare(self.a, self.b, self.left, self.right, True)

    def test_valid(self):
        self.assertTrue(self.result()['passed'])

    def test_canonical_and_legacy_aliases(self):
        report = self.b['rounds'][0]
        report['candidate_recall'] = copy.deepcopy(report['fast_pfsm'])
        self.assertTrue(self.result()['passed'])
        del report['fast_pfsm']
        self.assertTrue(self.result()['passed'])

    def test_conflicting_aliases(self):
        report = self.b['rounds'][0]
        report['candidate_recall'] = copy.deepcopy(report['fast_pfsm'])
        report['candidate_recall']['fit_budget'] = 19
        self.assertFalse(self.result()['passed'])

    def test_conflicting_performance_aliases(self):
        self.b['rounds'][0]['performance'] = dict(candidate_recall_correlation_seconds=1., fast_recall_seconds=2.)
        self.assertFalse(self.result()['passed'])

    def test_legacy_entry_preserves_failure_exit(self):
        (self.left/'run.json').write_text(json.dumps(dict(cases=[dict(self.a, id='case')])))
        (self.right/'run.json').write_text(json.dumps(dict(cases=[dict(self.b, id='case')])))
        for script in ('pfsm_candidate_recall_compare.py', 'pfsm_fast_compare.py'):
            output = Path(self.tmp.name)/(script+'.json')
            result = subprocess.run([sys.executable, str(module.with_name(script)),
                '--before', str(self.left), '--after', str(self.right), '--output', str(output)],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertFalse(json.loads(output.read_text())['passed'])

    def test_top5_miss(self):
        self.fitted.pop(2); self.flush()
        self.assertFalse(self.result()['passed'])

    def test_signed_zero(self):
        self.fitted[0]['parameters']['scales'][0] = -0.0; self.flush()
        self.assertFalse(self.result()['passed'])

    def test_non_top5_order(self):
        self.fitted[8], self.fitted[9] = self.fitted[9], self.fitted[8]; self.flush()
        self.assertFalse(self.result()['passed'])

    def test_missing_diagnostics(self):
        self.evidence.pop(); self.flush()
        self.assertFalse(self.result()['passed'])

    def test_selection_mismatch(self):
        self.evidence[22]['retained'] = True; self.flush()
        self.assertFalse(self.result()['passed'])

    def test_recall_miss(self):
        self.b['rounds'][0]['fast_pfsm']['attempts'][0]['recalled_indices'].remove(3)
        self.assertFalse(self.result()['passed'])

    def test_baseline_difference(self):
        self.b['rounds'][0]['baseline_Q'] = 24.999999999999996
        self.assertFalse(self.result()['passed'])

    def test_changed_budget(self):
        self.b['rounds'][0]['fast_pfsm']['fit_budget'] = 40
        self.assertFalse(self.result()['passed'])

    def version_two(self):
        self.b['rounds'][0]['fast_pfsm']['version'] = 2
        for row in self.evidence:
            i = row['original_index']
            row.update(original_score=25-i, residual_score=25-i, gain=25-i,
                       original_rank=i+1, residual_rank=i+1, local_rank=i+1,
                       recall_reason='original_channel' if i%2==0 else 'residual_channel',
                       retention_reason='local_rank_budget' if i<20 else '')
        self.flush()

    def test_version_two(self):
        self.version_two()
        self.assertTrue(self.result()['passed'])

    def test_wrong_approximate_rank(self):
        self.version_two()
        self.evidence[5]['original_rank'] = 7; self.flush()
        self.assertFalse(self.result()['passed'])

    def test_missing_selection_reason(self):
        self.version_two()
        self.evidence[5]['recall_reason'] = ''; self.flush()
        self.assertFalse(self.result()['passed'])

    def v2_full_fallback(self):
        self.version_two()
        fast=self.b['rounds'][0]['fast_pfsm'];fast.update(version=3,algorithm_version='v2',normalization_scope='full_pool')
        attempt=fast['attempts'][0]
        attempt.update(full_fallback=True,fallback_reason='formal_underestimation',retained_indices=list(range(25)))
        attempt['v2']=dict(g_star=6,initial_top20=list(range(20)),review_top20=list(range(20)),
            channel_protection=list(range(5)),review_indices=list(range(20)),unresolved_protection=[],
            proposed_indices=list(range(20)),retained_indices=list(range(25)),review_skipped=False,
            full_fallback=True,fallback_reason='formal_underestimation')
        self.fitted=copy.deepcopy(self.full)
        for row in self.evidence:
            row.update(retained=True,retention_reason='full_fallback',v2=dict(nnls_calls=3,seeds=[0],reviewed=False))
        self.flush()

    def test_v2_full_fallback_keeps_diagnostics(self):
        self.v2_full_fallback();self.assertTrue(self.result()['passed'])

    def test_v2_hidden_formal_evidence_rejected(self):
        self.v2_full_fallback();self.evidence[24]['v2']['formal_gain']=.3;self.flush()
        self.assertFalse(self.result()['passed'])

    def test_v2_overbudget_rejected(self):
        self.v2_full_fallback();self.evidence[3]['v2']['nnls_calls']=16;self.flush()
        self.assertFalse(self.result()['passed'])


class ReplayInputTests(unittest.TestCase):
    def setUp(self):
        input_module=module.with_name('pfsm_fast_replay_inputs.py')
        input_spec=importlib.util.spec_from_file_location('replay_inputs',input_module)
        self.adapter=importlib.util.module_from_spec(input_spec);input_spec.loader.exec_module(self.adapter)
        self.order=['a','b','c']
        self.full=[dict(id=c,original_index=i,rank=i+1) for i,c in enumerate(self.order)]

    def test_identity_difference_ignores_score_values(self):
        following=[dict(self.full[i],rank=n+1,Q=-999) for n,i in enumerate([2,0])]
        self.assertEqual(self.adapter.derive_selected(self.order,[self.full,following]),[[],['b']])

    def test_incomplete_first_round_rejected(self):
        with self.assertRaises(ValueError):self.adapter.derive_selected(self.order,[self.full[:2]])

    def test_missing_two_ids_rejected(self):
        with self.assertRaises(ValueError):self.adapter.derive_selected(self.order,[self.full,self.full[:1]])

    def test_changed_identity_binding_rejected(self):
        bad=copy.deepcopy(self.full);bad[1]['original_index']=0
        with self.assertRaises(ValueError):self.adapter.derive_selected(self.order,[bad])


class WorkComparisonTests(unittest.TestCase):
    def setUp(self):
        work_spec = importlib.util.spec_from_file_location('work_compare', module.with_name('pfsm_work_compare.py'))
        self.work = importlib.util.module_from_spec(work_spec)
        work_spec.loader.exec_module(self.work)
        self.before = dict(cases=[dict(id='analytic', passed=True, actual=dict(value=1., risk=0.),
                                     stderr=json.dumps(dict(nnls_solves=3, paired_samples=6)))])
        self.after = copy.deepcopy(self.before)
        self.after['cases'][0]['stderr'] = json.dumps(dict(nnls_solves=3, paired_samples=2))

    def test_reduced_work_preserves_science(self):
        report = self.work.compare(self.before, self.after)
        self.assertEqual(report['differences'], [])
        self.assertEqual(report['after']['paired_samples'], 2)

    def test_every_scientific_field_is_checked(self):
        self.after['cases'][0]['actual']['risk'] = -0.
        self.assertTrue(self.work.compare(self.before, self.after)['differences'])

    def test_solver_counts_and_failures_are_checked(self):
        self.after['cases'][0]['stderr'] = json.dumps(dict(nnls_solves=2, paired_samples=2))
        self.assertTrue(self.work.compare(self.before, self.after)['differences'])
        self.after = copy.deepcopy(self.before)
        self.after['cases'][0]['passed'] = False
        self.assertTrue(self.work.compare(self.before, self.after)['differences'])

    def test_missing_or_reordered_cases_are_checked(self):
        self.after['cases'].clear()
        self.assertTrue(self.work.compare(self.before, self.after)['differences'])
        self.after = copy.deepcopy(self.before)
        self.after['cases'][0]['id'] = 'other'
        self.assertTrue(self.work.compare(self.before, self.after)['differences'])


class IntegralComparatorTests(unittest.TestCase):
    def setUp(self):
        self.records = []
        # Independent ordered channels: fine 0..63, coarse 0..15, support 84..99,
        # relaxed 0..31. The union is 0..63 plus 84..99, without a final cap.
        for i in range(100):
            v = dict(normalized_fine_gain=100-i, normalized_coarse_gain=100-i,
                     relaxed_gain=100-i, fine_selected=i<64, coarse_selected=i<16,
                     original_selected=i>=84, residual_selected=i>=84, relaxed_selected=i<32,
                     integral_complete=True, relaxed_complete=True, uncertainty=[], relaxed_uncertainty='',
                     coarse_evaluations=65, fine_evaluations=32, relaxed_evaluations=205,
                     candidate_cache_columns=32, released=[], relaxed_null_shifts=[], relaxed_null_scales=[],
                     relaxed_best_shifts=[0.], relaxed_best_scales=[1.])
            self.records.append(dict(original_index=i, original_score=i, residual_score=i,
                                     protection_reason='', integral=v))
        indices = list(range(64))+list(range(84,100))
        self.attempt = dict(backend='bounded_integral_relaxed', switch_reason='candidate_recall_grid_limit',
            full_fallback=False, recalled_indices=indices, retained_indices=indices,
            integral=dict(source_revision=comparator.validate_integral.__globals__['SOURCE'], implementation='baseline',
                fine_bins=2048, coarse_bins=1024, bank_columns=0, fine_budget=64, coarse_budget=16,
                original_budget=16, residual_budget=16, relaxed_budget=32, nnls_limit=205, tail_mass_epsilon=1e-4,
                coarse_evaluations=6500, fine_evaluations=3200, relaxed_evaluations=20500))

    def check_result(self):
        return comparator.validate_integral(self.attempt, self.records, 0)

    def test_baseline_union(self):
        self.assertEqual(self.check_result(), [])

    def test_ties_bypass_v2_cap(self):
        for r in self.records:
            r['integral']['normalized_fine_gain'] = 1.
            r['integral']['fine_selected'] = True
        self.attempt['recalled_indices'] = self.attempt['retained_indices'] = list(range(100))
        self.assertEqual(self.check_result(), [])
        self.attempt['retained_indices'] = list(range(80))
        self.assertTrue(self.check_result())

    def test_fixed_resources_and_matching(self):
        for key in ('coarse_evaluations', 'fine_evaluations', 'relaxed_evaluations', 'candidate_cache_columns'):
            with self.subTest(key=key):
                self.records[0]['integral'][key] += 1000
                self.assertTrue(self.check_result())
                self.records[0]['integral'][key] -= 1000
        self.records[0]['integral']['relaxed_null_shifts'] = [.1]
        self.assertTrue(self.check_result())

    def test_protected_candidate_and_source(self):
        self.records[70]['integral']['uncertainty'] = ['numerical_failure']
        self.records[70]['protection_reason'] = 'numerical_failure'
        self.assertTrue(self.check_result())
        indices = sorted(self.attempt['retained_indices']+[70])
        self.attempt['recalled_indices'] = self.attempt['retained_indices'] = indices
        self.assertEqual(self.check_result(), [])
        self.attempt['integral']['implementation'] = 'A+B'
        self.assertTrue(self.check_result())


if __name__ == '__main__':
    unittest.main()
