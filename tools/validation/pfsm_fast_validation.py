#!/usr/bin/env python3
"""Frozen shadow/enabled comparisons, conditional diagnosis and evidence reuse."""
import argparse
import copy
import datetime
import hashlib
import json
import os
from pathlib import Path
import statistics
import shutil
import subprocess
import sys
import time
from pfsm_candidate_recall_compare import compare


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify(executable, complete=False):
    binding_path = executable.parent/'binding.json'
    binding = json.loads(binding_path.read_text(encoding='utf-8'))
    for name, expected in binding['files'].items():
        if complete or Path(name).suffix.lower() in ('.exe', '.dll') or name == 'qt.conf':
            if sha(executable.parent/name) != expected:
                raise RuntimeError('Frozen file changed: '+name)
    return sha(binding_path)


def idle():
    import psutil
    busy = []
    for p in psutil.process_iter(['pid', 'name']):
        name = (p.info['name'] or '').lower()
        if (name in ('cl.exe', 'cc1plus.exe', 'ninja.exe', 'qualpf_pfsm_benchmark.exe') or
                (name.startswith('qualpf_') and 'properties' in name)):
            busy.append(p.info)
    cpu = psutil.cpu_percent(interval=2)
    return dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), cpu_percent=cpu,
                competing_processes=busy, idle=not busy and cpu<10)


def spread(values):
    return dict(median=statistics.median(values), minimum=min(values), maximum=max(values))


def command_key(command):
    # Output location and frozen executable location are not scientific inputs.
    # Executable/runtime content is checked independently by the frozen binding.
    result = []
    i = 1
    while i < len(command):
        if command[i] == '--output':
            i += 2
        else:
            result.append(command[i]); i += 1
    return result


class CompletedRuns:
    def __init__(self, sources, report):
        self.entries = []
        for source in sources:
            path = Path(source).resolve()/'validation.json'
            previous = json.loads(path.read_text(encoding='utf-8'))
            for field in ('baseline_binding', 'fast_binding', 'manifest_sha256'):
                if previous.get(field) != report[field]:
                    raise RuntimeError(f'Reuse binding mismatch: {path}: {field}')
            for row in previous['measurements']:
                # Keep completed failures too. Interrupted processes have no exit
                # code and are not silently promoted to completed evidence.
                if row.get('exit_code') is not None:
                    self.entries.append((path, row))

    def find(self, command, executable):
        for source, row in self.entries:
            if command_key(row['command']) != command_key(command):
                continue
            directory = Path(row.get('output_directory') or
                             row['command'][row['command'].index('--output')+1])
            path = directory/'run.json'
            if path.is_file():
                result = json.loads(path.read_text(encoding='utf-8'))
                if result.get('executable_sha256') != sha(executable):
                    raise RuntimeError('Reuse executable mismatch: '+str(path))
            reused = copy.deepcopy(row)
            reused.update(output_directory=str(directory), reused_from=str(source),
                          reused_report_sha256=sha(source),
                          evidence_sha256={str(p.relative_to(directory)): sha(p)
                              for p in sorted(directory.rglob('*')) if p.is_file()})
            return directory, reused
        return None


