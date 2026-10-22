#!/usr/bin/env python3
"""Alternating Release PFSM measurements; comparisons are not numerical goldens."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import time


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def _recall_without_integral_timing(value):
    # Keep raw reports intact and ignore only these declared timing fields.
    # Other evidence, including unknown fields and resource counts, stays exact.
    result = copy.deepcopy(value)
    if result is not None:
        for attempt in result.get('attempts', []):
            integral = attempt.get('integral')
            if isinstance(integral, dict):
                for field in ('shared_seconds', 'integral_seconds', 'relaxed_seconds'):
                    integral.pop(field, None)
    return result


def scientific_equal(before, after):
    """Compare complete ranking/curves and final baseline diagnostics, not timing."""
    errors = []
    left, right = before['rounds'], after['rounds']
    if len(left) != len(right):
        return ['round count']
    fields = ('baseline_Q', 'baseline_Rwp_net', 'baseline_Rwp_obs',
              'baseline_parameters', 'baseline_diagnostics', 'baseline_termination', 'candidate_recall')
    for number, (a, b) in enumerate(zip(left, right)):
        for field in fields:
            first, second = a.get(field), b.get(field)
            if field == 'candidate_recall':
                first, second = _recall_without_integral_timing(first), _recall_without_integral_timing(second)
            if json.dumps(first, sort_keys=True) != json.dumps(second, sort_keys=True):
                errors.append(f'round {number}: {field}')
    for field in ('points', 'candidate_count', 'candidate_order_sha256'):
        if before[field] != after[field]:
            errors.append(field)
    return errors


def spread(values):
    return dict(median=statistics.median(values), minimum=min(values), maximum=max(values))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('before', 'after', 'database', 'manifest', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--case', action='append', dest='cases')
    parser.add_argument('--threads', default='1,4,12,20')
    parser.add_argument('--mode', choices=('direct', 'cache', 'both'), default='both')
    parser.add_argument('--candidate-recall', choices=('off', 'shadow', 'enabled'), default='off')
    parser.add_argument('--linear-algebra-cache-sizes', default='49152,1310720,25165824')
    parser.add_argument('--repetitions', type=int, default=5)
    parser.add_argument('--no-warmup', action='store_true', help='Skip both warmup executions')
    parser.add_argument('--full-library', action='store_true', help='Unconstrained stress test, default 3 pairs')
    parser.add_argument('--bind-threads', action='store_true', help='Bind identical OpenMP thread places in both variants')
    parser.add_argument('--timeout', type=float, default=7200)
    args = parser.parse_args()
    if args.repetitions <= 0 or args.timeout <= 0:
        parser.error('Positive repetitions and timeout required')
    threads = [int(s) for s in args.threads.split(',')]
    if not threads or len(set(threads)) != len(threads) or any(n <= 0 for n in threads):
        parser.error('Threads must be distinct positive integers')
    args.output.mkdir(parents=True, exist_ok=False)
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    available = [c['id'] for c in manifest['cases'] if c['status'] == 'ready']
    cases = args.cases or available
    if len(set(cases)) != len(cases) or not set(cases) <= set(available):
        parser.error('Cases must be distinct ready manifest IDs')
    modes = [False, True] if args.mode == 'both' else [args.mode == 'cache']
    env = os.environ.copy()
    env.update(MKL_NUM_THREADS='1', OPENBLAS_NUM_THREADS='1', BLIS_NUM_THREADS='1',
               OMP_DYNAMIC='FALSE', OMP_MAX_ACTIVE_LEVELS='1')
    if args.bind_threads:
        env.update(OMP_PROC_BIND='TRUE', OMP_PLACES='threads')
    repetitions = 3 if args.full_library and args.repetitions == 5 else args.repetitions
    report = dict(schema='qualpf.pfsm.performance-comparison.v1',
                  manifest_sha256=digest(args.manifest), before_sha256=digest(args.before),
                  after_sha256=digest(args.after), measurements=[], comparisons=[],
                  driver_sha256=digest(__file__),
                  candidate_recall=args.candidate_recall,
                  repetitions=repetitions, warmup_pairs=0 if args.no_warmup else 1,
                  linear_algebra_cache_sizes=args.linear_algebra_cache_sizes,
                  environment={k: env.get(k) for k in ('OMP_PROC_BIND', 'OMP_PLACES', 'OMP_DYNAMIC',
                               'OMP_MAX_ACTIVE_LEVELS', 'MKL_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'BLIS_NUM_THREADS')},
                  qualification='Performance-only; same-input A/B comparisons, never truth or replacement goldens')

    def save():
        (args.output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(label, case, count, cache, iteration):
        name = f'{case}-{count}-{"cache" if cache else "direct"}-{iteration}-{label}'
        directory = args.output / name
        command = [str(getattr(args, label).resolve()), '--database', str(args.database.resolve()),
                   '--manifest', str(args.manifest.resolve()), '--output', str(directory.resolve()),
                   '--case', case, '--threads', str(count), '--performance-only',
                   '--pfsm-candidate-recall', args.candidate_recall,
                   '--linear-algebra-cache-sizes', args.linear_algebra_cache_sizes]
        if cache:
            command.append('--template-cache')
        if args.full_library:
            command.extend(['--chemistry', 'none'])
        row = dict(name=name, command=command, case=case, threads=count, cache=cache,
                   iteration=iteration, variant=label)
        report['measurements'].append(row)
        save()
        print('START ' + name, flush=True)
        start = time.perf_counter()
        try:
            with (args.output / (name + '.log')).open('wb') as log:
                result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout)
            row['exit_code'] = result.returncode
        except subprocess.TimeoutExpired:
            row['timeout'] = True
            save()
            raise
        finally:
            row['process_seconds'] = time.perf_counter() - start
            save()
        if result.returncode:
            raise RuntimeError(f'{name} failed: {result.returncode}; original log preserved')
        output = json.loads((directory / 'run.json').read_text(encoding='utf-8'))['cases'][0]
        if output['status'] != 'performance_not_acceptance':
            raise RuntimeError(f'{name}: unexpected status')
        row.update(end_to_end_seconds=output['end_to_end_seconds'],
                   rounds=[dict(seconds=r['operation_to_result_seconds'],
                                peak_bytes=r['peak_process_resident_bytes']) for r in output['rounds']])
        save()
        print(f'FINISH {name}: {row["process_seconds"]:.3f}s', flush=True)
        return output, directory / case

    for case in cases:
        for count in threads:
            env['OMP_NUM_THREADS'] = str(count)
            for cache in modes:
                if not args.no_warmup:
                    run('before', case, count, cache, 'warmup')
                    run('after', case, count, cache, 'warmup')
                measurements = []
                for iteration in range(repetitions):
                    pair = {}
                    for label in (('before', 'after') if iteration % 2 == 0 else ('after', 'before')):
                        pair[label] = run(label, case, count, cache, iteration)
                    a, adir = pair['before']
                    b, bdir = pair['after']
                    errors = scientific_equal(a, b)
                    files = ['candidate-order.txt'] + [f'{stem}-{n}.{suffix}'
                        for n in range(len(a['rounds'])) for stem, suffix in [('round', 'jsonl'), ('baseline', 'tsv')]]
                    errors += [name for name in files if digest(adir / name) != digest(bdir / name)]
                    for number in range(len(a['rounds'])):
                        name = f'fast-{number}.jsonl'
                        if (adir / name).exists() != (bdir / name).exists():
                            errors.append(f'{name}: presence')
                        elif (adir / name).exists() and digest(adir / name) != digest(bdir / name):
                            errors.append(name)
                    measurements.append((a, b))
                    comparison = dict(case=case, threads=count, cache=cache, iteration=iteration,
                                      scientific_identical=not errors, differences=errors)
                    report['comparisons'].append(comparison)
                    save()
                    if errors:
                        raise RuntimeError(f'Numerical mismatch (preserved): {comparison}')
                group = dict(case=case, threads=count, cache=cache, rounds=[])
                for n in range(len(measurements[0][0]['rounds'])):
                    before = spread([a['rounds'][n]['operation_to_result_seconds'] for a, _ in measurements])
                    after = spread([b['rounds'][n]['operation_to_result_seconds'] for _, b in measurements])
                    group['rounds'].append(dict(round=n, before_seconds=before, after_seconds=after,
                        improvement_percent=100 * (1 - after['median'] / before['median']),
                        paired_improvement_percent=[100 * (1 - b['rounds'][n]['operation_to_result_seconds'] /
                            a['rounds'][n]['operation_to_result_seconds']) for a, b in measurements],
                        ranges_separated=after['maximum'] < before['minimum'],
                        regression_within_five_percent=after['median'] <= 1.05 * before['median'],
                        before_peak_bytes=max(a['rounds'][n]['peak_process_resident_bytes'] for a, _ in measurements),
                        after_peak_bytes=max(b['rounds'][n]['peak_process_resident_bytes'] for _, b in measurements)))
                group['before_end_to_end_seconds'] = spread([a['end_to_end_seconds'] for a, _ in measurements])
                group['after_end_to_end_seconds'] = spread([b['end_to_end_seconds'] for _, b in measurements])
                report.setdefault('groups', []).append(group)
                save()


if __name__ == '__main__':
    main()
