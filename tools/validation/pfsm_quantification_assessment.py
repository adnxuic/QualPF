#!/usr/bin/env python3
"""Freeze independent inputs, then assess selected-phase PFSM/RIR error.

This is conditional quantification, never phase-identification acceptance.
Masses are kept out of the runner manifest and never calibrate reference RIR.
The lowest numeric non-deleted structurally mapped card ID is fixed before fits.
Only the eight CPD mixtures and twenty development binary scans are selected;
the frozen ternary holdout is not read or run.
"""
import argparse
import copy
import csv
from decimal import Decimal
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import subprocess

CPD = {
    'a': ('1.15', '94.81', '4.04'), 'b': ('94.31', '4.33', '1.36'),
    'c': ('5.04', '1.36', '93.59'), 'd': ('13.53', '53.58', '32.89'),
    'e': ('55.12', '29.62', '15.25'), 'f': ('27.06', '17.72', '55.22'),
    'g': ('31.37', '34.42', '34.21'), 'h': ('35.12', '34.69', '30.19')}
FORMULAS = {'In2O3': 'In2O3_bixbyite', 'MnCO3': 'MnCO3_rhodochrosite',
    'Li2CO3': 'Li2CO3_zabuyelite', 'ZrO2': 'ZrO2_baddeleyite',
    'NiO': 'NiO_rocksalt', 'Bi2O3': 'Bi2O3_bismite', 'TiO2': 'TiO2_anatase',
    'ZnO': 'Zincite', 'V2O5': 'V2O5_shcherbinaite'}


def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False)+'\n', encoding='utf-8')


def mass_truth(case):
    if case['id'].startswith('cpd-1'):
        raw = dict(zip(('Corundum', 'Fluorite', 'Zincite'), CPD[case['id'][-1]]))
        source = 'https://www.iucr.org/__data/iucr/powder/QARR/results.htm Sample 1 weighed rows'
        qualification = 'Organizer weighed wt.%; published 1c and 1e sums are 99.99, explicitly normalized.'
    else:
        tokens = re.findall(r'([A-Za-z0-9()]+)_(\d+p\d+)', case['id'])
        if len(tokens) != 2:
            raise ValueError('Expected exactly two author mass tokens: '+case['id'])
        raw = {FORMULAS[formula]: token.replace('p', '.') for formula, token in tokens}
        source = 'https://doi.org/10.1021/acs.chemmater.5c02820.s002; original author XRDML filenames'
        qualification = ('Nominal weighed-mass tokens from author sample labels, not a separate weighing log. '
                         'No fitted Jade/Dara phase fractions are read; normalized to sum.')
    if set(raw) != {p['phase'] for p in case['truth']}:
        raise ValueError('Independent mass labels disagree with structural phase identities')
    total = sum(map(Decimal, raw.values()))
    return dict(raw_values=raw, sum=str(total), normalized_wt_percent={p: float(Decimal(v)/total*100) for p, v in raw.items()},
                source=source, qualification=qualification)


def freeze(args):
    root = args.output.resolve(); root.mkdir(parents=True, exist_ok=False)
    manifest_path = args.manifest.resolve(); base = manifest_path.parent
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    database = args.database.resolve()
    for suffix, expected in manifest['database_hashes'].items():
        if sha(Path(str(database)[:-3]+suffix)) != expected:
            raise ValueError('Frozen database mismatch: '+suffix)
    cases = [copy.deepcopy(c) for c in manifest['cases'] if c.get('validation_split') == 'development' and
             (c['id'] in {'cpd-1'+k for k in CPD} or c['id'].startswith('Dara_binary_'))]
    if len(cases) != 28 or any(c['status'] != 'ready' for c in cases):
        raise ValueError('Expected 8 CPD and 20 ready development binary scans')
    db = sqlite3.connect(database.as_uri()+'?mode=ro', uri=True)
    references, truths, frozen = {}, {}, []
    for case in cases:
        truths[case['id']] = mass_truth(case)
        case['data_file'] = str((base/case['data_file']).resolve())
        if sha(case['data_file']) != case['data_sha256']:
            raise ValueError('Observation hash mismatch')
        for entry in case['sources'] + case['chemical_constraints']['evidence']:
            original = (base/entry['path']).resolve()
            if sha(original) != entry['sha256']:
                raise ValueError('Original evidence hash mismatch')
            entry['path'] = Path(os.path.relpath(original, root)).as_posix()
        ids = []
        for phase in case['truth']:
            candidates = []
            for card_id in phase['accepted_ids']:
                row = db.execute('SELECT id,name,mineralname,chemical_formula,spacegroup,quality,rir,nd,dvalue,intensita FROM id WHERE id=?', (card_id,)).fetchone()
                if row and str(row[5]).strip() != 'D':
                    candidates.append(row)
            if not candidates:
                raise ValueError('No non-deleted structural card: '+phase['phase'])
            row = min(candidates, key=lambda r: int(r[0])); card_id = str(row[0]); ids.append(card_id)
            fields = ('id', 'name', 'mineralname', 'formula', 'spacegroup', 'quality', 'rir', 'nd', 'dvalue', 'intensity')
            card = dict(zip(fields, row)); card['id'] = card_id
            intensities = [float(v) for v in str(row[9]).split(',') if v.strip()]
            card['full_reference_maximum'] = max(intensities, default=0)
            card['raw_row_sha256'] = hashlib.sha256(json.dumps(list(row), ensure_ascii=False).encode()).hexdigest()
            card['phase'] = phase['phase']; references[card_id] = card
        case['quantification_card_ids'] = ids
        frozen.append(dict(id=case['id'], data_sha256=case['data_sha256'], sources=[dict(sha256=e['sha256'], filename=Path(e['path']).name) for e in case['sources']],
                           card_ids=ids, radiation=case['radiation']))
    db.close()
    # All card/normalization/RIR and truth inputs are frozen before any runner starts.
    evidence = dict(schema='qualpf.pfsm.quantification-inputs.v1', input_manifest_sha256=sha(manifest_path),
        database_hashes=manifest['database_hashes'], policy='Lowest numeric non-deleted structural card per phase, before fits; no RIR or intensity calibration.',
        scope='8 CPD and 20 development binary scans; conditional selected-phase quantification, no holdout or identification claim.',
        references=references, cases=frozen, independent_mass_truth=truths)
    save(root/'frozen-inputs.json', evidence)
    manifest['cases'] = cases
    save(root/'runner-manifest.json', manifest)  # Deliberately contains no mass truth.
    print(json.dumps(dict(cases=len(cases), cards=len(references), frozen_sha256=sha(root/'frozen-inputs.json'))))


