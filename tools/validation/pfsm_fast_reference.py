#!/usr/bin/env python3
"""Independent weighted IL convolution and SciPy NNLS reference, fixed 24 cases."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backprojection', action='store_true')
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dependency-dir', type=Path)
    args = parser.parse_args()
    if args.dependency_dir:
        sys.path.insert(0, str(args.dependency_dir))
    import numpy as np
    import scipy
    from scipy.optimize import nnls
    if scipy.__version__ != '1.16.2':
        raise RuntimeError('Requires pinned SciPy 1.16.2')
    if args.output.exists():
        raise RuntimeError('Refusing to overwrite reference evidence')
    rng = np.random.default_rng(781423)
    report = dict(schema=('qualpf.pfsm.fast-backprojection-reference.v1' if args.backprojection else 'qualpf.pfsm.fast-reference.v1'), scipy=scipy.__version__,
                  seed=781423, absolute_tolerance=2e-10,
                  probe_sha256=hashlib.sha256(args.probe.read_bytes()).hexdigest(), cases=[])
    for m in (0, 1, 2):
        for case in range(8):
            h = 4.0
            lo, hi = (-1.0, 1.0) if case != 7 else (0.0, 0.0)
            x = np.linspace(0, 180, 97) + .03*np.sin(np.arange(97))
            w = rng.uniform(.1, 3, len(x)); w[::9] = 0
            lines = [[(30+j*2, 1.), (31+j, .4), (179.9, .05)] for j in range(m+1)]
            if case == 6:
                lines[-1] = [(30, .25), (30, .75), (31, .4), (179.9, .05)]
            shifts = [0.0]*m
            alpha = 4*(2**(2/3)-1)
            def phi(t):
                return (1+alpha*(t/h)**2)**-1.5
            def direct(ll, shift=0):
                return sum(i*phi(x-q-shift) for q, i in ll)
            old = sum((j+1)*direct(lines[j]) for j in range(m))
            y = old + direct(lines[-1], .23) + rng.normal(0, .05, len(x))
            if case == 5:
                y = -np.abs(y)
            residual = y-old
            step = h/8
            origin = min(0., min(x))-max(abs(lo), abs(hi))-2*step
            end = max(180., max(x))+max(abs(lo), abs(hi))+2*step
            size = int(np.ceil((end-origin)/step))+1
            grid = origin + np.arange(size)*step
            # Independent direct kernel matrix, no FFT and no C++ sampled data.
            kernel = phi(grid[:, None]-grid[None, :])
            def deposit(xx, vv):
                out = np.zeros(size)
                for pos, value in zip(xx, vv):
                    t = (pos-origin)/step; j = min(int(t), size-2); f = t-j
                    out[j] += (1-f)*value; out[j+1] += f*value
                return out
            W = deposit(x, w); V = deposit(x, w*y); R = deposit(x, w*residual)
            denominator = np.dot(w, y*y)
            F, Fr, G = kernel@V, kernel@R, (kernel*kernel)@W
            scan = sorted(set([lo, hi, np.clip(0, lo, hi)] +
                             [lo+j*h/4 for j in range(1, int((hi-lo)/(h/4))+1) if lo+j*h/4<hi]))
            merged = {}
            for q, intensity in lines[-1]:
                merged[q] = merged.get(q, 0)+intensity
            def best(field):
                scores = []
                for shift in scan:
                    C = sum(i*np.interp(q+shift, grid, field) for q, i in merged.items())
                    B = sum(i*i*np.interp(q+shift, grid, G) for q, i in merged.items())
                    scores.append(max(0, C)**2/B/denominator)
                k = int(np.argmax(scores)); return scores[k], scan[k]
            a, sa = best(F); b, sb = best(Fr)
            occupied = W>0; xx = grid[occupied]
            sw = np.sqrt(W[occupied]/denominator); z = V[occupied]/np.sqrt(W[occupied])/np.sqrt(denominator)
            def profile(ll):
                return kernel@deposit([q for q, i in ll], [i for q, i in ll])
            def column(p, shift):
                return sw*np.interp(xx-shift, grid, p)
            columns = []
            for j in range(m):
                pp = profile(lines[j])
                columns.extend(column(pp, d) for d in sorted(set([max(lo, shifts[j]-h/4), shifts[j], min(hi, shifts[j]+h/4)])))
            base = np.column_stack(columns) if columns else np.empty((len(z), 0))
            qbase = nnls(base, z)[1]**2 if columns else np.dot(z, z)
            pp = profile(lines[-1]); qbest = qbase
            intervals = set((max(lo, s-h/4), s) for s in (sa, sb)) | set((s, min(hi, s+h/4)) for s in (sa, sb))
            proposals = []
            for left, right in sorted(intervals):
                matrix = np.column_stack([base]+[column(pp, s) for s in sorted(set([left, right]))])
                qbest = min(qbest, nnls(matrix, z)[1]**2)
                groups = [sorted(set([max(lo,s-h/4),s,min(hi,s+h/4)])) for s in shifts]
                groups.append(sorted(set([left,right])))
                proposals.append((matrix, groups, sa if sa in (left,right) else sb))
            expected = np.array([a, b, sa, sb, qbase, qbest, qbase-qbest])
            rows = [f'{len(x)} {m} {h} {lo} {hi}']
            rows += [' '.join(format(float(v), '.17g') for v in r) for r in zip(x, y, np.zeros(len(x)), w, residual)]
            for j, ll in enumerate(lines):
                rows.append((f'{j+1 if j<m else 0} ' if args.backprojection else '')+f'0 {len(ll)}')
                rows += [f'{q:.17g} {i:.17g}' for q, i in ll]
            result = subprocess.run([str(args.probe)]+(['backprojection'] if args.backprojection else []), input='\n'.join(rows)+'\n', text=True, capture_output=True)
            if args.backprojection:
                # Verify each proposed linear solution against independent SciPy,
                # then independently reconstruct its physical parameters and objective.
                # Rank-deficient coefficients need not match a particular NNLS solution.
                errors = []; checks = []; actual = None
                try:
                    actual = json.loads(result.stdout)
                    zz = np.sqrt(w/denominator)*y
                    rr = np.sqrt(w/denominator)*residual
                    q0 = float(rr@rr)
                    errors.append(abs(actual['baseline_q']-q0))
                    checks += [len(actual['trials'])==len(proposals),actual['calls']==2*len(proposals)]
                    gains = [0.0]
                    for trial,(matrix,groups,initial_shift) in zip(actual['trials'],proposals):
                        checks.append(bool(trial['initial_shift']==initial_shift))
                        coefficients=np.asarray(trial['coefficients'])
                        checks.append(bool(np.all(coefficients>=0)))
                        linear_residual=matrix@coefficients-z
                        independent_q=nnls(matrix,z)[1]**2
                        errors += [abs(float(linear_residual@linear_residual)-independent_q),abs(trial['linear_q']-independent_q)]
                        ss=[];offset=0
                        for j,group in enumerate(groups):
                            cc=coefficients[offset:offset+len(group)];total=float(sum(cc))
                            ss.append(float(np.dot(cc,group)/total) if total>0 else (shifts[j] if j<m else initial_shift))
                            offset+=len(group)
                        physical=np.column_stack([np.sqrt(w/denominator)*direct(ll,shift) for ll,shift in zip(lines,ss)])
                        scale, norm=nnls(physical,zz)
                        f=trial['physical'];product_scale=np.asarray(f['scales'])
                        pr=zz-physical@product_scale
                        change=physical@product_scale-(zz-rr)
                        gain=float(2*rr@change-change@change)
                        errors += [float(np.max(np.abs(np.asarray(f['shifts'])-ss))),abs(f['q']-norm**2),
                                   abs(float(pr@pr)-norm**2),abs(f['gain']-gain),abs(f['difference_gain']-(q0-f['q']))]
                        checks.append(bool(np.all(product_scale>=0)))
                        # KKT directly in the original physical matrix (scale invariant columns).
                        norms=np.linalg.norm(physical,axis=0);valid=norms>0
                        gradient=(physical[:,valid]/norms[valid]).T@(-pr)
                        active=product_scale[valid]>0
                        violation=np.where(active,np.abs(gradient),np.maximum(0,-gradient))
                        errors.append(float(np.max(violation)) if len(violation) else 0.)
                        gains.append(gain)
                    errors += [abs(actual['best']['gain']-max(gains)),abs(actual['best']['q']-(q0-max(gains)))]
                    error=max(errors)
                    passed=result.returncode==0 and all(checks) and error<=2e-10
                except (ValueError,KeyError,TypeError,IndexError) as exc:
                    error=None;passed=False;checks.append(str(exc))
                report['cases'].append(dict(id=f'm{m}-{case}',passed=bool(passed),maximum_error=error,
                    checks=checks,actual=actual,stdout=result.stdout if actual is None else None,stderr=result.stderr))
                continue
            actual = np.fromstring(result.stdout, sep=' ')
            error = float(np.max(np.abs(actual-expected))) if actual.shape == expected.shape else None
            passed = result.returncode == 0 and error is not None and error <= 2e-10
            report['cases'].append(dict(id=f'm{m}-{case}', passed=bool(passed), maximum_error=error,
                                        expected=expected.tolist(), actual=actual.tolist(), stderr=result.stderr))
    report['passed'] = all(c['passed'] for c in report['cases'])
    args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f"Fast PFSM independent reference: {sum(c['passed'] for c in report['cases'])}/24")
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
