from reference_paths import reference_path, provenance_path
"""Exact per-field approval gate. Proposal or earlier-phase approval is insufficient."""
import hashlib
import json
from pathlib import Path

PREFIX = 'docs/refactoring/phase-18/'
REQUIRED = {PREFIX + n for n in ('qualification-records.json', 'qualification-proof.json',
    'qualification-review.md', 'prove_qualification.py', 'contracts.md', 'oracle-seal.json',
    'chemistry-resource-hashes.json', 'cpp-replay-evidence.zip',
    'cpp-replay-evidence-manifest.json')} | {
    'tests/compatibility/card_replay.cpp', 'tests/compatibility/card_differential.py',
    'tests/compatibility/card_qualification.py', 'tests/data/theoretical-card-reference.zip',
    'tests/data/composition-density-reference-manifest.json', 'tests/data/rir-reference-manifest.json',
    'tests/data/card-reference-manifest.json', 'tools/crystallography/generate_chemistry.py'} | {
    'src/core/' + p + n for p, ext in [('crystallography/', '.cpp'),
                                     ('include/qualpf/crystallography/', '.h')]
    for n in [stem + ext for stem in ('chemistry', 'composition', 'theoretical_card')]} | {
    'src/core/crystallography/chemistry_data.inc'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def fingerprint(record):
    return sha(json.dumps({k: record[k] for k in ('case', 'field', 'legacy', 'cpp_by_compiler')},
                          sort_keys=True).encode())


def partition(failures, records, compiler, errors):
    by_key = {(r['case'], r['field']): r for r in records}
    accepted, unresolved = [], []
    for f in failures:
        r = by_key.get((f['case'], f['field']))
        candidate = None if r is None else dict(r, legacy=f['legacy'],
            cpp_by_compiler=dict(r['cpp_by_compiler'], **{compiler: f['cpp']}))
        ok = not errors and candidate is not None and fingerprint(candidate) == r['fingerprint']
        (accepted if ok else unresolved).append(f)
    return accepted, unresolved


def apply(root, runner_hash, compiler, failures):
    try:
        records = json.loads((reference_path(root / PREFIX / 'qualification-records.json')).read_text())
        decision = json.loads((reference_path(root / PREFIX / 'acceptance-decision.json')).read_text())
        proof = json.loads((reference_path(root / PREFIX / 'qualification-proof.json')).read_text())
        errors = []

        def require(test, reason):
            if not test:
                errors.append(reason)

        require(decision.get('status') == 'approved-by-user', 'explicit_approval_missing')
        require(decision.get('qualification_count') == len(records['records']), 'qualification_scope_changed')
        require(decision.get('numerical_equivalence_to_legacy') is False, 'non_equivalence_boundary_missing')
        hashes = decision.get('evidence_sha256', {})
        require(REQUIRED <= set(hashes), 'incomplete_evidence_binding')
        for path, digest in hashes.items():
            p = (reference_path(root / path)).resolve()
            require(p.is_relative_to(root.resolve()) and sha(p.read_bytes()) == digest, 'stale_evidence:' + path)
        require(proof.get('success') is True and proof.get('precision_decimal_digits') == 80 and
                proof.get('failures') == [], 'independent_proof_failed')
        require(records.get('proof_sha256') == sha((reference_path(root / PREFIX / 'qualification-proof.json')).read_bytes()),
                'proof_binding_changed')
        require(decision.get('compiler_fingerprints') == records['compiler_fingerprints'], 'compiler_binding_changed')
        require(runner_hash == records['compiler_fingerprints'].get(compiler, {}).get('runner_sha256'),
                'stale_compiler_fingerprint:' + compiler)
        seen = set()
        for r in records['records']:
            key = (r['case'], r['field'])
            require(key not in seen, 'duplicate_qualification')
            seen.add(key)
            require(r.get('independent_check_passed') is True and fingerprint(r) == r['fingerprint'],
                    'unproved_or_changed_field')
        accepted, unresolved = partition(failures, records['records'], compiler, errors)
        return accepted, unresolved, dict(valid=not errors, errors=errors)
    except (OSError, ValueError, KeyError, TypeError) as e:
        return [], list(failures), dict(valid=False, errors=['approval_unavailable:' + str(e)])
