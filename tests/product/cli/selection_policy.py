"""Real CLI characterization: cap boundaries and stable FOM ties, analytic input."""
import argparse
from contextlib import closing
import json
import math
import os
from pathlib import Path
import re
import sqlite3
import struct
import subprocess
import tempfile


def exercise(executable, output):
    report = {"status": "FAIL", "cases": []}
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        with tempfile.TemporaryDirectory(prefix="cli-selection-", dir=output.parent) as tmp:
            root = Path(tmp)
            pattern = root / "three-gaussians.xy"
            pattern.write_text("".join(f"{10+i*.02:.17g} {5+sum(h*math.exp(-.5*((10+i*.02-x)/.12)**2) for x,h in [(15,100),(23,200),(32,150)]):.17g}\n" for i in range(1501)), encoding="utf-8")
            database = root / "cards"
            wave = struct.unpack("f", struct.pack("f", 1.54059307))[0]
            with closing(sqlite3.connect(str(database) + ".sq")) as db:
                db.execute("CREATE TABLE infodb(ncard INTEGER,type TEXT)")
                db.execute("INSERT INTO infodb VALUES(3,'cod_inorg')")
                db.execute("CREATE TABLE id(id TEXT,name TEXT,mineralname TEXT,chemical_formula TEXT,spacegroup TEXT,quality TEXT,rir TEXT,nrec INTEGER,dvalue TEXT,intensita TEXT,n INTEGER)")
                # Two identical good cards give an exact tie; earlier shifted card scores lower.
                for identity, shift in [("20", .1), ("10", 0), ("30", 0)]:
                    d = ",".join(f"{wave/(2*math.sin(math.radians(x+shift)/2)):.17g}" for x in [15,23,32])
                    db.execute("INSERT INTO id VALUES(?,?,?,?,?,?,?,?,?,?,?)", (identity,"card"+identity,"m","F","P1","A","1",3,d,"100,200,150",3))
                db.commit()
            for suffix in [".sq.info", ".sq.infostat", ".sq.search"]:
                with closing(sqlite3.connect(str(database) + suffix)) as db:
                    db.execute("CREATE TABLE unused(id TEXT)"); db.commit()
            for cap, expected in [(4,["20","10","30"]),(3,["20","10","30"]),(2,["10","30"]),(1,["10"])]:
                settings = root / f"settings-{cap}"; (settings / "IC").mkdir(parents=True)
                (settings / "IC/qualpf.ini").write_text(
                    f"[databases]\ncount=1\n0\\inUse=true\n0\\name=cod_inorg\n0\\path={database.as_posix()}\n"
                    f"[maintenance]\nsettingsMigrationV1=true\n[SearchOptions]\nmaxEntries={cap}\ncheckStrongest=false\ncheckDeleted=false\nminFom=0\ndelta2thetaAuto=false\ndelta2theta=0.25\n", encoding="utf-8")
                env = os.environ.copy(); env.update(QT_QPA_PLATFORM="offscreen", MKL_NUM_THREADS="1")
                result = subprocess.run([str(executable),"--nogui","--settings-dir",str(settings),"--search",str(pattern)], capture_output=True, env=env, timeout=30)
                stdout = result.stdout.decode("utf-8", errors="replace")
                stderr = result.stderr.decode("utf-8", errors="replace")
                ids = re.findall(r"^\s+\[(\d+)\]", stdout, re.MULTILINE)
                report["cases"].append(dict(cap=cap, expected=expected, actual=ids, exit_code=result.returncode, stdout=stdout, stderr=stderr))
                if result.returncode != 0 or ids != expected or "Search failed:" in stderr:
                    raise AssertionError(report["cases"][-1])
            report["status"] = "PASS"
    finally:
        output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("--executable", type=Path, required=True); parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(); exercise(args.executable.resolve(), args.output.resolve())
