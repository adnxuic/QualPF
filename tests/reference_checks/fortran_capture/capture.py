"""Read the frozen classic numerical transcript; no native capture execution."""
import hashlib
import math
from pathlib import Path

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()








def wire(cases):
    rows = [str(len(cases))]
    for data in cases:
        rows += [" ".join(map(str, data["range"])), str(len(data["pattern"]))]
        rows += [" ".join(format(v, ".17g") for v in row) for row in data["pattern"]]
        rows.append(str(len(data["peaks"])))
        rows += [f'{p["two_theta"]:.17g} {p["intensity"]:.17g}' for p in data["peaks"]]
        refs = data["cards"][0]["reflections"]
        rows.append(str(len(refs)))
        rows += [f'{r["two_theta"]:.17g} {r["intensity"]:.17g}' for r in refs]
        p = data["parameters"]
        rows.append(" ".join(format(p[k], ".17g") for k in ("weight_position", "weight_intensity", "weight_phases", "delta")))
    return "\n".join(rows) + "\n"


def records(output):
    result = []
    for line in output.splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "ENV":
            if parts[1:] != ["4", "32", "64"]:
                raise ValueError("Unsupported REAL kinds: " + line)
        elif parts[0] == "BEGIN":
            result.append({})
        elif parts[0] != "END":
            result[-1].setdefault(parts[0], []).extend(parts[1:])
    return result


def observation(record):
    # Parse printed Legacy variables; no reimplementation of FOM arithmetic.
    fields = dict(zip(("total", "position", "intensity", "scale"), ([float(x)] for x in record["SCORE"])))
    for label, key in (("MERGED", "merged_intensities"), ("SCALED", "scaled_intensities"), ("DIFF", "position_difference_sum")):
        fields[key] = list(map(float, record[label]))
    for key, value in zip(("associated_sample_sum", "associated_card_sum", "total_sample_sum", "total_card_sum",
                           "database_fraction", "sample_fraction", "sqrt_database", "sqrt_sample"), record["TERMS"]):
        fields[key] = [float(value)]
    if any(not math.isfinite(x) for row in fields.values() for x in row):
        raise ValueError("Nonfinite reference: classify separately, never bless as a score")
    return {"error": None, "fields": fields, "discrete": {
        "disposition": ["0"], "counts": record["COUNTS"], "reflection_indices": record["REF"],
        "experimental_indices": [str(int(i)-1) if int(i) else "none" for i in record["ASS"]]}}
