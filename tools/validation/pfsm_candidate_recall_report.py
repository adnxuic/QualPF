"""Read canonical candidate-recall reports and historical Fast-PFSM aliases."""
import json


def compatible_field(record, canonical, legacy, default=None):
    if canonical in record and legacy in record:
        # Preserve signed zero and binary64 serialization semantics.
        encode = lambda value: json.dumps(value, sort_keys=True, allow_nan=False, separators=(',', ':'))
        if encode(record[canonical]) != encode(record[legacy]):
            raise ValueError(f'Conflicting {canonical} and {legacy} fields')
    return record.get(canonical, record.get(legacy, default))


def candidate_recall_report(record):
    performance = record.get('performance', {})
    for current, old in (('shared', 'shared'), ('correlation', 'recall'),
                         ('local', 'local'), ('initial', 'initial'), ('review', 'review')):
        compatible_field(performance, f'candidate_recall_{current}_seconds', f'fast_{old}_seconds')
    return compatible_field(record, 'candidate_recall', 'fast_pfsm', {})


def candidate_recall_version(record):
    compatible_field(record, 'candidate_recall_mode', 'fast_pfsm_mode')
    return compatible_field(record, 'candidate_recall_version', 'fast_pfsm_version')
