#!/usr/bin/env python3
"""Compare saved PFSM probe observations and work; never create numerical goldens."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path


def compare(before, after):
    differences = []
    totals = [Counter(), Counter()]
    for side, report in enumerate((before, after)):
        for row in report['cases']:
            if not row['passed']:
                differences.append(f"independent reference failed: {side}/{row.get('id', row.get('name'))}")
            text = row.get('stderr', row.get('probe_stderr', ''))
            totals[side].update(json.loads(text))
    if len(before['cases']) != len(after['cases']):
        differences.append('case count')
    for left, right in zip(before['cases'], after['cases']):
        identity = left.get('id', left.get('name'))
        if identity != right.get('id', right.get('name')):
            differences.append(f'{identity}: case identity')
        field = 'actual' if 'actual' in left else 'probe_stdout'
        if field not in left or field not in right:
            differences.append(f'{identity}: missing scientific observations')
        elif json.dumps(left[field], sort_keys=True) != json.dumps(right[field], sort_keys=True):
            differences.append(f'{identity}: scientific observations')
        left_work = json.loads(left.get('stderr', left.get('probe_stderr', '')))
        right_work = json.loads(right.get('stderr', right.get('probe_stderr', '')))
        if left_work['nnls_solves'] != right_work['nnls_solves']:
            differences.append(f'{identity}: NNLS solve count')
    return dict(differences=differences, before=dict(totals[0]), after=dict(totals[1]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reduced', action='append', default=[])
    args = parser.parse_args()
    if args.output.exists():
        raise RuntimeError('Refusing to overwrite evidence')
    report = compare(json.loads(args.before.read_text()), json.loads(args.after.read_text()))
    for field in args.reduced:
        if report['after'][field] >= report['before'][field]:
            report['differences'].append(f'work did not decrease: {field}')
    report['inputs'] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in (args.before, args.after)}
    report['qualification'] = 'Same-input implementation comparison; independent reference checks remain required.'
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
    return bool(report['differences'])


if __name__ == '__main__':
    raise SystemExit(main())
