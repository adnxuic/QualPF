"""Completeness/approval guard tests; corrupt reports must not pass."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from powder_differential import compare
from powder_qualification import apply, fingerprint, partition


class Protocol(unittest.TestCase):
    def fixture(self):
        c=[dict(id=0,name='one',category='normal')]
        r=dict(id=1,hkl=[1,0,0],multiplicity=2,d=1,two_theta=30,fc2=1,lorentz=1,polarization=1,lp=1,raw=2)
        data={'0':dict(reflections=[r],groups=[dict(id=1,representative=1,count=1,members=[1])],
                       lines=[dict(id=1,representative=1,d=1,two_theta=30,raw=2,relative=1000)])}
        tol={k:dict(absolute=0,relative=0) for k in ('correction','raw','relative')}
        return c,data,tol

    def test_complete(self):
        c,a,t=self.fixture();self.assertEqual(compare(c,a,a,{},t)['raw_failure_count'],0)

    def test_missing_or_dual_case(self):
        c,a,t=self.fixture()
        with self.assertRaises(AssertionError):compare(c,a,{}, {},t)
        with self.assertRaises(AssertionError):compare(c,a,a,{'0':{}},t)

    def test_missing_or_duplicate_reflection(self):
        c,a,t=self.fixture(); b=copy.deepcopy(a);b['0']['reflections']=[]
        with self.assertRaises(AssertionError):compare(c,a,b,{},t)
        b=copy.deepcopy(a);b['0']['reflections']*=2
        with self.assertRaises(AssertionError):compare(c,a,b,{},t)

    def test_membership_position_rank_and_nonfinite_fail(self):
        c,a,t=self.fixture()
        for key,value in [('representative',2),('d',9),('raw',float('nan')),('relative',500)]:
            b=copy.deepcopy(a);b['0']['lines'][0][key]=value
            self.assertGreater(compare(c,a,b,{},t)['raw_failure_count'],0)
        b=copy.deepcopy(a);b['0']['groups'][0]['members']=[1,1]
        self.assertGreater(compare(c,a,b,{},t)['raw_failure_count'],0)

    def test_absent_approval_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            a,u,status=apply(Path(directory),'hash','GNU',[dict(case=1)])
            self.assertEqual(len(u),1);self.assertFalse(status['valid']);self.assertEqual(a,[])

    def test_per_field_compiler_and_stale_matching(self):
        r=dict(case=67,field='line.1.relative',legacy=1,cpp_by_compiler={'GNU':341.,'MSVC':342.})
        r['fingerprint']=fingerprint(r)
        f=dict(case=67,field='line.1.relative',legacy=1,cpp=341.)
        self.assertEqual(len(partition([f],[r],'GNU',[])[0]),1)
        for change in ({'case':68},{'field':'normalized_rank'},{'legacy':2},{'cpp':342.}):
            self.assertEqual(len(partition([dict(f,**change)],[r],'GNU',[])[1]),1)
        self.assertEqual(len(partition([f],[r],'GNU',['stale'])[1]),1)

if __name__=='__main__':unittest.main()
