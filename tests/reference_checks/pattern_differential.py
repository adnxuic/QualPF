from reference_paths import reference_path
"""Replay immutable native evidence; never recapture or update expected values."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--runner', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--manifest', type=Path, default=reference_path(ROOT/'tests/data/pattern-reference-manifest.json'))
    args = ap.parse_args()
    manifest = json.loads(args.manifest.read_text())
    archive = reference_path(ROOT/'tests/data'/manifest['archive'])
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == manifest['archive_sha256']
    policy = manifest['contract']
    results = []
    with tempfile.TemporaryDirectory() as temp:
        folder = Path(temp)
        with zipfile.ZipFile(archive) as z:
            z.extractall(folder)
        for case in manifest['cases']:
            directory = folder/case['id']
            path = directory/case['input']
            retained = case['input'] in policy.get('retained_legacy_formats', [])
            expected = 'accepted' if case['native_status']=='accepted' and case['input'] not in policy['validation_rejections'] and not retained else 'rejected'
            process = subprocess.run([str(args.runner.resolve()), str(path)], capture_output=True, text=True, timeout=15)
            actual = process.stdout.splitlines()
            errors = []
            if process.returncode or not actual or actual[0] != expected:
                errors.append(f'status expected={expected} actual={process.stdout[:200]} exit={process.returncode}')
            maximum = 0
            if not errors and expected == 'accepted':
                frozen = (directory/'stdout.txt').read_text().split('PATTERN11',1)[1].strip().splitlines()[1:]
                if actual[1].split() != frozen[0].split(): errors.append('size/radiation')
                if actual[3].split() != frozen[2].split(): errors.append('processing-known flags')
                nw = int(actual[1].split()[1])
                canonical = float(__import__('re').search(r'CU_KA1_WAVE = ([0-9.]+)', (reference_path(ROOT/'src/core/include/qualpf/domain/radiation.h')).read_text())[1])
                default32 = __import__('struct').unpack('f', __import__('struct').pack('f', canonical))[0]
                for line in range(nw):
                    ow, ore = map(float, frozen[4+line].split())
                    aw, ar = map(float, actual[5+line].split())
                    if ar != ore or aw != (canonical if ow == default32 else ow): errors.append('exact radiation')
                if actual[4] != str(path): errors.append('source identity')
                # Source path differs by extraction directory; exact basename is frozen.
                if Path(frozen[3].strip()).name != path.name: errors.append('source basename')
                old = [float(x) for row in frozen[1:2]+frozen[4:] for x in row.split()]
                new = [float(x) for row in actual[2:3]+actual[5:] for x in row.split()]
                if len(old) != len(new): errors.append('numeric shape')
                else:
                    for i,(a,b) in enumerate(zip(old,new)):
                        maximum=max(maximum,abs(a-b))
                        if not math.isclose(a,b,rel_tol=policy['grid_raw_current_relative_tolerance'],abs_tol=policy['grid_raw_current_absolute_tolerance']):
                            errors.append(f'field {i}: {a} != {b}')
                            break
            results.append({'id':case['id'],'native':case['native_status'],'expected':expected,'retained_legacy_boundary':retained,'passed':not errors,'maximum_absolute_difference':maximum,'errors':errors})
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps({'archive_sha256':manifest['archive_sha256'],'passed':sum(r['passed'] for r in results),'total':len(results),'cases':results},indent=2)+'\n')
    print(f"Pattern differential: {sum(r['passed'] for r in results)}/{len(results)}")
    for r in results:
        if not r['passed']: print(r)
    return int(any(not r['passed'] for r in results))

if __name__ == '__main__': raise SystemExit(main())
