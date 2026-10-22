#!/usr/bin/env python3
"""Independent revealed-case shift ablation; never supplies product parameters.

Reconstructs IL columns from the read-only frozen database and saved observations.
Checks recorded objectives before interpreting fixed/released-old-shift controls.
Post-hoc formal starts are labelled diagnostic, never a proposed search policy.
"""
import argparse
from functools import lru_cache
import hashlib
import json
import math
from pathlib import Path
import sqlite3
import sys


def digest(path):
    sha = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(2**20), b''):
            sha.update(block)
    return sha.hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def dependencies(directory):
    if directory:
        sys.path.insert(0, str(directory.resolve()))
    import numpy as np
    import scipy
    from scipy.optimize import least_squares, minimize_scalar, nnls
    if scipy.__version__ != '1.16.2':
        raise RuntimeError('Expected pinned SciPy 1.16.2')
    return np, scipy, least_squares, minimize_scalar, nnls


class Model:
    def __init__(self, np, nnls, observations, lines, width):
        self.np, self.nnls = np, nnls
        self.x, self.y, self.w = observations
        self.lines, self.width = lines, width
        self.a = width / math.sqrt(4 * (math.cbrt(4.) - 1))
        self.radius = self.a * (1-1e-4) / math.sqrt(1e-4*(2-1e-4))
        # The diagnostic input has no explicit break column. Reject gaps rather
        # than invent missing metadata; masks/negative values remain supported.
        if np.any(np.diff(self.x) > 3*np.median(np.diff(self.x))):
            raise RuntimeError('This saved-curve diagnostic requires gap-free input')
        bounds = np.r_[self.x[0], (self.x[:-1]+self.x[1:])/2, self.x[-1]]
        edges = np.linspace(self.x[0], self.x[-1], min(2048, np.count_nonzero(self.w))+1)
        segments = []
        for i in range(len(self.x)):
            if self.w[i] == 0:
                continue
            lo, hi = bounds[i:i+2]
            for j in range(max(0, np.searchsorted(edges, lo, side='right')-1),
                           min(len(edges)-1, np.searchsorted(edges, hi, side='left'))):
                l, r = max(lo, edges[j]), min(hi, edges[j+1])
                if r > l:
                    segments.append((i, j, l, r, self.w[i]/(hi-lo)))
        seg = np.array(segments)
        self.samples, self.bins = seg[:, 0].astype(int), seg[:, 1].astype(int)
        self.lo, self.hi, self.density = seg[:, 2], seg[:, 3], seg[:, 4]
        self.fw = np.bincount(self.bins, weights=self.density*(self.hi-self.lo), minlength=len(edges)-1)
        sums = np.bincount(self.bins, weights=self.density*(self.hi-self.lo)*self.y[self.samples], minlength=len(self.fw))
        self.fy = np.divide(sums, self.fw, out=np.zeros_like(sums), where=self.fw>0)

    @lru_cache(maxsize=64)
    def column(self, objective, phase, shift):
        np = self.np
        if objective == 'point':
            result = np.zeros(len(self.x))
            for center, strength in self.lines[phase]:
                v = (self.x-center-shift)/self.a
                result += strength / (1+v*v)**1.5
            return result
        numerator = np.zeros(len(self.fw))
        for center, strength in self.lines[phase]:
            center += shift
            first = np.searchsorted(self.hi, center-self.radius, side='right')
            last = np.searchsorted(self.lo, center+self.radius, side='left')
            l = np.maximum(self.lo[first:last]-center, -self.radius)
            r = np.minimum(self.hi[first:last]-center, self.radius)
            # Stable primitive difference; reflection allows one positive-side
            # rationalization, including tails where saturated CDFs lose digits.
            negative = r <= 0
            l, r = np.where(negative, -r, l), np.where(negative, -l, r)
            mass = .5*(r/np.hypot(r, self.a)-l/np.hypot(l, self.a))
            same = l > 0
            ll, rr = l[same], r[same]
            hl, hr = np.hypot(ll, self.a), np.hypot(rr, self.a)
            mass[same] = .5*self.a**2*(rr-ll)*(rr+ll)/(hl*hr*(rr*hl+ll*hr))
            numerator += np.bincount(self.bins[first:last], weights=strength*self.density[first:last]*mass, minlength=len(self.fw))
        return np.divide(numerator, self.fw, out=np.zeros_like(numerator), where=self.fw>0)

    def solve(self, objective, shifts):
        np = self.np
        y, w = (self.y, self.w) if objective == 'point' else (self.fy, self.fw)
        denominator = float(np.dot(w, y*y))
        columns = np.column_stack([self.column(objective, j, float(s)) for j, s in enumerate(shifts)])
        root = np.sqrt(w/denominator)
        scales = self.nnls(columns*root[:, None], y*root, maxiter=10000)[0]
        residual = (y-columns@scales)*root
        return residual, dict(Q=float(residual@residual*denominator), scales=scales.tolist(),
                              shifts_degrees=list(shifts), denominator=denominator)


