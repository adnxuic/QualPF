#!/usr/bin/env python3
"""Exact formal PFSM comparison; fast diagnostics never substitute for fits."""
import argparse
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from pfsm_candidate_recall_report import candidate_recall_report
from pfsm_integral_recall_compare import validate_integral


def exact(value):
    # Deliberately retain signed zero and all serialized binary64 digits.
    return json.dumps(value, sort_keys=True, allow_nan=False, separators=(',', ':'))


def rows(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines() if line]


def compare(left, right, left_dir, right_dir, subset=False):
    errors, rounds = [], []
    left_dir, right_dir = Path(left_dir), Path(right_dir)
    for key in ('status', 'points', 'candidate_count', 'candidate_order_sha256', 'identified_phases',
                'phase_recall', 'performance_accept_ids', 'query', 'chemical_constraints'):
        if exact(left.get(key)) != exact(right.get(key)):
            errors.append(key)
    lr, rr = left.get('rounds', []), right.get('rounds', [])
    if len(lr) != len(rr):
        errors.append('round count')
    if not lr or not rr:
        errors.append('missing formal rounds')
    for n, (a, b) in enumerate(zip(lr, rr)):
        local = []
        for key in ('baseline_Q', 'baseline_Rwp_net', 'baseline_Rwp_obs', 'baseline_parameters',
                    'baseline_diagnostics', 'baseline_termination'):
            if exact(a.get(key)) != exact(b.get(key)):
                local.append(key)
        names = [f'baseline-{n}.tsv', 'candidate-order.txt']
        for name in names:
            if not (left_dir/name).is_file() or not (right_dir/name).is_file():
                local.append('missing '+name)
            elif (left_dir/name).read_bytes() != (right_dir/name).read_bytes():
                local.append(name)
        lf, rf = left_dir/f'round-{n}.jsonl', right_dir/f'round-{n}.jsonl'
        if not lf.is_file() or not rf.is_file():
            local.append('missing ranking')
            errors.extend(f'round {n}: {e}' for e in local)
            continue
        full, fitted = rows(lf), rows(rf)
        full_ids, fitted_ids = [v['id'] for v in full], [v['id'] for v in fitted]
        if len(set(full_ids)) != len(full_ids) or len(set(fitted_ids)) != len(fitted_ids):
            local.append('duplicate ranking identity')
        if full_ids[:5] != fitted_ids[:5]:
            local.append('Top-5 identity/order')
        if [v for v in full_ids if v in set(fitted_ids)] != fitted_ids:
            local.append('survivor order')
        by_id = {v['id']: v for v in full}
        for rank, row in enumerate(fitted, 1):
            expected = by_id.get(row['id'])
            if expected is None:
                local.append('unknown candidate '+row['id'])
                continue
            if row.get('rank') != rank:
                local.append('incorrect formal rank')
            expected = {k: v for k, v in expected.items() if k != 'rank'}
            actual = {k: v for k, v in row.items() if k != 'rank'}
            if exact(expected) != exact(actual):
                local.append('formal evidence '+row['id'])
        if not subset and lf.read_bytes() != rf.read_bytes():
            local.append('full ranking bytes')
        try:
            fast = candidate_recall_report(b)
            candidate_recall_report(a)
        except ValueError as error:
            local.append(str(error))
            fast = {}
        diagnostics = dict(round=n, pool=len(full), fitted=len(fitted))
        mode = fast.get('mode', 'off')
        if mode != 'off':
            if fast.get('recall_budget') != 500 or fast.get('fit_budget') != 20:
                local.append('changed fixed budgets')
            attempts = fast.get('attempts', [])
            if not attempts or attempts[-1].get('discarded'):
                local.append('missing final attempt')
            else:
                final = attempts[-1]
                universe = {r['original_index'] for r in full}
                recalled, retained = final['recalled_indices'], final['retained_indices']
                for indices in (recalled, retained):
                    if indices != sorted(set(indices)) or not set(indices) <= universe:
                        local.append('invalid selection identities')
                if not (fast.get('algorithm_version') == 'v2' and final.get('full_fallback')) and not set(retained) <= set(recalled):
                    local.append('formal selection outside recall')
                top = {r['original_index'] for r in full[:5]}
                recall_ok, final_ok = top <= set(recalled), top <= set(retained)
                diagnostics.update(recalled=len(recalled), recall_top5=recall_ok, final_top5=final_ok,
                                   fallback=final.get('fallback_reason', ''))
                if not recall_ok:
                    local.append('recall Top-5 miss')
                if not final_ok:
                    local.append('local Top-5 miss')
                if mode == 'shadow':
                    if final.get('recall_top5_retained') != recall_ok or final.get('final_top5_retained') != final_ok:
                        local.append('incorrect shadow recall flags')
                    if final.get('top5_indices') != [r['original_index'] for r in full[:5]]:
                        local.append('incorrect shadow Top-5 identities')
                if mode == 'enabled' and {r['original_index'] for r in fitted} != set(retained):
                    local.append('publication differs from selection')
                if fast.get('normalization_maximum') != max((r['delta_Rwp_net'] for r in full), default=0):
                    local.append('normalization maximum')
                expected_scope = 'full_pool' if len(fitted) == len(full) else 'formal_subset'
                if fast.get('normalization_scope') != expected_scope:
                    local.append('normalization scope')
                evidence_file = right_dir/f'fast-{n}.jsonl'
                if not evidence_file.is_file():
                    local.append('missing fast evidence')
                else:
                    evidence = rows(evidence_file)
                    for attempt in attempts:
                        records = [r for r in evidence if r.get('attempt') == attempt['attempt']]
                        backend = attempt.get('backend', 'v2')
                        if backend == 'bounded_integral_relaxed':
                            local.extend(validate_integral(attempt, records, len(b.get('baseline_parameters', {}).get('shifts_degrees', b.get('baseline_parameters', {}).get('shifts', [])))))
                        elif backend != 'v2':
                            local.append('unknown recall backend')
                        if attempt.get('full_fallback'):
                            if (set(attempt['retained_indices']) != universe or not attempt.get('fallback_reason') or
                                    (records and fast.get('algorithm_version') != 'v2')):
                                local.append('invalid full fallback')
                            if not records:
                                continue
                        if backend == 'v2' and fast.get('algorithm_version') == 'v2':
                            stage = attempt.get('v2', {})
                            if not stage or fast.get('version') != 3:
                                local.append('missing v2 selection/schema')
                            elif (stage.get('retained_indices') != attempt['retained_indices'] or
                                  stage.get('fallback_reason') != attempt.get('fallback_reason') or
                                  stage.get('full_fallback') != attempt.get('full_fallback')):
                                local.append('inconsistent v2 fallback')
                            if stage:
                                proposed = set(stage['proposed_indices'])
                                for key in ('initial_top20','review_top20','channel_protection','review_indices',
                                            'unresolved_protection','proposed_indices'):
                                    selection = stage[key]
                                    if selection != sorted(set(selection)) or not set(selection) <= set(attempt['recalled_indices']):
                                        local.append('invalid v2 '+key)
                                if (stage['review_skipped'] != (len(stage['review_indices']) > 80) or
                                    ((len(proposed) > 80 or stage['review_skipped']) and not stage['full_fallback'])):
                                    local.append('unprotected v2 budget overflow')
                                if not stage['full_fallback'] and proposed != set(attempt['retained_indices']):
                                    local.append('invalid v2 proposed set')
                                if attempt is final:
                                    diagnostics.update(initial_top5=top <= set(stage['initial_top20']),
                                        local_top5=top <= set(stage['review_top20']), review_skipped=stage['review_skipped'])
                                for row in records:
                                    v = row.get('v2')
                                    if row.get('recalled') != bool(v):
                                        local.append('missing v2 recalled evidence')
                                    if not v:
                                        continue
                                    if (v.get('nnls_calls', 999) > (15 if v.get('reviewed') else 3) or
                                        len(v.get('seeds', [])) > 3):
                                        local.append('v2 call/seed budget exceeded')
                                    if 'formal_gain' in v and row['original_index'] not in proposed:
                                        local.append('shadow formal evidence leaked into v2 decision')
                                    if v.get('numerical_protection') and not row.get('retained'):
                                        local.append('unprotected v2 numerical failure')
                        identities = [r['original_index'] for r in records]
                        if len(identities) != len(set(identities)) or set(identities) != universe:
                            local.append('incomplete fast candidate evidence')
                        if fast.get('version', 1) >= 2:
                            for score, rank in (('original_score', 'original_rank'),
                                                ('residual_score', 'residual_rank'), ('gain', 'local_rank')):
                                ordered = sorted((r for r in records if score in r and
                                                  (score != 'gain' or r.get('recalled'))),
                                                 key=lambda r: (-r[score], r['original_index']))
                                expected_ranks = {r['original_index']: i+1 for i, r in enumerate(ordered)}
                                if backend == 'v2' and any(r.get(rank) != expected_ranks.get(r['original_index']) for r in records):
                                    local.append('incorrect '+rank)
                            for row in records:
                                for selected, reason in (('recalled', 'recall_reason'), ('retained', 'retention_reason')):
                                    if bool(row.get(selected)) != bool(row.get(reason)):
                                        local.append('missing/inconsistent '+reason)
                        for key, field in (('recalled', 'recalled_indices'), ('retained', 'retained_indices')):
                            if {r['original_index'] for r in records if r.get(key)} != set(attempt[field]):
                                local.append('inconsistent '+key+' evidence')
                        for row in records:
                            if row.get('protection_reason') and not (row.get('recalled') and row.get('retained')):
                                local.append('unprotected approximation failure')
        rounds.append(diagnostics)
        errors.extend(f'round {n}: {e}' for e in local)
    return dict(passed=not errors, errors=errors, rounds=rounds)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('before', 'after', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--subset', action='store_true')
    args = p.parse_args()
    if args.output.exists():
        raise RuntimeError('Refusing to replace comparison evidence')
    a = json.loads((args.before/'run.json').read_text(encoding='utf-8'))
    b = json.loads((args.after/'run.json').read_text(encoding='utf-8'))
    comparisons = []
    right = {c['id']: c for c in b['cases']}
    for case in a['cases']:
        if case['id'] not in right:
            comparisons.append(dict(id=case['id'], passed=False, errors=['missing case']))
        else:
            comparisons.append(dict(id=case['id'], **compare(case, right[case['id']],
                args.before/case['id'], args.after/case['id'], args.subset)))
    result = dict(passed=all(c['passed'] for c in comparisons) and len(right) == len(a['cases']), comparisons=comparisons)
    args.output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
