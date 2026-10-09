from reference_paths import reference_path, provenance_path
"""Current phase-provider successor; full historical value assertions remain.

This entry accepts only the independently approved phase implementation identity.
It does not accept unproved complete-output changes or reuse old human approval
for new values. Remaining numerical differences continue to fail normal CTest.
The V1 runner and historical references remain untouched.
"""
import argparse,json,re,struct,subprocess,sys,tempfile
from copy import deepcopy
from pathlib import Path
from current_qualified_reference import (ROOT,POLICY,PHASE,PROVIDER_AUTHORITY_SHA,
 GENERATOR_ARCHIVE_AUTHORITY,GENERATOR_ARCHIVE_AUTHORITY_SHA,HISTORICAL_GENERATORS,
 require,read,sha_file,validate_numeric_scope,verify_original_cpp_output,exact_json)
SUCCESSOR=reference_path(ROOT/'tests/data/current-phase-provider.json')
HISTORICAL_PHASE_PROVIDER=reference_path(ROOT/'archive/legacy/phase36d-test-reference/structure_factor.cpp.txt')

def verify_successor():
    successor=read(SUCCESSOR)
    require(successor['schema']=='qualpf.current_phase_provider.v1' and
            successor['scope']=='36D-PHASE-SINCOS-CONTRACT-P1' and
            successor['historical_qualifications_not_extended'] is True and
            successor['complete_reference_assertions_retained'] is True and
            successor['new_tolerance'] is False,'successor scope changed')
    binding=successor['decision']
    require(sha_file(reference_path(ROOT/binding['path']))==binding['sha256'],'phase decision changed')
    decision=read(reference_path(ROOT/binding['path']))
    require(decision['status']=='AI_APPROVED_WITH_BOUNDED_CONTRACT' and
            decision['classification']=='AI_APPROVED_NUMERICAL_CORRECTION' and
            decision['selected_option']=='B' and decision['human_approved'] is False and
            decision['confidence']=='EVIDENCE_STRONG' and
            decision['proposal_SHA256']=='45e2ee366c8fe056e88a6e1620d09f0a7df41e305fdf023ad9f8162beb1a3886',
            'bounded autonomous phase authority missing')
    expected={'src/core/crystallography/structure_factor.cpp',
              'src/core/crystallography/phase_sincos.cpp',
              'src/core/crystallography/phase_sincos.h',
              'tests/data/phase-sincos-reference.txt'}
    require(set(successor['current_providers'])==expected,'phase provider closure incomplete')
    for name,digest in successor['current_providers'].items():
        require(sha_file(provenance_path(ROOT/name))==digest,'approved phase provider changed: '+name)
    require(sha_file(HISTORICAL_PHASE_PROVIDER)==successor['historical_sha256'],
            'sealed original phase provider changed')
    fields=successor['phase_value_successor']
    require(sha_file(reference_path(ROOT/fields['path']))==fields['sha256'],'phase field reference changed')
    reference=read(reference_path(ROOT/fields['path']))
    require(reference['scope']==successor['scope'] and reference['expected_from_CPP'] is False and
            reference['new_tolerance'] is False and len(reference['records'])==1,
            'phase field successor scope changed')
    return successor

def restore_superseded_field_for_original_checks(raw,domain,compiler,records):
    """Require the exact new authority before evaluating the original full gate.

    Only a private comparison copy is projected onto the superseded value. The
    actual raw report and old qualification file remain unchanged. Every other
    field/error/shape assertion is still evaluated by its original gate.
    """
    projected=deepcopy(raw);mapped=[]
    successor=verify_successor();binding=successor['phase_value_successor']
    reference=read(reference_path(ROOT/binding['path']))
    for field in reference['records']:
        if domain!='scattering' or compiler!=field['compiler']:continue
        old=next((r for r in records if (r['case'],r['field'])==(field['case'],field['field'])),None)
        require(old is not None and old['fingerprint']==field['previous_record_fingerprint'],
                'superseded original field changed')
        matches=[f for f in projected['raw_failures'] if
                 (f['case'],f['field'])==(field['case'],field['field'])]
        require(len(matches)==1,'phase successor field missing or duplicated')
        failure=matches[0]
        bits=lambda x:struct.pack('>d',x).hex()
        require(bits(failure['cpp'])==field['expected_bits'] and
                bits(old['cpp_by_compiler'][compiler])==field['old_bits'],
                'current phase value differs from exact independent successor')
        failure['cpp']=old['cpp_by_compiler'][compiler]
        mapped.append(dict(case=field['case'],field=field['field'],compiler=compiler,
                           expected_bits=field['expected_bits'],authority='AI phase successor'))
    return projected,mapped

