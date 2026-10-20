#!/usr/bin/env python3
"""Single serial v2 replay/enabled launcher; old full formal evidence is read-only."""
import argparse
import contextlib
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import time
from pfsm_candidate_recall_replay_inputs import sha,read,bind
from pfsm_candidate_recall_replay_compare import compare as compare_replay,verified
from pfsm_candidate_recall_compare import compare as compare_formal
from pfsm_fast_validation import verify
from pfsm_candidate_recall_report import candidate_recall_report, candidate_recall_version

PRIORITY=('cpd-1c','cpd-1f','cpd-1h','Dara_binary_TiO2_0p4009-ZnO_0p6010--2min',
          'Dara_binary_TiO2_0p4009-ZnO_0p6010--8min','Dara_precursors_Bi2O3',
          'Dara_reaction_2FeC2O4_H2O_2-Y2O3_200C_60min')


def save(path,value):
    temporary=path.with_suffix(path.suffix+'.tmp')
    temporary.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8');os.replace(temporary,path)


@contextlib.contextmanager
def lock(path):
    stream=path.open('a+b');stream.seek(0)
    if stream.read(1)==b'':stream.write(b'0');stream.flush()
    stream.seek(0)
    try:
        if os.name=='nt':
            import msvcrt
            msvcrt.locking(stream.fileno(),msvcrt.LK_NBLCK,1)
        else:
            import fcntl
            fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
        yield
    finally:stream.close()


def active_workloads():
    import psutil
    return [p.info for p in psutil.process_iter(['pid','name','exe','cmdline','create_time'])
        if (p.info['name'] or '').lower() in ('qualpf_pfsm_benchmark.exe','qualpf_pfsm_fast_replay.exe',
            'qualpf_pfsm_benchmark','qualpf_pfsm_fast_replay',
            'qualpf_pfsm_candidate_recall_replay.exe','qualpf_pfsm_candidate_recall_replay')]


def unrelated_workloads(busy,runtime,owned_output):
    """Require positive ownership separation; equal binaries remain duplicates."""
    own_hashes={sha(p) for p in runtime.glob('qualpf_pfsm*.exe') if p.is_file()}
    if not own_hashes:raise RuntimeError('Cannot identify owned PFSM runtime')
    result=[]
    for process in busy:
        executable=process.get('exe');command=process.get('cmdline') or []
        if not executable or '--output' not in command:
            raise RuntimeError('Existing PFSM workload has unknown ownership: '+str(process))
        try:
            digest=sha(Path(executable))
            output=Path(command[command.index('--output')+1]).resolve()
        except (OSError,IndexError) as error:
            raise RuntimeError('Existing PFSM workload ownership unavailable') from error
        if digest in own_hashes or output.is_relative_to(owned_output.resolve()):
            raise RuntimeError('Existing PFSM workload duplicates owned runtime/output: '+str(process))
        result.append(dict(process,executable_sha256=digest))
    return result


