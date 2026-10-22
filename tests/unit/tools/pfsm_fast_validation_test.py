#!/usr/bin/env python3
"""Scheduling and reuse controls; no product-generated numeric expectations."""
import copy
import json
import math
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[3]/'tools/validation'))
from pfsm_fast_validation import CompletedRuns, check_group, sha
import pfsm_candidate_recall_validation as v2
import pfsm_performance as performance


def integral_performance_fixture():
    # Explicit comparison inputs, not product-generated numerical expectations.
    attempt = dict(backend='bounded_integral_relaxed', retained_indices=[1, 4],
                   integral=dict(shared_seconds=.1, integral_seconds=.2, relaxed_seconds=.3,
                                 fine_baseline_q=2.0, fine_denominator=4.0,
                                 relaxed_evaluations=12, peak_buffer_bytes=1024))
    recall = dict(mode='enabled', attempts=[copy.deepcopy(attempt), copy.deepcopy(attempt)])
    return dict(points=3, candidate_count=5, candidate_order_sha256='fixed',
                rounds=[dict(baseline_Q=2.0, candidate_recall=copy.deepcopy(recall)) for _ in range(2)])


class PerformanceComparisonTests(unittest.TestCase):
    def test_integral_timings_are_ignored_without_mutating_reports(self):
        before = integral_performance_fixture()
        for round_index in range(2):
            for attempt_index in range(2):
                for field in ('shared_seconds', 'integral_seconds', 'relaxed_seconds'):
                    for missing in (False, True):
                        with self.subTest(round=round_index, attempt=attempt_index, field=field, missing=missing):
                            after = copy.deepcopy(before)
                            integral = after['rounds'][round_index]['candidate_recall']['attempts'][attempt_index]['integral']
                            if missing:
                                integral.pop(field)
                            else:
                                integral[field] += 1
                            original = json.dumps([before, after], sort_keys=True)
                            self.assertEqual(performance.scientific_equal(before, after), [])
                            self.assertEqual(json.dumps([before, after], sort_keys=True), original)

    def test_integral_evidence_differences_remain_exact_failures(self):
        before = integral_performance_fixture()
        for field, value in (('fine_baseline_q', math.nextafter(2.0, math.inf)),
                             ('fine_denominator', math.nextafter(4.0, math.inf)),
                             ('relaxed_evaluations', 13), ('peak_buffer_bytes', 2048),
                             ('future_seconds', 1.0)):
            with self.subTest(field=field):
                after = copy.deepcopy(before)
                integral = after['rounds'][1]['candidate_recall']['attempts'][1]['integral']
                integral['shared_seconds'] += 1
                integral[field] = value
                self.assertEqual(performance.scientific_equal(before, after), ['round 1: candidate_recall'])

    def test_recall_selection_and_other_paths_remain_checked(self):
        before = integral_performance_fixture()
        for field, value in (('retained_indices', [1, 3]), ('backend', 'v2'), ('shared_seconds', 1.0)):
            with self.subTest(field=field):
                after = copy.deepcopy(before)
                after['rounds'][0]['candidate_recall']['attempts'][0][field] = value
                self.assertEqual(performance.scientific_equal(before, after), ['round 0: candidate_recall'])
        for recall in (None, dict(mode='off'), dict(mode='enabled', attempts=[dict(backend='v2')])):
            with self.subTest(recall=recall):
                before['rounds'][0]['candidate_recall'] = recall
                self.assertEqual(performance.scientific_equal(before, copy.deepcopy(before)), [])
        after = copy.deepcopy(before)
        after['rounds'][0]['baseline_Q'] = math.nextafter(2.0, math.inf)
        self.assertEqual(performance.scientific_equal(before, after), ['round 0: baseline_Q'])


