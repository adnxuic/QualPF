"""Explicit S9 reference capture; never run by normal CXX-only tests.

Creates a NEW directory, retains wire inputs, raw stdout/stderr and hashes.
The runner calls original Fortran, not any C++ preprocessing implementation.
"""
import argparse
import hashlib
import json
import math
import pathlib
import random
import struct
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
BASELINE = "a00c7a4455e5a21c6d457cb807df264ee1b0a562"

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def f32(v):
    return struct.unpack("f", struct.pack("f", v))[0]

def measured(name):
    path = ROOT / "share/qualx/examples" / (name + ".dat")
    lines = [[float(v) for v in line.split()] for line in path.read_text().splitlines() if line.strip()]
    if len(lines[0]) == 2:
        return lines
    start, step, _ = map(f32, lines[0])
    values = [v for line in lines[1:] for v in line]
    return [[f32(start + f32(i * step)), v] for i, v in enumerate(values)]

def inputs():
    rng = random.Random(901)
    x = [10 + i * .02 for i in range(601)]
    def curve(background, peaks, noise=0):
        return [[t, background(t) + sum(a * math.exp(-.5 * ((t-c)/s)**2) for c,a,s in peaks)
                 + noise * rng.random()] for t in x]
    signals = {
        "flat": curve(lambda t: 5, []),
        "zero": curve(lambda t: 0, []),
        "weak": curve(lambda t: 100, [(14, 2, .10), (18, 3, .08)]),
        "overlap": curve(lambda t: 4, [(14, 100, .15), (14.25, 60, .16)]),
        "widths": curve(lambda t: 3, [(13, 100, .035), (18, 200, .7)]),
        "strong_background": curve(lambda t: 1000+8*t+t*t, [(14, 100, .1), (18, 200, .2)]),
        "noisy": curve(lambda t: 4, [(14, 40, .1), (18, 25, .2)], 20),
        "edges": curve(lambda t: 4, [(10.04, 100, .03), (21.96, 120, .04)]),
    }
    signals["irregular"] = [[t + .001*math.sin(i), y] for i,(t,y) in enumerate(signals["widths"])]
    for name in ("example1", "CPD2", "MIXT_5"):
        signals[name] = measured(name)
    for name, data in signals.items():
        for method, w, degree, derivative in ((1,4,5,0),(0,4,5,0),(0,7,9,3),(0,4,5,1),(0,4,5,2)):
            yield f"{name}-smooth-{method}-{derivative}", 0, data, [method,w,degree,derivative]
        for method in range(7):
            yield f"{name}-background-{method}", 1, data, [method,1,6,5,50,data[0][0],data[-1][0]]
        yield f"{name}-peaks", 2, data, [1.5,.1,-1,11,9,-1,-1,1]
    # Abnormal cases are captured separately, never normalized into successful outputs.
    yield "short-average", 0, [[10,1],[11,2],[12,1]], [1,4,5,0]
    yield "short-sg", 0, [[10,1],[11,2],[12,1]], [0,4,5,0]
    yield "duplicate-grid", 2, [[10,1]]*30, [1.5,.1,.15,11,9,-1,-1,1]
    yield "negative-fit", 2, [[t,-y] for t,y in signals["widths"]], [1.5,.1,.15,11,9,-1,-1,1]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--runner",required=True,type=pathlib.Path)
    parser.add_argument("--output",required=True,type=pathlib.Path)
    parser.add_argument("--suite",choices=("algorithms","exports"),default="algorithms")
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    sources={}
    for folder in ("legacy/fortran", "legacy/bridge/fortran"):
        for path in (ROOT/folder).rglob("*"):
            if path.suffix.lower() in (".f90", ".f"):
                relative=path.relative_to(ROOT).as_posix()
                original=subprocess.check_output(["git","show",f"{BASELINE}:{relative}"],cwd=ROOT)
                if original.replace(b"\r\n",b"\n") != path.read_bytes().replace(b"\r\n",b"\n"):
                    raise RuntimeError("Oracle source changed: "+relative)
                sources[relative]=sha(path)
    catalog=[]
    cases=inputs() if args.suite=="algorithms" else [(f"export-{i}",3,[[.5,1.234],[13.12345678,100],[89.99999,25.5]],[i]) for i in range(3)]
    for name,op,data,params in cases:
        case={"id":name,"operation":op,"pattern":data,"parameters":params,"wave":1.54056,"delta":.15}
        input_path=args.output/(name+".json")
        input_path.write_text(json.dumps(case,separators=(",",":")),encoding="utf-8")
        wire=f"{op} {len(data)} 1.54056 .15\n"+"".join(f"{x:.17g} {y:.17g}\n" for x,y in data)+" ".join(map(str,params))+"\n"
        wire_path=args.output/(name+".wire")
        wire_path.write_text(wire,encoding="ascii")
        try:
            result=subprocess.run([str(args.runner.resolve())],input=wire,text=True,capture_output=True,timeout=20)
            stdout,stderr,code=result.stdout,result.stderr,result.returncode
        except subprocess.TimeoutExpired as error:
            stdout=""; stderr="TIMEOUT: "+str(error); code=124
        raw=args.output/(name+".out")
        raw.write_text(stdout,encoding="utf-8")
        err=args.output/(name+".err")
        err.write_text(stderr,encoding="utf-8")
        abnormal=name in ("zero-peaks","short-sg","duplicate-grid","negative-fit","flat-peaks",
                          "zero-background-0","zero-background-1","zero-background-2")
        try:
            finite=bool(stdout.strip()) and all(math.isfinite(float(t)) for t in stdout.split()) if code==0 else False
        except ValueError:
            finite=False # Fortran STOP/nrerror can return success with diagnostic text.
        if op==3: finite=bool(stdout.strip()) and code==0
        status="exception" if abnormal or code or not finite else "captured"
        catalog.append({"id":name,"input":input_path.name,"input_sha256":sha(input_path),
                        "wire":wire_path.name,"wire_sha256":sha(wire_path),"raw":raw.name,"raw_sha256":sha(raw),
                        "stderr":err.name,"stderr_sha256":sha(err),"returncode":code,"status":status})
        print(name,status,code,flush=True)
    compiler=ROOT/"build-phase7/qt/Tools/mingw1310_64/bin/gfortran.exe"
    provenance={"source_commit":BASELINE,"sources":sources,"runner_sha256":sha(args.runner),
                "compiler":subprocess.check_output([str(compiler),"--version"],text=True),
                "flags":"product GNU Fortran Release -O0; default REAL binary32; serial",
                "capture_sha256":sha(pathlib.Path(__file__)),
                "wrapper_sha256":sha(ROOT/"tests/compatibility/preprocessing_capture.f90"),
                "contract_sha256":sha(ROOT/"docs/refactoring/phase-9/contracts.md"),
                "examples":{p.name:sha(p) for p in (ROOT/"share/qualx/examples").glob("*.dat")}}
    (args.output/"manifest.json").write_text(json.dumps({"schema_version":1,"kind":"fortran_capture", "provenance":provenance,"cases":catalog},indent=2),encoding="utf-8")

if __name__ == "__main__":
    main()
