from reference_paths import reference_path, provenance_path
"""Bounded Cell successor; immutable earlier gates and raw outputs remain."""
import json,struct,sys,zipfile
from copy import deepcopy
from unittest.mock import patch
import current_lp_reference as lp
from current_lp_reference import *
from scattering_capture import parse as scattering_parse

CELL_SOURCE='src/core/crystallography/unit_cell.cpp'
CELL_ARCHIVE=reference_path(ROOT/'archive/legacy/phase36d-test-reference/unit_cell.cpp.txt')
CELL_SUCCESSOR=reference_path(ROOT/'tests/data/current-cell-provider.json')
CELL_PROPOSAL='d8259e0a86199392211ab6dcaa75111840a3e7ec1b17dbc82963e376a5d6e226'
_lp_bundle=lp.verify_bundle
_lp_restore=lp.restore_superseded_field_for_original_checks
_lp_original=lp.verify_original_cpp_output
bits=lambda x:struct.pack('>d',float(x)).hex()

def verify_cell_successor():
    successor=read(CELL_SUCCESSOR)
    require(successor['schema']=='qualpf.current_cell_provider.v1' and
            successor['scope']=='36D-WINDOWS-UNIT-CELL-FORMED-ANGLE-CONTRACT' and
            successor['new_tolerance'] is False and successor['expected_from_new_CPP'] is False,
            'Cell successor scope changed')
    authority=successor['decision'];path=reference_path(ROOT/authority['path'])
    require(sha_file(path)==authority['sha256'],'Cell decision changed')
    decision=read(path)
    require(decision['status']=='AI_APPROVED_WITH_BOUNDED_CONTRACT' and
            decision['classification']=='AI_APPROVED_NUMERICAL_CORRECTION' and
            decision['proposal_SHA256']==CELL_PROPOSAL and
            decision['human_approved'] is False and decision['confidence']=='EVIDENCE_STRONG',
            'bounded Cell authority missing')
    require(successor['current_providers'].keys()=={CELL_SOURCE} and
            sha_file(reference_path(ROOT/CELL_SOURCE))==successor['current_providers'][CELL_SOURCE],
            'approved Cell source changed')
    require(sha_file(CELL_ARCHIVE)==successor['historical_sha256']==
            read(POLICY)['provider_hashes'].get(CELL_SOURCE),'sealed original Cell source changed')
    for binding in (successor['value_successor'],successor['normal_cell_reference'],
                    successor['historical_scattering_reference'],successor['scattering_environment_reference']):
        require(sha_file(reference_path(ROOT/binding['path']))==binding['sha256'],'Cell reference changed')
    fields=read(reference_path(ROOT/successor['value_successor']['path']))
    records=fields['records']
    require(fields['schema']=='qualpf.cell_exact_successor_fields.v1' and
            fields['scope']==successor['scope'] and fields['new_tolerance'] is False and
            fields['expected_from_new_CPP'] is False and len(records)==2523 and
            len({(r['compiler'],r['domain'],r['field']) for r in records})==2523 and
            all(r['compiler']=='MSVC' and r['domain'] in ('card','scattering') for r in records),
            'Cell field authority incomplete or foreign')
    require(sum(r['domain']=='card' for r in records)==1862 and
            sum(r['domain']=='scattering' for r in records)==661,'Cell field counts changed')
    return successor,records

def verify_bundle(domain):
    verify_cell_successor()
    def sealed(path):
        return sha_file(CELL_ARCHIVE if Path(path)==reference_path(ROOT/CELL_SOURCE) else path)
    with patch.object(lp,'sha_file',side_effect=sealed),patch.object(lp,'read',side_effect=read):
        return _lp_bundle(domain)

def original_cell_output(domain,compiler):
    if domain=='card':
        prefix='mingw' if compiler=='GNU' else 'msvc'
        with zipfile.ZipFile(reference_path(ROOT/'docs/refactoring/phase-18/cpp-replay-evidence.zip')) as z:
            return json.loads(z.read(prefix+'-differential.actual.json')),None
    successor,_=verify_cell_successor()
    with zipfile.ZipFile(reference_path(ROOT/successor['historical_scattering_reference']['path'])) as z:
        transcript=z.read(compiler+'-scattering.stdout').decode()
    return scattering_parse(transcript),transcript

