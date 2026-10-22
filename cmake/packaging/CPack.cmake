# CPack configuration
# Keep generated packages and CPack work under the selected build tree.
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages" CACHE PATH "CPack output directory")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(QUALPF_DISTRIBUTION_SUFFIX "")
if(QUALPF_DISTRIBUTION_AUDIT_ONLY)
    set(QUALPF_DISTRIBUTION_SUFFIX "-audit")
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-audit")
endif()
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${EXECUTABLE_NAME}-${PROJECT_VERSION}-Source${QUALPF_DISTRIBUTION_SUFFIX}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/configured_files/DistributionPackage.cmake")
set(CPACK_POST_BUILD_SCRIPTS "${CMAKE_BINARY_DIR}/configured_files/DistributionArtifacts.cmake")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_SOURCE_PACKAGE_CHECKSUM SHA256)
# Source packages exclude local caches and release products, retaining source inputs.
function(qualpf_cpack_escape_regex output input)
    foreach(character "." "+" "*" "?" "^" "$" "(" ")" "[" "]" "|")
        string(REPLACE "${character}" "\\${character}" input "${input}")
    endforeach()
    set(${output} "${input}" PARENT_SCOPE)
endfunction()
qualpf_cpack_escape_regex(QUALPF_CPACK_SOURCE_ROOT "${PROJECT_SOURCE_DIR}")
set(CPACK_SOURCE_IGNORE_FILES
    "[/][.]git($|[/]);[/]__pycache__[/];[.]py[co]$;${QUALPF_CPACK_SOURCE_ROOT}/build([_-][^/]*)?[/];${QUALPF_CPACK_SOURCE_ROOT}/releases[/];${QUALPF_CPACK_SOURCE_ROOT}/_CPack_Packages[/];${QUALPF_CPACK_SOURCE_ROOT}/[^/]+[.](deb|AppImage|exe)$;${QUALPF_CPACK_SOURCE_ROOT}/[.]evidence[/];${QUALPF_CPACK_SOURCE_ROOT}/lfs[/];[/]docs/[.]venv[/];[/]docs/qualx(_manual|pf_manual)[.]pdf$;CMakeUserPresets[.]json$")
list(APPEND CPACK_SOURCE_IGNORE_FILES
    "${QUALPF_CPACK_SOURCE_ROOT}/docs/site[/]"
    "${QUALPF_CPACK_SOURCE_ROOT}/docs/qualpf_manual[.]pdf$"
    "${QUALPF_CPACK_SOURCE_ROOT}/references[/]"
    "${QUALPF_CPACK_SOURCE_ROOT}/cmake-build-[^/]*[/]"
    "${QUALPF_CPACK_SOURCE_ROOT}/[.]venv[/]"
    "${QUALPF_CPACK_SOURCE_ROOT}/out[/]"
    "${QUALPF_CPACK_SOURCE_ROOT}/[.]cache[/]"
    "${QUALPF_CPACK_SOURCE_ROOT}/CMakeSettings[.]json$"
    "[/][.]pytest_cache[/]"
    "[/][.](mypy|ruff)_cache[/]"
    "[/]([.]DS_Store|Thumbs[.]db|desktop[.]ini)$")
