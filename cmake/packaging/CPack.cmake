# CPack configuration
# Keep generated packages and CPack work under the selected build tree.
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages" CACHE PATH "CPack output directory")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${EXECUTABLE_NAME}-${PROJECT_VERSION}-Source")
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
    "[/][.]pytest_cache[/]"
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
# Keep provisioned reference payloads outside source packages as well as Git.
set(QUALPF_REFERENCE_ARCHIVES "${PROJECT_SOURCE_DIR}/tests/reference/archives.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${QUALPF_REFERENCE_ARCHIVES}")
file(READ "${QUALPF_REFERENCE_ARCHIVES}" QUALPF_REFERENCE_ARCHIVE_MANIFEST)
string(JSON QUALPF_REFERENCE_ARCHIVE_COUNT LENGTH "${QUALPF_REFERENCE_ARCHIVE_MANIFEST}" files)
math(EXPR QUALPF_REFERENCE_ARCHIVE_LAST "${QUALPF_REFERENCE_ARCHIVE_COUNT} - 1")
foreach(index RANGE 0 ${QUALPF_REFERENCE_ARCHIVE_LAST})
    string(JSON archive GET "${QUALPF_REFERENCE_ARCHIVE_MANIFEST}" files ${index} path)
    qualpf_cpack_escape_regex(archive "${archive}")
    list(APPEND CPACK_SOURCE_IGNORE_FILES "${QUALPF_CPACK_SOURCE_ROOT}/${archive}$")
endforeach()

set(CPACK_RESOURCE_FILE_LICENSE  ${CMAKE_CURRENT_SOURCE_DIR}/deploy/Windows/licence.txt)
set(CPACK_PACKAGE_VENDOR "Institute of Crystallography - CNR")
set(CPACK_PACKAGE_CONTACT "corrado.cuocci@ic.cnr.it")
set(CPACK_PACKAGE_DESCRIPTION "Automatic solution and refinement of crystal structures")

if(WIN32)
    set(CPACK_GENERATOR "IFW")
    # Discover QtIFW on PATH, or set CPACK_IFW_ROOT in CMakeUserPresets.json.
    set(CPACK_IFW_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_IFW_PACKAGE_TITLE "${PROJECT_NAME}")
    set(CPACK_IFW_PACKAGE_PUBLISHER "Corrado Cuocci")
    set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Aero")
    set(CPACK_IFW_TARGET_DIRECTORY "@ApplicationsDir@/${PROJECT_NAME}")
    set(CPACK_IFW_PACKAGE_CONTROL_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/deploy/Windows/controlscript.qs")
    message("CONTROL: ${CPACK_IFW_PACKAGE_CONTROL_SCRIPT}")

    include(CPackIFW)

    CPACK_ADD_COMPONENT(${COMPONENT_NAME_MAIN})

    # The component HAS to be installed
    cpack_ifw_configure_component(${COMPONENT_NAME_MAIN} ESSENTIAL FORCED_INSTALLATION)

    # Adds license info to package.xml file
    # Also copies licence.txt to 'meta' directory to display the license text in the installer.
    cpack_ifw_configure_component(${COMPONENT_NAME_MAIN} LICENSES "${PROJECT_NAME} License" ${CPACK_RESOURCE_FILE_LICENSE})

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
    string(REPLACE "@QUALPF_DISPLAY_NAME@" "${PROJECT_NAME}"
        QUALPF_IFW_INSTALL_SCRIPT "${QUALPF_IFW_INSTALL_SCRIPT}")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/configured_files/deploy/Windows")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/configured_files/deploy/Windows/installscript.qs"
        "${QUALPF_IFW_INSTALL_SCRIPT}")
    cpack_ifw_configure_component(${COMPONENT_NAME_MAIN}
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

    set(CPACK_DEBIAN_FILE_NAME "${EXECUTABLE_NAME}-${PROJECT_VERSION}-${DISTRO_CODENAME}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")
    set(CPACK_DEBIAN_PACKAGE_NAME "${EXECUTABLE_NAME}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Corrado Cuocci <corrado.cuocci@ic.cnr.it>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6 | libqt6core6t64, libqt6gui6 | libqt6gui6t64, libqt6widgets6 | libqt6widgets6t64, libqt6sql6 | libqt6sql6t64, libqt6sql6-sqlite, libqt6printsupport6 | libqt6printsupport6t64, libqt6network6 | libqt6network6t64, libqt6opengl6 | libqt6opengl6t64, libqt6openglwidgets6 | libqt6openglwidgets6t64")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_SOURCE_GENERATOR "TGZ")
    include(CPack)
endif()

#include(CPack)

#CPACK_ADD_COMPONENT(${COMPONENT_NAME_MAIN})
