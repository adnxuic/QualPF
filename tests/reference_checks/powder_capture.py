from reference_paths import reference_path, provenance_path
"""Freeze original Fortran observations, independently of Phase 17 C++."""
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def cases():
    with zipfile.ZipFile(reference_path(ROOT/'tests/data/scattering-reference.zip')) as z:
        old = json.loads(z.read('cases.json'))
    result = []
    for c in old:
        if c['mode']=='STRUCT' and c['name'].startswith('cod-'):
            item = copy.deepcopy(c)
            item.update(sync=0, tolerance=.00005, low=1 if c['radiation']!=2 else .05,
                        high=85 if c['radiation']!=2 else 1, category='normal')
            result.append(item)
    systems = [('cubic',221,[5,5,5,90,90,90]),
               ('tetragonal',123,[5,5,7,90,90,90]),
               ('orthorhombic',47,[5,6,7,90,90,90]),
               ('hexagonal',191,[5,5,7,90,90,120]),
               ('monoclinic',10,[5,6,7,90,103,90]),
               ('triclinic',1,[5,6,7,76,83,67])]
    for name,group,cell in systems:
        for rad,sync in [(0,0),(1,0),(0,1),(2,0)]:
            result.append(dict(mode='STRUCT',name=name+f'-r{rad}-s{sync}',group=group,setting=1,
                cell=cell,sites=[dict(species='Si',xyz=[.137,.271,.389],occupancy=1,adp=0,tensor=[0]*6)],
                radiation=rad,anomalous=0,anisotropic=1,wavelength=.8,low=2,high=45,
                sync=sync,tolerance=.00005,category='normal',
                provenance=dict(kind='synthetic six crystal systems; original native reflection generation removes extinctions')))
    def raw(name,angles,fc2=None,mult=None,rad=0,sync=0,tolerance=.00005,category='normal'):
        result.append(dict(mode='RAW',name=name,radiation=rad,sync=sync,wavelength=1.23456789,
            tolerance=tolerance,category=category,
            reflections=[dict(hkl=[i+1,0,0],multiplicity=(mult or [2]*len(angles))[i],
                         two_theta=a,fc2=(fc2 or [100]*len(angles))[i]) for i,a in enumerate(angles)],
            provenance=dict(kind='synthetic scalar/overlap boundary; no upstream structure claimed')))
    for rad in (0,1,2):
        for sync in (0,1):
            raw(f'angles-r{rad}-s{sync}',[.01,.1,1,20,60,90,120,160,175],
                [0,1e-12,1e-5,1,100,1e6,5e9,4,1e-7],[1,2,48,6,12,24,96,2,1],rad,sync)
    raw('transitive-abc',[20,20.00004,20.00008,20.01],[1,4,9,16])
    raw('exact-overlap',[30,30,30,31],[1,4,9,16],[1,2,48,2])
    raw('near-boundary',[20,20.000049,21,21.000051,22,22.00005])
    raw('isolated-zero',[10,20,30],[0,100,0])
    raw('equal-intensity-same-angle',[30,30,31],[4,4,16],[2,2,1],tolerance=0)
    raw('exact-binary-threshold',[.125,.1875,.25,.375],[1,4,9,16],tolerance=.0625)
    raw('all-zero',[10,20],[0,0],category='all-zero-observation')
    raw('sub-epsilon-maximum',[60,70],[1e-20,4e-20],category='normalization-floor-observation')
    raw('singular-zero',[0],[1],category='invalid-observation')
    raw('singular-180',[180],[1],category='invalid-observation')
    raw('invalid-negative-fc2',[30],[-1],category='invalid-observation')
    raw('invalid-multiplicity',[30],[1],[0],category='invalid-observation')
    raw('invalid-radiation',[30],[1],rad=99,category='invalid-observation')
    raw('invalid-angle-nan',[float('nan')],[1],category='invalid-observation')
    raw('invalid-overlap',[20,20.00001],[1,4],tolerance=-1,category='invalid-observation')
    raw('empty',[],category='empty-observation')
    for i,c in enumerate(result): c['id']=i
    return result


def protocol(records):
    out=[str(reference_path(ROOT/'share/qualx/files/AtomProperties.xen')),str(reference_path(ROOT/'share/qualx/files/syminfo.lib'))]
    def row(x): out.append(' '.join(str(v) for v in x))
    for c in records:
        row([c['mode'],c['id'],c['radiation'],c['sync'],c['wavelength'],c['tolerance']])
        if c['mode']=='STRUCT':
            row([c['group'],c['setting'],len(c['sites']),c['anomalous'],c['anisotropic'],c['low'],c['high']])
            row(c['cell'])
            for s in c['sites']: row([s['species'],*s['xyz'],s['occupancy'],s['adp'],*s['tensor']])
        else:
            row([len(c['reflections'])])
            for r in c['reflections']: row([*r['hkl'],r['multiplicity'],r['two_theta'],r['fc2']])
    return '\n'.join(out)+'\n'


def parse(stdout):
    out={}
    for line in stdout.splitlines():
        p=line.split()
        if not p: continue
        if p[0]=='CASE':
            c=dict(radiation=int(p[2]),sync=int(p[3]),count=int(p[4]),wavelength=float(p[5]),
                   tolerance=float(p[6]),reflections=[],groups=[],lines=[],rank=[])
            out[p[1]]=c
        elif p[0]=='REF':
            c['reflections'].append(dict(id=int(p[1]),hkl=list(map(int,p[2:5])),multiplicity=int(p[5]),
                **dict(zip(('d','two_theta','fc2','lorentz','polarization','lp','raw','fc'),map(float,p[6:])))))
        elif p[0]=='MEMBERS':
            c['groups'].append(dict(id=int(p[1]),representative=int(p[2]),count=int(p[3]),members=list(map(int,p[4:]))))
        elif p[0]=='LINE':
            c['lines'].append(dict(id=int(p[1]),representative=int(p[2]),
                **dict(zip(('d','two_theta','raw','relative'),map(float,p[3:])))))
        elif p[0]=='RANK': c['rank']=list(map(int,p[1:]))
    return out
