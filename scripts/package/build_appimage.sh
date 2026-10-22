#!/bin/bash
#
# Builds a qualpf AppImage from the current source tree.
#
# Usage: ./build_appimage.sh
# An old compiler-selector argument is accepted and ignored.
#
# Produces build/linux-appimage output and releases/<version>/linux-appimage/.
#
# Requires: cmake, a Qt6 dev install, patchelf, curl (to fetch linuxdeploy
# and appimagetool on first run — cached under build/linux-appimage/tools/).
# For a native GTK/GNOME look (instead of Qt's generic Fusion style), also
# install qt6-gtk-platformtheme before building: its plugin is bundled only
# if present at build time.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Deprecated compiler selectors are accepted as inert compatibility arguments.
# The product now selects its C++ compiler through CXX/CMake.
if [ "$#" -gt 0 ] && [ "${1:-}" != --audit-only ]; then
    echo "Deprecated compiler selector ignored; building C++ only."
fi
NUM_THREADS=$(nproc)
AUDIT_ONLY=${QUALPF_DISTRIBUTION_AUDIT_ONLY:-OFF}
if [ "${1:-}" = --audit-only ]; then AUDIT_ONLY=ON; fi
DIST_ARGS=(--release)
SUFFIX=""
if [ "$AUDIT_ONLY" = ON ]; then DIST_ARGS=(--audit-only); SUFFIX=-audit; fi

BUILD_DIR="$SRC_DIR/build/linux-appimage"
PROG_NAME=$(grep -m1 'EXECUTABLE_NAME' "$SRC_DIR/CMakeLists.txt" | sed 's/.*set(EXECUTABLE_NAME[[:space:]]*"\([^"]*\)").*/\1/')
APPDIR="$BUILD_DIR/${PROG_NAME}.AppDir"
TOOLS_DIR="$BUILD_DIR/tools"

echo -e "${BLUE}=========================================="
echo "QualPF - AppImage Build"
echo -e "==========================================${NC}"
echo ""
echo "C++ compiler       : ${CXX:-c++}"
echo "Compilation threads: ${NUM_THREADS}"
echo ""


if ! command -v curl &> /dev/null; then
    echo -e "${RED}ERROR: curl not found (required to fetch linuxdeploy/appimagetool)${NC}"
    exit 1
fi

# Reuse this configuration and cached packaging tools.
mkdir -p "$TOOLS_DIR"

# Step 1: configure + build + install into a portable AppDir/usr tree
# (same "bundle everything" layout as a manual /opt install, just
# skipping the per-user desktop-integration step — see BUILD_APPIMAGE
# in CMakeLists.txt).
echo -e "${YELLOW}Step 1/5: Configuration with CMake (AppImage mode)...${NC}"
cmake -B "$BUILD_DIR/cmake" -S "$SRC_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_APPIMAGE=ON \
    -DQUALPF_DISTRIBUTION_AUDIT_ONLY="$AUDIT_ONLY" \
    -DCMAKE_INSTALL_PREFIX="$APPDIR/usr"
python3 -B "$SRC_DIR/tools/validation/check_distribution.py" --root "$SRC_DIR" \
    --build "$BUILD_DIR/cmake" --kind AppImage "${DIST_ARGS[@]}"
echo -e "${GREEN}✓ Configuration completed${NC}"
echo ""

echo -e "${YELLOW}Step 2/5: Compilation (using ${NUM_THREADS} threads)...${NC}"
cmake --build "$BUILD_DIR/cmake" -j${NUM_THREADS}
cmake --install "$BUILD_DIR/cmake"
echo -e "${GREEN}✓ Compilation and install completed${NC}"
echo ""

# Step 2: AppDir metadata (desktop file with a relative Exec=, + icon)
echo -e "${YELLOW}Step 3/5: Assembling AppDir metadata...${NC}"
mkdir -p "$APPDIR/usr/share/applications"
# Windows checkouts can use CRLF; desktop Icon/Exec values must not retain CR.
sed 's/\r$//' "$BUILD_DIR/cmake/${PROG_NAME}.appimage.desktop" \
    > "$APPDIR/usr/share/applications/qualpf.desktop"
chmod 644 "$APPDIR/usr/share/applications/qualpf.desktop"
install -Dm644 "$SRC_DIR/deploy/linux/icons/256x256/qualpf.png" \
    "$APPDIR/usr/share/icons/hicolor/256x256/apps/qualpf.png"
