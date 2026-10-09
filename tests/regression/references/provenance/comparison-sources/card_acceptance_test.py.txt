"""Exercise rejection paths without granting approval to real Phase 18 evidence."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

import card_differential as differential
import card_qualification as qualification


class AcceptanceContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='qualpf-card-gate-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.record = dict(case=1, field='synthetic.field', legacy=1.,
                           cpp_by_compiler={'GNU': 2.}, independent_check_passed=True)
        self.record['fingerprint'] = qualification.fingerprint(self.record)
        self.failure = dict(case=1, field='synthetic.field', legacy=1., cpp=2.)
        for path in qualification.REQUIRED:
            p = self.root / path
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('synthetic test evidence only')
        self.write('qualification-proof.json', dict(success=True, precision_decimal_digits=80, failures=[]))
        self.records = dict(records=[self.record], compiler_fingerprints={'GNU': {'runner_sha256': 'test-runner'}},
            proof_sha256=qualification.sha((self.root/qualification.PREFIX/'qualification-proof.json').read_bytes()))
        self.write('qualification-records.json', self.records)
        self.decision = dict(status='approved-by-user', qualification_count=1,
            numerical_equivalence_to_legacy=False, compiler_fingerprints=self.records['compiler_fingerprints'],
            evidence_sha256={p:qualification.sha((self.root/p).read_bytes()) for p in qualification.REQUIRED})

    def write(self, name, data):
        (self.root/qualification.PREFIX/name).write_text(json.dumps(data))

    def run_gate(self, failures=None, runner='test-runner', compiler='GNU'):
        return qualification.apply(self.root, runner, compiler, failures or [self.failure])

    def reject(self, result):
        accepted, unresolved, _ = result
        self.assertEqual(accepted, [])
        self.assertEqual(len(unresolved), 1)

    def test_missing_pending_and_stale_approval(self):
        self.reject(self.run_gate())
        for status in ('pending_explicit_user_acceptance', 'approved-by-agent', 'approved-by-user'):
            d = copy.deepcopy(self.decision)
            d['status'] = status
            if status == 'approved-by-user': d['evidence_sha256'].pop(next(iter(qualification.REQUIRED)))
            self.write('acceptance-decision.json', d)
            self.reject(self.run_gate())

    def test_exact_scope_and_compiler(self):
        self.write('acceptance-decision.json', self.decision)
        self.assertEqual(len(self.run_gate()[0]), 1)
        self.reject(self.run_gate(runner='rebuilt-runner'))
        self.reject(self.run_gate(compiler='unknown-compiler'))
        for key, value in [('cpp', 2.000000000000001), ('legacy', 1.01),
                           ('field', 'new.field'), ('case', 2)]:
            changed = dict(self.failure, **{key:value})
            self.reject(self.run_gate([changed]))

    def test_changed_evidence_and_failed_proof(self):
        self.write('acceptance-decision.json', self.decision)
        self.write('qualification-review.md', 'changed')
        self.reject(self.run_gate())
        self.write('qualification-proof.json', dict(success=False, precision_decimal_digits=80, failures=['failure']))
        self.reject(self.run_gate())

    def test_duplicate_or_unproved_records(self):
        for rows in ([self.record, self.record], [dict(self.record, independent_check_passed=False)]):
            records = dict(self.records, records=rows)
            self.write('qualification-records.json', records)
            decision = copy.deepcopy(self.decision)
            decision['qualification_count'] = len(rows)
            path = qualification.PREFIX+'qualification-records.json'
            decision['evidence_sha256'][path] = qualification.sha((self.root/path).read_bytes())
            self.write('acceptance-decision.json', decision)
            self.reject(self.run_gate())

    def test_protocol_rejects_incomplete_duplicate_and_nonfinite(self):
        valid = 'CASE 1\nERROR composition composition.cell_mass invalid_composition\nERROR lines powder.lines empty_line_set\nEND\n'
        self.assertEqual(set(differential.parse(valid)), {'1'})
        for malformed in (valid.replace('END\n',''), valid+valid, valid+'END\n',
                          valid.replace('END', 'CASE 2\nEND'),
                          valid.replace('END','ERROR lines powder.lines empty_line_set\nEND'),
                          valid.replace('END','COMPOSITION 1 nan 1 1 1\nEND'),
                          valid.replace('END','SITE 1 1 1 1\nSITE 1 1 1 1\nEND'),
                          valid.replace('END','COMPOSITION 1 1 1 1 1\nEND')):
            with self.subTest(malformed=malformed):
                with self.assertRaises(ValueError): differential.parse(malformed)


if __name__ == '__main__': unittest.main()
