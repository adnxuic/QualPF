"""Offline, fail-closed field comparison. No expected-output generation."""
import argparse
import hashlib
import json
import math
import pathlib
import subprocess

POLICY_SHA256 = "822e6baaa74a345181c437ff8f251170a69e58e1dd2ae315ab3665bb2ccd77e6"
EXCEPTIONS = {
    "flat-peaks": "Flat-signal derivative normalization produces NaN candidates",
    "zero-background-0": "Zero automatic-fit objective divides by zero; selected coefficients undefined",
    "zero-background-1": "Zero automatic-fit objective divides by zero; selected coefficients undefined",
    "zero-background-2": "Zero automatic-fit objective divides by zero; selected coefficients undefined",
    "zero-peaks": "Zero signal and derivative normalization divide by zero",
    "short-sg": "Legacy SG reads outside the short input (crash or timeout)",
    "duplicate-grid": "Degenerate Gaussian fit invokes nrerror/STOP, possibly with exit code zero",
    "negative-fit": "Gaussian log fit receives negative intensities",
}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def decode(text,op):
    if op==3: return {"text":text.replace("\r\n","\n")}
    tokens=text.split()
    if tokens and tokens[0]=="ERROR":
        return {"error":" ".join(tokens[1:])}
    data=list(map(float,tokens)); at=0
    def take(n):
        nonlocal at
        if at+n>len(data): raise ValueError("Truncated output")
        result=data[at:at+n]; at+=n; return result
    def count():
        v=take(1)[0]
        if v<0 or int(v)!=v: raise ValueError("Invalid count")
        return int(v)
    fields={}; discrete={}
    if op==0:
        n=count(); discrete["samples"]=n; fields["signal"]=take(n)
    elif op==1:
        n,points,coefs=count(),count(),count()
        discrete.update(samples=n,points=points,coefficients=coefs)
        fields["origin"]=take(1); fields["background"]=take(n)
        fields["points"]=take(2*points); fields["coefficients"]=take(coefs)
    else:
        n,candidates,window,degree=count(),count(),count(),count()
        discrete.update(peaks=n,candidates=candidates,window=window,degree=degree)
        fields["search_range"]=take(1); fields["threshold"]=take(1); fields["automatic_tolerance"]=take(1)
        for prefix,size,columns in (("peaks",n,6),("candidates",candidates,5)):
            rows=[take(columns) for _ in range(size)]
            for col,name in enumerate(("two_theta","d","intensity","fwhm","area","delta_d")[:columns]):
                fields[prefix+"."+name]=[row[col] for row in rows]
    if at!=len(data): raise ValueError("Unexpected trailing output")
    if any(not math.isfinite(v) for values in fields.values() for v in values): raise ValueError("Nonfinite field")
    return {"fields":fields,"discrete":discrete}

def compare(actual,expected,tolerances):
    if "text" in expected:
        return {"status":"passed" if actual==expected else "failed","reason":"exact export text comparison"}
    if "error" in actual: return {"status":"failed","reason":actual["error"]}
    if actual["discrete"]!=expected["discrete"]:
        return {"status":"failed","reason":"discrete mismatch","actual":actual["discrete"],"expected":expected["discrete"]}
    stats={}; passed=True
    for field,ref in expected["fields"].items():
        values=actual["fields"][field]
        if len(values)!=len(ref): raise ValueError("Field length mismatch")
        absolute,relative=tolerances[field.split(".")[-1]]
        differences=[abs(a-b) for a,b in zip(values,ref)]
        bad=[i for i,(a,b) in enumerate(zip(values,ref)) if not math.isfinite(a) or abs(a-b)>absolute+relative*abs(b)]
        passed &= not bad
        stats[field]={"count":len(ref),"maximum_absolute_error":max(differences,default=0),"failures":len(bad),
                      "first_failure":bad[0] if bad else None,"atol":absolute,"rtol":relative}
    return {"status":"passed" if passed else "failed","fields":stats}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--runner",required=True,type=pathlib.Path)
    parser.add_argument("--manifest",required=True,type=pathlib.Path)
    parser.add_argument("--report",required=True,type=pathlib.Path)
    args=parser.parse_args()
    manifest=json.loads(args.manifest.read_text(encoding="utf-8"))
    if manifest.get("kind")!="fortran_capture" or not manifest.get("cases"): raise ValueError("Missing Fortran evidence")
    policy=pathlib.Path(__file__).with_name("preprocessing_tolerances.json")
    if sha(policy)!=POLICY_SHA256: raise ValueError("Frozen tolerance policy changed")
    tolerances=json.loads(policy.read_text())
    for filename,key in (("capture-script.py","capture_sha256"),("capture-wrapper.f90","wrapper_sha256")):
        if sha(args.manifest.parent/filename)!=manifest["provenance"][key]:
            raise ValueError("Capture provenance changed: "+filename)
    ids=[case["id"] for case in manifest["cases"]]
    if len(ids)!=len(set(ids)): raise ValueError("Duplicate case identifier")
    results=[]; exceptions=[]
    for case in manifest["cases"]:
        for key in ("input","wire","raw","stderr"):
            path=args.manifest.parent/case[key]
            if sha(path)!=case[key+"_sha256"]: raise ValueError("Changed evidence "+case["id"]+" "+key)
        if case["status"]=="exception":
            if case["id"] not in EXCEPTIONS: raise ValueError("Unregistered Legacy exception: "+case["id"])
            exceptions.append({"id":case["id"],"returncode":case["returncode"],"reason":EXCEPTIONS[case["id"]]}); continue
        if case["status"]!="captured" or case["returncode"]!=0:
            raise ValueError("Incomplete capture: "+case["id"])
        raw=(args.manifest.parent/case["raw"]).read_text()
        input=json.loads((args.manifest.parent/case["input"]).read_text())
        output=subprocess.run([str(args.runner.resolve())],input=(args.manifest.parent/case["wire"]).read_text(),text=True,capture_output=True,timeout=60)
        try:
            if output.returncode: raise ValueError("Replay failed: "+output.stderr)
            result=compare(decode(output.stdout,input["operation"]),decode(raw,input["operation"]),tolerances)
        except ValueError as error:
            result={"status":"failed","reason":str(error)}
        result["id"]=case["id"]; results.append(result)
    report={"runner_sha256":sha(args.runner),"manifest_sha256":sha(args.manifest),"tolerances_sha256":sha(policy),
            "passed":sum(r["status"]=="passed" for r in results),"failed":sum(r["status"]=="failed" for r in results),
            "exceptions_excluded":exceptions,"cases":results}
    args.report.write_text(json.dumps(report,indent=2),encoding="utf-8")
    print(json.dumps({k:v for k,v in report.items() if k!="cases"}))
    for r in results:
        if r["status"]=="failed": print(r["id"],r["reason"] if "reason" in r else ",".join(f for f,v in r["fields"].items() if v["failures"]))
    return 1 if report["failed"] else 0

if __name__=="__main__":
    raise SystemExit(main())
