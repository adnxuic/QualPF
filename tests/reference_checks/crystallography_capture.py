from reference_paths import reference_path, provenance_path
"""Explicit Legacy observation; never calls the C++ foundation or overwrites captures."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def parse(text):
    lines=text.split('FOUNDATION13\n',1)[1].splitlines()
    return {v[0]:[float(x) for x in v[1:]] for line in lines if (v:=line.split())}

def cases():
    cells = {
        'cubic':[5,5,5,90,90,90], 'tetragonal':[4,4,9,90,90,90],
        'orthorhombic':[3,7,11,90,90,90], 'hexagonal':[4,4,7,90,90,120],
        'rhombohedral':[5,5,5,77,77,77], 'monoclinic':[4,6,9,90,108,90],
        'triclinic':[4,5,6,72,83,64], 'anisotropic':[.01,10,10000,90,90,90],
        'small':[1e-5,2e-5,3e-5,81,93,107], 'large':[1e5,2e5,3e5,81,93,107],
        'near_zero_angle':[5,6,7,90,90,1], 'near_pi_angle':[5,6,7,90,90,179],
        'near_triangle_limit':[5,6,7,60,60,119],
    }
    # Extracted from the actual COD cards already exercised by repository Phase 7-12
    # workflows. No structure CIF fixtures exist in the tracked repository.
    real=json.loads((reference_path(ROOT/'tests/data/crystallography-real-cells.json')).read_text())
    for row in real['records']:
        cells['repository_workflow_cod_'+row['id']]=row['cell']
    hkls=[[1,0,0],[0,1,0],[0,0,1],[1,2,3],[-1,-2,-3],[2,-3,5],[1001,-997,991]]
    output=[]
    for name,p in cells.items():
        for i,hkl in enumerate(hkls):
            output.append(dict(id=f'{name}-{i}',cell=p,hkl=hkl,wavelength=min(p[:3])*1e-6,
                               category='limiting' if name.startswith('near_') else 'normal',expected_error=None))
    for name in ('cubic','tetragonal','orthorhombic','hexagonal','rhombohedral','monoclinic','triclinic'):
        for radiation,wave in [('cu',1.54059307),('mo',.71073)]:
            output.append(dict(id=name+'-'+radiation,cell=cells[name],hkl=[1,1,1],wavelength=wave,
                               category='normal',expected_error=None))
    output.append(dict(id='backscatter',cell=cells['cubic'],hkl=[1,0,0],wavelength=10,
                       category='normal',expected_error=None))
    def edge(name,p=None,h=None,w=None,reason=None):
        output.append(dict(id=name,cell=p or cells['cubic'],hkl=h if h is not None else [1,2,3],
                           wavelength=1 if w is None else w,category='historical_edge',expected_error=reason))
    for name,p in [('zero_length',[0,5,5,90,90,90]),('negative_length',[-5,5,5,90,90,90]),
                   ('nan_length',['NaN',5,5,90,90,90]),('inf_length',['Infinity',5,5,90,90,90])]:
        edge(name,p,reason='invalid_cell_length')
    for name,a in [('zero_angle',0),('negative_angle',-1),('straight_angle',180),('large_angle',181),('nan_angle','NaN'),('inf_angle','Infinity')]:
        edge(name,[5,5,5,90,90,a],reason='invalid_cell_angle')
    for name,p in [('singular',[5,5,5,60,60,120]),('impossible',[5,5,5,30,30,100]),
                   ('sum360',[5,5,5,120,120,120]),('near_singular',[5,5,5,90,90,1e-6])]:
        edge(name,p,reason='degenerate_cell')
    for name,p in [('overflow_cell',[1e150]*3+[90]*3),('underflow_cell',[1e-150]*3+[90]*3)]:
        edge(name,p,reason='cell_range')
    edge('zero_hkl',h=[0,0,0],reason='zero_hkl')
    for name,w in [('zero_wavelength',0),('negative_wavelength',-1),('nan_wavelength','NaN'),('inf_wavelength','Infinity')]:
        edge(name,w=w,reason='invalid_wavelength')
    edge('asin_overflow',w=10,reason='bragg_domain')
    return output
