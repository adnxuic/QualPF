#!/bin/bash
# Build a source archive using the relocated native Debian package script.
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <file.tar.gz>"
    exit 1
fi
TAR_GZ="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
mkdir -p "$REPO_DIR/build"
WORK_DIR="$(mktemp -d "$REPO_DIR/build/package-source.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
tar -xzf "$TAR_GZ" -C "$WORK_DIR"
SRC_DIR="$(find "$WORK_DIR" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
if [ -z "$SRC_DIR" ]; then
    echo "Error: could not locate extracted source directory inside $TAR_GZ"
    exit 1
fi
bash "$SRC_DIR/scripts/package/build_deb.sh"
VERSION="$(grep -m1 'project\s*(' "$SRC_DIR/CMakeLists.txt" | sed 's/.*VERSION[[:space:]]\+\([0-9][0-9.]*\).*/\1/')"
RELEASE_DIR="$REPO_DIR/releases/${VERSION:-unknown}/linux-deb"
mkdir -p "$RELEASE_DIR"
# build_deb.sh collects the package, its checksum and the revision/toolchain
# record here. Copy the complete release set before the temporary source exits.
SOURCE_RELEASE_DIR="$SRC_DIR/releases/${VERSION:-unknown}/linux-deb"
test -f "$SOURCE_RELEASE_DIR/artifacts.json"
cp -a "$SOURCE_RELEASE_DIR/." "$RELEASE_DIR/"
shopt -s nullglob
for metadata in "$SRC_DIR/build/linux-deb/packages/"*.sha256 \
                "$SRC_DIR/build/linux-deb/packages/"*-artifacts.json \
                "$SRC_DIR/build/linux-deb/packages/distribution-inventory.json"; do
    if [ -f "$metadata" ]; then cp -a "$metadata" "$RELEASE_DIR/"; fi
done
echo "Packages and release metadata written to $RELEASE_DIR"