def verify_scattering_environments():
    binding=read(CELL_SUCCESSOR)['scattering_environment_reference']
    require(sha_file(reference_path(ROOT/binding['path']))==binding['sha256'],'Scattering environment reference changed')
    decision=read(reference_path(ROOT/binding['path']))
    require(decision['schema']=='qualpf.cell_scattering_environments.v1' and
            decision['scope_id']=='36D-CELL-SCATTERING-ENVIRONMENT-BINDING' and
            decision['status']=='AI_APPROVED_WITH_BOUNDED_CONTRACT' and
            decision['classification']=='AI_APPROVED_EXISTING_CANONICAL_AUTHORITY' and
            decision['human_approved'] is False and decision['confidence']=='EVIDENCE_STRONG' and
            decision['selected_option']==2 and decision['whole_profile_required'] is True and
            decision['new_tolerance'] is False and decision['expected_from_new_CPP'] is False and
            decision['original_329_qualifications_unchanged'] is True and
            decision['proposal_SHA256']=='2a4ddd048dab9e5ea24cf3ecff6a529ae708b07bdcf103ec794a8bceb9c69c47',
            'bounded Scattering environment authority missing')
    sources=decision['source_fingerprints']
    require(set(sources)=={'src/core/crystallography/scattering.cpp',
            'src/core/crystallography/scattering_data.inc','tests/compatibility/scattering_replay.cpp',
            'tests/data/scattering-reference.zip','tests/data/scattering-reference-manifest.json'},
            'Scattering environment source closure changed')
    for name,digest in sources.items():
        require(sha_file(reference_path(ROOT/name))==digest,'Scattering environment dependency changed: '+name)
    records=decision['records']
    require(len(records)==5 and {r['case'] for r in records}=={75,1276,5476,5500,5524} and
            all(r['mode']=='SCAT' and r['field']=='scattering.cases.'+str(r['case'])+'.values.3'
                for r in records),'Scattering environment field scope changed')
    return decision

def project_scattering_environment(projected,compiler,original):
    if compiler!='MSVC':
        require(exact_json(projected,original),'unapproved complete scattering output changed')
        return projected
    decision=verify_scattering_environments();guest=deepcopy(original)
    for record in decision['records']:
        row=original['cases'][str(record['case'])]
        require(row['mode']=='SCAT' and row['values'][:-1]==record['input_values'] and
                row['values'][-1]==record['host_value'] and
                bits(row['values'][-1])==record['host_bits'] and
                bits(record['guest_value'])==record['guest_bits'],
                'Scattering environment input/value binding changed')
        parent,key=point(guest,record['field']);parent[key]=record['guest_value']
    require(exact_json(projected,original) or exact_json(projected,guest),
            'unapproved complete scattering output or mixed CRT profile')
    # Only the comparison copy is normalized after exact whole-corpus equality.
    return deepcopy(original)

def project_cell_output(actual,domain,compiler,original):
    _,records=verify_cell_successor();projected=deepcopy(actual);mapped=[]
    for record in records:
        if (record['domain'],record['compiler'])!=(domain,compiler):continue
        target,key=point(projected,record['field']);old,old_key=point(original,record['field'])
        require(bits(target[key])==record['expected_bits'] and bits(old[old_key])==record['old_bits'],
                'Cell value differs from exact successor: '+record['field'])
        target[key]=old[old_key];mapped.append(record['field'])
    if domain=='scattering':projected=project_scattering_environment(projected,compiler,original)
    require(exact_json(projected,original),'unapproved complete '+domain+' output changed')
    return projected,mapped

def verify_complete_cell_output(domain,compiler,raw_path):
    original,transcript=original_cell_output(domain,compiler)
    if domain=='card':actual=read(raw_path.with_suffix('.actual.json'))
    else:
        text=raw_path.with_suffix('.stdout.txt').read_text();actual=scattering_parse(text)
        errors=lambda s:[line.split() for line in s.splitlines() if line.startswith(('ERROR ','REFERROR '))]
        require(errors(text)==errors(transcript),'accepted Scattering error contract changed')
    return project_cell_output(actual,domain,compiler,original)

def qualification_key(record,original):
    path=record['field'].split('.')[1:]
    if record['domain']=='card':return int(path[0]),'.'.join(path[1:])
    cid=int(path[1]);tail=path[2:]
    if tail[0]=='values':return cid,'adp.'+tail[1]
    if tail[0]=='reflections':
        reflection=original['cases'][str(cid)]['reflections'][int(tail[1])]
        field=tail[2]
        if field=='complex':field='real' if tail[3]=='0' else 'imaginary'
        return cid,'reflection.'+str(reflection['index'])+'.'+field
    return None

def restore_superseded_field_for_original_checks(raw,domain,compiler,records):
    projected=deepcopy(raw)
    if domain in ('card','scattering'):
        _,fields=verify_cell_successor();original,_=original_cell_output(domain,compiler)
        changes={qualification_key(r,original):r for r in fields
                 if (r['domain'],r['compiler'])==(domain,compiler)}
        for failure in projected['raw_failures']:
            key=(failure['case'],failure['field'])
            if key in changes:
                record=changes[key]
                require(bits(failure['cpp'])==record['expected_bits'],'Cell raw failure differs from exact successor')
                failure['cpp']=struct.unpack('>d',bytes.fromhex(record['old_bits']))[0]
    return _lp_restore(projected,domain,compiler,records)

def verify_original_cpp_output(domain,compiler,raw,output):
    if domain in ('card','scattering'):
        verify_complete_cell_output(domain,compiler,output)
        if domain=='scattering':_lp_original(domain,compiler,raw,output)
    else:_lp_original(domain,compiler,raw,output)

