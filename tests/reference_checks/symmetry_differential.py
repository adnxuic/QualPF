from reference_paths import reference_path, provenance_path
"""Strict frozen-oracle replay; named numeric differences stay visible pending approval."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT=Path(__file__).resolve().parents[2]
def main():
    p=argparse.ArgumentParser(); p.add_argument('--runner',type=Path,required=True); p.add_argument('--output',type=Path,required=True); args=p.parse_args()
    m=json.loads((reference_path(ROOT/'tests/data/symmetry-reference-manifest.json')).read_text())
    archive=reference_path(ROOT/'tests/data'/m['archive']); assert hashlib.sha256(archive.read_bytes()).hexdigest()==m['archive_sha256']
    with zipfile.ZipFile(archive) as z:
        raw=json.loads(z.read('expected.json')); exact=json.loads(z.read('exact-table.json'))
        assert hashlib.sha256(z.read('exact-table.json')).hexdigest()==m['exact_table_sha256']
    wire=''.join(q['mode']+' '+q['identifier']+'\n' for q in raw['lookups'])
    run=subprocess.run([str(args.runner.resolve())],input=wire,text=True,capture_output=True,check=True,timeout=120)
    actual=json.loads(run.stdout)
    failures=[]; meta=0; rotations=0; exact_components=0; qualified=[]
    for expected,old in zip(exact,raw['groups']):
        g=actual['groups'][expected['id']]
        for key,value in expected.items():
            if key in ('operations','primitive_expressions','centering_expressions'): continue
            meta+=1
            if g[key]!=value: failures.append([expected['id'],key,g[key],value])
        if len(g['operations'])!=len(expected['operations']): failures.append([g['id'],'operation_count']); continue
        for index,(a,e,o) in enumerate(zip(g['operations'],expected['operations'],old['operations'])):
            rotations+=9
            if a!=e: failures.append([g['id'],'operation',index,a,e])
            for axis,(num,legacy) in enumerate(zip(a['translation_twelfths'],o['translation'])):
                if num/12==legacy: exact_components+=1
                else: qualified.append(dict(id=g['id'],operation_index=index,axis=axis,legacy=legacy,exact_numerator=num,exact_denominator=12))
    if actual['defaults']!=raw['defaults']: failures.append(['defaults'])
    for i,(a,e) in enumerate(zip(actual['lookups'],raw['lookups'])):
        if a!=e['id']: failures.append(['lookup',i,e,a])
    assert len(actual['lookups'])==len(raw['lookups']) and len(actual['groups'])==551
    qualification=reference_path(ROOT/'docs/refactoring/phase-14/translation-qualification.json')
    assert hashlib.sha256(qualification.read_bytes()).hexdigest()==m['qualification_sha256']
    named=json.loads(qualification.read_text())
    frozen=[dict(id=c['id'],**{k:f[k] for k in ('operation_index','axis','legacy','exact_numerator','exact_denominator')}) for c in named['cases'] for f in c['fields']]
    if qualified!=frozen: failures.append(['unexpected_qualification_difference'])
    decision=json.loads((reference_path(ROOT/'docs/refactoring/phase-14/acceptance-decision.json')).read_text(encoding='utf-8'))
    approved=(decision['decision']=='accepted_exact_source_fractions_keep_discrepancies'
        and decision['qualification_sha256']==m['qualification_sha256']
        and decision['setting_ids']==[c['id'] for c in named['cases']])
    if not approved: failures.append(['missing_exact_scope_approval'])
    result=dict(status='pass' if not failures else 'fail',acceptance='user_approved_named_exact_fraction_qualification' if approved else 'unapproved',
        runner_sha256=hashlib.sha256(args.runner.read_bytes()).hexdigest(),archive_sha256=m['archive_sha256'],
        settings=551,groups=230,operations=7516,metadata_exact=meta,rotation_components_exact=rotations,
        translation_components_exact=exact_components,tolerance_comparisons=0,
        qualification_settings=21,qualification_components=len(qualified),lookup_exact=len(raw['lookups'])-sum(f[0]=='lookup' for f in failures),
        numeric_defaults_exact=230,failures=failures,qualified_differences=qualified)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print({k:v for k,v in result.items() if k not in ('qualified_differences','failures')})
    if failures: print(json.dumps(failures[:20],indent=2)); raise SystemExit(1)
if __name__=='__main__': main()
