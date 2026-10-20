#!/usr/bin/env python3
"""Bind frozen physical replay inputs; keep formal rankings in a separate comparison file."""
import argparse
import hashlib
import json
from pathlib import Path


from pfsm_candidate_recall_report import candidate_recall_report

def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def bind(path):
    path=Path(path).resolve()
    return dict(path=path.as_posix(),sha256=sha(path))


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def ranking(path):
    return [json.loads(line) for line in Path(path).read_text(encoding='utf-8').splitlines() if line]


def derive_selected(order,rankings):
    """Identity differences alone recover acceptance order; scores never enter the kernel."""
    if not rankings or len(set(order))!=len(order):
        raise ValueError('Missing full ranking or duplicate universe')
    selected=[];result=[];previous=set(order)
    for number,rows in enumerate(rankings):
        current={row['id'] for row in rows}
        if len(current)!=len(rows) or not current<=set(order):
            raise ValueError('Duplicate/unknown formal identity')
        for rank,row in enumerate(rows,1):
            if row['rank']!=rank or row['original_index']>=len(order) or order[row['original_index']]!=row['id']:
                raise ValueError('Unbound formal identity/order')
        removed=previous-current
        if number==0:
            if removed:raise ValueError('Reference is not a complete first-round ranking')
        elif len(removed)!=1 or current-previous:
            raise ValueError('Cannot recover exactly one accepted identity')
        else:selected.append(next(iter(removed)))
        if current!=set(order)-set(selected):raise ValueError('Incomplete reference pool')
        result.append(list(selected));previous=current
    return result


def prepare(measurement):
    if measurement['variant']!='shadow' or measurement.get('exit_code') is None:
        raise ValueError('Replay requires a completed full shadow reference')
    command=measurement['command']
    directory=Path(measurement.get('output_directory') or command[command.index('--output')+1])
    run=read(directory/'run.json');case_id=measurement['case']
    case=next(c for c in run['cases'] if c['id']==case_id)
    item=next(c for c in run['manifest']['cases'] if c['id']==case_id)
    source=directory/case_id
    order_path=source/'candidate-order.txt';order=order_path.read_text(encoding='utf-8').splitlines()
    if case['candidate_count']!=len(order) or case['candidate_order_sha256']!=sha(order_path):
        raise ValueError('Candidate universe binding mismatch')
    manifest=Path(command[command.index('--manifest')+1])
    if run['manifest_sha256']!=sha(manifest) or read(manifest)!=run['manifest']:
        raise ValueError('Frozen manifest mismatch')
    formal=[ranking(source/f'round-{n}.jsonl') for n in range(len(case['rounds']))]
    selected=derive_selected(order,formal)
    database=Path(command[command.index('--database')+1]);stem=str(database)[:-3]
    bindings={suffix:bind(stem+suffix) for suffix in ('.sq','.sq.info','.sq.infostat','.sq.search')}
    if {k:v['sha256'] for k,v in bindings.items()}!=run['database_hashes']:
        raise ValueError('Frozen database mismatch')
    width=read(source/'width.json')['H0_degrees']
    bounds=item.get('shift_bounds',[-.5,.5])
    physical=dict(schema='qualpf.pfsm.fast-replay-input.v1',case=case_id,database=bindings,
        candidate_order=bind(order_path),width=width,lower_shift=bounds[0],upper_shift=bounds[1],
        radiation=item['radiation'],template_cache=run['template_cache'],query=case['query'],rounds=[])
    comparison=dict(schema='qualpf.pfsm.fast-replay-comparison.v1',case=case_id,
        source_run=bind(directory/'run.json'),source_summary=bind(source/'summary.json'),
        source_manifest=bind(manifest),source_width=bind(source/'width.json'),
        command=command,identification_status=case['status'],rounds=[])
    for number,report in enumerate(case['rounds']):
        standalone=read(source/f'round-{number}.json')
        for field in ('baseline_Q','baseline_parameters','baseline_diagnostics','baseline_termination'):
            if standalone[field]!=report[field]:raise ValueError('Inconsistent frozen baseline')
        parameters=report['baseline_parameters']
        if len(parameters['scales'])!=len(selected[number]) or len(parameters['shifts_degrees'])!=len(selected[number]):
            raise ValueError('Unaligned frozen selected parameters')
        if candidate_recall_report(report)['mode']!='shadow' or candidate_recall_report(report)['normalization_scope']!='full_pool':
            raise ValueError('Reference is not complete formal evidence')
        physical['rounds'].append(dict(round=number,observations=bind(source/f'baseline-{number}.tsv'),
            selected_ids=selected[number],scales=parameters['scales'],shifts=parameters['shifts_degrees'],
            baseline_Q=report['baseline_Q']))
        comparison['rounds'].append(dict(round=number,formal=bind(source/f'round-{number}.jsonl'),
            report=bind(source/f'round-{number}.json'),v1_fast=bind(source/f'fast-{number}.jsonl')))
    return physical,comparison


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,action='append',required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--case',action='append',dest='cases')
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    result=dict(schema='qualpf.pfsm.fast-replay-input-index.v1',sources=[bind(p) for p in args.source],entries=[],nonready=[])
    seen=set()
    for source in args.source:
        previous=read(source)
        result['nonready'].extend(c for c in previous.get('cases',[]) if c.get('status')!='ready')
        for row in previous['measurements']:
            if row.get('variant')!='shadow' or row.get('exit_code') is None or (args.cases and row['case'] not in args.cases):continue
            key=f"{row['case']}-{row['threads']}-{'cache' if row['cache'] else 'direct'}"
            if key in seen:raise ValueError('Duplicate completed source: '+key)
            seen.add(key)
            physical,comparison=prepare(row)
            input_path=args.output/(key+'.input.json');comparison_path=args.output/(key+'.comparison.json')
            input_path.write_text(json.dumps(physical,indent=2)+'\n',encoding='utf-8')
            comparison['input']=bind(input_path)
            comparison_path.write_text(json.dumps(comparison,indent=2)+'\n',encoding='utf-8')
            result['entries'].append(dict(key=key,input=bind(input_path),comparison=bind(comparison_path),rounds=len(physical['rounds'])))
    (args.output/'index.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(f"Bound {len(result['entries'])} configurations and {sum(e['rounds'] for e in result['entries'])} rounds; no fitting performed")
    return 0


if __name__=='__main__':raise SystemExit(main())