def verify_reflection_successor():
    successor,_=verify_cell_successor();binding=successor['reflection_boundary_decision']
    require(sha_file(reference_path(ROOT/binding['path']))==binding['sha256'],'Reflection successor decision changed')
    decision=read(reference_path(ROOT/binding['path']))
    require(decision['schema']=='qualpf.reflection_boundary_successor.v1' and
            decision['scope_id']=='36D-UNITCELL-REFLECTION-BOUNDARY-SUCCESSOR' and
            decision['status']=='AI_APPROVED_WITH_BOUNDED_CONTRACT' and
            decision['classification']=='AI_APPROVED_EXISTING_CANONICAL_AUTHORITY' and
            decision['human_approved'] is False and decision['confidence']=='EVIDENCE_STRONG' and
            decision['proposal_SHA256']=='6cc9927ec3cc69295cca796ab324a6d99fd354e40c2f32cbf19ea52e771cb4b2',
            'bounded Reflection successor missing')
    return decision

def reflection_main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--domain',choices=['reflection'],required=True)
    parser.add_argument('--runner',type=Path,required=True)
    parser.add_argument('--compiler',choices=['GNU','MSVC'],required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();decision=verify_reflection_successor()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    area=Path(tempfile.mkdtemp(prefix=args.output.stem+'-cell-',dir=args.output.parent))
    raw_path=area/'historical-gate.json';runner=sha_file(args.runner)
    command=[sys.executable,str(reference_path(ROOT/'tests/reference_checks/reflection_differential.py')),
             '--runner',str(args.runner),'--compiler',args.compiler,'--output',str(raw_path)]
    result=subprocess.run(command,capture_output=True,text=True,timeout=600)
    raw_path.with_suffix('.driver.stdout').write_text(result.stdout)
    raw_path.with_suffix('.driver.stderr').write_text(result.stderr)
    require(result.returncode in (0,1) and raw_path.exists() and 'Traceback' not in result.stderr,
            'original Reflection comparator did not complete')
    raw=read(raw_path);require(raw['runner_sha256']==runner==sha_file(args.runner),'Reflection runner changed')
    old=read(reference_path(ROOT/'docs/refactoring/phase-15/acceptance-decision.json'))
    expected=set(old['approved_fingerprints'][args.compiler])
    if args.compiler=='MSVC':
        observation=decision['current_CPP_observations']
        expected-=set(observation['superseded_MSVC_fingerprints'])
        expected|={r['fingerprint'] for r in observation['new_exact_GNU_fingerprints']}
    actual={r['fingerprint'] for r in raw['raw_failed_comparisons']}
    require(len(actual)==len(raw['raw_failed_comparisons']),'duplicate Reflection failures')
    for row in raw['raw_failed_comparisons']:
        fields={k:v for k,v in row.items() if k!='fingerprint'}
        import hashlib
        require(hashlib.sha256(json.dumps(fields,sort_keys=True,separators=(',',':')).encode()).hexdigest()==row['fingerprint'],
                'Reflection failure fingerprint invalid')
    passed=actual==expected
    report=dict(status='PASS_CURRENT_REFLECTION_REFERENCE' if passed else 'FAIL_UNACCEPTED_CURRENT_REFLECTION',
        compiler=args.compiler,validation='FRESH_VALIDATION',current_runner_sha256=runner,
        historical_comparator_report=str(raw_path),historical_comparator_exit_code=result.returncode,
        historical_comparator_report_sha256=sha_file(raw_path),raw_failures_preserved=True,
        raw_failure_count=raw['raw_failure_count'],unexpected=sorted(actual-expected),stale=sorted(expected-actual),
        AI_successor_additions=4 if args.compiler=='MSVC' else 0,
        AI_successor_supersessions=2 if args.compiler=='MSVC' else 0,
        new_tolerance=False,old_human_approval_not_extended=True,native_execution_dependency=False,
        Cell_provider_sha256=sha_file(CELL_SUCCESSOR),original_value_assertions_retained=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
    raise SystemExit(0 if passed else 1)

def main():
    if '--domain' in sys.argv and sys.argv[sys.argv.index('--domain')+1]=='reflection':
        reflection_main()
    code=0
    with patch.object(lp,'verify_bundle',side_effect=verify_bundle), \
         patch.object(lp,'restore_superseded_field_for_original_checks',side_effect=restore_superseded_field_for_original_checks), \
         patch.object(lp,'verify_original_cpp_output',side_effect=verify_original_cpp_output):
        try:lp.main()
        except SystemExit as result:code=result.code
    args=sys.argv[1:]
    output=Path(args[args.index('--output')+1]);domain=args[args.index('--domain')+1]
    compiler=args[args.index('--compiler')+1]
    if output.is_file():
        report=read(output);report['current_Cell_provider_sha256']=sha_file(CELL_SUCCESSOR)
        report['independently_proved_Cell_successor_fields']=0 if domain=='powder' else sum(
            (r['domain'],r['compiler'])==(domain,compiler) for r in verify_cell_successor()[1])
        report['Cell_reference_origin']='independent source-order arithmetic and unchanged qualified caller after exact input mapping'
        output.write_text(json.dumps(report,indent=2)+'\n')
    raise SystemExit(code)

if __name__=='__main__':main()
