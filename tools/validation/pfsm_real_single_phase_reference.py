#!/usr/bin/env python3
"""Independent dense-grid/Brent check of a completed real-data initial PFSM round.

The experimental/background/weight snapshot is the fixed input problem, not an
independent validation of background estimation. Expected fit values are computed
from SQL d/I, the mathematical IL expression and SciPy 1.16.2, never from product
fit parameters. This diagnostic does not generate regression goldens or rankings.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import sys


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--round-directory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dependency-dir', type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError('Refusing to overwrite independent comparison evidence')
    if args.dependency_dir:
        sys.path.insert(0, str(args.dependency_dir.resolve()))
    import numpy as np
    import scipy
    from scipy.optimize import minimize_scalar
    if scipy.__version__ != '1.16.2':
        raise ValueError('Requires frozen SciPy 1.16.2')
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    run = json.loads((args.round_directory.parent / 'run.json').read_text(encoding='utf-8'))
    if digest(args.manifest) != run['manifest_sha256']:
        raise ValueError('Manifest differs from the completed round source')
    if digest(args.database) != manifest['database_hashes']['.sq']:
        raise ValueError('Database differs from frozen manifest')
    item = next(c for c in manifest['cases'] if c['id'] == args.round_directory.name)
    baseline = args.round_directory / 'baseline-0.tsv'
    width_file = args.round_directory / 'width.json'
    ranking_file = args.round_directory / 'round-0.jsonl'
    # Only identity is used to define the reference problem. No product shifts,
    # scales, objectives, labels or termination information seed the optimizer.
    with ranking_file.open(encoding='utf-8') as stream:
        product = json.loads(next(stream))
    db = sqlite3.connect(args.database.resolve().as_uri() + '?mode=ro', uri=True)
    try:
        nd, d, intensity = db.execute(
            'select nd,dvalue,intensita from id where id=?', (product['id'],)).fetchone()
    finally:
        db.close()
    d = np.array([float(v) for v in d.rstrip(',').split(',')])
    intensity = np.array([float(v) for v in intensity.rstrip(',').split(',')])
    if not nd or len(d) != nd or len(intensity) != nd:
        raise ValueError('Invalid paired reference')
    x, y, background, weights = np.loadtxt(baseline, usecols=(0, 1, 2, 3), unpack=True)
    raw_file = args.manifest.parent / item['data_file']
    if digest(raw_file) != item['data_sha256']:
        raise ValueError('Observation file differs from frozen manifest')
    raw_x, raw_y = np.loadtxt(raw_file, usecols=(0, 1), unpack=True)
    if not np.array_equal(raw_x, x) or not np.array_equal(raw_y, y):
        raise ValueError('Published observations differ from original frozen observations')
    net = y - background
    width = json.loads(width_file.read_text(encoding='utf-8'))['H0_degrees']
    positions, strengths = [], []
    components = item['radiation']
    total = sum(c['weight'] for c in components)
    # Preserve line/component ordering; unreachable entries are dropped as pairs.
    for spacing, strength in zip(d, intensity):
        for component in components:
            ratio = component['wavelength_angstrom'] / (2 * spacing)
            if 0 < ratio <= 1:
                positions.append(2 * np.degrees(np.arcsin(ratio)))
                strengths.append(strength * component['weight'] / total)
    delta = x[:, None] - np.array(positions)[None, :]
    strengths = np.array(strengths)
    alpha = 4 * (2 ** (2 / 3) - 1)

    def reference(shift):
        column = np.sum(strengths * (1 + alpha * ((delta - shift) / width) ** 2) ** -1.5, axis=1)
        norm = np.dot(weights, column * column)
        scale = max(0., np.dot(weights * column, net) / norm) if norm > 0 else 0.
        residual = net - scale * column
        return float(np.dot(weights, residual * residual)), float(scale)

    # The interval comes from the frozen request, never from a product optimum.
    lower, upper = item.get('shift_bounds', [-.5, .5])
    if not np.isfinite([lower, upper]).all() or lower >= upper:
        raise ValueError('Independent grid requires an ordered finite nonfixed interval')
    grid = np.linspace(lower, upper, 2049)
    values = [reference(t)[0] for t in grid]
    candidates = [(values[0], grid[0]), (values[-1], grid[-1])]
    for k in range(1, len(grid) - 1):
        if values[k] <= values[k - 1] and values[k] <= values[k + 1]:
            optimum = minimize_scalar(lambda t: reference(t)[0], method='bounded',
                                      bounds=(grid[k - 1], grid[k + 1]),
                                      options={'xatol': 1e-13, 'maxiter': 1000})
            if not optimum.success:
                raise ValueError('Independent Brent refinement did not converge')
            candidates.append((float(optimum.fun), float(optimum.x)))
    q, shift = min(candidates)
    _, scale = reference(shift)
    parameters = product['parameters']
    if len(parameters['scales']) != 1 or len(parameters['shifts_degrees']) != 1:
        raise ValueError('This check requires an initial single-column candidate')
    errors = dict(q=abs(q - product['Q']), scale=abs(scale - parameters['scales'][0]),
                  shift=abs(shift - parameters['shifts_degrees'][0]))
    tolerances = dict(q=2e-8 * max(1., q), scale=2e-5, shift=2e-5)
    passed = all(errors[k] <= tolerances[k] for k in tolerances)
    evidence = dict(schema='qualpf.pfsm.real-single-phase-reference.v1',
                    case=item['id'], card=product['id'], scipy=scipy.__version__,
                    scope='Independent fitter check conditional on the frozen background and weights; not whole-case identification acceptance.',
                    product_code_revision=run['code_revision'],
                    product_executable_sha256=run['executable_sha256'],
                    input_hashes={str(p): digest(p) for p in
                                  (args.manifest, raw_file, baseline, width_file, ranking_file)},
                    interval=[lower, upper], grid_points=len(grid), refined_basins=len(candidates)-2,
                    reference=dict(q=q, scale=scale, shift_degrees=shift),
                    product=dict(q=product['Q'], parameters=parameters),
                    absolute_errors=errors, tolerances=tolerances, passed=passed)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(evidence, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(case=item['id'], passed=passed, absolute_errors=errors)))
    return 0 if passed else 1


if __name__ == '__main__':
    sys.exit(main())
