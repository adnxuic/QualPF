#!/usr/bin/env python3
"""Frozen-budget additive old-shift channel; revealed-pool diagnosis only."""
import argparse
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import time
from pfsm_shift_diagnostic import digest, read

CONFIG = dict(version='bounded-old-shift-diagnostic-v1', radius_H0=1.5, maximum_half_span=.5,
    released_old_coordinates=2, old_bank_positions=257, starts=['v1_fine','v1_original_support'],
    stage_ticks=[64,16,4,1], offsets=[-2,-1,0,1,2], radius_ticks=128,
    maximum_extra_nnls_per_card=205, candidate_cache_columns=32, extra_budget=32,
    cutoff_epsilon_multiplier=128, matched_null='bounded searches from formal baseline and candidate old shifts',
    selection='unchanged v1 union + top32 relaxed + new numerical/zero-contribution uncertainty',
    core_integrals='unchanged fine layer, 2048 bins, tail 1e-4; no formal fit or formal seed')


def wire(path, observations, old, selected, cards, width, threads, radiation=()):
    with path.open('x',encoding='utf-8',newline='\n') as stream:
        def row(values):
            stream.write(' '.join(format(v,'.17g') if isinstance(v,float) else str(v) for v in values)+'\n')
        row([len(observations),len(old),len(cards),threads,width,-.5,.5,len(radiation)])
        for r in radiation:
            row([r['wavelength_angstrom'],r['weight']])
        for values in observations:
            row(values)
        def lines(refs):
            row([len(refs)])
            for values in refs:
                row(values)
        for shift,refs in zip(old,selected):
            row([shift]); lines(refs)
        for card in cards:
            row([card['original_index'],card['id'],card['fine_shift'],card['original_shift']])
            lines(card['lines'])


def execute(probe, directory, label, arguments=()):
    command=[str(probe.resolve()),*arguments]
    start=time.perf_counter()
    with (directory/'input.txt').open('rb') as source, (directory/(label+'.jsonl')).open('xb') as result, (directory/(label+'.log')).open('xb') as log:
        process=subprocess.run(command,stdin=source,stdout=result,stderr=log)
    receipt=dict(command=command,exit_code=process.returncode,process_seconds=time.perf_counter()-start,
                 probe_sha256=digest(probe),input_sha256=digest(directory/'input.txt'))
    (directory/(label+'-terminal.json')).write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    if process.returncode:
        raise RuntimeError(f'{label} actual exit {process.returncode}; retained receipt/log')
    rows=[json.loads(line) for line in (directory/(label+'.jsonl')).read_text().splitlines()]
    return rows[0],rows[1:]


def retain(rows,budget):
    ordered=sorted((row for row in rows if not row['uncertainty']),key=lambda row:(-row['normalized_gain'],row['original_index']))
    if not ordered:
        return set(),None
    cutoff=ordered[min(budget,len(ordered))-1]['normalized_gain']
    eps=128*sys.float_info.epsilon*max(1,abs(cutoff))
    return {r['id'] for r in ordered if r['normalized_gain']>=cutoff-eps},cutoff


