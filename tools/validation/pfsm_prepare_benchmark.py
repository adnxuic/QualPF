#!/usr/bin/env python3
"""Freeze raw observations, metadata and structural truth before any product run.

This is an input adapter, never an expected-output generator. It reads SQLite
read-only. It preserves decimal observations/errors and rejects ambiguous data.
"""
import argparse
from contextlib import closing
from decimal import Decimal
import hashlib
import json
import math
import os
from pathlib import Path
import re
import sqlite3
import xml.etree.ElementTree as ET


def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def read_rows(path, fmt):
    text = path.read_text(encoding='utf-8-sig')
    if fmt == 'nist_measured_cif':
        # NIST DBD files have a separately labelled measured block. Never read
        # the calculated loop later in the same file as observations.
        start = text.index('_pd_proc_ls_weight') + len('_pd_proc_ls_weight')
        rows = []
        for line in text[start:].splitlines():
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith(('data_', '_', 'loop_', 'stop_')):
                break
            row = line.split()
            if len(row) != 3:
                raise ValueError('Expected three NIST measured-loop columns')
            rows.append(row)
        expected = int(re.search(r'_pd_proc_number_of_points\s+(\d+)', text).group(1))
        if len(rows) != expected:
            raise ValueError('NIST measured-loop count disagrees with declared size')
        return rows, {}
    if fmt == 'xrdml':
        root = ET.fromstring(text)
        ns = {'x': root.tag.split('}')[0][1:]}
        scans = root.findall('.//x:scan', ns)
        if len(scans) != 1:
            raise ValueError('Only a single XRDML scan is supported')
        points = scans[0].find('x:dataPoints', ns)
        positions = points.find("x:positions[@axis='2Theta']", ns)
        if positions is None or positions.attrib.get('unit') != 'deg':
            raise ValueError('XRDML coordinates require explicit 2Theta degrees')
        start = Decimal(positions.find('x:startPosition', ns).text)
        end = Decimal(positions.find('x:endPosition', ns).text)
        counts = points.find('x:counts', ns)
        if counts is None:
            counts = points.find('x:intensities', ns)
        if counts is None or counts.attrib.get('unit') != 'counts':
            raise ValueError('XRDML requires explicit counts; rates cannot be inferred')
        values = counts.text.split()
        if len(values) < 3 or end <= start:
            raise ValueError('XRDML requires at least three ascending samples')
        step = (end-start)/(len(values)-1)
        wave = root.find('.//x:usedWavelength', ns)
        radiation = [dict(wavelength_angstrom=float(wave.find('x:kAlpha1', ns).text), weight=1),
                     dict(wavelength_angstrom=float(wave.find('x:kAlpha2', ns).text),
                          weight=float(wave.find('x:ratioKAlpha2KAlpha1', ns).text))]
        return [[str(start+i*step), y] for i, y in enumerate(values)], {'radiation': radiation}
    lines = [x.strip() for x in text.splitlines() if x.strip() and not x.lstrip().startswith('#')]
    if fmt == 'packed_start_step_end':
        start, step, end = map(Decimal, lines[0].split())
        values = ' '.join(lines[1:]).split()
        if step <= 0 or abs(start+(len(values)-1)*step-end) > abs(step)/100:
            raise ValueError('Packed-grid endpoint does not match observation count')
        return [[str(start+i*step), y] for i, y in enumerate(values)], {}
    if fmt in ('columns', 'csv_sigma'):
        rows = [re.split(r'[,\s]+', x) for x in lines]
        if fmt == 'csv_sigma':
            wave = re.search(r'Calibrated wavelength\s*=\s*([\d.]+)', text)
            if wave is None:
                raise ValueError('Missing calibrated CSV wavelength')
            metadata = {'radiation': [dict(wavelength_angstrom=float(wave[1]), weight=1)]}
            declared = re.search(r'No\. steps\s*=\s*(\d+)', text)
            if declared:
                metadata['format_diagnostics'] = dict(
                    header_steps=int(declared[1]), tabulated_rows=len(rows),
                    header_steps_equal_rows=int(declared[1]) == len(rows),
                    coordinate_policy='Use every explicit tabulated coordinate; never truncate or reconstruct a CSV grid from acquisition headers.')
            normalization = re.search(r'Normalization factor\s*=\s*([-+0-9.eE]+)', text)
            if normalization:
                factor = float(normalization[1])
                if not math.isfinite(factor) or factor <= 0:
                    raise ValueError('Invalid recorded CSV normalization factor')
                metadata['intensity_scaled'] = True
                metadata.setdefault('format_diagnostics', {})['normalization_factor'] = normalization[1]
            return rows, metadata
        return rows, {}
    raise ValueError('Unsupported raw format: '+fmt)


