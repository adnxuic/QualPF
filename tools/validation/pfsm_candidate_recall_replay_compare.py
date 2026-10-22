#!/usr/bin/env python3
"""Independent A/B/C screening comparison; frozen formal evidence never feeds fast fitting."""
import argparse
import json
import math
from pathlib import Path
from pfsm_candidate_recall_replay_inputs import sha, read, ranking


from pfsm_candidate_recall_report import candidate_recall_report
from pfsm_integral_recall_compare import validate_integral

def verified(binding):
    path=Path(binding['path'])
    if sha(path)!=binding['sha256']:raise ValueError('Comparison binding mismatch: '+str(path))
    return path


def pure_a(records):
    rows=sorted((r for r in records if 'a' in r and 'rank' in r['a']),key=lambda r:r['a']['rank'])
    if not rows:return set()
    cut=rows[min(20,len(rows))-1]['a']['best']['gain']
    return {r['original_index'] for r in rows if r['a']['rank']<=20 or
            abs(r['a']['best']['gain']-cut)<=128*math.ulp(1.)*max(1,abs(cut),abs(r['a']['best']['gain']))}


def compare(binding,output):
    input_path=verified(binding['input']);run=read(output/'run.json')
    if run['input_sha256']!=sha(input_path) or run.get('formal_fits')!=0 or not run.get('completed'):
        raise ValueError('Replay binding/status mismatch')
    verified(binding['source_run']);verified(binding['source_summary']);verified(binding['source_manifest']);verified(binding['source_width'])
    if len(run['rounds'])!=len(binding['rounds']):raise ValueError('Incomplete replay rounds')
    reports=[]
    for frozen,actual in zip(binding['rounds'],run['rounds']):
        n=frozen['round']
        if actual['round']!=n:raise ValueError('Round identity mismatch')
        evidence=output/f'replay-{n}.jsonl'
        if sha(evidence)!=actual['evidence_sha256']:raise ValueError('Replay evidence mismatch')
        formal=ranking(verified(frozen['formal']));old=read(verified(frozen['report']));verified(frozen['v1_fast'])
        records=ranking(evidence);by_index={r['original_index']:r for r in records}
        formal_by_index={r['original_index']:r for r in formal};pool=set(formal_by_index)
        if set(by_index)!=pool or set(actual['pool'])!=pool:raise ValueError('Replay universe mismatch')
        top={r['original_index'] for r in formal[:5]}
        old_selected=set(candidate_recall_report(old)['attempts'][-1]['retained_indices'])
        if actual.get('backend', 'v2') == 'bounded_integral_relaxed':
            errors = validate_integral(actual, records, len(next(r for r in read(input_path)['rounds'] if r['round'] == n)['shifts']))
            retained = set(actual['retained_indices'])
            if errors or set(actual['recalled']) != retained:
                raise ValueError('Invalid integral replay: '+str(errors))
            reports.append(dict(round=n, backend='bounded_integral_relaxed', switch_reason=actual['switch_reason'],
                recall_top5=top<=retained, final_screening_top5=top<=retained, formal_count=len(retained),
                formal_fits=0, formal_preservation='requires_new_enabled_validation', fallback='',
                integral=actual['integral'], old_top5_losses=[dict(id=formal_by_index[i]['id'],
                    formal_rank=formal_by_index[i]['rank'], recovered=i in retained) for i in sorted(top-old_selected)]))
            continue
        recalled=set(actual['recalled']);a_top=pure_a(records)
        selection=actual.get('bc');underestimated=[];final=set();local=set();initial=set();channel=set();protected=set()
        if selection:
            initial=set(selection['initial_top20']);local=set(selection['review_top20'])
            channel=set(selection['channel_protection']);protected=set(selection['unresolved_protection'])
            final=set(selection['retained_indices']);fallback=selection['fallback_reason']
            if not selection['full_fallback']:
                # Simulate only the selected-card gate. The complete table is a comparator input, not a ranker input.
                for i in selection['proposed_indices']:
                    gain=formal_by_index[i]['delta_Q']/actual['denominator']
                    local_gain=by_index[i]['bc']['best']['gain']
                    if gain-local_gain>.25*max(gain,selection['g_star']):underestimated.append(i)
                if underestimated:final=pool;fallback='formal_underestimation'
        else:fallback='not_run'
        losses=[]
        for i in sorted(top-old_selected,key=lambda i:formal_by_index[i]['rank']):
            row=by_index[i];reasons=[]
            if selection:
                if i in local and not selection['review_skipped']:reasons.append('local_top20')
                if i in channel:reasons.append('channel_protection')
                if i in protected:reasons.append('risk_protection')
                if fallback:reasons.append('full_fallback')
            if i not in final:reasons.append('still_missing')
            losses.append(dict(id=row['id'],formal_rank=formal_by_index[i]['rank'],
                a_rank=row.get('a',{}).get('rank'),initial_rank=row.get('bc',{}).get('initial_rank'),
                reviewed_rank=row.get('bc',{}).get('local_rank'),a_local_recovered=i in a_top,
                recovery_reasons=reasons,primary_recovery=reasons[0] if reasons else 'not_run',
                candidate_reasons=row.get('bc',{}).get('retention_reasons',[])))
        reports.append(dict(round=n,backend='v2',recall_top5=top<=recalled,a_local_top5=top<=a_top if 'a_top20' in actual else None,
            initial_top5=top<=initial if selection else None,
            reviewed_top5=(top<=local if not selection['review_skipped'] else None) if selection else None,
            review_skipped=selection['review_skipped'] if selection else None,
            final_screening_top5=top<=final if selection else None,formal_fits=0,
            formal_preservation='requires_new_enabled_validation',fallback=fallback,
            underestimated_indices=underestimated,formal_count=len(final),old_top5_losses=losses,
            initial_seconds=actual.get('initial_seconds'),review_seconds=actual.get('review_seconds'),a_seconds=actual.get('a_seconds')))
    return dict(schema='qualpf.pfsm.fast-replay-assessment.v1',case=binding['case'],
        input_sha256=sha(input_path),replay_run_sha256=sha(output/'run.json'),rounds=reports,
        screening_passed=all(r['recall_top5'] and r['final_screening_top5'] for r in reports),formal_fits=0)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binding',type=Path,required=True);p.add_argument('--replay',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    if args.output.exists():raise ValueError('Refusing to replace evidence')
    result=compare(read(args.binding),args.replay)
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(f"{result['case']}: {sum(bool(r['final_screening_top5']) for r in result['rounds'])}/{len(result['rounds'])} screening rounds; zero new formal fits")
    return 0 if result['screening_passed'] else 1


if __name__=='__main__':raise SystemExit(main())