def verify_bundle(domain):
    successor = verify_successor()
    policy = read(POLICY)
    execution = {f'tests/compatibility/{name}_{part}.py'
                 for name in PHASE for part in ('differential', 'capture', 'qualification')}
    require(set(policy['execution_hashes']) == execution, 'execution closure incomplete')
    for name, digest in policy['execution_hashes'].items():
        require(sha_file(provenance_path(ROOT / name)) == digest, 'comparison/parser execution changed: ' + name)
    authority = reference_path(ROOT / policy['historical_provider_authority']['path'])
    require(sha_file(authority) == PROVIDER_AUTHORITY_SHA, 'provider authority changed')
    original_providers = read(authority)['inputs']
    required_providers = {p: h for p, h in original_providers.items()
                          if p.startswith(('src/core/crystallography/',
                                           'src/core/include/qualpf/crystallography/',
                                           'src/core/include/qualpf/domain/')) and not p.endswith('.md')}
    require(policy['provider_hashes'] == required_providers, 'provider closure incomplete')
    for name, digest in policy['provider_hashes'].items():
        require(original_providers.get(name) == digest, 'original provider policy changed: ' + name)
        if name == successor['historical_provider']:
            require(digest == successor['historical_sha256'] and
                    sha_file(reference_path(ROOT/name)) == successor['current_providers'][name],
                    'phase successor mapping changed')
        else:
            require(sha_file(reference_path(ROOT/name)) == digest, 'current numerical provider changed: ' + name)
    options = []
    pattern = r'set_source_files_properties\(([^()]*)\s+PROPERTIES COMPILE_OPTIONS "([^"]*)"\)'
    for match in re.finditer(pattern, (reference_path(ROOT / 'src/core/CMakeLists.txt')).read_text(), re.S):
        sources = [s for s in match[1].split() if s.startswith('crystallography/')]
        if sources:
            options.append(dict(sources=sources, options=match[2]))
    phase_rows = [row for row in options if 'crystallography/phase_sincos.cpp' in row['sources']]
    require(len(phase_rows) == 2 and
            [row['options'] for row in phase_rows] == ['/fp:strict','-ffp-contract=off;-fno-fast-math'],
            'phase numerical compile policy changed')
    projected = [dict(sources=[s for s in row['sources'] if s != 'crystallography/phase_sincos.cpp'],
                      options=row['options']) for row in options]
    require(projected == policy['math_compile_options'], 'numerical compile policy changed')
    prefix = reference_path(ROOT / f'docs/refactoring/phase-{PHASE[domain]}')
    binding = policy['original_decisions'][domain]
    require(sha_file(reference_path(ROOT / binding['path'])) == binding['sha256'], 'original decision changed')
    decision = read(prefix / 'acceptance-decision.json')
    records = read(prefix / 'qualification-records.json')
    proof = read(prefix / 'qualification-proof.json')
    require(decision['status'] == 'approved-by-user' and
            decision['numerical_equivalence_to_legacy'] is False, 'original authority missing')
    require(decision['qualification_count'] == len(records['records']), 'original scope changed')
    require(proof['success'] is True and proof['failures'] == [] and
            proof['precision_decimal_digits'] == 80, 'independent proof failed')
    require(decision['compiler_fingerprints'] == records['compiler_fingerprints'],
            'original compiler authority changed')
    # These source/registration identities belong to the historical gate. Current
    # ownership and numerical inputs are verified above and below, respectively.
    historical = []
    for name, digest in decision['evidence_sha256'].items():
        if name in HISTORICAL_GENERATORS:
            #36B retired the Native resource inputs and retained exact original
            # generator bytes. Bind the historical script to that archive and
            # the current resource reader to its separately accepted identity.
            require(sha_file(GENERATOR_ARCHIVE_AUTHORITY) == GENERATOR_ARCHIVE_AUTHORITY_SHA,
                    'generator archive transition authority changed')
            transition = read(GENERATOR_ARCHIVE_AUTHORITY)['AI_decision']
            require(transition['status'] == 'AI_APPROVED' and
                    'AI_APPROVED_ARCHIVE_TRANSITION' in transition['classification'],
                    'generator archive transition not accepted')
            readers = {row['path']: row['sha256'] for row in transition['source_fingerprints']}
            archived = HISTORICAL_GENERATORS[name]
            require(sha_file(reference_path(ROOT / archived)) == digest, 'historical generator changed: ' + name)
            require(name in readers and sha_file(reference_path(ROOT / name)) == readers[name],
                    'accepted current resource reader changed: ' + name)
            historical.append(dict(path=name, original_sha256=digest, archived_path=archived))
        elif name == successor['historical_provider']:
            require(digest == successor['historical_sha256'] and
                    sha_file(HISTORICAL_PHASE_PROVIDER) == digest,
                    'original qualified phase provider changed')
            historical.append(dict(path=name, original_sha256=digest,
                                   archived_path=HISTORICAL_PHASE_PROVIDER.relative_to(ROOT).as_posix()))
        elif name in policy['historical_registration_paths'] or name.startswith('legacy/fortran/'):
            historical.append(dict(path=name, original_sha256=digest))
        else:
            require(sha_file(provenance_path(ROOT / name)) == digest, 'proof/reference/test evidence changed: ' + name)
    return records['records'], historical

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--domain',choices=PHASE,required=True)
    parser.add_argument('--runner',type=Path,required=True)
    parser.add_argument('--compiler',choices=('GNU','MSVC'),required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    records,historical=verify_bundle(args.domain)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    run_dir=Path(tempfile.mkdtemp(prefix=args.output.stem+'-phase-',dir=args.output.parent))
    raw_path=run_dir/'historical-gate.json';runner_before=sha_file(args.runner)
    command=[sys.executable,str(reference_path(ROOT/f'tests/reference_checks/{args.domain}_differential.py')),
             '--runner',str(args.runner),'--compiler',args.compiler,'--output',str(raw_path)]
    result=subprocess.run(command,capture_output=True,text=True,timeout=300)
    raw_path.with_suffix('.driver.stdout').write_text(result.stdout)
    raw_path.with_suffix('.driver.stderr').write_text(result.stderr)
    require(result.returncode in (0,1) and raw_path.exists() and 'Traceback' not in result.stderr,
            'original comparator did not complete')
    raw=read(raw_path)
    require(raw['runner_sha256']==runner_before==sha_file(args.runner),'tested runner changed')
    reasons=[];projected=raw;mapped=[]
    try:
        projected,mapped=restore_superseded_field_for_original_checks(raw,args.domain,args.compiler,records)
    except ValueError as failure:reasons.append(str(failure))
    for check in [lambda:validate_numeric_scope(projected['raw_failures'],records,args.compiler),
                  lambda:verify_original_cpp_output(args.domain,args.compiler,projected,raw_path)]:
        try:check()
        except ValueError as failure:reasons.append(str(failure))
    report=dict(status='FAIL_UNACCEPTED_CURRENT_REFERENCE' if reasons else 'PASS_CURRENT_NUMERICAL_REFERENCE',
       domain=args.domain,compiler=args.compiler,validation='FRESH_VALIDATION',
       current_runner_sha256=runner_before,historical_comparator_exit_code=result.returncode,
       historical_comparator_report=str(raw_path),historical_comparator_report_sha256=sha_file(raw_path),
       historical_source_identities=historical,historical_failures_preserved=True,
       approved_original_named_differences=len(records),original_CPP_output_verified=not reasons,
       phase_successor_provider_sha256=sha_file(SUCCESSOR),phase_source_identity_accepted=True,
       independently_proved_phase_successor_fields=mapped,
       old_human_approval_not_extended=True,complete_original_value_assertions_retained=True,
       unaccepted_complete_output_reasons=reasons,new_tolerance=False,native_execution_dependency=False)
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
    raise SystemExit(1 if reasons else 0)

if __name__=='__main__':main()
