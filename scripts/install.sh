#!/bin/bash
# Automatic installation script for QualPF
# Usage: ./install.sh [qt_prefix]
# Optional Qt6 installation path; old compiler-token + Qt-prefix calls remain accepted.

set -e  # Exit on error

QUALPF_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QUALPF_REPO_DIR="$(cd "$QUALPF_SCRIPT_DIR/.." && pwd)"
QUALPF_BUILD_DIR="$QUALPF_REPO_DIR/build/linux-gnu"
cd "$QUALPF_REPO_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Arguments
# Deprecated compiler selectors are accepted as inert compatibility arguments.
# The product now selects its C++ compiler through CXX/CMake.
case "${1##*/}" in
    gfortran|ifort|ifx|flang|flang-new|g95|mpifort|mpiifort|mpiifx)
        echo "Deprecated compiler selector ignored; building C++ only."
        shift ;;
esac
QT_PREFIX=${1:-}

# Number of threads for parallel compilation
NUM_THREADS=$(nproc)

# Read program info from CMakeLists.txt
PROG_NAME=$(grep -m1 'EXECUTABLE_NAME' CMakeLists.txt | sed 's/.*set(EXECUTABLE_NAME[[:space:]]*"\([^"]*\)").*/\1/')
PROG_VERSION=$(grep -m1 'project\s*(' CMakeLists.txt | sed 's/.*VERSION[[:space:]]\+\([0-9][0-9.]*\).*/\1/')
INSTALL_PREFIX="${QUALPF_INSTALL_PREFIX:-/opt/${PROG_NAME}-${PROG_VERSION}}"
case "$INSTALL_PREFIX" in
    /*) ;;
    *) echo "ERROR: QUALPF_INSTALL_PREFIX must be an absolute path" >&2; exit 1 ;;
esac

# Detect Linux distribution family
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        for val in "${ID:-}" ${ID_LIKE:-}; do
            case "$val" in
                debian|ubuntu|linuxmint|pop|elementary|kali|raspbian|mint)
                    echo "debian"; return ;;
                fedora|rhel|centos|rocky|almalinux|ol|redhat)
                    echo "redhat"; return ;;
            esac
        done
        echo "unknown"
    else
        echo "unknown"
    fi
}

DISTRO_FAMILY=$(detect_distro)

# Distribution-specific package info
case "$DISTRO_FAMILY" in
    debian)
        PKG_MANAGER="apt-get"
        PKG_UPDATE="sudo apt-get update"
        PKG_INSTALL="sudo apt-get install cmake build-essential pkg-config qt6-base-dev libqt6sql6 libqt6printsupport6 libeigen3-dev libboost-dev"
        ;;
    redhat)
        if command -v dnf &> /dev/null; then
            PKG_MANAGER="dnf"
        else
            PKG_MANAGER="yum"
        fi
        PKG_UPDATE="sudo ${PKG_MANAGER} check-update || true"
        PKG_INSTALL="sudo ${PKG_MANAGER} install cmake gcc gcc-c++ make pkgconfig qt6-qtbase-devel eigen3-devel boost-devel"
        ;;
    *)
        PKG_MANAGER="unknown"
        PKG_UPDATE=""
        PKG_INSTALL=""
        ;;
esac

echo -e "${BLUE}=========================================="
echo "QualPF Installation"
echo -e "==========================================${NC}"
echo ""
echo "C++ compiler     : ${CXX:-c++}"
echo "Compile threads  : ${NUM_THREADS}"
if [ -n "${QT_PREFIX}" ]; then
    echo "Qt prefix        : ${QT_PREFIX}"
fi
echo ""

# Print install hint based on distro
print_install_hint() {
    if [ -n "$PKG_INSTALL" ]; then
        echo "Install dependencies with:"
        [ -n "$PKG_UPDATE" ] && echo "  ${PKG_UPDATE}"
        echo "  ${PKG_INSTALL}"
    else
        echo "Please install manually: CMake 3.20+, make, a C++20 compiler, pkg-config, Qt 6.7+ (Widgets, Sql, PrintSupport, Network), Eigen 3.4+, Boost headers"
    fi
    echo "Qt must be 6.7 or newer; supply a matching Qt SDK prefix if the distro package is older."
}

# Check mandatory build tools
echo -e "${YELLOW}Checking dependencies...${NC}"
MISSING_DEPS=()

command -v cmake      &> /dev/null || MISSING_DEPS+=("cmake")
command -v make       &> /dev/null || MISSING_DEPS+=("make")
command -v pkg-config &> /dev/null || MISSING_DEPS+=("pkg-config")

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${RED}ERROR: Missing dependencies: ${MISSING_DEPS[*]}${NC}"
    echo ""
    print_install_hint
    exit 1
fi

echo -e "${GREEN}✓ Build tools are present; CMake will check Qt 6.7+, Eigen 3.4 and Boost${NC}"
echo ""

# ── Step 1: Configuration ──────────────────────────────────────────────────────
echo -e "${YELLOW}Step 1/3: Configuration with CMake...${NC}"

CMAKE_ARGS=(-B "$QUALPF_BUILD_DIR" -S "$QUALPF_REPO_DIR" -DCMAKE_BUILD_TYPE=Release
            "-DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}")
if [ -n "${QT_PREFIX}" ]; then
    CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=${QT_PREFIX}")
fi

cmake "${CMAKE_ARGS[@]}"

echo ""
echo "Installation prefix: ${INSTALL_PREFIX}"
echo ""
echo -e "${GREEN}✓ Configuration completed${NC}"
echo ""

# ── Step 2: Compilation ────────────────────────────────────────────────────────
echo -e "${YELLOW}Step 2/3: Compilation (using ${NUM_THREADS} threads)...${NC}"
cmake --build "$QUALPF_BUILD_DIR" -j${NUM_THREADS}

echo -e "${GREEN}✓ Compilation completed${NC}"
echo ""

# ── Step 3: Installation ───────────────────────────────────────────────────────
echo -e "${YELLOW}Step 3/3: Installing the program...${NC}"
echo "Password required for 'sudo cmake --install $QUALPF_BUILD_DIR --prefix $INSTALL_PREFIX'"
sudo cmake --install "$QUALPF_BUILD_DIR" --prefix "$INSTALL_PREFIX"

echo -e "${GREEN}✓ Installation completed${NC}"
echo ""

# Verify installation
if [ -f "${INSTALL_PREFIX}/bin/${PROG_NAME}" ]; then
    echo -e "${GREEN}=========================================="
    echo "✓ Installation completed successfully!"
    echo -e "==========================================${NC}"
    echo ""
    echo "Program installed in : ${INSTALL_PREFIX}"
    echo "Executable           : ${INSTALL_PREFIX}/bin/${PROG_NAME}"
    echo ""
    echo "To run the program:"
    echo "  ${INSTALL_PREFIX}/bin/${PROG_NAME}"
    echo ""
    echo "Or add to PATH:"
    echo "  export PATH=${INSTALL_PREFIX}/bin:\$PATH"
    echo ""
else
    echo -e "${RED}ERROR: File ${INSTALL_PREFIX}/bin/${PROG_NAME} was not created${NC}"
    exit 1
fi
