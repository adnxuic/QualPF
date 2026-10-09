from reference_paths import reference_path
"""Replay immutable native observations; preserve every failed comparison."""
import argparse
import collections
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import zipfile

ROOT=Path(__file__).resolve().parents[2]
def fingerprint(x): return hashlib.sha256(json.dumps(x,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def main():
    p=argparse.ArgumentParser(); p.add_argument('--runner',required=True,type=Path); p.add_argument('--output',required=True,type=Path)
    p.add_argument('--compiler',required=True,choices=['GNU','MSVC'])
    a=p.parse_args()
    manifest=json.loads((reference_path(ROOT/'tests/data/reflection-reference-manifest.json')).read_text())
    archive=reference_path(ROOT/'tests/data'/manifest['archive'])
    assert hashlib.sha256(archive.read_bytes()).hexdigest()==manifest['archive_sha256']
    with zipfile.ZipFile(archive) as z:
        for name,digest in manifest['files'].items(): assert hashlib.sha256(z.read(name)).hexdigest()==digest,name
        cases=json.loads(z.read('cases.json')); expected=json.loads(z.read('expected.json'))
        wire=z.read('input.txt').decode().split('\n',1)[1]
    env=os.environ.copy()
    run=subprocess.run([str(a.runner.resolve())],input=wire,text=True,capture_output=True,env=env,timeout=300)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.with_suffix('.stdout.json').write_text(run.stdout)
    a.output.with_suffix('.stderr.txt').write_text(run.stderr)
    if run.returncode: raise RuntimeError(f'Replay failed {run.returncode}')
    actual=json.loads(run.stdout); assert len(actual)==len(expected)
    counts=collections.Counter(); failures=[]
    def leaves(value):
        return sum(map(leaves,value)) if isinstance(value,(list,tuple)) else 1
    def compare(case,field,old,new,location,exact=True):
        counts['exact_comparisons' if exact else 'tolerance_comparisons']+=1
        if exact: counts['exact_leaf_comparisons']+=leaves(old)
        if exact: good=old==new
        elif field=='d': good=math.isfinite(new) and abs(old-new)<=2e-5*abs(old)
        else: good=math.isfinite(new) and abs(old-new)<=1e-4+2e-6*abs(old)
        if not good:
            failure=dict(case=case['id'],name=case['name'],group_id=case['group_id'],field=field,
                         location=location,legacy=old,cpp=new)
            failure['fingerprint']=fingerprint(failure)
            failures.append(failure)
    for c,old,new in zip(cases,expected,actual):
        assert c['id']==old['id']==new['id']
        assert len(old['probes'])==len(new['probes']) and len(old['enumerations'])==len(new['enumerations'])
        for j,(q,r) in enumerate(zip(old['probes'],new['probes'])):
            if q['hkl']==[0,0,0]:
                compare(c,'zero_contract','zero_hkl',r.get('error'),dict(probe=j)); continue
            for field in ('hkl','images','orbit','powder_orbit','canonical','multiplicity','absent','d','two_theta'):
                compare(c,field,q[field],r.get(field),dict(probe=j,hkl=q['hkl']),field not in ('d','two_theta'))
        for j,(left,right) in enumerate(zip(old['enumerations'],new['enumerations'])):
            loc=dict(enumeration=j)
            if not isinstance(right,list): compare(c,'enumeration_error','success',right,loc); continue
            lhs={tuple(r['hkl']):r for r in left}; rhs={tuple(r['hkl']):r for r in right}
            compare(c,'membership',sorted(lhs),sorted(rhs),loc)
            compare(c,'ordering',[r['hkl'] for r in left],[r['hkl'] for r in right],loc)
            for h in sorted(lhs.keys() & rhs.keys()):
                for field in ('multiplicity','d','two_theta'):
                    compare(c,field,lhs[h][field],rhs[h][field],dict(enumeration=j,hkl=h),field=='multiplicity')
    acceptance=reference_path(ROOT/'docs/refactoring/phase-15/acceptance-decision.json')
    approved=set()
    if acceptance.exists():
        decision=json.loads(acceptance.read_text(encoding='utf-8'))
        records=reference_path(ROOT/'docs/refactoring/phase-15/qualification-records.json')
        if decision.get('status')=='approved':
            assert hashlib.sha256(records.read_bytes()).hexdigest()==decision['records_sha256']
            approved=set(decision['approved_fingerprints'][a.compiler])
            record_data=json.loads(records.read_text())
            assert approved=={r['comparison']['fingerprint'] for r in record_data['records'] if a.compiler in r['compilers']}
            proof=records.with_name('qualification-proof.json')
            assert hashlib.sha256(proof.read_bytes()).hexdigest()==record_data['proof_sha256']
    raw={f['fingerprint'] for f in failures}
    unexpected=raw-approved; stale=approved-raw
    report=dict(status='passed' if not unexpected and not stale else 'failed',compiler=a.compiler,
        runner_sha256=hashlib.sha256(a.runner.read_bytes()).hexdigest(),archive_sha256=manifest['archive_sha256'],
        counts=dict(counts),settings=551,groups=230,cases=len(cases),raw_failed_comparisons=failures,
        raw_failure_count=len(failures),approved_failure_count=len(raw & approved),
        unexpected_count=len(unexpected),stale_approval_count=len(stale),
        failure_categories=dict(collections.Counter(f['field'] for f in failures)))
    a.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='raw_failed_comparisons'},indent=2))
    return int(report['status']!='passed')
if __name__=='__main__': raise SystemExit(main())
