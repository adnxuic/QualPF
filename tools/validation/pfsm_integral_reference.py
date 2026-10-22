#!/usr/bin/env python3
"""Independent cell-overlap / extended-precision IL / pinned SciPy NNLS oracle."""
import argparse
from decimal import Decimal, localcontext
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--dependency-dir', type=Path)
    args = p.parse_args()
    if args.output.exists():
        raise RuntimeError('Fresh output required; never overwrite failed evidence')
    if args.dependency_dir:
        sys.path.insert(0, str(args.dependency_dir.resolve()))
    import numpy as np
    import scipy
    from scipy.optimize import nnls
    if scipy.__version__ != '1.16.2':
        raise RuntimeError('Expected pinned SciPy 1.16.2')
    rng = np.random.default_rng(104731)
    report = dict(schema='qualpf.pfsm.integral-reference.v1', scipy=scipy.__version__,
                  integral_reference_decimal_digits=50,
                  oracle_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  probe_sha256=hashlib.sha256(args.probe.read_bytes()).hexdigest(),
                  tolerance='2e-10 * max(1, abs(reference))', cases=[])
    ld = np.longdouble
    alpha = 4*(ld(2)**(ld(2)/3)-1)
    for n in (71, 3001):
        for m in (1, 2, 4):
            for variant in range(4):
                h = (.02, .003, 1e-5, .02)[variant]
                x = np.linspace(28, 36, n)
                x[1:-1] += .0003*np.sin(np.arange(1, n-1))
                weights = rng.uniform(.3, 2, n)
                if variant == 1:
                    weights[::7] = 0
                y = np.sin(x)*.1+rng.normal(0, .03, n)
                lines = [[(29.5+.3*j, 1), (32+.57*j, .4), (36.05, .15)] for j in range(m)]
                if variant == 3 and m > 1:
                    lines[-1] = lines[0][:]
                shifts = rng.uniform(-.08, .08, m)
                if variant == 3 and m > 1:
                    shifts[-1] = shifts[0]
                for refs, shift in zip(lines, shifts):
                    for q, intensity in refs:
                        y += intensity*(1+float(alpha)*((x-q-shift)/h)**2)**-1.5
                bounds = np.r_[x[0], (x[:-1]+x[1:])/2, x[-1]]
                edges = np.linspace(x[0], x[-1], min(2048, np.count_nonzero(weights))+1)
                segments = []
                for i in range(n):
                    if weights[i] == 0:
                        continue
                    lo, hi = bounds[i:i+2]
                    for j in range(max(0, np.searchsorted(edges, lo, side='right')-1),
                                   min(len(edges)-1, np.searchsorted(edges, hi, side='left'))):
                        a, b = max(lo, edges[j]), min(hi, edges[j+1])
                        if b > a:
                            segments.append((i, j, a, b, weights[i]/(hi-lo)))
                seg = np.array(segments)
                samples, bins = seg[:, 0].astype(int), seg[:, 1].astype(int)
                lo, hi, density = seg[:, 2].astype(ld), seg[:, 3].astype(ld), seg[:, 4]
                w = np.bincount(bins, weights=density*np.asarray(hi-lo, float), minlength=len(edges)-1)
                v = np.bincount(bins, weights=density*np.asarray(hi-lo, float)*y[samples], minlength=len(w))
                def merge(v):
                    return np.bincount(np.arange(len(v))//2, weights=v)
                # Decimal supplies the same genuine extended precision on MSVC,
                # where NumPy longdouble may have only binary64 precision.
                with localcontext() as precision:
                    precision.prec = 50
                    aa = Decimal.from_float(h)/(4*(Decimal(4)**(Decimal(1)/3)-1)).sqrt()
                    epsilon = Decimal('0.0001')
                    radius = aa*(1-epsilon)/(epsilon*(2-epsilon)).sqrt()
                    square = aa*aa
                def exact_mass(left, right, center):
                    with localcontext() as precision:
                        precision.prec = 50
                        c = Decimal.from_float(center)
                        left = max(Decimal.from_float(float(left))-c, -radius)
                        right = min(Decimal.from_float(float(right))-c, radius)
                        if right <= left:
                            return 0.
                        return float((right/(right*right+square).sqrt()-left/(left*left+square).sqrt())/2)
                columns = []
                for refs, shift in zip(lines, shifts):
                    numerator = np.zeros(len(w))
                    for q, intensity in refs:
                        # Independent 50-digit primitive with clipped endpoints;
                        # no product matrix or product rationalization is read.
                        mass = np.fromiter((exact_mass(l, r, float(q+shift)) for l,r in zip(lo,hi)), float)
                        numerator += np.bincount(bins, weights=intensity*density*mass, minlength=len(w))
                    columns.append(numerator)
                expected = []
                denominators, bases, gains = [], [], []
                for coarse in (True, False):
                    ww, vv = (merge(w), merge(v)) if coarse else (w, v)
                    used = ww > 0
                    yy = vv[used]/ww[used]
                    cols = [(merge(col) if coarse else col)[used]/ww[used] for col in columns]
                    def solve(cols):
                        if not cols:
                            return float(np.dot(ww[used], yy*yy))
                        mat = np.column_stack(cols)
                        scales = nnls(np.sqrt(ww[used])[:, None]*mat, np.sqrt(ww[used])*yy, maxiter=10000)[0]
                        r = yy-mat@scales
                        return float(np.dot(ww[used], r*r))
                    base = solve(cols[:-1])
                    bases.append(base); gains.append(base-min(base, solve(cols)))
                    denominators.append(float(np.dot(ww[used], yy*yy)))
                expected = np.array(bases+gains+denominators)
                wire = [f'{n} {m} {h:.17g}']
                wire += [' '.join(f'{v:.17g}' for v in row) for row in zip(x, y, weights)]
                for refs, shift in zip(lines, shifts):
                    wire.append(f'{shift:.17g} {len(refs)}')
                    wire += [f'{q:.17g} {i:.17g}' for q, i in refs]
                payload = '\n'.join(wire)+'\n'
                run = subprocess.run([str(args.probe.resolve())], input=payload, text=True, capture_output=True)
                actual = np.fromstring(run.stdout, sep=' ')
                passed = run.returncode == 0 and actual.shape == (6,) and bool(np.all(np.abs(actual-expected) <= 2e-10*np.maximum(1, np.abs(expected))))
                report['cases'].append(dict(name=f'{n}-{m}-{variant}', input_sha256=hashlib.sha256(payload.encode()).hexdigest(),
                    expected=expected.tolist(), actual=actual.tolist(), passed=passed, exit_code=run.returncode, stderr=run.stderr))
    report['passed'] = all(c['passed'] for c in report['cases'])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f"Independent integral: {sum(c['passed'] for c in report['cases'])}/{len(report['cases'])} PASS")
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
