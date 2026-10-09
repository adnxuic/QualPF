"""Verify C++ numerical replay against the frozen native reference."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def save(p, obj):
    p.write_text(json.dumps(obj, indent=2, allow_nan=False) + '\n', encoding='utf-8')


def samples():
    # Tolerances are the Phase 4 proposed bounds, fixed BEFORE capture/comparison.
    rows = []
    def add(id, op, x, y=None, extra='', m=1, atol=1e-6, rtol=1e-5):
        wire = f'{op} {len(x)} {m}\n' + ' '.join(map(str,x)) + '\n'
        wire += ' '.join(map(str,y if y is not None else [0]*len(x))) + '\n' + extra + '\n'
        rows.append(dict(id=id, wire=wire, absolute=atol, relative=rtol))
    add('N01-reductions',1,[3,4,-2,1],[2,-1,3,4],atol=1e-12,rtol=1e-10)
    add('N02-location',2,[0,2,5],extra='-1\n0\n1\n2\n3.5\n5\n9',m=7,atol=0,rtol=0)
    add('N03-bragg-d',3,[20,60,120,179,180],extra='1.54056',atol=1e-5,rtol=0)
    add('N03-bragg-angle',11,[1,2,4,8],extra='1.54056',atol=1e-4,rtol=0)
    add('N04-sample',4,[1,2,3,4])
    add('N04-constant',4,[5,5,5,5])
    add('N04-offset-dp',10,[1e12+1,1e12+2,1e12+3,1e12+4],atol=1e-12,rtol=1e-10)
    add('N05-trapezoid',5,[0,1,3],[1,3,7])
    add('N05-singleton',5,[0],[5])
    add('N06-natural',6,[0,1,3],[1,3,7],extra='1e31 1e31\n-1\n0\n0.5\n2\n3\n4',m=6)
    add('N06-clamped',6,[0,1],[0,1],extra='0 0\n0\n0.25\n0.5\n0.75\n1',m=5)
    add('N06-cubic',6,[0,1,2,4],[0,1,8,64],extra='0 48\n0.5\n1.5\n3\n5',m=4)
    add('N07-pivot-lu',7,[0,0],[4,7],extra='0 2\n1 3',atol=1e-12,rtol=1e-10)
    add('N08-least-squares',8,[0,1,2,3],[1,2,2,4],extra='1 0\n1 1\n1 2\n1 3',m=2,atol=1e-12,rtol=1e-10)
    add('N08-polynomial',9,[-2,-1,0,1,2],[9,2,1,6,17],m=3)
    return rows

def execute(runner, wire):
    run = subprocess.run([str(runner)],input=wire,text=True,capture_output=True,timeout=60,check=True)
    values = [float(x) for x in run.stdout.split()]
    if not values or not all(math.isfinite(x) for x in values):
        raise ValueError('Nonfinite or missing oracle/replay output')
    return run, values


def verify(runner, path, report=None):
    manifest=json.loads(path.read_text(encoding='utf-8'))
    if manifest['kind']!='legacy_numerical_capture' or not manifest['cases']:
        raise ValueError('Invalid numerical manifest')
    policy={r['id']:r for r in samples()}
    if {c['id'] for c in manifest['cases']} != set(policy):
        raise ValueError('Incomplete numerical reference collection')
    protected={path.resolve(),runner.resolve()}
    protected.update((path.parent/c[k]['path']).resolve() for c in manifest['cases'] for k in ('input','raw','expected'))
    for descriptor in manifest.get('evidence',[]):
        artifact=(path.parent/descriptor['path']).resolve()
        if sha(artifact)!=descriptor['sha256']:raise ValueError('Evidence hash mismatch')
        protected.add(artifact)
    if report and report.resolve() in protected:
        raise ValueError('Report must not overwrite reference evidence or runner')
    ids=set(); outcomes=[]
    for case in manifest['cases']:
        if case['id'] in ids or case['status']!='reviewed': raise ValueError('Unreviewed/duplicate case')
        ids.add(case['id'])
        files={}
        for key in ('input','raw','expected'):
            p=path.parent/case[key]['path']
            if sha(p)!=case[key]['sha256']: raise ValueError('Hash mismatch: '+str(p))
            files[key]=p
        expected=json.loads(files['expected'].read_text())
        if expected != [float(x) for x in files['raw'].read_text().split()]:
            raise ValueError('Expected does not match raw Fortran output')
        _,actual=execute(runner,files['input'].read_text())
        t=case['tolerance']; a=t['absolute']; r=t['relative']
        approved=policy[case['id']]
        if (a,r)!=(approved['absolute'],approved['relative']):
            raise ValueError('Frozen tolerance changed')
        if files['input'].read_text().strip()!=approved['wire'].strip():
            raise ValueError('Frozen input changed')
        if not all(math.isfinite(v) and v>=0 for v in (a,r)): raise ValueError('Invalid tolerance')
        ok=len(actual)==len(expected) and all(math.isfinite(old) and abs(new-old)<=a+r*abs(old) for new,old in zip(actual,expected))
        outcomes.append(dict(id=case['id'],passed=ok,expected=expected,actual=actual))
        print(('PASS ' if ok else 'FAIL ')+case['id'])
    result=dict(passed=sum(c['passed'] for c in outcomes),failed=sum(not c['passed'] for c in outcomes),cases=outcomes,runner_sha256=sha(runner))
    if report: save(report,result)
    return 1 if result['failed'] else 0

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runner',required=True,type=Path);p.add_argument('--manifest',required=True,type=Path)
    p.add_argument('--report',type=Path)
    args=p.parse_args()
    raise SystemExit(verify(args.runner.resolve(),args.manifest.resolve(),args.report))
