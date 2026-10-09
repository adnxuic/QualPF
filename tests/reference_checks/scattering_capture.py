from reference_paths import reference_path, provenance_path
"""Build/freeze exact native inputs. Never uses the new C++ implementation."""
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SOURCES = reference_path(ROOT / 'tests/data/structure-factor-reference/sources')


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def site(species, xyz, occupancy=1, adp=0, tensor=None):
    return dict(species=species, xyz=xyz, occupancy=occupancy, adp=adp,
                tensor=tensor or [0]*6)


def structures():
    # Explicit reviewed transcription: no CIF reader is part of replay or core.
    # Absent ADP is represented as zero for this physical-core fixture, without
    # invoking the out-of-scope exportqualx default-B/card-generation policy.
    records = [
        ('9013103', 227, 1, [5.437]*3+[90]*3, [site('Si', [0,0,0])]),
        ('1538052', 216, 1, [5.436]*3+[90]*3,
         [site('S',[.25]*3), site('Zn',[0]*3,.667), site('Ge',[0]*3,.333)]),
        ('4343150', 225, 1, [5.42513]*3+[90]*3,
         [site('Ce',[0]*3,.9,1),site('Sm',[0]*3,.1,1),site('O',[.25]*3,.991,1)]),
        ('1000041', 225, 1, [5.62]*3+[90]*3,
         [site('Na1+',[0]*3),site('Cl1-',[.5]*3)]),
        ('9000775', 154, 1, [4.916,4.916,5.4054,90,90,120],
         [site('Si',[.46970,0,0],1,2,[.00854,.00716,.00725,.00358,-.00001,-.00002]),
          site('O',[.41350,.26690,.11910],1,2,[.01745,.01322,.01229,.00973,-.00291,-.00408])]),
        ('1536389', 1, 1, [4.916,4.917,5.407,90,90,120],
         [site('Si4+',[.4702,0,.3333],1,1),site('Si4+',[0,.47,.6667],1,1),
          site('O2-',[.8533,.5861,.8813],1,1),site('O2-',[.2674,.4137,.7859],1,1),
          site('O2-',[.7338,.1465,.549],1,1),site('O2-',[.414,.2679,.2159],1,1),
          site('O2-',[.1477,.7326,.4532],1,1),site('Si4+',[.5306,.5305,-.0001],1,1),
          site('O2-',[.5862,.8539,.118],1,1)])]
    out=[]
    for cod, group, setting, cell, sites in records:
        path=SOURCES/(cod+'.cif')
        provenance=dict(url=f'https://www.crystallography.net/cod/{cod}.cif',
                        sha256=sha(path), source_file=str(path.relative_to(ROOT)),
                        transcription='Cell and atom loops, ESD removed; charge signs normalized to resource labels; absent ADP=0; no CIF/card default B policy',
                        prior_workflow=cod in ('9013103','1538052','4343150','1536389'))
        refs=[[1,0,0],[1,1,1],[2,0,0],[2,2,0],[2,2,2],[4,2,2],[4,4,0],[0,0,1],[0,0,3]]
        for rad,anom,anis,wave in [(0,0,1,1.54059307),(0,1,0,.71073),(0,1,1,.71073),(1,0,1,1.54059307),(2,0,1,.025)]:
            out.append(dict(name=f'cod-{cod}-r{rad}-an{anom}-ad{anis}',
                            group=group,setting=setting,cell=cell,sites=sites,
                            reflections=refs, radiation=rad,anomalous=anom,
                            anisotropic=anis,wavelength=wave,provenance=provenance))
    for group,xyz in [(1,[.137,.271,.389]),(2,[.137,.271,.389]),(2,[0,0,0]),
                      (6,[.137,0,.389]),(3,[0,.271,0]),(4,[.137,.271,.389]),
                      (19,[.137,.271,.389]),(14,[0,0,0])]:
        cell=[5,6,7,90,90,90] if group!=1 else [5,6,7,76,83,67]
        for occ in (0,.375,1):
            for adp,tensor in [(0,[0]*6),(0,[.8,0,0,0,0,0]),(1,[.01,0,0,0,0,0])]:
                out.append(dict(name=f'single-g{group}-{xyz}-occ{occ}-adp{adp}-{tensor[0]}',
                    group=group,setting=1,cell=cell,sites=[site('Fe',xyz,occ,adp,tensor)],
                    reflections=[[1,0,0],[1,2,3],[-1,-2,-3],[0,1,0],[3,3,3]],
                    radiation=0,anomalous=0,anisotropic=1,wavelength=.71073,
                    provenance=dict(kind='synthetic isolated physical boundary')))
    for tensor in ([.01,.02,.03,.003,-.002,.004],[10,20,30,0,0,0]):
        out.append(dict(name=f'single-triclinic-anis-{tensor[0]}',group=1,setting=1,
            cell=[5,6,7,76,83,67],sites=[site('Cu',[.137,.271,.389],.75,2,tensor)],
            reflections=[[1,0,0],[1,2,3],[-1,-2,-3],[3,3,3]],radiation=0,
            anomalous=0,anisotropic=1,wavelength=.71073,
            provenance=dict(kind='synthetic nonorthogonal anisotropic boundary')))
    return out