echo -e "${GREEN}✓ Metadata assembled${NC}"
echo ""

# Step 3: fetch linuxdeploy + appimagetool (cached across runs)
echo -e "${YELLOW}Step 4/5: Fetching linuxdeploy and appimagetool...${NC}"
LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
APPIMAGETOOL="$TOOLS_DIR/appimagetool-x86_64.AppImage"

if [ ! -x "$LINUXDEPLOY" ]; then
    curl -fsSL -o "$LINUXDEPLOY" \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x "$LINUXDEPLOY"
fi
if [ ! -x "$APPIMAGETOOL" ]; then
    curl -fsSL -o "$APPIMAGETOOL" \
        https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x "$APPIMAGETOOL"
fi
echo -e "${GREEN}✓ Tools ready${NC}"
echo ""

# Containers (and many CI runners) have no /dev/fuse: run tools extracted.
export APPIMAGE_EXTRACT_AND_RUN=1

# Step 4: let linuxdeploy pull in non-Qt runtime deps not already bundled
# by the CMake install, write AppRun, and
# wire the desktop file + icon into the AppDir root.
#
# Qt plugins (platforms/xcb, platformthemes/gtk3, sqldrivers/sqlite, ...)
# are loaded via dlopen(), so linuxdeploy never sees them by walking the
# qualpf executable's own link graph. Pass each one explicitly via
# --library so linuxdeploy also discovers and bundles *their* transitive
# dependencies (e.g. libqgtk3.so pulls in GTK3/glib/atspi) instead of
# silently falling back to the host's — possibly incompatible — copies.
echo -e "${YELLOW}Step 5/5: Running linuxdeploy and appimagetool...${NC}"
linuxdeploy_args=(
    --appdir "$APPDIR"
    --executable "$APPDIR/usr/bin/$PROG_NAME"
    --desktop-file "$APPDIR/usr/share/applications/qualpf.desktop"
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/qualpf.png"
)
while IFS= read -r -d '' plugin_lib; do
    linuxdeploy_args+=(--library "$plugin_lib")
done < <(find "$APPDIR/usr/plugins" -name '*.so' -print0)

"$LINUXDEPLOY" "${linuxdeploy_args[@]}"
DIST_REVIEW_ARGS=()
if [ -n "${QUALPF_DISTRIBUTION_RUNTIME_REVIEW:-}" ]; then
    DIST_REVIEW_ARGS+=(--review "$QUALPF_DISTRIBUTION_RUNTIME_REVIEW")
fi
if [ -n "${QUALPF_CORRESPONDING_SOURCE_ARCHIVE:-}" ]; then
    DIST_REVIEW_ARGS+=(--source-artifact "$QUALPF_CORRESPONDING_SOURCE_ARCHIVE")
fi
python3 -B "$SRC_DIR/tools/validation/check_distribution.py" --root "$SRC_DIR" \
    --build "$BUILD_DIR/cmake" --stage "$APPDIR" --kind AppImage \
    "${DIST_ARGS[@]}" "${DIST_REVIEW_ARGS[@]}" \
    --output "$APPDIR/usr/share/qualpf/distribution-inventory.json"

VERSION="$(grep -m1 'project\s*(' "$SRC_DIR/CMakeLists.txt" | sed 's/.*VERSION[[:space:]]\+\([0-9][0-9.]*\).*/\1/')"
OUTPUT="$BUILD_DIR/${PROG_NAME}-${VERSION:-unknown}-x86_64${SUFFIX}.AppImage"
ARCH=x86_64 "$APPIMAGETOOL" "$APPDIR" "$OUTPUT"

RELEASE_DIR="$SRC_DIR/releases/${VERSION:-unknown}/linux-appimage"
mkdir -p "$RELEASE_DIR"
cp "$OUTPUT" "$RELEASE_DIR/"
python3 -B "$SRC_DIR/tools/validation/check_distribution.py" --root "$SRC_DIR" \
    --build "$BUILD_DIR/cmake" --record-artifacts "$RELEASE_DIR/artifacts.json" \
    --artifact "$RELEASE_DIR/$(basename "$OUTPUT")" --tool "$LINUXDEPLOY" --tool "$APPIMAGETOOL"
echo -e "${GREEN}=========================================="
echo "✓ AppImage created successfully!"
echo -e "==========================================${NC}"
echo ""
echo "File: $RELEASE_DIR/$(basename "$OUTPUT")"