def check_group(run, comparison, workflow):
    if workflow == 'four-way':
        baseline, off = run('baseline'), run('off')
        if not comparison(baseline, off, 'baseline-off'):
            return dict(correct=False, withheld='baseline/off mismatch')
        shadow = run('shadow')
        if not comparison(off, shadow, 'shadow'):
            return dict(correct=False, withheld='shadow failed; enabled and timing withheld')
        enabled = run('enabled')
        return dict(correct=comparison(off, enabled, 'enabled'))
    shadow, enabled = run('shadow'), run('enabled')
    screening_ok = comparison(shadow, shadow, 'shadow-screening')
    formal_ok = comparison(shadow, enabled, 'enabled')
    result = dict(correct=screening_ok and formal_ok, diagnostic_baseline_off=False)
    if not result['correct']:
        result['diagnostic_baseline_off'] = True
        baseline, off = run('baseline'), run('off')
        result['baseline_off_equal'] = comparison(baseline, off, 'baseline-off')
        result['off_shadow_equal_and_screening_passed'] = comparison(off, shadow, 'shadow')
        result['off_enabled_passed'] = comparison(off, enabled, 'enabled')
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'benchmark', 'manifest', 'database', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--case', action='append', dest='cases')
    p.add_argument('--threads', default='12')
    p.add_argument('--cache', choices=('direct', 'cache', 'both'), default='cache')
    p.add_argument('--linear-algebra-cache-sizes', default='49152,1310720,25165824')
    p.add_argument('--truth-free', action='store_true')
    p.add_argument('--full-library', action='store_true')
    p.add_argument('--timing', action='store_true')
    p.add_argument('--require-twofold', action='store_true')
    p.add_argument('--timeout', type=int, default=7200)
    p.add_argument('--workflow', choices=('shadow-enabled', 'four-way'), default='shadow-enabled')
    p.add_argument('--reuse', type=Path, action='append', default=[],
                   help='Reuse completed matching runs from a prior validation directory; never overwrite it')
    args = p.parse_args()
    if args.timing and args.workflow != 'four-way':
        p.error('Baseline paired timing requires an explicit --workflow four-way; shadow/enabled is diagnostic only')
    args.baseline, args.benchmark = args.baseline.resolve(), args.benchmark.resolve()
    args.manifest, args.database, args.output = args.manifest.resolve(), args.database.resolve(), args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    shutil.copyfile(Path(__file__), args.output/'runner.py')
    shutil.copyfile(Path(__file__).with_name('pfsm_fast_compare.py'), args.output/'comparator.py')
    shutil.copyfile(args.manifest, args.output/'manifest-copy.json')
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    selected = [c for c in manifest['cases'] if not args.cases or c['id'] in args.cases]
    if args.cases and set(args.cases) != {c['id'] for c in selected}:
        raise RuntimeError('Unknown requested cases')
    counts = [int(n) for n in args.threads.split(',')]
    if not counts or any(n<=0 for n in counts):
        raise RuntimeError('Positive thread counts required')
    modes = [False, True] if args.cache == 'both' else [args.cache == 'cache']
    report = dict(schema='qualpf.pfsm.fast-validation.v2', fixed_budgets=[500, 20], workflow=args.workflow,
        baseline_binding=verify(args.baseline, True), fast_binding=verify(args.benchmark, True),
        manifest_sha256=sha(args.manifest), driver_sha256=sha(Path(__file__)),
        comparator_sha256=sha(Path(__file__).with_name('pfsm_fast_compare.py')),
        cases=[dict(id=c['id'], status=c['status'], reason=c.get('reason'),
                    split=c.get('validation_split')) for c in selected],
        measurements=[], comparisons=[], groups=[], host_checks=[], complete=False)
    completed = CompletedRuns(args.reuse, report)
    def save():
        path = args.output/'validation.json'
        temporary = path.with_suffix('.tmp')
        temporary.write_text(json.dumps(report, indent=2, allow_nan=False)+'\n', encoding='utf-8')
        temporary.replace(path)
    save()
    def run(case, threads, cache, mode, label):
        executable = args.baseline if mode == 'baseline' else args.benchmark
        verify(executable)
        name = f'{case}-{threads}-{"cache" if cache else "direct"}-{mode}-{label}'
        directory = args.output/name
        command = [str(executable), '--database', str(args.database), '--manifest', str(args.manifest),
                   '--output', str(directory), '--case', case, '--threads', str(threads),
                   '--linear-algebra-cache-sizes', args.linear_algebra_cache_sizes]
        if mode != 'baseline':
            command += ['--fast-pfsm', mode]
        if cache:
            command += ['--template-cache']
        if args.truth_free:
            command += ['--performance-only']
        if args.full_library:
            command += ['--chemistry', 'none']
        reused = completed.find(command, executable) if label == 'correctness' else None
        if reused is not None:
            directory, row = reused
            path = directory/'run.json'
            cases = json.loads(path.read_text(encoding='utf-8')).get('cases', []) if path.is_file() else []
            result = cases[0] if len(cases) == 1 and cases[0]['id'] == case else None
            report['measurements'].append(row); save()
            print('REUSE '+row['label'], flush=True)
            return result, directory/case, row
        env = os.environ.copy()
        env.update(PATH=str(executable.parent)+os.pathsep+env.get('PATH', ''),
                   QT_PLUGIN_PATH=str(executable.parent/'plugins'), TEMP=str(args.output), TMP=str(args.output),
                   OMP_NUM_THREADS=str(threads), OMP_DYNAMIC='FALSE', OMP_MAX_ACTIVE_LEVELS='1',
                   OPENBLAS_NUM_THREADS='1', MKL_NUM_THREADS='1', BLIS_NUM_THREADS='1')
        row = dict(label=name, command=command, output_directory=str(directory), variant=mode,
                   case=case, threads=threads, cache=cache,
                   environment={k:env.get(k) for k in ('OMP_NUM_THREADS','OMP_DYNAMIC','OMP_MAX_ACTIVE_LEVELS',
                                'OMP_PROC_BIND','OMP_PLACES','OPENBLAS_NUM_THREADS','MKL_NUM_THREADS','QT_PLUGIN_PATH')})
        report['measurements'].append(row); save()
        print('START '+name, flush=True)
        start = time.perf_counter()
        launch_wall_ns = time.time_ns()
        with (args.output/(name+'.log')).open('xb') as log:
            try:
                child = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
                row['pid'] = child.pid; save()
                row['exit_code'] = child.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                child.kill(); child.wait(); row['timeout'] = True
                row['exit_code'] = child.returncode
        row['process_seconds'] = time.perf_counter()-start
        startup = directory/'run-start.json'
        if startup.is_file():
            # Includes loader, Qt, CLI setup and database verification. This is
            # not a claim to isolate the operating-system loader by itself.
            row['launch_to_metadata_seconds'] = (startup.stat().st_mtime_ns-launch_wall_ns)/1e9
        path = directory/'run.json'
        result = None
        if path.is_file():
            cases = json.loads(path.read_text(encoding='utf-8')).get('cases', [])
            if len(cases) == 1 and cases[0]['id'] == case:
                result = cases[0]
                row['status'] = result.get('status')
                row['rounds'] = [dict(operation_seconds=r.get('operation_to_result_seconds'),
                                     peak_bytes=r.get('peak_process_resident_bytes')) for r in result.get('rounds', [])]
                row['end_to_end_seconds'] = result.get('end_to_end_seconds')
        save(); print('FINISH '+name+': '+str(row.get('status', 'missing result')), flush=True)
        return result, directory/case, row
    def comparison(left, right, kind):
        a, adir, arun = left; b, bdir, brun = right
        if a is None or b is None:
            value = dict(passed=False, errors=['Missing completed process output'], rounds=[])
        else:
            value = compare(a, b, adir, bdir, kind == 'enabled')
            if arun.get('exit_code') != brun.get('exit_code') or arun.get('timeout') or brun.get('timeout'):
                value['errors'].append('process completion/exit mismatch'); value['passed'] = False
        value.update(before=arun['label'], after=brun['label'], kind=kind)
        report['comparisons'].append(value); save()
        return value['passed']
    try:
        for item in selected:
            if item['status'] != 'ready':
                continue
            case = item['id']
            for threads in counts:
                for cache in modes:
                    group = dict(case=case, threads=threads, cache=cache, correct=False, timing='not_requested')
                    report['groups'].append(group); save()
                    group.update(check_group(lambda mode: run(case, threads, cache, mode, 'correctness'),
                                             comparison, args.workflow))
                    group['complete'] = True; save()
                    if not group['correct']:
                        continue
                    if not args.timing:
                        continue
                    pairs = []
                    for repetition in range(5):
                        check = idle(); report['host_checks'].append(check); save()
                        if not check['idle']:
                            group['timing'] = 'withheld_busy_host'; save(); break
                        pair = {}
                        for mode in (('baseline','enabled') if repetition%2 == 0 else ('enabled','baseline')):
                            pair[mode] = run(case, threads, cache, mode, f'timing-{repetition}')
                        if not comparison(pair['baseline'], pair['enabled'], 'enabled'):
                            group['timing'] = 'failed_scientific_comparison'; save(); break
                        pairs.append(pair)
                    if len(pairs) == 5:
                        group['timing_rounds'] = []
                        for n in range(len(pairs[0]['baseline'][0]['rounds'])):
                            before = spread([pair['baseline'][0]['rounds'][n]['operation_to_result_seconds'] for pair in pairs])
                            after = spread([pair['enabled'][0]['rounds'][n]['operation_to_result_seconds'] for pair in pairs])
                            group['timing_rounds'].append(dict(round=n, baseline=before, enabled=after,
                                speedup=before['median']/after['median'],
                                passed=after['median'] <= before['median']*(.5 if args.require_twofold else 1.05)))
                        group['timing'] = 'passed' if all(r['passed'] for r in group['timing_rounds']) else 'failed_speed'
                    save()
        verify(args.baseline, True); verify(args.benchmark, True)
        report['complete'] = True
        report['passed'] = bool(report['groups']) and all(g['correct'] and
            (not args.timing or g['timing'] == 'passed') for g in report['groups'])
        save()
    except BaseException as error:
        report['interrupted_or_error'] = repr(error); save(); raise
    print(f"Fast PFSM: {sum(g['correct'] for g in report['groups'])}/{len(report['groups'])} correctness groups; passed={report['passed']}")
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