def ablate(model, least_squares, minimize_scalar, old, formal, fine_shift):
    np = model.np
    result = {}
    for objective in ('point', 'fine'):
        baseline = model.solve(objective, old)[1]
        snapshots = {}
        for mask in range(1 << len(old)):
            shifts = [formal[j] if mask & (1 << j) else s for j, s in enumerate(old)]
            for candidate, shift in (('screen', fine_shift), ('formal', formal[-1])):
                name = f'old_mask_{mask}_candidate_{candidate}'
                row = model.solve(objective, shifts+[shift])[1]
                row['gain'] = baseline['Q']-row['Q']
                row['normalized_gain'] = row['gain']/row['denominator']
                snapshots[name] = row
        fits = []
        for mask in range(1 << len(old)):
            free = [j for j in range(len(old)) if mask & (1 << j)] + [len(old)]
            # Matched null model: a released old shift might improve the
            # two-phase baseline even without the candidate. Keep that control
            # separate from gain against the recorded common baseline.
            nulls = [baseline]
            if free[:-1]:
                for seed in (old, formal[:-1]):
                    def null_shifts(t):
                        full = old[:]
                        for j, value in zip(free[:-1], t):
                            full[j] = float(value*model.width)
                        return full
                    fit = least_squares(lambda t:model.solve(objective, null_shifts(t))[0],
                        np.array(seed)[free[:-1]]/model.width,
                        bounds=(-.5/model.width,.5/model.width), jac='3-point',
                        ftol=1e-8,xtol=1e-8,gtol=1e-8,max_nfev=100*len(old))
                    nulls.append(model.solve(objective,null_shifts(fit.x))[1])
            null = min(nulls,key=lambda r:r['Q'])
            starts = [old+[fine_shift], formal[:]]
            fitted = []
            for seed in starts:
                template = old+[fine_shift]
                def shifts(t):
                    full = template[:]
                    for j, value in zip(free, t):
                        full[j] = float(value*model.width)
                    return full
                fit = least_squares(lambda t:model.solve(objective, shifts(t))[0],
                    np.array(seed)[free]/model.width, bounds=(-.5/model.width, .5/model.width),
                    jac='3-point', ftol=1e-8, xtol=1e-8, gtol=1e-8,
                    max_nfev=100*(len(old)+1))
                row = model.solve(objective, shifts(fit.x))[1]
                row.update(status=int(fit.status), success=bool(fit.success), nfev=int(fit.nfev),
                           start=seed, free_indices=free, gain=baseline['Q']-row['Q'])
                row['normalized_gain'] = row['gain']/row['denominator']
                fitted.append(row)
            best = min(fitted,key=lambda r:r['Q'])
            fits.append(dict(released_old_mask=mask, starts=fitted, best=best,
                             null_controls=nulls, matched_null=null,
                             matched_null_gain=null['Q']-best['Q']))
        # Global 1-D scan/Brent control: distinguish finite candidate scanning
        # from the fixed-old constraint. This is diagnostic work, not v1 budget.
        grid = np.linspace(-.5, .5, 65)
        costs = [model.solve(objective, old+[float(t)])[1]['Q'] for t in grid]
        refined = []
        for j in range(len(grid)):
            if (not j or costs[j] <= costs[j-1]) and (j == len(grid)-1 or costs[j] <= costs[j+1]):
                fit = minimize_scalar(lambda t:model.solve(objective, old+[float(t)])[1]['Q'],
                    bounds=(grid[max(0,j-1)],grid[min(j+1,len(grid)-1)]), method='bounded',
                    options={'xatol':1e-10})
                refined.append(model.solve(objective, old+[float(fit.x)])[1])
        best = min(refined, key=lambda r:r['Q'])
        best.update(gain=baseline['Q']-best['Q'])
        best['normalized_gain'] = best['gain']/best['denominator']
        result[objective] = dict(baseline=baseline, snapshots=snapshots, fits=fits, fixed_old_scan_brent=best)
        print(f'{objective}: baseline {baseline["Q"]:.9f}; fixed-old best {best["Q"]:.9f}; released ' +
              ', '.join(f'{r["released_old_mask"]}:{r["best"]["Q"]:.9f}' for r in fits), flush=True)
    return result


