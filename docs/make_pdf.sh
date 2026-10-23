#!/bin/bash
# Generate QualPF PDF manual from Markdown sources.
# Requires: pandoc, xelatex (texlive-xetex), python3
#
# Usage: bash make_pdf.sh [output.pdf]

set -eu
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-${SCRIPT_DIR}/qualpf_manual.pdf}"
python3 "${SCRIPT_DIR}/../tools/documentation/build_manual.py" --output "${OUT}" \
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
  "${SCRIPT_DIR}/user/references.md"
echo "PDF generated with source provenance: ${OUT}"