def structural_map(db, recipes):
    with closing(sqlite3.connect(db.as_uri()+'?mode=ro', uri=True)) as main, \
         closing(sqlite3.connect(Path(str(db)+'.info').as_uri()+'?mode=ro', uri=True)) as info:
        cells = {str(row[0]): row[1:] for row in info.execute('select id,a,b,c,alpha,beta,gamma from info')}
        cards = main.execute('select id,chemical_formula,spacegroup,name,mineralname from id order by id').fetchall()
    mapped = {}
    for name, rule in recipes.items():
        result = []
        for ident, formula, spacegroup, chemical_name, mineral in cards:
            alias = any(str(ident) == a['id'] and formula.strip() == a['formula']
                        for a in rule.get('formula_aliases', []))
            if formula.strip() not in rule['formulas'] and not alias:
                continue
            cell = cells.get(str(ident))
            if cell is None or any(v is None or not math.isfinite(v) or v <= 0 for v in cell):
                continue
            matched = None
            for variant in [rule] + rule.get('variants', []):
                if (spacegroup or '').split(':')[0].strip() not in variant['spacegroups']:
                    continue
                if all(abs(cell[k]-variant['cell'][k]) <=
                       (variant['cell'][k]*rule['relative_cell_tolerance'] if k < 3 else rule['angle_tolerance_degrees'])
                       for k in range(6)):
                    matched = variant.get('setting', 'primary')
                    break
            if matched is None:
                continue
            result.append(dict(id=str(ident), formula=formula, spacegroup=spacegroup, cell=cell,
                               name=chemical_name, mineral=mineral, matched_setting=matched,
                               formula_alias=alias))
        mapped[name] = dict(rule=rule, cards=result)
    return mapped


def validate_cases(cases):
    identifiers = [c['id'] for c in cases]
    if len(set(identifiers)) != len(identifiers) or any(
            not re.fullmatch(r'[A-Za-z0-9_-]+', x) for x in identifiers):
        raise ValueError('Case identifiers must be unique safe directory names')
    families = {}
    for case in cases:
        split = case.get('validation_split')
        if split is None:
            continue
        if split not in ('development', 'holdout') or not case.get('evaluation_family'):
            raise ValueError('Explicit validation splits require a valid split and sample family')
        family = case['evaluation_family']
        if family in families and families[family] != split:
            raise ValueError('One physical sample family cannot cross validation splits')
        families[family] = split