class PerformanceScheduleTests(unittest.TestCase):
    def run_schedule(self, arguments, mismatch=False, integral_timing=False):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        executable = root / 'benchmark.exe'
        executable.write_bytes(b'independent scheduling fixture')
        manifest = root / 'manifest.json'
        manifest.write_text(json.dumps(dict(cases=[dict(id='sample', status='ready')])))
        output = root / 'comparison'
        launches = []

        def run(command, **options):
            launches.append(command)
            directory = Path(command[command.index('--output') + 1])
            directory.mkdir()
            case_directory = directory / 'sample'
            case_directory.mkdir()
            after = directory.name.endswith('-after')
            case = dict(status='performance_not_acceptance', points=1, candidate_count=1,
                        candidate_order_sha256='fixed', end_to_end_seconds=1.0,
                        rounds=[dict(operation_to_result_seconds=1.0, peak_process_resident_bytes=1024)])
            if integral_timing:
                recall = integral_performance_fixture()['rounds'][0]['candidate_recall']
                if after:
                    for attempt in recall['attempts']:
                        for field in ('shared_seconds', 'integral_seconds', 'relaxed_seconds'):
                            attempt['integral'][field] += 1
                case['rounds'][0]['candidate_recall'] = recall
            (directory / 'run.json').write_text(json.dumps(dict(cases=[case])))
            for name in ('candidate-order.txt', 'round-0.jsonl', 'baseline-0.tsv'):
                content = 'independent comparison fixture'
                if mismatch and after and name == 'round-0.jsonl':
                    content = 'changed comparison fixture'
                (case_directory / name).write_text(content)
            return SimpleNamespace(returncode=0)

        command = ['pfsm_performance.py', '--before', str(executable), '--after', str(executable),
                   '--database', str(root / 'database'), '--manifest', str(manifest),
                   '--output', str(output), '--case', 'sample', '--mode', 'cache',
                   '--threads', '12', '--repetitions', '1', *arguments]
        with patch.object(sys, 'argv', command), patch.object(performance.subprocess, 'run', side_effect=run):
            if mismatch:
                with self.assertRaisesRegex(RuntimeError, 'Numerical mismatch'):
                    performance.main()
            else:
                performance.main()
        return launches, json.loads((output / 'comparison.json').read_text())

    def test_single_pair_without_warmup_launches_only_twice(self):
        launches, report = self.run_schedule(['--no-warmup'])
        self.assertEqual(len(launches), 2)
        self.assertEqual([(row['iteration'], row['variant']) for row in report['measurements']],
                         [(0, 'before'), (0, 'after')])
        self.assertEqual(report['warmup_pairs'], 0)
        self.assertEqual(report['repetitions'], 1)
        self.assertTrue(report['comparisons'][0]['scientific_identical'])

    def test_default_still_runs_one_warmup_pair(self):
        launches, report = self.run_schedule([])
        self.assertEqual(len(launches), 4)
        self.assertEqual([row['iteration'] for row in report['measurements']], ['warmup', 'warmup', 0, 0])
        self.assertEqual(report['warmup_pairs'], 1)

    def test_full_library_preserves_explicit_single_pair(self):
        launches, report = self.run_schedule(['--no-warmup', '--full-library'])
        self.assertEqual(len(launches), 2)
        self.assertTrue(all(command[-2:] == ['--chemistry', 'none'] for command in launches))
        self.assertEqual(report['repetitions'], 1)

    def test_no_warmup_keeps_scientific_mismatch_failure(self):
        launches, report = self.run_schedule(['--no-warmup'], mismatch=True)
        self.assertEqual(len(launches), 2)
        self.assertFalse(report['comparisons'][0]['scientific_identical'])
        self.assertEqual(report['comparisons'][0]['differences'], ['round-0.jsonl'])

    def test_integral_timing_changes_allow_performance_report_completion(self):
        launches, report = self.run_schedule(['--no-warmup', '--candidate-recall', 'enabled'], integral_timing=True)
        self.assertEqual(len(launches), 2)
        self.assertTrue(report['comparisons'][0]['scientific_identical'])
        self.assertEqual(report['comparisons'][0]['differences'], [])
        self.assertEqual(len(report['groups'][0]['rounds']), 1)


class WorkflowTests(unittest.TestCase):
    def check_modes(self, failures):
        launched, comparisons = [], []
        def run(mode):
            launched.append(mode)
            return mode
        def compare(a, b, kind):
            comparisons.append((a, b, kind))
            return kind not in failures
        return check_group(run, compare, 'shadow-enabled'), launched, comparisons

    def test_success_does_not_launch_baseline_or_off(self):
        result, launched, _ = self.check_modes(set())
        self.assertTrue(result['correct'])
        self.assertEqual(launched, ['shadow', 'enabled'])

    def test_screening_failure_still_runs_enabled_and_diagnoses(self):
        result, launched, _ = self.check_modes({'shadow-screening'})
        self.assertFalse(result['correct'])
        self.assertEqual(launched, ['shadow', 'enabled', 'baseline', 'off'])
        self.assertTrue(result['baseline_off_equal'])

    def test_formal_failure_diagnoses_same_group(self):
        result, launched, comparisons = self.check_modes({'enabled'})
        self.assertFalse(result['correct'])
        self.assertEqual(launched, ['shadow', 'enabled', 'baseline', 'off'])
        self.assertIn(('off', 'enabled', 'enabled'), comparisons)


class ReuseTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.exe = self.root/'benchmark.exe'; self.exe.write_bytes(b'independent executable binding fixture')
        self.output = self.root/'completed'; self.output.mkdir()
        (self.output/'run.json').write_text(json.dumps({'executable_sha256': sha(self.exe), 'cases': []}))
        self.command = [str(self.exe), '--output', str(self.output), '--case', 'sample', '--threads', '12']
        self.binding = dict(baseline_binding='baseline', fast_binding='fast', manifest_sha256='manifest')
        self.previous = dict(self.binding, measurements=[dict(command=self.command, exit_code=1, label='failed')])
        self.save()

    def save(self):
        (self.root/'validation.json').write_text(json.dumps(self.previous))

    def find(self, command=None):
        return CompletedRuns([self.root], self.binding).find(command or self.command, self.exe)

    def test_completed_failure_reused_without_overwriting(self):
        before = (self.root/'validation.json').read_bytes()
        command = self.command.copy(); command[2] = str(self.root/'new')
        directory, row = self.find(command)
        self.assertEqual(directory, self.output)
        self.assertEqual(row['exit_code'], 1)
        self.assertEqual(row['evidence_sha256']['run.json'], sha(self.output/'run.json'))
        self.assertEqual((self.root/'validation.json').read_bytes(), before)

    def test_interrupted_process_not_reused(self):
        self.previous['measurements'][0].pop('exit_code'); self.save()
        self.assertIsNone(self.find())

    def test_settings_mismatch_not_reused(self):
        command = self.command.copy(); command[-1] = '4'
        self.assertIsNone(self.find(command))

    def test_changed_frozen_binding_rejected(self):
        self.previous['fast_binding'] = 'changed'; self.save()
        with self.assertRaisesRegex(RuntimeError, 'Reuse binding mismatch'):
            self.find()

    def test_changed_executable_rejected(self):
        self.exe.write_bytes(b'changed executable')
        with self.assertRaisesRegex(RuntimeError, 'Reuse executable mismatch'):
            self.find()


