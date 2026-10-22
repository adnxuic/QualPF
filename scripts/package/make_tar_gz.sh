#!/bin/bash
# Build the current local QualPF source package; no remote product is cloned.
# Usage: ./make_tar_gz.sh [CMake configure options ...]
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$REPO_DIR/build/source-package"
VERSION=$(grep -m1 'project[[:space:]]*(' "$REPO_DIR/CMakeLists.txt" | sed 's/.*VERSION[[:space:]]\+\([0-9][0-9.]*\).*/\1/')
PROG_NAME=$(grep -m1 'EXECUTABLE_NAME' "$REPO_DIR/CMakeLists.txt" | sed 's/.*set(EXECUTABLE_NAME[[:space:]]*"\([^"]*\)").*/\1/')
if command -v python3 >/dev/null 2>&1 && python3 -c 'import mkdocs' >/dev/null 2>&1; then
    python3 -m mkdocs build --strict -f "$REPO_DIR/docs/mkdocs.yml"
else
    echo "MkDocs unavailable; source delivery contains the tracked manual sources."
fi
if command -v pandoc >/dev/null 2>&1 && command -v xelatex >/dev/null 2>&1; then
    bash "$REPO_DIR/docs/make_pdf.sh"
else
    echo "Pandoc/XeLaTeX unavailable; source delivery contains the tracked manual sources."
fi
cmake -S "$REPO_DIR" -B "$BUILD_DIR" "$@"
cpack --config "$BUILD_DIR/CPackSourceConfig.cmake" -G TGZ -B "$BUILD_DIR/packages"
SOURCE_NAME=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["source_artifact"])' \
    "$BUILD_DIR/configured_files/distribution-build.json")
ARCHIVE="$BUILD_DIR/packages/$SOURCE_NAME"
test -f "$ARCHIVE"
RELEASE_DIR="$REPO_DIR/releases/$VERSION/source"
mkdir -p "$RELEASE_DIR"
cp "$ARCHIVE" "$RELEASE_DIR/"
python3 -B "$REPO_DIR/tools/validation/check_distribution.py" --root "$REPO_DIR" \
    --build "$BUILD_DIR" --record-artifacts "$RELEASE_DIR/artifacts.json" \
    --artifact "$RELEASE_DIR/$(basename "$ARCHIVE")"
echo "Source package: $RELEASE_DIR/$(basename "$ARCHIVE")"