# Local example collections stay private; retain only the independent CoO fixture.
file(GLOB_RECURSE QUALPF_LOCAL_EXAMPLES LIST_DIRECTORIES true CONFIGURE_DEPENDS
    RELATIVE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/tests/real_example_data/*")
foreach(example IN LISTS QUALPF_LOCAL_EXAMPLES)
    if(NOT example STREQUAL "tests/real_example_data/03_reference_structures" AND
       NOT example STREQUAL "tests/real_example_data/03_reference_structures/CoO.cif")
        set(example_is_directory false)
        if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/${example}")
            set(example_is_directory true)
        endif()
        qualpf_cpack_escape_regex(example "${example}")
        if(example_is_directory)
            list(APPEND CPACK_SOURCE_IGNORE_FILES "${QUALPF_CPACK_SOURCE_ROOT}/${example}[/]")
        else()
            list(APPEND CPACK_SOURCE_IGNORE_FILES "${QUALPF_CPACK_SOURCE_ROOT}/${example}$")
        endif()
    endif()
endforeach()
# The tracked-only exporter includes the preferred symmetry generator input;
# other sealed archives remain independent test prerequisites outside delivery.
set(QUALPF_REFERENCE_ARCHIVES "${PROJECT_SOURCE_DIR}/tests/reference/archives.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${QUALPF_REFERENCE_ARCHIVES}")
file(READ "${QUALPF_REFERENCE_ARCHIVES}" QUALPF_REFERENCE_ARCHIVE_MANIFEST)
string(JSON QUALPF_REFERENCE_ARCHIVE_COUNT LENGTH "${QUALPF_REFERENCE_ARCHIVE_MANIFEST}" files)
math(EXPR QUALPF_REFERENCE_ARCHIVE_LAST "${QUALPF_REFERENCE_ARCHIVE_COUNT} - 1")
foreach(index RANGE 0 ${QUALPF_REFERENCE_ARCHIVE_LAST})
    string(JSON archive GET "${QUALPF_REFERENCE_ARCHIVE_MANIFEST}" files ${index} path)
    if(NOT archive STREQUAL "tests/data/symmetry-reference.zip")
        qualpf_cpack_escape_regex(archive "${archive}")
        list(APPEND CPACK_SOURCE_IGNORE_FILES "${QUALPF_CPACK_SOURCE_ROOT}/${archive}$")
    endif()
endforeach()
string(JSON QUALPF_EXCLUDED_LEGACY_COUNT LENGTH "${QUALPF_DISTRIBUTION_JSON}" excluded_legacy_assets)
math(EXPR QUALPF_EXCLUDED_LEGACY_LAST "${QUALPF_EXCLUDED_LEGACY_COUNT} - 1")
foreach(index RANGE 0 ${QUALPF_EXCLUDED_LEGACY_LAST})
    string(JSON excluded GET "${QUALPF_DISTRIBUTION_JSON}" excluded_legacy_assets ${index})
    qualpf_cpack_escape_regex(excluded "${excluded}")
    list(APPEND CPACK_SOURCE_IGNORE_FILES "${QUALPF_CPACK_SOURCE_ROOT}/${excluded}$")
endforeach()

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VENDOR "${QUALPF_PUBLISHER}")
set(CPACK_PACKAGE_CONTACT "${QUALPF_SUPPORT}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${QUALPF_HOMEPAGE}")
set(CPACK_PACKAGE_DESCRIPTION "${QUALPF_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${QUALPF_DESCRIPTION}")

if(WIN32)
    set(CPACK_GENERATOR "IFW")
    # Discover QtIFW on PATH, or set CPACK_IFW_ROOT in CMakeUserPresets.json.
    set(CPACK_IFW_PACKAGE_NAME "${PROJECT_NAME}${QUALPF_DISTRIBUTION_SUFFIX}")
    set(CPACK_IFW_PACKAGE_START_MENU_DIRECTORY "${PROJECT_NAME}${QUALPF_DISTRIBUTION_SUFFIX}")
    set(CPACK_IFW_PACKAGE_TITLE "${PROJECT_NAME}")
    if(QUALPF_DISTRIBUTION_AUDIT_ONLY)
        set(CPACK_IFW_PACKAGE_TITLE "${PROJECT_NAME} ${PROJECT_VERSION} (local audit)")
    endif()
    set(CPACK_IFW_PACKAGE_PUBLISHER "${QUALPF_PUBLISHER}")
    set(CPACK_IFW_PACKAGE_WINDOW_ICON "${PROJECT_SOURCE_DIR}/src/gui/resources/branding/qualpf.png")
    set(CPACK_IFW_PACKAGE_ICON "${PROJECT_SOURCE_DIR}/src/gui/resources/branding/qualpf.ico")
    set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Aero")
    set(CPACK_IFW_TARGET_DIRECTORY "@ApplicationsDir@/${PROJECT_NAME}${QUALPF_DISTRIBUTION_SUFFIX}")
    set(CPACK_IFW_PACKAGE_CONTROL_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/deploy/Windows/controlscript.qs")
    message("CONTROL: ${CPACK_IFW_PACKAGE_CONTROL_SCRIPT}")

    include(CPackIFW)

    CPACK_ADD_COMPONENT(${COMPONENT_NAME_MAIN})

    # Generate only the C++ runtime choice; retain IFW's @TargetDir@ and other
    # path tokens verbatim rather than expanding them as CMake variables.
    if(MSVC)
        set(QUALPF_IFW_MSVC_RUNTIME true)
    else()
        set(QUALPF_IFW_MSVC_RUNTIME false)
    endif()
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/deploy/Windows/installscript.qs" QUALPF_IFW_INSTALL_SCRIPT)
    string(REPLACE "@QUALPF_IFW_MSVC_RUNTIME@" "${QUALPF_IFW_MSVC_RUNTIME}"
        QUALPF_IFW_INSTALL_SCRIPT "${QUALPF_IFW_INSTALL_SCRIPT}")
    string(REPLACE "@QUALPF_EXECUTABLE_NAME@" "${EXECUTABLE_NAME}"
        QUALPF_IFW_INSTALL_SCRIPT "${QUALPF_IFW_INSTALL_SCRIPT}")
    set(QUALPF_IFW_DISPLAY_NAME "${PROJECT_NAME}")
    if(QUALPF_DISTRIBUTION_AUDIT_ONLY)
        set(QUALPF_IFW_DISPLAY_NAME "${PROJECT_NAME} (audit)")
    endif()
    string(REPLACE "@QUALPF_DISPLAY_NAME@" "${QUALPF_IFW_DISPLAY_NAME}"
        QUALPF_IFW_INSTALL_SCRIPT "${QUALPF_IFW_INSTALL_SCRIPT}")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/configured_files/deploy/Windows")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/configured_files/deploy/Windows/installscript.qs"
        "${QUALPF_IFW_INSTALL_SCRIPT}")
    cpack_ifw_configure_component(${COMPONENT_NAME_MAIN}
        ESSENTIAL FORCED_INSTALLATION
        DESCRIPTION "${QUALPF_DESCRIPTION}"
        LICENSES "${QUALPF_LICENSE}" "${CPACK_RESOURCE_FILE_LICENSE}"
        SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/configured_files/deploy/Windows/installscript.qs")

    # The old QualX feed installs a foreign product. This C++ package has no
    # qualified online provider; maintenance remains available for this package.

    include(CPack)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB")
    execute_process(
        COMMAND dpkg --print-architecture
        OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND lsb_release -c -s
        OUTPUT_VARIABLE DISTRO_CODENAME
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # Set default architecture if dpkg is not available
    if(NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    endif()

    # Set default codename if lsb_release is not available
    if(NOT DISTRO_CODENAME)
        set(DISTRO_CODENAME "debian")
    endif()

    set(CPACK_DEBIAN_FILE_NAME "${EXECUTABLE_NAME}-${PROJECT_VERSION}-${DISTRO_CODENAME}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}${QUALPF_DISTRIBUTION_SUFFIX}.deb")
    set(CPACK_DEBIAN_PACKAGE_NAME "${EXECUTABLE_NAME}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${QUALPF_PUBLISHER} <${QUALPF_RELEASE_CONTACT_EMAIL}>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6 | libqt6core6t64, libqt6gui6 | libqt6gui6t64, libqt6widgets6 | libqt6widgets6t64, libqt6sql6 | libqt6sql6t64, libqt6sql6-sqlite, libqt6printsupport6 | libqt6printsupport6t64, libqt6network6 | libqt6network6t64, libqt6opengl6 | libqt6opengl6t64, libqt6openglwidgets6 | libqt6openglwidgets6t64")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_SOURCE_GENERATOR "TGZ")
    include(CPack)
endif()

#include(CPack)

#CPACK_ADD_COMPONENT(${COMPONENT_NAME_MAIN})