def self_test(np, least_squares, minimize_scalar, nnls):
    from scipy.integrate import quad
    x = np.linspace(29,33,251)
    lines = [[(30,1)],[(31,1),(32,.4)],[(31.12,.8),(32.16,.5)]]
    h = .25
    truth = [.03,.3,.1]
    def column(refs,shift):
        return sum(s*(1+4*(2**(2/3)-1)*((x-q-shift)/h)**2)**-1.5 for q,s in refs)
    y = sum(scale*column(refs,shift) for refs,shift,scale in zip(lines,truth,[1,.8,.6]))
    weights = 1+.2*np.sin(x)
    weights[::13] = 0
    model = Model(np,nnls,(x,y,weights),lines,h)
    assert model.solve('point',truth)[1]['Q'] < 1e-20
    for phase in range(3):
        actual = model.column('fine',phase,truth[phase])
        for j in (40,120,190):
            expected = 0.
            for k in np.flatnonzero(model.bins == j):
                for q,s in lines[phase]:
                    left = max(model.lo[k],q+truth[phase]-model.radius)
                    right = min(model.hi[k],q+truth[phase]+model.radius)
                    if right > left:
                        value = quad(lambda t:s/(2*model.a)*(1+((t-q-truth[phase])/model.a)**2)**-1.5,
                                     left,right,epsabs=1e-12,epsrel=1e-12)[0]
                        expected += value*model.density[k]
            expected /= model.fw[j]
            assert abs(actual[j]-expected) <= 2e-10*max(1,abs(expected))
    result = ablate(model,least_squares,minimize_scalar,[.03,-.2],truth,.1)
    point = result['point']['fits']
    assert point[0]['best']['Q'] > .01
    assert point[2]['best']['Q'] < 1e-12 and point[3]['best']['Q'] < 1e-12
    assert point[2]['best']['shifts_degrees'][0] == .03
    print('PASS: independent quadrature, weighted NNLS and causal fixed/released-old-shift controls')
    return 0


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--case-directory', type=Path)
    p.add_argument('--manifest', type=Path)
    p.add_argument('--database', type=Path)
    p.add_argument('--output', type=Path)
    p.add_argument('--dependency-dir', type=Path)
    p.add_argument('--self-test', action='store_true')
    p.add_argument('--round', type=int, default=2)
    p.add_argument('--card', default='7218520')
    args = p.parse_args()
    np, scipy, least_squares, minimize_scalar, nnls = dependencies(args.dependency_dir)
    if args.self_test:
        return self_test(np,least_squares,minimize_scalar,nnls)
    if any(getattr(args,key) is None for key in ('case_directory','manifest','database','output')):
        p.error('case-directory, manifest, database and fresh output are required')
    args.output.mkdir(parents=True, exist_ok=False)
    summary = read(args.case_directory/'summary.json')
    trial = summary['rounds'][args.round]
    diagnostic = trial['forced_integral_diagnostic']
    selected = diagnostic['selected_identities']
    if len(selected) != 2 or trial['prescreen']['mode'] != 'off':
        raise RuntimeError('Expected a preserved two-old-phase full-pool diagnostic')
    manifest = read(args.manifest)
    item = next(c for c in manifest['cases'] if c['id'] == summary['id'])
    for suffix, expected in manifest['database_hashes'].items():
        path = args.database.with_suffix(suffix)
        if digest(path) != expected:
            raise RuntimeError(f'Frozen database hash mismatch: {path}')
    data = np.loadtxt(args.case_directory/f'baseline-{args.round}.tsv')
    # XRDML/TSV has no explicit discontinuity metadata for this diagnostic.
    if any(k in item for k in ('break_before', 'mask', 'gaps')):
        raise RuntimeError('Explicit source masks/breaks require additional provenance')
    formal_rows = [json.loads(line) for line in (args.case_directory/f'round-{args.round}.jsonl').read_text().splitlines()]
    card = next(c for c in formal_rows if c['id'] == args.card)
    screen_rows = [json.loads(line) for line in (args.case_directory/f'forced-integral-round-{args.round}/prescreen-{args.round}.jsonl').read_text().splitlines()]
    screen = next(c for c in screen_rows if c['id'] == args.card)
    old, formal = trial['baseline_parameters']['shifts_degrees'], card['parameters']['shifts_degrees']
    h = read(args.case_directory/'width.json')['H0_degrees']
    refs = []
    with sqlite3.connect(args.database.resolve().as_uri()+'?mode=ro', uri=True) as conn:
        for identity in selected+[args.card]:
            d, intensity, count = conn.execute('SELECT dvalue,intensita,nd FROM id WHERE id=?',(identity,)).fetchone()
            def tokens(value):
                return [float(v) for v in (value.decode() if isinstance(value,bytes) else value).rstrip(',').split(',')]
            d, intensity = tokens(d), tokens(intensity)
            if len(d) != len(intensity) or len(d) != count:
                raise RuntimeError('Invalid paired frozen reflections')
            refs.append([(2*math.asin(.5*r['wavelength_angstrom']/q)*180/math.pi,
                          strength*r['weight']/sum(v['weight'] for v in item['radiation']))
                         for q,strength in zip(d,intensity) for r in item['radiation']
                         if .5*r['wavelength_angstrom']/q <= 1 and r['weight']>0])
    model = Model(np, nnls, (data[:,0],data[:,1]-data[:,2],data[:,3]), refs, h)
    checks = []
    def check(name, actual, expected, tolerance):
        checks.append(dict(name=name, actual=actual, expected=expected, tolerance=tolerance,
                           passed=abs(actual-expected)<=tolerance*max(1,abs(expected))))
    check('point_baseline',model.solve('point',old)[1]['Q'],trial['baseline_Q'],2e-8)
    check('point_formal_location',model.solve('point',formal)[1]['Q'],card['Q'],2e-8)
    fine_base = model.solve('fine',old)[1]['Q']
    check('fine_baseline',fine_base,diagnostic['attempts'][-1]['fine_baseline_Q'],2e-10)
    check('fine_screen_gain',fine_base-model.solve('fine',old+[screen['fine_shift']])[1]['Q'],screen['fine_gain'],2e-10)
    evidence = [args.manifest, args.case_directory/'summary.json',args.case_directory/'width.json',
                args.case_directory/f'baseline-{args.round}.tsv',args.case_directory/f'round-{args.round}.jsonl',
                args.case_directory/f'forced-integral-round-{args.round}/prescreen-{args.round}.jsonl']
    report = dict(schema='qualpf.pfsm.shift-diagnostic.v1',scipy=scipy.__version__,
        qualification='Independent revealed-case ablation; matched diagnostic seeds include post-hoc formal solution; no routing, parameter, rank-repair or performance claim.',
        tool_sha256=digest(Path(__file__)), input_sha256={str(path.resolve()):digest(path) for path in evidence},
        card=args.card, formal_rank=card['rank'], selected_identities=selected, width=h,
        old_shifts=old, formal_shifts=formal, reproduction_checks=checks,
        fine_rank=1+sum(r['normalized_fine_gain']>screen['normalized_fine_gain'] for r in screen_rows if r['normalized_fine_gain'] is not None),
        fine_cutoff=sorted((r['normalized_fine_gain'] for r in screen_rows if r['normalized_fine_gain'] is not None),reverse=True)[63])
    path = args.output/'diagnostic.json'
    path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    if not all(c['passed'] for c in checks):
        print(json.dumps(checks,indent=2))
        return 1
    report['ablation'] = ablate(model,least_squares,minimize_scalar,old,formal,screen['fine_shift'])
    report['inputs_unchanged'] = all(digest(Path(path)) == sha for path,sha in report['input_sha256'].items())
    report['passed'] = report['inputs_unchanged'] and all(
        any(start['success'] for start in row['starts']) for layer in report['ablation'].values() for row in layer['fits'])
    path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
