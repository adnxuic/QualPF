from reference_paths import reference_path, provenance_path
"""Fail-closed application of the user's exact Phase 16 qualification scope."""
import hashlib
import json
from pathlib import Path

PREFIX='docs/refactoring/phase-16/'
REQUIRED_EVIDENCE={
    PREFIX+name for name in ('qualification-records.json','qualification-review.md',
        'qualification-proof.json','prove_qualification.py','table-resource-hashes.json',
        'test-results/approved-mingw-differential.json','test-results/approved-msvc-differential.json')
} | {
    'tests/data/scattering-reference.zip',
    'tests/data/scattering-reference-manifest.json',
    'tests/data/structure-factor-reference-manifest.json',
    'src/core/include/qualpf/crystallography/scattering.h',
    'src/core/include/qualpf/crystallography/structure_factor.h',
    'src/core/crystallography/scattering.cpp',
    'src/core/crystallography/structure_factor.cpp',
    'src/core/crystallography/scattering_data.inc',
    'tests/compatibility/scattering_replay.cpp',
    'tools/crystallography/generate_scattering.py',
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def fingerprint(record):
    return sha(json.dumps({k:record[k] for k in ('case','field','legacy','cpp_by_compiler')},
                          sort_keys=True).encode())


def approval_errors(records,decision,proof,compiler,runner_sha256,observed_hashes):
    """Validate evidence independently of numerical failure matching."""
    errors=[]
    def require(ok,reason):
        if not ok:errors.append(reason)
    require(decision.get('status')=='approved-by-user','explicit_approval_missing')
    require(decision.get('qualification_count')==329==len(records['records']), 'qualification_scope_changed')
    require(decision.get('numerical_equivalence_to_legacy') is False,'non_equivalence_boundary_missing')
    bound=decision.get('evidence_sha256',{})
    require(REQUIRED_EVIDENCE<=set(bound),'evidence_binding_incomplete')
    for name,digest in bound.items():
        require(observed_hashes.get(name)==digest,'stale_evidence:'+name)
    require(decision.get('qualification_records_sha256')==bound.get(PREFIX+'qualification-records.json'),
            'qualification_hash_binding_mismatch')
    require(records.get('independent_failures')==0 and proof.get('success') is True and
            proof.get('records')==329 and proof.get('failures')==[] and
            proof.get('precision_decimal_digits')==80,'independent_proof_failed')
    require(records.get('script_sha256')==bound.get(PREFIX+'prove_qualification.py'),'proof_script_changed')
    require(decision.get('compiler_fingerprints')==records.get('compiler_fingerprints'),
            'compiler_fingerprint_binding_mismatch')
    compiler_record=records.get('compiler_fingerprints',{}).get(compiler,{})
    require(bool(compiler_record) and runner_sha256==compiler_record.get('runner_sha256'),
            'stale_compiler_fingerprint:'+compiler)
    for key,name in (('GNU','mingw'),('MSVC','msvc')):
        require(records.get('compiler_fingerprints',{}).get(key,{}).get('report_sha256')==
                bound.get(PREFIX+f'test-results/approved-{name}-differential.json'),
                'original_report_fingerprint_mismatch:'+key)
    seen=set()
    for record in records['records']:
        key=(record['case'],record['field'])
        require(key not in seen,'duplicate_qualification')
        seen.add(key)
        require(record.get('independent_check_passed') is True,'unproved_qualification')
        require(set(record['cpp_by_compiler'])=={'GNU','MSVC'},'compiler_values_incomplete')
        require(fingerprint(record)==record.get('fingerprint'),'field_fingerprint_changed')
    return errors


def partition_failures(failures,records,compiler,errors):
    accepted=[];unresolved=[]
    by_key={(r['case'],r['field']):r for r in records['records']}
    for failure in failures:
        record=by_key.get((failure['case'],failure['field']))
        # Reconstruct the approved field fingerprint using this run's values.
        # This also distinguishes signed zero and numerical JSON representations.
        candidate=None if record is None else dict(record,legacy=failure['legacy'],
            cpp_by_compiler=dict(record['cpp_by_compiler'],**{compiler:failure['cpp']}))
        matches=not errors and candidate is not None and fingerprint(candidate)==record['fingerprint']
        (accepted if matches else unresolved).append(failure)
    return accepted,unresolved


def apply_qualifications(root,runner_sha256,compiler,failures):
    """Missing, unreadable or stale approval evidence never qualifies a failure."""
    directory=reference_path(root/PREFIX)
    try:
        records=json.loads((directory/'qualification-records.json').read_text(encoding='utf-8'))
        decision=json.loads((directory/'acceptance-decision.json').read_text(encoding='utf-8'))
        proof=json.loads((directory/'qualification-proof.json').read_text(encoding='utf-8'))
        observed={}
        for name in REQUIRED_EVIDENCE | set(decision.get('evidence_sha256',{})):
            path=(reference_path(root/name)).resolve()
            if not path.is_relative_to(root.resolve()):
                raise ValueError('evidence path outside repository')
            observed[name]=sha(path.read_bytes())
        errors=approval_errors(records,decision,proof,compiler,runner_sha256,observed)
        accepted,unresolved=partition_failures(failures,records,compiler,errors)
        return accepted,unresolved,dict(valid=not errors,errors=errors,
            decision_sha256=sha((directory/'acceptance-decision.json').read_bytes()),
            qualification_records_sha256=observed[PREFIX+'qualification-records.json'],
            numerical_equivalence_to_legacy=False)
    except (OSError,ValueError,KeyError,TypeError) as error:
        return [],list(failures),dict(valid=False,errors=['unreadable_approval:'+str(error)])
