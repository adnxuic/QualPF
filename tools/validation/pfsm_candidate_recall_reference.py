#!/usr/bin/env python3
"""Independent anchored NNLS/physical-model checks and 80-digit IL derivatives."""
import argparse
import decimal
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--dependency-dir',type=Path)
    args=parser.parse_args()
    if args.output.exists():raise RuntimeError('Refusing to overwrite evidence')
    if args.dependency_dir:sys.path.insert(0,str(args.dependency_dir))
    import numpy as np
    import scipy
    from scipy.optimize import nnls
    if scipy.__version__!='1.16.2':raise RuntimeError('Requires pinned SciPy 1.16.2')
    report=dict(schema='qualpf.pfsm.fast-v2-reference.v1',scipy=scipy.__version__,seed=781423,
        absolute_tolerance=2e-10,probe_sha256=hashlib.sha256(args.probe.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),cases=[])
    rng=np.random.default_rng(781423)
    for m in (0,1,2):
        for case in range(8):
            h=4.;lo,hi=(-1.,1.) if case!=7 else (0.,0.)
            x=np.linspace(0,180,97)+.03*np.sin(np.arange(97))
            w=rng.uniform(.1,3,len(x));w[::9]=0
            lines=[[(30+j*2,1.),(31+j,.4),(179.9,.05)] for j in range(m+1)]
            if case==4 and m==2:lines[1]=list(lines[0])
            if case==6:lines[-1]=[(30,.25),(30,.75),(31,.4),(179.9,.05)]
            old_scales=[float(j+1) if case!=3 else 0. for j in range(m)]
            alpha=4*(2**(2/3)-1)
            def direct(ll,shift=0):
                return sum(i*(1+alpha*((x-q-shift)/h)**2)**-1.5 for q,i in ll)
            def derivative(ll,shift):
                return sum(i*3*alpha*(x-q-shift)/h**2*(1+alpha*((x-q-shift)/h)**2)**-2.5 for q,i in ll)
            old=sum((old_scales[j]*direct(lines[j]) for j in range(m)),np.zeros(len(x)))
            y=old+direct(lines[-1],.23)+rng.normal(0,.05,len(x))
            if case==5:y=-np.abs(y)
            residual=y-old;den=float(w@(y*y));sw=np.sqrt(w/den);z=sw*y;r0=sw*residual;q0=float(r0@r0);m0=sw*old
            def columns(shifts):return np.column_stack([sw*direct(ll,s) for ll,s in zip(lines,shifts)])
            def kkt(matrix,scales):
                norms=np.linalg.norm(matrix,axis=0);valid=norms>0
                gradient=(matrix[:,valid]/norms[valid]).T@(matrix@scales-z)
                v=np.where(scales[valid]>0,np.abs(gradient),np.maximum(0,-gradient))
                return float(max(v)) if len(v) else 0.
            def gain(model):
                change=model-m0;return float(2*r0@change-change@change)
            rows=[f'{len(x)} {m} {h} {lo} {hi}']
            rows+=[' '.join(format(float(v),'.17g') for v in row) for row in zip(x,y,np.zeros(len(x)),w,residual)]
            for j,ll in enumerate(lines):
                rows.append(f'{old_scales[j] if j<m else 0} 0 {len(ll)}')
                rows += [f'{q:.17g} {intensity:.17g}' for q,intensity in ll]
            for mode in ('anchored','anchored-cache'):
                run=subprocess.run([str(args.probe),mode],input='\n'.join(rows)+'\n',text=True,capture_output=True)
                errors=[];checks=[];actual=None
                try:
                    actual=json.loads(run.stdout)
                    checks += [run.returncode==0,not actual['protected'],1<=len(actual['seeds'])<=3,
                               len(actual['starts'])==len(actual['seeds']),actual['calls']==5*len(actual['seeds']),actual['calls']<=15]
                    errors.append(abs(actual['baseline_q']-q0));gains=[0.]
                    for start_index,start in enumerate(actual['starts']):
                        checks.append(len(start['steps'])==2)
                        initial=start['initial_fit'];initial_columns=columns(initial['shifts'])
                        initial_scales=np.asarray(initial['scales']);initial_model=initial_columns@initial_scales
                        initial_q=float((z-initial_model)@(z-initial_model))
                        errors += [abs(initial['q']-nnls(initial_columns,z)[1]**2),abs(initial['q']-initial_q),
                                   abs(initial['gain']-gain(initial_model)),kkt(initial_columns,initial_scales)]
                        checks.append(initial['shifts']==[0.]*m+[actual['seeds'][start_index]])
                        previous=None
                        for step_index,step in enumerate(start['steps']):
                            anchor=np.asarray(step['anchor_shifts']);scales=np.asarray(step['anchor_scales'])
                            a=columns(anchor);model=a@scales;current_q=float((z-model)@(z-model));gains.append(gain(model))
                            errors += [kkt(a,scales),abs(current_q-nnls(a,z)[1]**2)]
                            if step_index==0:
                                errors.append(float(np.max(np.abs(anchor-np.array([0.]*m+[actual['seeds'][start_index]])))))
                                checks.append(step['radius']==.125)
                            else:
                                expected_anchor=previous['physical']['shifts'] if previous['accepted'] else previous['anchor_shifts']
                                expected_scales=previous['physical']['scales'] if previous['accepted'] else previous['anchor_scales']
                                checks += [step['anchor_shifts']==expected_anchor,step['anchor_scales']==expected_scales,
                                           step['radius']==(.125 if previous['accepted'] else .0625)]
                            radius=step['radius'];bounds=[];local=[]
                            for j,ll in enumerate(lines):
                                lower=max(-radius,(lo-anchor[j])/h);upper=min(radius,(hi-anchor[j])/h)
                                bounds.append((lower,upper));d=sw*h*derivative(ll,anchor[j])
                                local.append(a[:,j]+lower*d)
                                if lower!=upper:local.append(a[:,j]+upper*d)
                            linear=np.column_stack(local);coef=np.asarray(step['coefficients'])
                            linear_model=linear@coef
                            errors += [abs(float((z-linear_model)@(z-linear_model))-nnls(linear,z)[1]**2),kkt(linear,coef)]
                            checks.append(bool(np.all(coef>=0)))
                            physical_shifts=[];offset=0
                            for j,(lower,upper) in enumerate(bounds):
                                if lower==upper:t=lower;offset+=1
                                else:
                                    u,v=coef[offset:offset+2];offset+=2;t=(lower*u+upper*v)/(u+v) if u+v>0 else 0.
                                physical_shifts.append(float(np.clip(anchor[j]+h*t,lo,hi)))
                            f=step['physical'];aa=columns(physical_shifts);ss=np.asarray(f['scales']);mm=aa@ss
                            qq=float((z-mm)@(z-mm));gg=gain(mm);gains.append(gg)
                            change=mm-model;actual_gain=float(2*(z-model)@change-change@change)
                            linear_change=linear_model-model;predicted=float(2*(z-model)@linear_change-linear_change@linear_change)
                            errors += [float(np.max(np.abs(np.asarray(f['shifts'])-physical_shifts))),abs(f['q']-qq),
                                abs(f['q']-nnls(aa,z)[1]**2),abs(f['gain']-gg),abs(f['difference_gain']-(q0-qq)),
                                kkt(aa,ss),abs(step['actual']-actual_gain),abs(step['predicted']-predicted),
                                abs(step['distortion']-abs(predicted-actual_gain))]
                            checks.append(bool(np.all(ss>=0)))
                            tolerance=128*np.finfo(float).eps*max(1,q0,current_q,qq)
                            # The recorded arithmetic controls acceptance; independent quantities above must agree.
                            expected_accept=step['actual']>tolerance and step['predicted']>0 and step['actual']/step['predicted']>=.1
                            checks.append(step['accepted']==expected_accept)
                            previous=step
                    errors += [abs(actual['best']['gain']-max(gains)),abs(actual['best']['q']-(q0-max(gains)))]
                    error=max(errors);passed=all(checks) and error<=2e-10
                except (ValueError,KeyError,TypeError,IndexError) as exc:
                    error=None;passed=False;checks.append(str(exc))
                report['cases'].append(dict(id=f'{mode}-m{m}-{case}',passed=bool(passed),maximum_error=error,
                    checks=[c if isinstance(c,str) else bool(c) for c in checks],actual=actual,stderr=run.stderr,stdout=run.stdout if actual is None else None))
    # Independent high precision analytic IL derivatives, including cache edges and narrow peaks.
    decimal.getcontext().prec=80
    D=decimal.Decimal
    alpha=D(4)*(D(4)**(D(1)/D(3))-1)
    for h in (.03,.4,4.):
        for shift in (-h/4,h/4):
            x=np.linspace(30-5*h,30+5*h,37);ll=[(30.,1.),(30+h,.4),(30+12*h,.03)]
            rows=[f'{len(x)} 0 {h:.17g} {-h/4:.17g} {h/4:.17g}']
            rows += [f'{xx:.17g} 1 0 1 1' for xx in x]
            rows += [f'{shift:.17g} {len(ll)}']+[f'{q:.17g} {i:.17g}' for q,i in ll]
            run=subprocess.run([str(args.probe),'derivative'],input='\n'.join(rows)+'\n',text=True,capture_output=True)
            result=json.loads(run.stdout) if run.returncode==0 else None
            errors=[]
            if result:
                for k,xx in enumerate(x):
                    value=D(0);gradient=D(0);hh=D(format(h,'.17g'));delta=D(format(shift,'.17g'))
                    for q,intensity in ll:
                        distance=D(format(xx,'.17g'))-D(format(q,'.17g'))-delta
                        a=alpha/(hh*hh);v=1+a*distance*distance;root=v.sqrt();strength=D(format(intensity,'.17g'))
                        value+=strength/(v*root);gradient+=strength*3*a*distance/(v*v*root)
                    errors += [abs(result['intensity'][k]-float(value)),h*abs(result['derivative'][k]-float(gradient))]
            maximum=max(errors) if errors else None
            report['cases'].append(dict(id=f'derivative-h{h}-s{shift}',passed=maximum is not None and maximum<=2e-10,
                maximum_error=maximum,stderr=run.stderr))
    report['passed']=all(c['passed'] for c in report['cases'])
    args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f"PFSM candidate recall (v2) independent reference: {sum(c['passed'] for c in report['cases'])}/{len(report['cases'])}")
    return 0 if report['passed'] else 1


if __name__=='__main__':raise SystemExit(main())