def self_test(args):
    from pfsm_shift_diagnostic import Model, dependencies
    np,_,_,_,nnls=dependencies(args.dependency_dir)
    args.output.mkdir(parents=True,exist_ok=False)
    report=[]
    for label,width in [('analytic',.25),('narrow',.025),('very-narrow',.0025)]:
        directory=args.output/label; directory.mkdir()
        x=np.linspace(29,33,251)
        refs=[[(30.,1.)],[(31.,1.),(32.,.4)],[(31.12,.8),(32.16,.5)]]
        truth=[.03,.3,.1]
        # Independent point-sampled analytic mixture; no product-generated golden.
        y=sum(scale*sum(s*(1+4*(2**(2/3)-1)*((x-q-shift)/width)**2)**-1.5 for q,s in lines)
              for lines,shift,scale in zip(refs,truth,[1,.8,.6]))
        w=1+.2*np.sin(x);w[::13]=0
        old=[.03,-.02]
        cards=[dict(id='target',original_index=0,fine_shift=.1,original_shift=.12,lines=refs[2]),
               dict(id='zero',original_index=1,fine_shift=0.,original_shift=0.,lines=[])]
        observations=[tuple(map(float,row)) for row in zip(x,y,w)]
        wire(directory/'input.txt',observations,old,refs[:2],cards,width,2)
        meta,rows=execute(args.probe,directory,'probe')
        model=Model(np,nnls,(x,y,w),refs,width)
        target=rows[0]
        checks=[]
        for field,shifts in [('fixed_q',old+[.1]),('best_q',target['best_shifts']),('null_q',target['null_shifts'])]:
            expected=model.solve('fine',shifts)[1]['Q']
            checks.append(abs(target[field]-expected)<=2e-10*max(1,abs(expected)))
        assert all(checks),(label,checks)
        assert all(r['evaluations']<=205 and r['candidate_cache_columns']<=32 for r in rows)
        assert meta['old_bank_columns']<=257*len(old)
        assert rows[1]['uncertainty']=='no_candidate_contribution'
        if label=='analytic':
            assert target['best_q'] < target['fixed_q']*.1, target
            fixture=observations,old,refs,cards,width
        report.append(dict(case=label,nnls_checks=checks,meta=meta,evaluations=target['evaluations']))
    # Same spectral problem with input order/thread count changed must retain
    # per-identity scores/parameters; no timing or thread completion tie breaks.
    first=args.output/'analytic'
    deterministic=args.output/'deterministic';deterministic.mkdir()
    observations,old,refs,cards,width=fixture
    wire(deterministic/'input.txt',observations,old,refs[:2],list(reversed(cards)),width,1)
    _,same=execute(args.probe,deterministic,'probe')
    original=[json.loads(line) for line in (first/'probe.jsonl').read_text().splitlines()][1:]
    assert sorted(same,key=lambda r:r['id'])==sorted(original,key=lambda r:r['id'])
    with (first/'input.txt').open('rb') as source:
        cancelled=subprocess.run([str(args.probe.resolve()),'--cancel'],stdin=source,capture_output=True)
    assert cancelled.returncode==130 and not cancelled.stdout
    tied=[dict(id=str(i),original_index=i,normalized_gain=1.,uncertainty='') for i in range(40)]
    assert len(retain(tied,32)[0])==40
    (args.output/'self-test.json').write_text(json.dumps(dict(passed=True,cases=report,
        deterministic=True,cancellation_exit=cancelled.returncode,boundary_ties=True),indent=2)+'\n',encoding='utf-8')
    print('PASS: independent fine objectives, causal shift release, 100x width resource controls, zero protection, determinism, cancellation and cutoff ties')
    return 0


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('probe','case-directory','manifest','database','output','dependency-dir'):
        p.add_argument('--'+name,type=Path,required=name in ('probe','output'))
    p.add_argument('--threads',type=int,default=12)
    p.add_argument('--round',type=int,default=2)
    p.add_argument('--self-test',action='store_true')
    args=p.parse_args()
    if args.self_test:
        return self_test(args)
    if any(getattr(args,k) is None for k in ('case_directory','manifest','database')):
        p.error('case-directory, manifest and database required')
    args.output.mkdir(parents=True,exist_ok=False)
    summary=read(args.case_directory/'summary.json')
    trial=summary['rounds'][args.round]
    diagnostic=trial['forced_integral_diagnostic']
    assert trial['prescreen']['mode']=='off' and not diagnostic['formal_state_mutated']
    manifest=read(args.manifest)
    item=next(c for c in manifest['cases'] if c['id']==summary['id'])
    database_hashes={str(args.database.with_suffix(s).resolve()):v for s,v in manifest['database_hashes'].items()}
    assert all(digest(Path(path))==sha for path,sha in database_hashes.items())
    if any(k in item for k in ('break_before','mask','gaps')):
        raise RuntimeError('Saved-curve adapter requires no explicit break metadata')
    observations=[]
    for line in (args.case_directory/f'baseline-{args.round}.tsv').read_text().splitlines():
        if line.startswith('#'):continue
        x,y,b,w,*_=map(float,line.split());observations.append((x,y-b,w))
    differences=sorted(observations[i][0]-observations[i-1][0] for i in range(1,len(observations)))
    assert max(differences)<=3*differences[len(differences)//2]
    width=read(args.case_directory/'width.json')['H0_degrees']
    source=args.case_directory/f'forced-integral-round-{args.round}/prescreen-{args.round}.jsonl'
    v1=sorted((json.loads(line) for line in source.read_text().splitlines()),key=lambda r:r['original_index'])
    selected=diagnostic['selected_identities']
    assert len(v1)==summary['candidate_count']-len(selected)
    identities=selected+[r['id'] for r in v1]
    with sqlite3.connect(args.database.resolve().as_uri()+'?mode=ro',uri=True) as conn:
        raw=conn.execute('SELECT id,dvalue,intensita,nd FROM id WHERE id IN ('+','.join('?' for _ in identities)+')',identities).fetchall()
    references={}
    for identity,d,intensity,count in raw:
        def tokens(value):
            if not value:return []
            if isinstance(value,bytes):value=value.decode()
            return [float(v) for v in value.rstrip(',').split(',')]
        d,intensity=tokens(d),tokens(intensity)
        assert len(d)==len(intensity)==count
        references[str(identity)]=list(zip(d,intensity))
    cards=[dict(id=r['id'],original_index=r['original_index'],fine_shift=r['fine_shift'],
                original_shift=r['original']['shift'],lines=references[r['id']]) for r in v1]
    wire(args.output/'input.txt',observations,trial['baseline_parameters']['shifts_degrees'],
         [references[i] for i in selected],cards,width,args.threads,item['radiation'])
    provenance=[args.manifest,args.case_directory/'summary.json',args.case_directory/'width.json',source,
                args.case_directory/f'baseline-{args.round}.tsv',args.case_directory/f'round-{args.round}.jsonl']
    hashes={str(path.resolve()):digest(path) for path in provenance}
    # Freeze before the native pool measurement. The native input contains no
    # formal candidate evidence, card-specific exception or structural truth.
    (args.output/'frozen-config.json').write_text(json.dumps(dict(configuration=CONFIG,
        sources_sha256={str(path):digest(path) for path in (Path(__file__),Path('tests/unit/core/pfsm_relaxed_probe.cpp'))},
        input_sha256=hashes,database_sha256=database_hashes,wire_sha256=digest(args.output/'input.txt'),
        threads=args.threads,qualification='Revealed-case whole-pool diagnostic; no production routing or performance admission.'),indent=2)+'\n',encoding='utf-8')
    meta,rows=execute(args.probe,args.output,'pool')
    assert len(rows)==len(v1) and [r['id'] for r in rows]==[r['id'] for r in v1]
    replay=[]
    baseline=diagnostic['attempts'][-1]['fine_baseline_Q']
    for old,new in zip(v1,rows):
        if not new['complete'] or old['fine_gain'] is None:continue
        actual=max(0.,baseline-new['fixed_q'])
        expected=old['fine_gain']
        replay.append(dict(id=old['id'],actual=actual,expected=expected,
            passed=abs(actual-expected)<=2e-10*max(1,abs(expected))))
    relaxed,cutoff=retain(rows,CONFIG['extra_budget'])
    old_union={r['id'] for r in v1 if r['retained']}
    protected={r['id'] for r in rows if r['uncertainty']}
    union=old_union|relaxed|protected
    # Formal rows are consumed only after all candidate scoring is complete.
    formal=[json.loads(line) for line in (args.case_directory/f'round-{args.round}.jsonl').read_text().splitlines()]
    top5=[r['id'] for r in formal[:5]]
    ordered=sorted((r for r in rows if not r['uncertainty']),key=lambda r:(-r['normalized_gain'],r['original_index']))
    for row in rows:
        row.update(relaxed_selected=row['id'] in relaxed,old_retained=row['id'] in old_union,
                   retained=row['id'] in union)
    target=next(r for r in rows if r['id']=='7218520')
    report=dict(schema='qualpf.pfsm.relaxed-pool-assessment.v1',configuration=CONFIG,metadata=meta,
        pool_size=len(rows),completed=sum(r['complete'] for r in rows),
        target=target,target_rank=None if target['uncertainty'] else 1+sum(r['normalized_gain']>target['normalized_gain'] for r in ordered),
        relaxed_cutoff=cutoff,old_union_count=len(old_union),relaxed_count=len(relaxed),
        added_ids=[r['id'] for r in rows if r['id'] in union-old_union],retained_count=len(union),
        retained_ids=[r['id'] for r in rows if r['retained']],uncertain_ids=sorted(protected),
        top5=[dict(id=i,formal_rank=j+1,old_retained=i in old_union,relaxed_rank=next((k+1 for k,r in enumerate(ordered) if r['id']==i),None),retained=i in union) for j,i in enumerate(top5)],
        top5_retained=all(i in union for i in top5),replay_checks=replay,
        fixed_replay_passed=all(r['passed'] for r in replay),maximum_evaluations=max(r['evaluations'] for r in rows),
        total_evaluations=sum(r['evaluations'] for r in rows),
        maximum_candidate_cache_columns=max(r['candidate_cache_columns'] for r in rows),
        inputs_unchanged=all(digest(Path(path))==sha for path,sha in hashes.items()),
        qualification='Whole revealed round-2 pool only; formal evidence reused unchanged. No formal fitting rerun, enabled run, generalized recall or performance claim.')
    report['resource_checks_passed']=report['maximum_evaluations']<=205 and report['maximum_candidate_cache_columns']<=32 and meta['old_bank_columns']<=257*len(selected)
    report['passed']=report['fixed_replay_passed'] and report['resource_checks_passed'] and report['inputs_unchanged'] and report['top5_retained'] and target['retained']
    (args.output/'selection.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows),encoding='utf-8')
    (args.output/'assessment.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k in ('pool_size','completed','target_rank','retained_count','top5','fixed_replay_passed','maximum_evaluations','passed')},indent=2))
    return 0 if report['passed'] else 1


if __name__=='__main__':
    raise SystemExit(main())