def freeze_chemical_constraints(value, root, output):
    """Freeze independently supplied composition evidence, never structural truth."""
    if value is None:
        return dict(status='evidence_pending', reason='No independent chemical-composition evidence supplied')
    if not isinstance(value, dict):
        raise ValueError('Chemical constraints must be an object')
    result = dict(value)
    if value.get('status') != 'supported':
        if not isinstance(value.get('reason'), str) or not value['reason'].strip():
            raise ValueError('Unsupported chemical evidence requires a reason')
        result['status'] = 'evidence_pending'
        return result
    symbols = set(('H He Li Be B C N O F Ne Na Mg Al Si P S Cl Ar K Ca Sc Ti V Cr Mn Fe Co Ni Cu Zn '
                   'Ga Ge As Se Br Kr Rb Sr Y Zr Nb Mo Tc Ru Rh Pd Ag Cd In Sn Sb Te I Xe Cs Ba La Ce '
                   'Pr Nd Pm Sm Eu Gd Tb Dy Ho Er Tm Yb Lu Hf Ta W Re Os Ir Pt Au Hg Tl Pb Bi Po At Rn '
                   'Fr Ra Ac Th Pa U Np Pu Am Cm Bk Cf Es Fm Md No Lr Rf Db Sg Bh Hs Mt Ds Rg Cn Nh Fl '
                   'Mc Lv Ts Og').split())
    elements = value.get('elements')
    if value.get('kind') != 'allowed_elements' or not isinstance(elements, list) or not elements or any(
            not isinstance(e, str) or e not in symbols for e in elements) or len(set(elements)) != len(elements):
        raise ValueError('Chemical constraints require unique valid allowed elements')
    if not isinstance(value.get('basis'), str) or not value['basis'].strip():
        raise ValueError('Chemical constraints require an independent composition basis')
    evidence = value.get('evidence')
    if not isinstance(evidence, list) or not evidence:
        raise ValueError('Chemical constraints require source evidence')
    frozen = []
    for entry in evidence:
        if not isinstance(entry, dict) or not isinstance(entry.get('path'), str) or not entry['path'] or not isinstance(
                entry.get('locator'), str) or not entry['locator'].strip():
            raise ValueError('Chemical evidence requires a source path and locator')
        relative = Path(entry['path'])
        source = (root/relative).resolve()
        if relative.is_absolute() or not source.is_relative_to(root.resolve()):
            raise ValueError('Chemical recipe evidence must stay within the input root')
        actual = sha(source)
        if 'sha256' in entry and entry['sha256'] != actual:
            raise ValueError('Chemical recipe source hash mismatch: '+str(source))
        frozen_entry = dict(entry)
        frozen_entry.update(path=Path(os.path.relpath(source, output)).as_posix(), sha256=actual)
        frozen.append(frozen_entry)
    result.update(elements=sorted(elements), evidence=frozen)
    return result


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--database', type=Path, required=True)
    cli.add_argument('--recipes', type=Path, required=True)
    cli.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    cli.add_argument('--output', type=Path, required=True)
    args = cli.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    root = args.root.resolve()
    recipe = json.loads(args.recipes.read_text(encoding='utf-8'))
    validate_cases(recipe['cases'])
    db = args.database.resolve()
    hashes = {suffix: sha(Path(str(db)[:-3]+suffix)) for suffix in ('.sq', '.sq.info', '.sq.infostat', '.sq.search')}
    mapped = structural_map(db, recipe['phase_rules'])
    (output/'structural-mapping.json').write_text(json.dumps(mapped, indent=2)+'\n')
    cases = []
    for source in recipe['cases']:
        item = dict(source)
        if item['status'] != 'ready':
            try:
                item['chemical_constraints'] = freeze_chemical_constraints(item.get('chemical_constraints'), root, output)
            except (ValueError, OSError, KeyError) as error:
                item['chemical_constraints'] = dict(status='evidence_pending', reason=str(error))
            cases.append(item)
            continue
        try:
            raw = root/item.pop('raw_file')
            rows, inferred = read_rows(raw, item.pop('format'))
            item.update(inferred)
            columns = 3 if item['noise'] in ('standard_deviation', 'least_squares_weight') else 2
            previous = -math.inf
            for row in rows:
                values = list(map(float, row))
                if len(row) != columns or not all(map(math.isfinite, values)) or values[0] <= previous:
                    raise ValueError('Invalid data dimensions or coordinates')
                previous = values[0]
                if columns == 3 and values[2] <= 0:
                    raise ValueError('Nonpositive uncertainty/weight')
            if len(rows) < 3:
                raise ValueError('Insufficient observations')
            target = output/(item['id']+'.tsv')
            target.write_text('\n'.join('\t'.join(row) for row in rows)+'\n', encoding='utf-8')
            item['data_file'] = target.name
            item['data_sha256'] = sha(target)
            sources = [raw] + [root/p for p in item.pop('evidence_files', [])]
            item['sources'] = [dict(path=Path(os.path.relpath(p, output)).as_posix(), sha256=sha(p)) for p in sources]
            item['chemical_constraints'] = freeze_chemical_constraints(item.get('chemical_constraints'), root, output)
            truth = []
            for phase in item.pop('phases'):
                mapping = mapped[phase]
                truth.append(dict(phase=phase, accepted_ids=[c['id'] for c in mapping['cards']],
                                  mapping_basis=mapping['rule']))
            item['truth'] = truth
            if any(not p['accepted_ids'] for p in truth):
                item['status'] = 'uncovered_truth_phase'
                item['reason'] = 'No card satisfies the frozen structural mapping'
            item['conversion'] = 'Decimal token preservation; generated grid only for explicit start/step/end or XRDML. No clipping, subtraction, resampling or normalization.'
        except (ValueError, OSError, KeyError, AttributeError) as error:
            item['status'] = 'input_unresolved'
            item['reason'] = str(error)
        cases.append(item)
    manifest = dict(schema='qualpf.pfsm.benchmark.v1', recipe_sha256=sha(args.recipes),
                    database_hashes=hashes, structural_mapping_sha256=sha(output/'structural-mapping.json'),
                    validation_split_policy=recipe.get('validation_split_policy', {}), cases=cases)
    (output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'ready': sum(c['status'] == 'ready' for c in cases), 'total': len(cases),
                      'unresolved': [(c['id'], c.get('reason')) for c in cases if c['status'] != 'ready']}, indent=2))


if __name__ == '__main__':
    main()
