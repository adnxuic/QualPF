#!/usr/bin/env python3
"""Headless acceptance contract using independent analytic data, not COD goldens."""
import argparse
import contextlib
import copy
import ctypes
import hashlib
import json
import math
from pathlib import Path
import re
import sqlite3
import subprocess
import sys
import tempfile
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', required=True)
    parser.add_argument('--replay-runner')
    parser.add_argument('--comparison-runner', help='Optional pre-refactor binary for exact regression comparison')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='pfsm-cli-') as tmp:
        root = Path(tmp)
        db = root/'test.sq'
        c = sqlite3.connect(db)
        c.execute('CREATE TABLE infodb(a,b,c,d,e)')
        c.execute("INSERT INTO infodb VALUES('0','fixture',9,'COD','independent analytic truth')")
        c.execute('CREATE TABLE id(id TEXT,name TEXT,mineralname TEXT,chemical_formula TEXT,spacegroup TEXT,quality TEXT,rir TEXT,nrec INTEGER,dvalue TEXT,intensita TEXT,n INTEGER,nd INTEGER)')
        c.execute('CREATE TABLE chemical(id TEXT,chemical_element TEXT)')
        # Independent synthetic phase chemistries: the two true phases have
        # different subsets of the allowed union. Card 4 contains an additional
        # disallowed element; card 8 has no recorded elements at all.
        elements = [('Si', 'O'), ('Al', 'O'), ('Si',), ('Al', 'O', 'Fe'),
                    ('O',), ('Al',), ('Fe',), (), ('Si', 'O')]
        positions = [30, 33, 28.5, 29, 31, 31.5, 32, 34, 34.5]
        for i, x in enumerate(positions):
            c.execute('INSERT INTO id VALUES(?,?,?,?,?,?,?,?,?,?,?,?)',
                      (str(i+1), 'analytic', '', 'fixture', 'fixture', 'C', '', 1,
                       str(1/(2*math.sin(math.radians(x/2))))+',', '1,', i+1, 1))
            c.executemany('INSERT INTO chemical VALUES(?,?)',
                          [(str(i+1), element) for element in elements[i]])
        c.commit()
        c.close()
        for suffix in ('.info', '.infostat', '.search'):
            conn = sqlite3.connect(str(db)+suffix)
            conn.execute('CREATE TABLE unused(id TEXT)')
            conn.commit()
            conn.close()
        data = root/'input.tsv'
        def peak(x, q):
            return (1+4*(2**(2/3)-1)*((x-q)/.2)**2)**(-1.5)
        data.write_text('\n'.join(f'{x:.17g}\t{7+100*peak(x,30.061)+30*peak(x,32.964):.17g}'
                                  for x in (28+i*.01 for i in range(701)))+'\n')
        truth = [dict(phase=name, accepted_ids=[str(i)], mapping_basis={'analytic_position': q})
                 for i, name, q in [(1,'strong',30),(2,'weak',33)]]
        chemical_source = root/'chemical-source.txt'
        chemical_source.write_text('Independent analytic fixture: the strong phase contains Si and O; '
                                   'the weak phase contains Al and O. The sample allowed-element union '
                                   'is Al, O, Si. Database decoys can contain Fe or have no chemistry.\n')
        chemical = dict(status='supported', kind='allowed_elements', elements=['Si', 'Al', 'O'],
                        basis='Union of independently declared synthetic phase chemistries',
                        evidence=[dict(path=chemical_source.name, sha256=sha(chemical_source),
                                       locator='Independent analytic fixture entry')])
        item = dict(id='analytic', status='ready', truth_source='Independent analytic IL positions/amplitudes',
                    truth=truth, data_file=data.name, data_sha256=sha(data), signal_source='analytic',
                    intensity_units='arbitrary', noise_source='explicit equal weights', noise='equal',
                    unclipped_signal=True, constant_background=7, shift_bounds=[-.12,.12],
                    radiation_source='analytic lambda=1', radiation=[dict(wavelength_angstrom=1,weight=1)],
                    chemical_constraints=chemical)
        manifest = dict(schema='qualpf.pfsm.benchmark.v1', cases=[item], database_hashes={
            suffix:sha(root/('test'+suffix)) for suffix in ('.sq','.sq.info','.sq.infostat','.sq.search')})
        path = root/'manifest.json'
        def run(name, extra=(), expected=0):
            path.write_text(json.dumps(manifest))
            command = [args.runner,'--database',str(db),'--manifest',str(path),
                       '--output',str(root/name),'--threads','2',*extra]
            if name == 'success' and sys.platform == 'win32':
                # Reproduce a reader that permits reads but denies replacement.
                kernel = ctypes.WinDLL('kernel32', use_last_error=True)
                kernel.CreateFileW.restype = ctypes.c_void_p
                kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_ulong, ctypes.c_ulong,
                                              ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_void_p]
                kernel.CloseHandle.argtypes = [ctypes.c_void_p]
                process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                           text=True, encoding='utf-8')
                held = set()
                deadline = time.monotonic() + 60
                with contextlib.ExitStack() as readers:
                    try:
                        while process.poll() is None and time.monotonic() < deadline:
                            progress = [root/name/'run-start.json',
                                        *(root/name).glob('*/summary-through-round-*.json')]
                            for entry in progress:
                                if entry in held or not entry.exists():
                                    continue
                                handle = kernel.CreateFileW(str(entry), 0x80000000, 1, None, 3, 0, None)
                                assert handle != ctypes.c_void_p(-1).value, ctypes.get_last_error()
                                readers.callback(kernel.CloseHandle, handle)
                                held.add(entry)
                            time.sleep(.005)
                        stdout, stderr = process.communicate(timeout=max(.001, deadline-time.monotonic()))
                    except BaseException:
                        process.kill()
                        process.communicate()
                        raise
                assert root/name/'run-start.json' in held, 'No report reader was exercised'
                result = subprocess.CompletedProcess(command, process.returncode, stdout, stderr)
            else:
                result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=60)
            report_path=root/name/'run.json'
            assert result.returncode == expected, (result.returncode,result.stdout,result.stderr,
                                                    report_path.read_text() if report_path.exists() else '')
            report = json.loads(report_path.read_text()) if report_path.exists() else None
            if args.comparison_runner and expected == 0 and report and 'cases' in report:
                before_command = list(command)
                before_command[0] = args.comparison_runner
                before = root / ('before-' + name)
                before_command[before_command.index('--output') + 1] = str(before)
                checked = subprocess.run(before_command, capture_output=True, text=True, encoding='utf-8', timeout=60)
                assert checked.returncode == 0, checked.stderr
                previous = json.loads((before/'run.json').read_text())
                assert len(previous['cases']) == len(report['cases'])
                for old, current in zip(previous['cases'], report['cases']):
                    assert old['id'] == current['id']
                    for prior_round, current_round in zip(old.get('rounds', []), current.get('rounds', [])):
                        for field in ('baseline_Q', 'baseline_Rwp_net', 'baseline_parameters', 'baseline_diagnostics',
                                      'baseline_termination', 'candidate_recall', 'top5'):
                            assert prior_round.get(field) == current_round.get(field), (name, field)
                    for prior_file in (before/old['id']).glob('*'):
                        if prior_file.name.startswith(('round-', 'baseline-', 'fast-')) and prior_file.suffix in ('.jsonl','.tsv') or prior_file.name == 'candidate-order.txt':
                            assert prior_file.read_bytes() == (root/name/current['id']/prior_file.name).read_bytes(), (name, prior_file.name)
            return report
        frozen_cache = ['--linear-algebra-cache-sizes', '49152,1310720,25165824']
        success = run('success', frozen_cache)
        assert success['linear_algebra_cache'] == dict(mode='explicit_process_start',
            l1_bytes=49152, l2_bytes=1310720, l3_bytes=25165824)
        assert success['fast_pfsm_mode']=='off' and success['fast_pfsm_version']=='v2'
        v2 = run('v2-shadow-small', [*frozen_cache,'--pfsm-candidate-recall','shadow','--pfsm-candidate-recall-version','v2'])
        for report in v2['cases'][0]['rounds']:
            assert report['fast_pfsm']['version']==3 and report['fast_pfsm']['algorithm_version']=='v2'
        run('bad-fast-version',['--fast-pfsm-version','unknown'],expected=1)
        assert success['candidate_recall_mode'] == success['fast_pfsm_mode'] == 'off'
        assert success['candidate_recall_version'] == success['fast_pfsm_version'] == 'v2'
        for report in v2['cases'][0]['rounds']:
            assert report['candidate_recall'] == report['fast_pfsm']
            for current, old in [('shared','shared'),('correlation','recall'),('local','local'),('initial','initial'),('review','review')]:
                assert report['performance'][f'candidate_recall_{current}_seconds'] == report['performance'][f'fast_{old}_seconds']
        run('retired-v1',['--fast-pfsm-version','v1'],expected=1)
        run('retired-v1-canonical',['--pfsm-candidate-recall-version','v1'],expected=1)
        run('mode-conflict',['--pfsm-candidate-recall','off','--fast-pfsm','enabled'],expected=1)
        run('version-conflict',['--pfsm-candidate-recall-version','v2','--fast-pfsm-version','v1'],expected=1)
        same = run('matching-aliases', [*frozen_cache,'--pfsm-candidate-recall','shadow','--fast-pfsm','shadow',
            '--pfsm-candidate-recall-version','v2','--fast-pfsm-version','v2'])
        assert same['candidate_recall_mode'] == 'shadow'
        for number in range(len(v2['cases'][0]['rounds'])):
            assert (root/f'matching-aliases/analytic/round-{number}.jsonl').read_bytes() == (root/f'v2-shadow-small/analytic/round-{number}.jsonl').read_bytes()

        if args.replay_runner:
            sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'tools/validation'))
            from pfsm_candidate_recall_replay_inputs import prepare
            physical,comparison=prepare(dict(variant='shadow',exit_code=0,case='analytic',
                output_directory=str(root/'v2-shadow-small'),command=[args.runner,'--database',str(db),
                '--manifest',str(path),'--output',str(root/'v2-shadow-small')]))
            replay_input=root/'replay-input.json';replay_input.write_text(json.dumps(physical),encoding='utf-8')
            replay_command=[args.replay_runner,'--input',str(replay_input),'--output',str(root/'replayed'),
                '--threads','2','--stage','abc']
            replay_result=subprocess.run(replay_command,capture_output=True,text=True,timeout=60)
            assert replay_result.returncode==0,(replay_result.stdout,replay_result.stderr)
            from pfsm_candidate_recall_replay_inputs import bind
            from pfsm_candidate_recall_replay_compare import compare as compare_replay
            comparison['input']=bind(replay_input)
            assessed=compare_replay(comparison,root/'replayed')
            assert assessed['screening_passed'] and assessed['formal_fits']==0
            replayed=json.loads((root/'replayed/run.json').read_text(encoding='utf-8'))
            assert replayed['formal_fits']==0 and len(replayed['rounds'])==3
            assert all(r['formal_fits']==0 and set(r['pool'])==set(r['recalled']) and
                set(r['pool'])==set(r['bc']['retained_indices']) for r in replayed['rounds'])
            physical['rounds'][0]['observations']['sha256']='0'*64
            bad_input=root/'replay-invalid.json';bad_input.write_text(json.dumps(physical),encoding='utf-8')
            rejected=subprocess.run([args.replay_runner,'--input',str(bad_input),'--output',str(root/'replay-invalid')],
                capture_output=True,text=True,timeout=60)
            assert rejected.returncode==1 and not (root/'replay-invalid/run.json').exists()

        repeated = run('same-blocking', frozen_cache)
        assert repeated['linear_algebra_cache'] == success['linear_algebra_cache']
        for number in range(3):
            for stem, suffix in [('round', 'jsonl'), ('baseline', 'tsv')]:
                assert (root/f'success/analytic/{stem}-{number}.{suffix}').read_bytes() == (
                    root/f'same-blocking/analytic/{stem}-{number}.{suffix}').read_bytes()
        for i, invalid in enumerate(['49152,1310720', '-1,1310720,25165824',
                                     '49152,32768,25165824', 'x,1310720,25165824',
                                     '9223372036854775808,1310720,25165824']):
            # Startup argument failures use the existing main-level exit 1;
            # per-case validation failures below retain exit 2.
            run(f'invalid-blocking-{i}', ['--linear-algebra-cache-sizes', invalid], expected=1)
        assert re.fullmatch(r'(?:[0-9a-f]{40}|[0-9a-f]{64}|unavailable)', success['code_revision'])
        assert success['chemistry_mode_requested']=='evidence' and success['acceptance_role']=='primary'
        report = success['cases'][0]
        # Progress files are immutable: Windows readers can hold a report without
        # allowing replacement. Final paths retain the completed-report contract.
        started = json.loads((root/'success/run-start.json').read_text())
        assert 'cases' not in started and started['manifest_sha256'] == success['manifest_sha256']
        for number in range(2):
            checkpoint = json.loads((root/f'success/analytic/summary-through-round-{number}.json').read_text())
            assert len(checkpoint['rounds']) == number + 1
        assert not (root/'success/analytic/summary-through-round-2.json').exists()
        assert report['status']=='passed_top5_assisted' and report['identified_phases']==2
        assert [r['top5'][0]['id'] for r in report['rounds'][:2]]==['1','2']
        assert report['candidate_count']==6 and len(report['rounds'])==3
        assert report['chemical_constraints']['elements']==['Al','O','Si']
        assert report['chemical_constraints']['source_hashes_verified']
        assert report['chemical_constraints']['evidence']==chemical['evidence']
        assert report['query']['elements']=='Al and O and Si' and report['query']['element_operator']=='any'
        assert report['query']['executed'] and not report['query']['truth_based_filters']
        assert report['rounds'][2]['baseline_Rwp_net'] < .015
        pool={'1','2','3','5','6','9'}
        assert set((root/'success/analytic/candidate-order.txt').read_text().splitlines())==pool
        for number, accepted in enumerate([set(),{'1'},{'1','2'}]):
            ranked={json.loads(line)['id'] for line in
                    (root/f'success/analytic/round-{number}.jsonl').read_text().splitlines()}
            assert ranked==pool-accepted, (number,ranked)
        for mode in ('shadow', 'enabled'):
            variant=run('fast-'+mode, ['--fast-pfsm', mode])
            assert variant['fast_pfsm_mode']==mode
            for number, r in enumerate(variant['cases'][0]['rounds']):
                fast=r['fast_pfsm']
                assert fast['mode']==mode and fast['recall_budget']==500 and fast['fit_budget']==20
                assert fast['attempts'][-1]['fallback_reason']=='small_pool'
                assert (root/f'fast-{mode}/analytic/round-{number}.jsonl').read_bytes()==(root/f'success/analytic/round-{number}.jsonl').read_bytes()
        run('invalid-fast', ['--fast-pfsm','invalid'], expected=1)
        unrestricted=run('unrestricted',['--chemistry','none'])
        assert unrestricted['linear_algebra_cache'] == dict(mode='automatic_cpu_detection')
        assert unrestricted['acceptance_role']=='supplemental' and unrestricted['chemistry_mode_requested']=='none'
        unrestricted_case=unrestricted['cases'][0]
        assert unrestricted_case['candidate_count']==9 and unrestricted_case['status']=='passed_top5_assisted'
        assert unrestricted_case['chemical_constraints']['status']=='not_requested'
        assert unrestricted_case['query']['scope']=='unconstrained'
        # Conditional quantification never claims full-library identification.
        item['quantification_card_ids']=['1','2']
        conditional=run('quantification',['--quantify-selected'])
        assert conditional['acceptance_role']=='not_acceptance'
        assert conditional['mode']=='conditional-selected-quantification'
        case=conditional['cases'][0]
        assert case['candidate_count']==2 and case['query']['truth_based_filters']
        assert case['specified_phase_count']==2 and 'phase_recall' not in case
        assert case['status']=='conditional_quantification_unavailable'  # Analytic lambda=1 is not Cu.
        csv=(root/'quantification/analytic/quantification.csv').read_text(encoding='utf-8')
        assert 'unsupported_rir_radiation' in csv and 'PFSM/RIR' in csv
        del item['quantification_card_ids']
        assert set((root/'unrestricted/analytic/candidate-order.txt').read_text().splitlines())==set(map(str,range(1,10)))
        for number, accepted in enumerate([set(),{'1'},{'1','2'}]):
            ranked={json.loads(line)['id'] for line in
                    (root/f'unrestricted/analytic/round-{number}.jsonl').read_text().splitlines()}
            assert ranked==set(map(str,range(1,10)))-accepted
        # A separate, independently prescribed Fe-only decoy matches both
        # observed peak centers/amplitude ratios. Its better numerical score
        # cannot override source-supported sample chemistry.
        conn=sqlite3.connect(db)
        decoy_d=','.join(str(1/(2*math.sin(math.radians(x/2)))) for x in [30.061,32.964])+','
        conn.execute('INSERT INTO id VALUES(?,?,?,?,?,?,?,?,?,?,?,?)',
                     ('10','overlap decoy','','Fe','fixture','C','',1,decoy_d,'100,30,',10,2))
        conn.execute('INSERT INTO chemical VALUES(?,?)',('10','Fe'))
        conn.commit();conn.close()
        manifest['database_hashes']['.sq']=sha(db)
        overlap_none=run('overlap-none',['--chemistry','none','--profile-candidates','10'])
        assert overlap_none['cases'][0]['status']=='profiling_not_acceptance'
        assert overlap_none['cases'][0]['candidate_count']==10
        unrestricted_scores={r['id']:r['Q'] for r in map(json.loads,
            (root/'overlap-none/analytic/round-0.jsonl').read_text().splitlines())}
        assert unrestricted_scores['10'] < min(unrestricted_scores['1'],unrestricted_scores['2'])
        overlap_evidence=run('overlap-evidence',['--profile-candidates','10'])
        assert overlap_evidence['cases'][0]['status']=='profiling_not_acceptance'
        assert overlap_evidence['cases'][0]['candidate_count']==len(pool)
        assert {json.loads(line)['id'] for line in
                (root/'overlap-evidence/analytic/round-0.jsonl').read_text().splitlines()}==pool
        conn=sqlite3.connect(db)
        conn.execute('DELETE FROM chemical WHERE id=?',('10',))
        conn.execute('DELETE FROM id WHERE id=?',('10',))
        conn.commit();conn.close()
        manifest['database_hashes']['.sq']=sha(db)
        profile=run('profile', ['--profile-candidates','3'])
        assert profile['acceptance_role']=='not_acceptance'
        assert profile['cases'][0]['status']=='profiling_not_acceptance'
        assert run('prepare', ['--prepare-only'])['cases'][0]['status']=='prepared_not_evaluated'
        # Performance cases carry no invented truth, and retain the full chemical
        # pool through an explicitly recorded acceptance sequence.
        truth, truth_source = item.pop('truth'), item.pop('truth_source')
        timed = run('performance', ['--performance-only'])
        assert timed['acceptance_role']=='not_acceptance' and timed['mode']=='performance-only'
        case = timed['cases'][0]
        assert case['status']=='performance_not_acceptance' and case['candidate_count']==len(pool)
        assert len(case['rounds'])==1 and 'phase_recall' not in case and 'true_phase_count' not in case
        assert case['points']==701 and case['import_seconds']>=0 and case['preflight_preparation_seconds']>=0
        assert case['end_to_end_seconds']>=case['rounds'][0]['operation_to_result_seconds']
        r = case['rounds'][0]
        assert 'truth_card_ranks' not in r and 'false_cards_in_top5' not in r
        assert r['display_seconds'] is None and r['publication_seconds']>=0 and r['search_seconds']>0
        item['performance_accept_ids']=['1','2']
        replay = run('performance-replay', ['--performance-only'])['cases'][0]
        assert replay['status']=='performance_not_acceptance' and replay['replayed_phase_count']==2
        assert len(replay['rounds'])==3 and 'phase_recall' not in replay
        for number, accepted in enumerate([set(),{'1'},{'1','2'}]):
            ranked={json.loads(line)['id'] for line in
                    (root/f'performance-replay/analytic/round-{number}.jsonl').read_text().splitlines()}
            assert ranked==pool-accepted
        item['performance_accept_ids']=['1','1']
        assert run('performance-duplicate', ['--performance-only'], expected=2)['cases'][0]['status']=='error'
        item['performance_accept_ids']=['missing']
        assert run('performance-missing', ['--performance-only'], expected=2)['cases'][0]['status']=='error'
        del item['performance_accept_ids']
        run('performance-conflict', ['--performance-only','--quantify-selected'], expected=1)
        item['truth'], item['truth_source'] = truth, truth_source
        cached = run('cached', ['--template-cache'])['cases'][0]
        assert cached['status']=='passed_top5_assisted' and cached['identified_phases']==2
        assert cached['rounds'][-1]['baseline_diagnostics']['exact_final_verification']
        assert cached['rounds'][-1]['baseline_diagnostics']['interpolated_iterations']
        assert abs(cached['rounds'][-1]['baseline_Rwp_net']-report['rounds'][-1]['baseline_Rwp_net'])<1e-8
        run('invalid-chemistry-mode', ['--chemistry','invalid'], expected=1)
        del item['chemical_constraints']
        pending=run('missing-chemistry',expected=2)['cases'][0]
        assert pending['status']=='chemical_evidence_pending' and not pending['query']['executed']
        assert not (root/'missing-chemistry/analytic/candidate-order.txt').exists()
        pending=run('profile-missing-chemistry',['--profile-candidates','3'],expected=2)['cases'][0]
        assert pending['status']=='chemical_evidence_pending' and not pending['query']['executed']
        prepared=run('prepare-missing-chemistry',['--prepare-only'])['cases'][0]
        assert prepared['status']=='prepared_not_evaluated'
        assert prepared['chemical_constraints']['status']=='evidence_pending' and not prepared['query']['executed']
        for status in ['evidence_pending','unsupported']:
            item['chemical_constraints']=dict(status=status,reason='Independent chemical source unavailable')
            pending=run('chemical-'+status,expected=2)['cases'][0]
            assert pending['status']=='chemical_evidence_pending' and not pending['query']['executed']
        invalid=[]
        for label, key, value in [('symbol','elements',['Al','Xx']),('case','elements',['al','O']),
                                  ('duplicate','elements',['Al','Al']),('empty','elements',[]),
                                  ('formula','elements',['Al2O3']),('not-array','elements','Al O'),
                                  ('kind','kind','required_elements'),('status','status','unknown'),
                                  ('basis','basis','  '),('sources','evidence',[])]:
            entry=copy.deepcopy(chemical);entry[key]=value;invalid.append((label,entry))
        for label,key,value in [('locator','locator',' '),('sha-format','sha256','abc'),
                                ('hash-drift','sha256','0'*64),('absolute-path','path',str(chemical_source))]:
            entry=copy.deepcopy(chemical);entry['evidence'][0][key]=value;invalid.append((label,entry))
        invalid.append(('not-object',['Al','O']))
        for label,entry in invalid:
            item['chemical_constraints']=entry
            rejected=run('chemical-invalid-'+label,expected=2)['cases'][0]
            assert rejected['status']=='error' and not rejected['query']['executed'], (label,rejected)
            assert not (root/f'chemical-invalid-{label}/analytic/candidate-order.txt').exists()
        # Supplemental mode does not require, validate, or infer chemistry.
        ignored=run('none-invalid-chemistry',['--chemistry','none'])['cases'][0]
        assert ignored['candidate_count']==9 and ignored['chemical_constraints']['status']=='not_requested'
        item['chemical_constraints']=chemical
        original_chemical_bytes=chemical_source.read_bytes()
        chemical_source.write_text('Changed independent chemistry evidence\n')
        rejected=run('chemical-source-drift',expected=2)['cases'][0]
        assert rejected['status']=='error' and 'Chemical source hash mismatch' in rejected['error']
        assert not rejected['query']['executed']
        chemical_source.write_bytes(original_chemical_bytes)
        item['validation_split'] = 'development'
        item['evaluation_family'] = 'analytic'
        manifest['cases'].append(dict(item, id='heldout', validation_split='holdout', evaluation_family='heldout'))
        selected = run('split-development', ['--split','development','--prepare-only'])
        assert selected['validation_split']=='development' and [c['id'] for c in selected['cases']]==['analytic']
        selected = run('split-holdout', ['--split','holdout','--prepare-only'])
        assert selected['validation_split']=='holdout' and [c['id'] for c in selected['cases']]==['heldout']
        run('invalid-split', ['--split','invalid'], expected=1)
        manifest['cases'] = [item]
        item['intensity_scaled'] = True
        item['noise'] = 'raw_counts'
        invalid_counts = run('scaled-as-counts', expected=2)['cases'][0]
        assert invalid_counts['status']=='error' and 'raw Poisson counts' in invalid_counts['error']
        item['noise'] = 'equal'
        del item['intensity_scaled']
        # Hash drift is rejected before fitting. A prior output directory is never overwritten.
        item['data_sha256']='0'*64
        assert run('hash-drift',expected=2)['cases'][0]['status']=='error'
        item['data_sha256']=sha(data)
        manifest['database_hashes']['.sq']='0'*64
        run('db-drift',expected=1)
        manifest['database_hashes']['.sq']=sha(db)
        run('success',expected=1)
        # An explicit integer nd=0 with empty paired arrays is a legal zero
        # template. Malformed, fractional or absent counts must not become zero.
        def set_last(count, d=None, intensity=None):
            conn = sqlite3.connect(db)
            conn.execute('UPDATE id SET nd=?,dvalue=?,intensita=? WHERE id=?',
                         (count,d,intensity,'9'))
            conn.commit()
            conn.close()
            manifest['database_hashes']['.sq']=sha(db)
        set_last(0)
        assert run('zero-template')['cases'][0]['status']=='passed_top5_assisted'
        for label,count in [('text','invalid'),('fraction',.5),('absent',None)]:
            set_last(count)
            assert run('invalid-count-'+label,expected=2)['cases'][0]['status']=='error'
        set_last(1,'1.5,','')
        assert run('unpaired',expected=2)['cases'][0]['status']=='error'
        item['status']='evidence_pending';item['reason']='Deliberately missing independent truth'
        assert run('missing-truth',expected=2)['cases'][0]['status']=='evidence_pending'
        print('PASS: headless PFSM chemical subsets, complete retained pools, primary/supplemental distinction, '
              'Top-5, atomic acceptance, provenance and profiling distinction')


if __name__=='__main__':
    main()
