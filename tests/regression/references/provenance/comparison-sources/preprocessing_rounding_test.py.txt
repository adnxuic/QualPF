"""Independent exact Decimal oracle for scalar compatibility rounding.

binary64 cases cover the bounded normal Cholesky operands, not a general libm
replacement. Cancellation and ties are included alongside deterministic data.
"""
import argparse
from decimal import Decimal, localcontext
import math
import random
import struct
import subprocess

def narrow(x): return struct.unpack('f',struct.pack('f',x))[0]
def bits(x): return struct.unpack('I',struct.pack('f',x))[0]
def from_bits(x): return struct.unpack('f',struct.pack('I',x))[0]

def nearest32(exact):
    middle=narrow(float(exact));word=bits(middle)
    candidates=[from_bits(i) for i in (max(0,word-1),word,word+1) if i<0x7f800000 or 0x80000000<=i<0xff800000]
    return min(candidates,key=lambda x:(abs(Decimal.from_float(x)-exact),bits(x)&1))

def main():
    p=argparse.ArgumentParser();p.add_argument('--runner',required=True);args=p.parse_args()
    rng=random.Random(901);cases=[]
    for kind in (32,64):
        cast=narrow if kind==32 else float
        for i in range(4000):
            a,b,c=[cast(math.ldexp(rng.uniform(-1,1),rng.randint(-60,60))) for _ in range(3)]
            if i%3==0:c=cast(-cast(a*b))
            cases.append((kind,a,b,c))
        k=23 if kind==32 else 52
        cases.extend([(kind,1.+2.**-k,1.-2.**-k,-1.),
                      (kind,1.,2.**(-k-1),1.),
                      (kind,1.+2.**-k,2.**(-k-1),1.),
                      (kind,1.-2.**(-k-1),2.**(-k-1),1.)])
    data=''.join(f'{k} {a:.17g} {b:.17g} {c:.17g}\n' for k,a,b,c in cases)
    result=subprocess.run([args.runner],input=data,text=True,capture_output=True,check=True)
    values=list(map(float,result.stdout.split()));assert len(values)==len(cases)
    with localcontext() as context:
        context.prec=500
        for case,actual in zip(cases,values):
            kind,a,b,c=case;exact=Decimal.from_float(a)*Decimal.from_float(b)+Decimal.from_float(c)
            expected=nearest32(exact) if kind==32 else float(exact)
            assert actual==expected,(case,actual,expected)
    print(f'PASS: {len(cases)} independently rounded products/sums, cancellation and midpoint cases')

if __name__=='__main__':main()