def make_cases():
    resource=(reference_path(ROOT/'share/qualx/files/AtomProperties.xen')).read_text().splitlines()[2:]
    rows=[line for line in resource if line.strip() and not line.startswith('#')]
    assert len(rows)%9==0
    species=[rows[i].split()[0].capitalize() for i in range(0,len(rows),9)]
    cases=[]
    for label in species:
        for rad in range(3):
            for s2 in (0,1e-12,.0025,.04,.25,1,4,25):
                cases.append(dict(mode='SCAT',species=label,radiation=rad,s2=s2,category='normal'))
    # Real d values from the already frozen Phase 15 native ReflectionSet.
    with zipfile.ZipFile(reference_path(ROOT/'tests/data/reflection-reference.zip')) as archive:
        lines=archive.read('stdout.txt').decode().splitlines()
    real_d=[]
    for line in lines:
        p=line.split()
        if p and p[0]=='REF':
            d=float(p[-2])
            if d>0 and len(real_d)<12 and d not in real_d: real_d.append(d)
    for label in ('Si','O','Fe','Cu','Pb'):
        for d in real_d:
            cases.append(dict(mode='SCAT',species=label,radiation=0,s2=1/(4*d*d),
                              category='normal',source='Phase 15 frozen REF d',d=d))
    for label in ('H','O2-','Fe','U','Rf','Q','NotAnAtom'):
        for s2 in (-1,1e8,float('nan'),float('inf')):
            cases.append(dict(mode='SCAT',species=label,radiation=0,s2=s2,category='invalid-observation'))
    for z in range(1,112):
        for wave in (1.54059307,1.54442756,.71073,1.78897,.8,2.12345):
            cases.append(dict(mode='ANOM',z=z,wavelength=wave,category='normal' if z<=92 else 'unsupported-observation'))
    nist=(reference_path(ROOT/'legacy/fortran/crystallography/nist_table.f90')).read_text()
    conversion=1e7*6.62607015e-34*2.998e8/1.602176634e-19
    for z in (1,6,14,26,29,82,92):
        block=re.search(r'case\('+str(z)+r'\)(.*?)(?=\n\s*case|end select)',nist,re.S).group(1)
        array=block.split('nist_tab(:) = [')[1].split(']')[0]
        nums=[float(x) for x in re.findall(r'[-+]?\d*\.?\d+(?:[Ee][-+]?\d+)?',
                                         re.sub(r'!.*','',array))]
        energy=nums[::4]
        for e in (energy[0],energy[-1],energy[len(energy)//2]):
            for offset in (-2e-6,0,2e-6):
                cases.append(dict(mode='ANOM',z=z,wavelength=conversion/(e*(1+offset)),
                                  energy=e,category='boundary-observation'))
    for z in (0,1,29,92,93,112):
        for wave in (0,-1,1e-9,1e9,float('nan'),float('inf')):
            cases.append(dict(mode='ANOM',z=z,wavelength=wave,category='invalid-observation'))
    for cell in ([5,5,5,90,90,90],[5,6,7,76,83,67],[4.916,4.916,5.4054,90,90,120]):
        for tensor in ([0]*6,[.01,.02,.03,.003,-.002,.004],[10,20,30,0,0,0],
                       [-.01,.02,.03,0,0,0],[.01,.01,.01,.1,0,0],[float('nan'),0,0,0,0,0]):
            cases.append(dict(mode='ADP',cell=cell,tensor=tensor,
                              category='normal' if tensor[0]>=0 and abs(tensor[3])<.02 else 'invalid-observation'))
    cases += [dict(mode='STRUCT',**s) for s in structures()]
    for i,c in enumerate(cases): c['id']=i
    return cases


def number(v):
    if isinstance(v,float) and math.isnan(v): return 'NaN'
    if isinstance(v,float) and math.isinf(v): return 'Infinity' if v>0 else '-Infinity'
    return str(v)


def protocol(cases):
    lines=[str(reference_path(ROOT/'share/qualx/files/AtomProperties.xen')),str(reference_path(ROOT/'share/qualx/files/syminfo.lib'))]
    def row(values): lines.append(' '.join(map(number,values)))
    for c in cases:
        row([c['mode'],c['id']])
        if c['mode']=='SCAT': row([c['species'],c['radiation'],c['s2']])
        elif c['mode']=='ANOM': row([c['z'],c['wavelength']])
        elif c['mode']=='ADP': row(c['cell']); row(c['tensor'])
        else:
            row([c['group'],c['setting'],len(c['sites']),len(c['reflections']),
                 c['radiation'],c['anomalous'],c['anisotropic'],c['wavelength']])
            row(c['cell'])
            for s in c['sites']: row([s['species'],*s['xyz'],s['occupancy'],s['adp'],*s['tensor']])
            for h in c['reflections']: row(h)
    return '\n'.join(lines)+'\n'


def parse(stdout):
    out=dict(tables=[],cases={})
    current=None
    for line in stdout.splitlines():
        p=line.split()
        if not p: continue
        if p[0]=='TABLE': out['tables'].append(p[1:])
        elif p[0] in ('SCAT','ANOM','ADP','UNKNOWN'):
            out['cases'][p[1]]=dict(mode=p[0],values=p[2:])
        elif p[0]=='STRUCT':
            current=dict(mode='STRUCT',group_id=int(p[2]),operations=int(p[3]),sites=[],reflections=[])
            out['cases'][p[1]]=current
        elif p[0]=='SITE': current['sites'].append([float(x) for x in p[2:]])
        elif p[0]=='FC':
            current['reflections'].append(dict(index=int(p[1]),hkl=[int(x) for x in p[2:5]],
                phase=int(p[5]),s2=float(p[6]),fc=float(p[7]),fc2=float(p[8]),two_theta=float(p[9])))
        elif p[0]=='COMPLEX': current['reflections'][-1]['complex']=[float(x) for x in p[1:]]
    return out