def execute(command,output,receipt_path,environment,allow_unrelated=False):
    if receipt_path.exists():
        receipt=read(receipt_path)
        if receipt['command']!=command:raise RuntimeError('Completed command binding mismatch')
        if receipt.get('exit_code') is None:raise RuntimeError('Unfinished receipt requires ownership diagnosis: '+str(receipt_path))
        for name,expected in receipt['files'].items():
            if sha(output/name)!=expected:raise RuntimeError('Completed evidence changed: '+name)
        return receipt,True
    busy=active_workloads()
    if busy and not allow_unrelated:raise RuntimeError('Existing PFSM workload; refusing duplication: '+str(busy))
    if busy:busy=unrelated_workloads(busy,Path(command[0]).resolve().parent,receipt_path.parent)
    if output.exists():raise RuntimeError('Unreceipted output requires diagnosis: '+str(output))
    import psutil
    receipt=dict(command=command,started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        exit_code=None,cpu_percent_before=psutil.cpu_percent(interval=.2),environment=environment,
        concurrent_workloads_before=busy,controlled_timing=False)
    save(receipt_path,receipt)
    start=time.perf_counter()
    with receipt_path.with_suffix('.stdout.log').open('w',encoding='utf-8') as stdout,receipt_path.with_suffix('.stderr.log').open('w',encoding='utf-8') as stderr:
        process=subprocess.Popen(command,stdout=stdout,stderr=stderr,env=dict(os.environ,**environment))
        receipt['pid']=process.pid;receipt['process_created']=psutil.Process(process.pid).create_time();save(receipt_path,receipt)
        receipt['exit_code']=process.wait()
    receipt['process_seconds']=time.perf_counter()-start
    receipt['concurrent_workloads_after']=active_workloads()
    receipt['files']={p.relative_to(output).as_posix():sha(p) for p in output.rglob('*') if p.is_file()}
    receipt['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();save(receipt_path,receipt)
    return receipt,False


def unavailable_replay(physical,input_binding,destination,receipt_path,receipt):
    """Classify a bounded fast-layer exclusion, without inventing a native run."""
    reason='Fast grid node limit'
    stderr=receipt_path.with_suffix('.stderr.log')
    if receipt.get('exit_code')!=1 or not stderr.is_file() or stderr.read_text(encoding='utf-8').strip()!='pfsm.fast: '+reason:
        return None
    start=read(destination/'run-start.json')
    if start['input_sha256']!=sha(verified(input_binding)) or start.get('formal_fits')!=0 or start['executable_sha256']!=sha(receipt['command'][0]):
        raise RuntimeError('Unavailable replay binding mismatch')
    rounds=[]
    for frozen in physical['rounds']:
        data=verified(frozen['observations']).read_text(encoding='utf-8').splitlines()
        x=[float(line.split()[0]) for line in data if line.strip() and not line.startswith('#')]
        step=physical['width']/8; margin=max(abs(physical['lower_shift']),abs(physical['upper_shift']))
        origin=min(0.,min(x))-margin-2*step;end=max(180.,max(x))+margin+2*step
        nodes=math.ceil((end-origin)/step)+1
        if nodes<=131072:raise RuntimeError('Reported grid exclusion is not supported by frozen inputs')
        rounds.append(dict(round=frozen['round'],grid_nodes=nodes,fast_layer_executed=False,
            a_local_top5=None,initial_top5=None,reviewed_top5=None,final_screening_top5=None,
            formal_fits=0,formal_preservation='requires_new_enabled_full_pool_validation'))
    return dict(schema='qualpf.pfsm.fast-replay-unavailable.v1',case=physical['case'],
        reason=reason,screening_passed=None,rounds=rounds,formal_fits=0,
        original_failure_receipt=bind(receipt_path),original_stderr=bind(stderr),native_run_completed=False)


def comparisons_passed(comparisons):
    covered={v['key'].removeprefix('enabled-') for v in comparisons
        if v['mode']=='enabled' and v['passed'] is True and v.get('covers_unavailable_replay')}
    return all(v['passed'] is True or (v['mode']=='replay' and v['passed'] is None and
        v['key'].removeprefix('replay-') in covered) for v in comparisons)


def enabled_command(old,executable,output):
    flags = {'--fast-pfsm', '--fast-pfsm-version', '--pfsm-candidate-recall', '--pfsm-candidate-recall-version'}
    command = [str(executable)]
    tokens = iter(old[1:])
    for token in tokens:
        if token in flags:
            next(tokens)  # Both aliases are replaced by one explicit current choice.
        elif token.split('=', 1)[0] not in flags:
            command.append(token)
    command[command.index('--output')+1] = str(output)
    command += ['--pfsm-candidate-recall', 'enabled', '--pfsm-candidate-recall-version', 'v2']
    return command


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--inputs',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--mode',choices=('replay','enabled','both'),default='both')
    p.add_argument('--case',action='append',dest='cases');p.add_argument('--resume',action='store_true')
    p.add_argument('--allow-unrelated-workloads',action='store_true',
        help='Descriptive validation only: permit positively identified different runtimes outside this cohort; record load.')
    args=p.parse_args();args.output=args.output.resolve();args.runtime=args.runtime.resolve()
    if args.output.exists() and not args.resume:raise RuntimeError('Output exists; use explicit resume to verify completed receipts')
    args.output.mkdir(parents=True,exist_ok=True)
    with lock(args.output/'launcher.lock'):
        index=read(args.inputs);benchmark=args.runtime/'qualpf_pfsm_benchmark.exe';replay=args.runtime/'qualpf_pfsm_candidate_recall_replay.exe'
        if not replay.exists(): replay=args.runtime/'qualpf_pfsm_fast_replay.exe'  # Frozen historical runtime.
        binding=verify(benchmark,complete=True)
        configuration=dict(input_index=bind(args.inputs),runtime_binding=binding,protocol='reuse-full-formal; new fast replay/enabled only',
            fixed_budgets=dict(recall=500,normal=20,maximum_review_or_formal=80),controlled_speed_acceptance=False,allow_unrelated_workloads=args.allow_unrelated_workloads)
        report_path=args.output/'validation.json'
        report=read(report_path) if report_path.exists() else dict(schema='qualpf.pfsm.fast-v2-validation.v1',configuration=configuration,
            nonready=index['nonready'],measurements=[],comparisons=[])
        if report['configuration']!=configuration:raise RuntimeError('Resume source/runtime mismatch')
        entries=[e for e in index['entries'] if not args.cases or read(verified(e['input']))['case'] in args.cases]
        entries.sort(key=lambda e:PRIORITY.index(read(verified(e['input']))['case']) if read(verified(e['input']))['case'] in PRIORITY else len(PRIORITY))
        versions={name:sha(Path(__file__).with_name(name)) for name in
            ('pfsm_candidate_recall_validation.py','pfsm_candidate_recall_replay_compare.py',
             'pfsm_candidate_recall_compare.py','pfsm_candidate_recall_report.py')}
        if versions not in report.setdefault('workflow_versions',[]):report['workflow_versions'].append(versions)
        assessor=hashlib.sha256(json.dumps(versions,sort_keys=True).encode()).hexdigest()[:12]
        save(report_path,report)
        modes=('replay','enabled') if args.mode=='both' else (args.mode,)
        for mode in modes:
            for entry in entries:
                physical=read(verified(entry['input']));comparison=read(verified(entry['comparison']));key=mode+'-'+entry['key']
                destination=args.output/key;receipt_path=args.output/(key+'.receipt.json')
                if mode=='replay':command=[str(replay),'--input',str(verified(entry['input'])),'--output',str(destination),'--stage','abc','--threads','12']
                else:command=enabled_command(comparison['command'],benchmark,destination)
                threads=command[command.index('--threads')+1]
                environment=dict(OMP_NUM_THREADS=threads,OMP_DYNAMIC='FALSE',OMP_MAX_ACTIVE_LEVELS='1',
                    OPENBLAS_NUM_THREADS='1',MKL_NUM_THREADS='1',QT_PLUGIN_PATH=str(args.runtime/'plugins'))
                print(key,flush=True)
                receipt,reused=execute(command,destination,receipt_path,environment,args.allow_unrelated_workloads)
                measurement=dict(key=key,mode=mode,case=physical['case'],receipt=bind(receipt_path),reused=reused,
                    exit_code=receipt['exit_code'],process_seconds=receipt['process_seconds'],output_directory=str(destination))
                report['measurements']=[m for m in report['measurements'] if m['key']!=key]+[measurement];save(report_path,report)
                unavailable=None
                if mode=='replay' and not (destination/'run.json').is_file():
                    unavailable=unavailable_replay(physical,entry['input'],destination,receipt_path,receipt)
                if not (destination/'run.json').is_file() and unavailable is None:
                    report['stopped']=dict(key=key,reason='No completed run; diagnose before any further workload');save(report_path,report)
                    raise RuntimeError('Incomplete run: '+key)
                assessment_path=args.output/(key+'.assessment-'+assessor+'.json')
                if assessment_path.exists():
                    old=next((v for v in report['comparisons'] if v['key']==key),None)
                    if not old or sha(assessment_path)!=old['assessment']['sha256']:raise RuntimeError('Unbound completed assessment')
                    assessment=read(assessment_path)
                elif mode=='replay':
                    assessment=unavailable or compare_replay(comparison,destination);save(assessment_path,assessment)
                else:
                    original_path=verified(comparison['source_run']);original=read(original_path);actual=read(destination/'run.json')
                    if actual['executable_sha256']!=sha(benchmark) or candidate_recall_version(actual)!='v2':raise RuntimeError('Wrong enabled executable/configuration')
                    before=next(c for c in original['cases'] if c['id']==physical['case']);after=next(c for c in actual['cases'] if c['id']==physical['case'])
                    replay_row=next((v for v in report['comparisons'] if v['key']=='replay-'+entry['key']),None)
                    needs_full=replay_row is not None and replay_row.get('unavailable_reason') is not None
                    assessment=compare_formal(before,after,original_path.parent/physical['case'],destination/physical['case'],subset=not needs_full)
                    if needs_full:
                        full=all(candidate_recall_report(r)['normalization_scope']=='full_pool' and
                            candidate_recall_report(r)['attempts'][-1]['full_fallback'] for r in after['rounds'])
                        if not full:assessment['errors'].append('Unavailable fast replay requires actual full-pool enabled validation');assessment['passed']=False
                        assessment['covers_unavailable_replay']=full and assessment['passed']
                    assessment.update(case=physical['case'],reference=bind(original_path),actual=bind(destination/'run.json'),
                        original_identification_status=before['status'],new_identification_status=after['status'])
                    save(assessment_path,assessment)
                passed=assessment['screening_passed'] if mode=='replay' else assessment['passed']
                report['comparisons']=[v for v in report['comparisons'] if v['key']!=key]+[dict(key=key,mode=mode,passed=passed,assessment=bind(assessment_path),
                    unavailable_reason=assessment.get('reason') if unavailable else None,
                    covers_unavailable_replay=assessment.get('covers_unavailable_replay',False))]
                report.pop('stopped',None);save(report_path,report)
                print(key+(': FAST LAYER UNAVAILABLE; full enabled verification required' if passed is None else ': PASS' if passed else ': FAILED (preserved)'),flush=True)
        if verify(benchmark,complete=True)!=binding:raise RuntimeError('Runtime binding changed during validation')
        report['requested_modes_complete']=list(modes);report['passed']=comparisons_passed(report['comparisons']);save(report_path,report)
        return 0 if report['passed'] else 1


if __name__=='__main__':raise SystemExit(main())
