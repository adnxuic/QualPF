#!/usr/bin/env python3
"""Scheduling and reuse controls; no product-generated numeric expectations."""
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[3]/'tools/validation'))
from pfsm_fast_validation import CompletedRuns, check_group, sha
import pfsm_candidate_recall_validation as v2


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