def evaluate(args):
    inputs = args.inputs.resolve(); evidence = json.loads((inputs/'frozen-inputs.json').read_text(encoding='utf-8'))
    destination = args.output.resolve()
    if destination.exists():
        raise ValueError('Preserve prior results; output must be new')
    command = [str(args.runner.resolve()), '--database', str(args.database.resolve()), '--manifest', str(inputs/'runner-manifest.json'),
               '--output', str(destination), '--threads', '1', '--split', 'development', '--quantify-selected']
    with (inputs/(destination.name+'-runner.log')).open('w', encoding='utf-8') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=1800, check=False)
    if result.returncode:
        raise RuntimeError('Assessment runner failed; original log retained: '+str(result.returncode))
    run = json.loads((destination/'run.json').read_text(encoding='utf-8'))
    comparisons, errors = [], {'CPD': [], 'Dara_binary': []}
    for case in run['cases']:
        truth = evidence['independent_mass_truth'][case['id']]
        final = case['rounds'][-1]
        record = dict(id=case['id'], status=case['status'], rwp_net=final['baseline_Rwp_net'], phases=[])
        with (destination/case['id']/'quantification.csv').open(encoding='utf-8', newline='') as stream:
            for row in csv.DictReader(stream):
                reference = evidence['references'][row['phase_id']]
                if float(row['reference_maximum']) != reference['full_reference_maximum']:
                    raise ValueError('Product used a different reference intensity normalization')
                expected = truth['normalized_wt_percent'][reference['phase']]
                actual = float(row['weight_percent']) if row['weight_percent'] else None
                error = actual-expected if actual is not None else None
                if error is not None:
                    errors['CPD' if case['id'].startswith('cpd-1') else 'Dara_binary'].append(abs(error))
                record['phases'].append(dict(phase=reference['phase'], card_id=row['phase_id'], expected_wt_percent=expected,
                    actual_wt_percent=actual, signed_error_pp=error, effective_rir=row['effective_rir'], reason=row['reason']))
        comparisons.append(record)
    metrics = {group: dict(phase_occurrences=len(values), mean_absolute_error_pp=sum(values)/len(values) if values else None,
                          maximum_absolute_error_pp=max(values) if values else None) for group, values in errors.items()}
    all_errors = sum(errors.values(), [])
    metrics['all'] = dict(phase_occurrences=len(all_errors), mean_absolute_error_pp=sum(all_errors)/len(all_errors) if all_errors else None,
                          maximum_absolute_error_pp=max(all_errors) if all_errors else None)
    report = dict(schema='qualpf.pfsm.quantification-assessment.v1', frozen_inputs_sha256=sha(inputs/'frozen-inputs.json'),
        executable_sha256=sha(args.runner), run_sha256=sha(destination/'run.json'), code_revision=run['code_revision'], compiler=run['compiler'],
        qualifications=['Conditional on fixed structurally mapped phase identities; not identification acceptance.',
            'One common fixed IL width, existing solver/defaults and raw library RIR; no fraction-derived calibration.',
            'RIR provenance/radiation and profile-model mismatch remain scientific limitations.',
            'Dara masses are author nominal sample-label tokens, not an independently retained weighing log.',
            'All 28 cases retained, including unavailable results; no new accuracy tolerance or confidence interval.',
            'The 22-case frozen holdout is untouched.'], metrics=metrics, cases=comparisons)
    save(destination/'assessment.json', report)
    print(json.dumps(metrics, indent=2))


def main():
    cli = argparse.ArgumentParser(description=__doc__); sub = cli.add_subparsers(dest='command', required=True)
    parser = sub.add_parser('freeze'); parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--database', type=Path, required=True); parser.add_argument('--output', type=Path, required=True)
    parser = sub.add_parser('evaluate'); parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--database', type=Path, required=True); parser.add_argument('--runner', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = cli.parse_args(); (freeze if args.command == 'freeze' else evaluate)(args)


if __name__ == '__main__':
    main()
