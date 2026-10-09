#!/bin/bash
# Script to build Debian package for QualPF
# The package will install to /usr (standard Debian location)

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$REPO_DIR/build/linux-deb"
cd "$REPO_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# An old compiler-selector argument is accepted and ignored.
# Deprecated compiler selectors are accepted as inert compatibility arguments.
# The product now selects its C++ compiler through CXX/CMake.
if [ "$#" -gt 0 ]; then
    echo "Deprecated compiler selector ignored; building C++ only."
fi

# Number of threads for parallel compilation
NUM_THREADS=$(nproc)

echo -e "${BLUE}=========================================="
echo "QualPF - Debian Package Build"
echo -e "==========================================${NC}"
echo ""
echo "C++ compiler       : ${CXX:-c++}"
echo "Compilation threads: ${NUM_THREADS}"
echo "Package will install to: /usr"
echo ""

# Check if compiler is available


# Check required tools for packaging
if ! command -v dpkg-deb &> /dev/null; then
    echo -e "${RED}ERROR: dpkg-deb not found (required for building .deb packages)${NC}"
    exit 1
fi

# Reuse the dedicated build tree; never remove another configuration.
mkdir -p "$BUILD_DIR/packages"

# Step 1: Configuration for Debian package
echo -e "${YELLOW}Step 1/4: Configuration with CMake (Debian package mode)...${NC}"
cmake -B "$BUILD_DIR" -S "$REPO_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_DEB_PACKAGE=ON

echo -e "${GREEN}✓ Configuration completed${NC}"
echo ""

# Step 2: Compilation
echo -e "${YELLOW}Step 2/4: Compilation (using ${NUM_THREADS} threads)...${NC}"
cmake --build "$BUILD_DIR" -j${NUM_THREADS}

echo -e "${GREEN}✓ Compilation completed${NC}"
echo ""

# Step 3: Create Debian package
echo -e "${YELLOW}Step 3/4: Creating Debian package...${NC}"
cpack --config "$BUILD_DIR/CPackConfig.cmake" -G DEB -B "$BUILD_DIR/packages"
cd "$BUILD_DIR/packages"

echo -e "${GREEN}✓ Package created${NC}"
echo ""

# Step 4: Show package info
echo -e "${YELLOW}Step 4/4: Package information...${NC}"
VERSION="$(grep -m1 'project\s*(' "$REPO_DIR/CMakeLists.txt" | sed 's/.*VERSION[[:space:]]\+\([0-9][0-9.]*\).*/\1/')"
PROG_NAME=$(grep -m1 'EXECUTABLE_NAME' "$REPO_DIR/CMakeLists.txt" | sed 's/.*set(EXECUTABLE_NAME[[:space:]]*"\([^"]*\)").*/\1/')
DEB_FILE=$(ls -1 "${PROG_NAME}-${VERSION}"*.deb | head -1)
RELEASE_DIR="$REPO_DIR/releases/${VERSION:-unknown}/linux-deb"

if [ -f "$DEB_FILE" ]; then
    mkdir -p "$RELEASE_DIR"
    cp "$DEB_FILE" "$RELEASE_DIR/"
    echo -e "${GREEN}=========================================="
    echo "✓ Debian package created successfully!"
    echo -e "==========================================${NC}"
    echo ""
    echo "Package file: $RELEASE_DIR/$DEB_FILE"
    echo ""
    echo "Package information:"
    dpkg-deb -I "$DEB_FILE" | grep -E "Package:|Version:|Architecture:|Maintainer:|Depends:"
    echo ""
    echo "Package contents:"
    dpkg-deb -c "$DEB_FILE" | head -20
    echo ""
    echo "To install the package:"
    echo "  sudo dpkg -i $DEB_FILE"
    echo "  sudo apt-get install -f  # Install missing dependencies if needed"
    echo ""
    PKG_NAME=$(dpkg-deb --field "$DEB_FILE" Package)
    echo "To uninstall:"
    echo "  sudo dpkg -r $PKG_NAME"
    echo ""
else
    echo -e "${RED}ERROR: Package file not found${NC}"
    exit 1
fi
