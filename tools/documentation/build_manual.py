#!/usr/bin/env python3
"""Build the user manual with local PDF links and matching source provenance."""
import argparse
import json
import os
from pathlib import Path
import posixpath
import re
import subprocess
import sys
import tempfile
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
USER = ROOT / "docs/user"


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def navigation_sources():
    return [USER / name for name in re.findall(
        r"^\s+- [^:\n]+:\s+([\w./-]+\.md)\s*$",
        (ROOT / "docs/mkdocs.yml").read_text(encoding="utf-8"), re.MULTILINE)]


def build(output, sources):
    if [p.resolve() for p in sources] != [p.resolve() for p in navigation_sources()]:
        raise ValueError("PDF sources must match the complete manual navigation in order")
    documents = {}
    anchors = {}
    for source in sources:
        source = source.resolve()
        name = source.relative_to(USER).as_posix()
        document = json.loads(run("pandoc", str(source), "-f", "markdown+gfm_auto_identifiers", "-t", "json",
                                  capture_output=True).stdout)
        documents[name] = document
        prefix = name.removesuffix(".md").replace("/", "-") + "--"
        for block in document["blocks"]:
            if block["t"] == "Header":
                original = block["c"][1][0]
                target = prefix + original
                anchors[(name, original)] = target
                anchors.setdefault((name, ""), target)
                block["c"][1][0] = target

    def rewrite(value, name):
        if isinstance(value, dict):
            # Keep a workflow illustration with its explanation instead of
            # floating it into the middle of a later paragraph/page.
            if value.get("t") == "Figure":
                value["t"] = "Div"
                value["c"] = [value["c"][0], value["c"][2]]
            if value.get("t") in {"Link", "Image"}:
                target = value["c"][-1][0]
                parts = urlsplit(target)
                if not parts.scheme and not parts.netloc:
                    path = posixpath.normpath(posixpath.join(posixpath.dirname(name), unquote(parts.path))) if parts.path else name
                    if value["t"] == "Link":
                        key = (path, unquote(parts.fragment))
                        if key not in anchors:
                            raise ValueError(f"Unresolved PDF link in {name}: {target}")
                        value["c"][-1][0] = "#" + anchors[key]
                    else:
                        image = (USER / path).resolve()
                        if not image.is_relative_to(USER) or not image.is_file():
                            raise ValueError(f"Missing/unsafe manual image: {target}")
                        value["c"][-1][0] = image.relative_to(ROOT).as_posix()
            for child in value.values():
                rewrite(child, name)
        elif isinstance(value, list):
            for child in value:
                rewrite(child, name)

    combined = dict(next(iter(documents.values())))
    combined["meta"] = dict(combined["meta"])
    combined["meta"]["header-includes"] = {"t": "MetaBlocks", "c": [
        {"t": "RawBlock", "c": ["latex", "\\usepackage{needspace}"]}]}
    combined["blocks"] = [{"t": "RawBlock", "c": ["latex", "\\clearpage"]}]
    for name, document in documents.items():
        rewrite(document["blocks"], name)
        if name == "references.md":
            combined["blocks"].append({"t": "RawBlock", "c": ["latex", "\\clearpage"]})
        blocks = document["blocks"]
        for index, block in enumerate(blocks):
            if block["t"] == "Header" and any(b["t"] == "Table" for b in blocks[index + 1:index + 3]):
                combined["blocks"].append({"t": "RawBlock", "c": ["latex", "\\Needspace{18\\baselineskip}"]})
            combined["blocks"].append(block)
    publisher = json.loads((ROOT / "deploy/distribution.json").read_text(encoding="utf-8"))["product"]["publisher"]
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    scratch = ROOT / "build/documentation"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="manual-", dir=scratch) as temporary:
        ast = Path(temporary) / "manual.json"
        ast.write_text(json.dumps(combined, ensure_ascii=False), encoding="utf-8")
        run("pandoc", str(ast), "-f", "json", "-o", str(output), "--pdf-engine=xelatex",
            "--toc", "--toc-depth=2", "-V", "geometry:margin=2.5cm", "-V", "fontsize=11pt",
            "-V", "lang=en", "-V", "mainfont=DejaVu Serif", "-V", "sansfont=DejaVu Sans",
            "-V", "monofont=DejaVu Sans Mono", "--metadata", "title=QualPF User Manual",
            "--metadata", "author=" + publisher, cwd=ROOT,
            env={**os.environ, "TMP": temporary, "TEMP": temporary, "TMPDIR": temporary})
    run(sys.executable, "-B", str(ROOT / "tools/validation/check_distribution.py"),
        "--root", str(ROOT), "--write-manual-proof", "--manual-file", str(output),
        "--manual-proof", str(ROOT / "build/documentation/qualpf_manual-provenance.json"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "docs/qualpf_manual.pdf")
    parser.add_argument("sources", type=Path, nargs="*")
    args = parser.parse_args()
    sources = args.sources or navigation_sources()
    build(args.output, sources)


if __name__ == "__main__":
    main()
