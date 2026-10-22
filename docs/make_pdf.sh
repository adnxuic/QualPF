#!/bin/bash
# Generate QualPF PDF manual from Markdown sources.
# Requires: pandoc, xelatex (texlive-xetex), python3
#
# Usage: bash make_pdf.sh [output.pdf]

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-${SCRIPT_DIR}/qualpf_manual.pdf}"
PUBLISHER=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["product"]["publisher"])' \
  "${SCRIPT_DIR}/../deploy/distribution.json")

pandoc \
  "${SCRIPT_DIR}/user/index.md" \
  "${SCRIPT_DIR}/user/installation.md" \
  "${SCRIPT_DIR}/user/database-installation.md" \
  "${SCRIPT_DIR}/user/introduction/index.md" \
  "${SCRIPT_DIR}/user/introduction/getting-started.md" \
  "${SCRIPT_DIR}/user/settings.md" \
  "${SCRIPT_DIR}/user/introduction/examples.md" \
  "${SCRIPT_DIR}/user/data-reduction/index.md" \
  "${SCRIPT_DIR}/user/data-reduction/background.md" \
  "${SCRIPT_DIR}/user/data-reduction/initial-peak-search.md" \
  "${SCRIPT_DIR}/user/data-reduction/peak-search.md" \
  "${SCRIPT_DIR}/user/data-reduction/smoothing.md" \
  "${SCRIPT_DIR}/user/data-reduction/kalpha2-stripping.md" \
  "${SCRIPT_DIR}/user/search-match/index.md" \
  "${SCRIPT_DIR}/user/search-match/search-match.md" \
  "${SCRIPT_DIR}/user/search-match/restraints.md" \
  "${SCRIPT_DIR}/user/references.md" \
  -o "${OUT}" \
  --pdf-engine=xelatex \
  --toc \
  --toc-depth=2 \
  -V geometry:margin=2.5cm \
  -V fontsize=11pt \
  -V lang=en \
  -V mainfont="DejaVu Serif" \
  -V sansfont="DejaVu Sans" \
  -V monofont="DejaVu Sans Mono" \
  --metadata title="QualPF Manual" \
  --metadata author="${PUBLISHER}"

python3 -B "${SCRIPT_DIR}/../tools/validation/check_distribution.py" \
  --root "${SCRIPT_DIR}/.." --write-manual-proof --manual-file "${OUT}" \
  --manual-proof "${SCRIPT_DIR}/../build/documentation/qualpf_manual-provenance.json"
echo "PDF generated with source provenance: ${OUT}"
