from reference_paths import reference_path
"""Replay reviewed frozen native observations; never regenerate expected output."""
import argparse, hashlib, json, math, subprocess, zipfile
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('--runner', required=True)
p.add_argument('--report', required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
m = json.loads((reference_path(root/'tests/data/kalpha2-reference-manifest.json')).read_text())
archive = reference_path(root/'tests/data'/m['archive'])
assert hashlib.sha256(archive.read_bytes()).hexdigest() == m['archive_sha256']
results = []
with zipfile.ZipFile(archive) as z:
    for c in m['cases']:
        for file, sha in c['artifacts'].items():
            assert hashlib.sha256(z.read(file)).hexdigest() == sha
        run = subprocess.run([a.runner], input=z.read(c['id']+'.wire').decode(), text=True, capture_output=True, timeout=30)
        lines = run.stdout.splitlines()
        item = dict(id=c['id'], passed=False, returncode=run.returncode)
        if c['product_rejection']:
            item['passed'] = run.returncode == 0 and lines == ['ERROR '+c['product_rejection']]
        elif run.returncode == 0 and lines and lines[0] == 'OK':
            raw = z.read(c['id']+'.stdout.txt').decode().replace('\r\n','\n').split('AFTER12\n')[1].splitlines()
            expected = [float(row.split()[1]) for row in raw[1:int(raw[0].split()[0])+1]]
            got = list(map(float, lines[1:]))
            errors = [abs(x-y) for x,y in zip(got, expected)]
            item['max_absolute_error'] = max(errors, default=0)
            item['passed'] = len(got) == len(expected) and all(math.isfinite(x) and abs(x-y) <= m['tolerance']['absolute']+m['tolerance']['relative']*abs(y) for x,y in zip(got, expected))
        results.append(item)
report = dict(reference_sha256=m['archive_sha256'], cases=results, passed=all(r['passed'] for r in results))
Path(a.report).write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
raise SystemExit(0 if report['passed'] else 1)