class V2SerialTests(unittest.TestCase):
    def test_command_preserves_scientific_inputs(self):
        old=['old.exe','--manifest','frozen.json','--output','old','--case','sample','--threads','12','--fast-pfsm','shadow','--template-cache']
        command=v2.enabled_command(old,Path('v2.exe'),Path('new'))
        self.assertEqual(old[old.index('--fast-pfsm')+1],'shadow')
        self.assertEqual(command[command.index('--pfsm-candidate-recall')+1],'enabled')
        self.assertEqual(command[-2:],['--pfsm-candidate-recall-version','v2'])
        self.assertEqual(command[command.index('--manifest')+1],'frozen.json')

    def test_current_command_collapses_both_aliases(self):
        old = ['old.exe','--output','old','--fast-pfsm','shadow','--pfsm-candidate-recall','shadow',
               '--fast-pfsm-version=v2','--pfsm-candidate-recall-version','v2']
        command = v2.enabled_command(old, Path('new.exe'), Path('new'))
        self.assertEqual(command.count('--pfsm-candidate-recall'), 1)
        self.assertEqual(command.count('--pfsm-candidate-recall-version'), 1)
        self.assertNotIn('--fast-pfsm', command)
        self.assertEqual(command[-4:], ['--pfsm-candidate-recall','enabled','--pfsm-candidate-recall-version','v2'])

    def test_receipt_reuses_completed_failure_and_rejects_changed_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);output=root/'run';receipt=root/'receipt.json'
            script='from pathlib import Path; p=Path('+repr(str(output))+'); p.mkdir(); (p/"run.json").write_text("{}"); raise SystemExit(2)'
            command=[sys.executable,'-c',script]
            # Host telemetry is not part of receipt/reuse behavior; keep the unit fixture stdlib-only.
            telemetry=SimpleNamespace(cpu_percent=lambda interval:0,
                Process=lambda pid:SimpleNamespace(create_time=lambda:0))
            with patch.object(v2,'active_workloads',return_value=[]),patch.dict(sys.modules,psutil=telemetry):
                first,reused=v2.execute(command,output,receipt,{})
            self.assertFalse(reused);self.assertEqual(first['exit_code'],2)
            with patch.object(v2.subprocess,'Popen',side_effect=AssertionError('Duplicate process')):
                second,reused=v2.execute(command,output,receipt,{})
            self.assertTrue(reused);self.assertEqual(first['pid'],second['pid'])
            (output/'run.json').write_text('{"changed":true}')
            with self.assertRaisesRegex(RuntimeError,'evidence changed'):v2.execute(command,output,receipt,{})

    def test_running_workload_prevents_launch(self):
        with tempfile.TemporaryDirectory() as tmp,patch.object(v2,'active_workloads',return_value=[dict(pid=17)]):
            with self.assertRaisesRegex(RuntimeError,'Existing PFSM workload'):
                v2.execute(['not-launched'],Path(tmp)/'out',Path(tmp)/'receipt.json',{})

    def test_unrelated_runtime_is_recorded_but_duplicate_copy_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);runtime=root/'runtime';runtime.mkdir()
            own=runtime/'qualpf_pfsm_benchmark.exe';own.write_bytes(b'frozen')
            other=root/'different.exe';other.write_bytes(b'different algorithm')
            cohort=root/'cohort'
            busy=[dict(pid=17,exe=str(other),cmdline=[str(other),'--output',str(root/'elsewhere')])]
            records=v2.unrelated_workloads(busy,runtime,cohort)
            self.assertEqual(records[0]['executable_sha256'],sha(other))
            other.write_bytes(own.read_bytes())
            with self.assertRaisesRegex(RuntimeError,'duplicates owned'):
                v2.unrelated_workloads(busy,runtime,cohort)
            other.write_bytes(b'different algorithm');busy[0]['cmdline'][-1]=str(cohort/'case')
            with self.assertRaisesRegex(RuntimeError,'duplicates owned'):
                v2.unrelated_workloads(busy,runtime,cohort)
            with self.assertRaisesRegex(RuntimeError,'unknown ownership'):
                v2.unrelated_workloads([dict(pid=18)],runtime,cohort)

    def test_grid_unavailable_never_claims_a_replay_or_formal_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);output=root/'run';output.mkdir()
            exe=root/'probe';exe.write_bytes(b'probe');data=root/'observations.tsv'
            data.write_text('# data\n2 1 0 1 0 1\n80 1 0 1 0 1\n')
            physical=dict(case='narrow',width=.001,lower_shift=-.5,upper_shift=.5,
                rounds=[dict(round=0,observations=v2.bind(data))])
            input_path=root/'input.json';input_path.write_text(json.dumps(physical))
            (output/'run-start.json').write_text(json.dumps(dict(input_sha256=sha(input_path),formal_fits=0,executable_sha256=sha(exe))))
            receipt_path=root/'case.receipt.json';receipt=dict(exit_code=1,command=[str(exe)])
            receipt_path.write_text(json.dumps(receipt));stderr=receipt_path.with_suffix('.stderr.log')
            stderr.write_text('pfsm.fast: Fast grid node limit\n')
            result=v2.unavailable_replay(physical,v2.bind(input_path),output,receipt_path,receipt)
            self.assertIsNone(result['screening_passed']);self.assertFalse(result['native_run_completed'])
            self.assertFalse((output/'run.json').exists())
            replay=dict(key='replay-case',mode='replay',passed=None)
            enabled=dict(key='enabled-case',mode='enabled',passed=True,covers_unavailable_replay=False)
            self.assertFalse(v2.comparisons_passed([replay,enabled]))
            enabled['covers_unavailable_replay']=True
            self.assertTrue(v2.comparisons_passed([replay,enabled]))
            enabled['passed']=False
            self.assertFalse(v2.comparisons_passed([replay,enabled]))
            physical['width']=1
            with self.assertRaisesRegex(RuntimeError,'not supported'):
                v2.unavailable_replay(physical,v2.bind(input_path),output,receipt_path,receipt)
            stderr.write_text('some other numerical failure')
            self.assertIsNone(v2.unavailable_replay(physical,v2.bind(input_path),output,receipt_path,receipt))

    def test_unfinished_receipt_needs_ownership_diagnosis(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'receipt.json';p.write_text(json.dumps(dict(command=['x'],exit_code=None)))
            with self.assertRaisesRegex(RuntimeError,'ownership diagnosis'):v2.execute(['x'],Path(tmp)/'out',p,{})


if __name__ == '__main__':
    unittest.main()
