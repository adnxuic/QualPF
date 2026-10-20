#!/usr/bin/env python3
"""Independent PFSM oracle. This script never creates expectations from product output.

Requires SciPy 1.16.2; optionally use --dependency-dir for an isolated pip target.
All tolerances and cases are specified before executing the C++ probe.
"""
import argparse
import hashlib
import inspect
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dependency-dir', type=Path)
    parser.add_argument('--template-cache', action='store_true')
    args = parser.parse_args()
    if args.dependency_dir:
        sys.path.insert(0, str(args.dependency_dir.resolve()))
    import numpy as np
    import scipy
    from scipy.optimize import least_squares, nnls, minimize_scalar
    from scipy.optimize._lsq import trf
    if scipy.__version__ != '1.16.2':
        raise RuntimeError(f'Expected pinned SciPy 1.16.2, got {scipy.__version__}')
    rng = np.random.default_rng(18423)
    x = np.linspace(28, 36, 651) + .0004*np.sin(np.arange(651))
    h = .19
    alpha = 4*(2**(2/3)-1)

    def columns(lines, shifts):
        return np.column_stack([sum(intensity*(1+alpha*((x-q-shift)/h)**2)**-1.5
                                    for q, intensity in phase)
                                for phase, shift in zip(lines, shifts)])

    cases = []
    for m in (1, 2, 4):
        for k in range(8):
            lines = [[(30+j*.34, 1.), (32+j*.57, .3+j*.1)] for j in range(m)]
            true_shift = rng.uniform(-.095, .095, m)
            scale = rng.uniform(.4, 3., m)
            if k == 6 and m > 1:
                scale[-1] = 0  # Active constraint, with unconstrained shifts.
            background = 2 + .04*(x-30)
            y = background + columns(lines, true_shift)@scale + rng.normal(0, .002, len(x))
            if k == 7:
                y[::11] -= .2  # Negative net values retained with fixed weights.
            weights = rng.uniform(.4, 1.4, len(x))
            lower, upper = np.full(m, -.15), np.full(m, .15)
            if k == 5:
                upper[0] = true_shift[0]-.03  # Constrained boundary optimum.
            cases.append((f'm{m}-case{k}', lines, y, background, weights, lower, upper))
    report = {'schema': 'qualpf.pfsm.scipy-comparison.v1', 'scipy': scipy.__version__,
              'template_cache': args.template_cache,
              'trf_sha256': hashlib.sha256(inspect.getsource(trf).encode()).hexdigest(),
              'seed': 18423, 'tolerances': {'q': '2e-8 * max(1, Qreference)',
                                         'scale': '2e-5', 'identifiable_shift': '2e-5 degrees'},
              'cases': []}
    for name, lines, y, background, weights, lower, upper in cases:
        m = len(lines)
        net = y-background
        denominator = np.dot(weights, net*net)
        root_weight = np.sqrt(weights/denominator)
        def solve(t):
            a = columns(lines, h*np.asarray(t))*root_weight[:, None]
            s = nnls(a, net*root_weight, maxiter=1000)[0]
            return a@s-net*root_weight, s
        initial = np.maximum(lower, np.minimum(upper, np.zeros(m)))
        reference = least_squares(lambda t: solve(t)[0], initial/h, jac='3-point',
                                  bounds=(lower/h, upper/h), method='trf', tr_solver='exact',
                                  ftol=1e-12, xtol=1e-12, gtol=1e-12, max_nfev=2000)
        residual, expected_scale = solve(reference.x)
        expected_q = float(residual@residual*denominator)
        brent_q = None
        if m == 1:
            # Global dense scan followed by bounded Brent, independent of TRF.
            grid = np.linspace(lower[0]/h, upper[0]/h, 4001)
            values = [float(solve([t])[0]@solve([t])[0]) for t in grid]
            best = int(np.argmin(values))
            result = minimize_scalar(lambda t: float(solve([t])[0]@solve([t])[0]),
                                     bounds=(grid[max(0,best-1)], grid[min(len(grid)-1,best+1)]),
                                     method='bounded', options={'xatol': 1e-13})
            brent_q = min(values[0], values[-1], result.fun)*denominator
            if abs(brent_q-expected_q) > 1e-9*max(1, expected_q):
                raise RuntimeError('Independent Brent and TRF disagree')
        payload = [f'{len(x)} {m} {h}']
        payload.extend(' '.join(map(repr, map(float, row))) for row in zip(x,y,background,weights))
        for j, phase in enumerate(lines):
            payload.append(f'{lower[j]} {upper[j]} {initial[j]} {len(phase)}')
            payload.extend(f'{q} {s}' for q,s in phase)
        command = [str(args.probe.resolve())] + (['--template-cache'] if args.template_cache else [])
        process = subprocess.run(command, input='\n'.join(payload)+'\n',
                                 text=True, capture_output=True, timeout=60)
        entry = {'name':name, 'reference_q':expected_q, 'reference_scales':expected_scale.tolist(),
                 'reference_shifts':(reference.x*h).tolist(), 'brent_q':brent_q,
                 'probe_stdout':process.stdout, 'probe_stderr':process.stderr, 'passed':False}
        if process.returncode == 0:
            rows = [list(map(float, line.split())) for line in process.stdout.splitlines()]
            measured_q = rows[0][0]
            parameters = np.array(rows[1:])
            entry['passed'] = bool(abs(measured_q-expected_q) <= 2e-8*max(1,expected_q)
                and np.max(np.abs(parameters[:,0]-expected_scale)) <= 2e-5
                and all(abs(parameters[j,1]-reference.x[j]*h) <= 2e-5
                        for j in range(m) if expected_scale[j] > 1e-3))
        report['cases'].append(entry)
        # Write immediately to preserve any failure even if a later case crashes.
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    passed = sum(row['passed'] for row in report['cases'])
    print(f'{passed}/{len(cases)} independent SciPy/Brent cases passed')
    return 0 if passed == len(cases) else 1


if __name__ == '__main__':
    sys.exit(main())
