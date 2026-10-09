# Preserve the existing C++ build-profile contract while retiring the Native
# flag module. Its historical RelWithDebInfo alias remains a migration seam;
# changing that policy belongs to the post-F build-policy cleanup.
string(TOUPPER "${CMAKE_BUILD_TYPE}" QUALPF_BUILD_TYPE_UPPER)
if(NOT QUALPF_BUILD_TYPE_UPPER)
    set(CMAKE_BUILD_TYPE RELEASE CACHE STRING "Build profile" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not given, defaulting to RELEASE")
elseif(QUALPF_BUILD_TYPE_UPPER STREQUAL "RELWITHDEBINFO")
    set(CMAKE_BUILD_TYPE DEBUG CACHE STRING "Build profile" FORCE)
elseif(QUALPF_BUILD_TYPE_UPPER MATCHES "^(DEBUG|RELEASE|TESTING)$")
    set(CMAKE_BUILD_TYPE "${QUALPF_BUILD_TYPE_UPPER}" CACHE STRING "Build profile" FORCE)
else()
    message(FATAL_ERROR "CMAKE_BUILD_TYPE not valid, choices are DEBUG, RELEASE, or TESTING")
endif()

# Defines CMAKE_INSTALL_BINDIR/LIBDIR/etc. Needed explicitly (rather than
# relying on qt_standard_project_setup(), which only pulls this in for
# Qt >= 6.3) since e.g. Ubuntu 22.04 still ships Qt 6.2.
include(GNUInstallDirs)

# Option to build Debian package (installs to /usr)
option(BUILD_DEB_PACKAGE "Build for Debian package installation in /usr" OFF)

# Option to build a portable tree for AppImage packaging.
# Uses the same "bundle everything" install layout as a manual /opt install
# (see below), but skips the per-user desktop-integration install step,
# which makes no sense when installing into an AppDir.
option(BUILD_APPIMAGE "Build a portable tree for AppImage packaging" OFF)

# Set default installation prefix
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if(BUILD_DEB_PACKAGE)
        # For Debian package: install to /usr (standard location)
        set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Installation prefix for Debian package" FORCE)
        message(STATUS "Building Debian package, using prefix: ${CMAKE_INSTALL_PREFIX}")
    else()
        # For manual installation: install to /opt/qualpf-version
        set(CMAKE_INSTALL_PREFIX "/opt/${EXECUTABLE_NAME}-${PROJECT_VERSION}" CACHE PATH "Installation prefix" FORCE)
        message(STATUS "CMAKE_INSTALL_PREFIX not specified, defaulting to: ${CMAKE_INSTALL_PREFIX}")
    endif()
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set RPATH for Qt libraries on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
    # RPATH points to ../lib relative to the executable location
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()
