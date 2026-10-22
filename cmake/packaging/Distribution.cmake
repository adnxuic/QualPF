include_guard(GLOBAL)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(QUALPF_DISTRIBUTION_RUNTIME_REVIEW "" CACHE FILEPATH "Package-specific runtime/source review JSON")
set(QUALPF_CORRESPONDING_SOURCE_ARCHIVE "" CACHE FILEPATH "Exact corresponding-source TGZ for a release binary")
set(QUALPF_DISTRIBUTION_CHECKER "${PROJECT_SOURCE_DIR}/tools/validation/check_distribution.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${QUALPF_DISTRIBUTION_CHECKER}")

# A new commit must refresh the generated provenance even when source bytes did
# not otherwise trigger configuration. Source archives use their portable stamp.
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    foreach(ref HEAD)
        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${ref}"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE git_file OUTPUT_STRIP_TRAILING_WHITESPACE)
        get_filename_component(git_file "${git_file}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${git_file}")
    endforeach()
    execute_process(COMMAND "${GIT_EXECUTABLE}" symbolic-ref --quiet HEAD
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE git_ref OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(git_ref)
        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${git_ref}"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE git_file OUTPUT_STRIP_TRAILING_WHITESPACE)
        get_filename_component(git_file "${git_file}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${git_file}")
    endif()
elseif(EXISTS "${PROJECT_SOURCE_DIR}/distribution-source.json")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/distribution-source.json")
endif()

set(QUALPF_EIGEN_VERSION "unknown")
get_filename_component(QUALPF_EIGEN_PREFIX "${Eigen3_DIR}/../../.." ABSOLUTE)
set(QUALPF_EIGEN_MACROS "${QUALPF_EIGEN_PREFIX}/include/eigen3/Eigen/src/Core/util/Macros.h")
if(EXISTS "${QUALPF_EIGEN_MACROS}")
    file(READ "${QUALPF_EIGEN_MACROS}" eigen_macros)
    foreach(part WORLD MAJOR MINOR)
        string(REGEX MATCH "#define EIGEN_${part}_VERSION +([0-9]+)" match "${eigen_macros}")
        set(eigen_${part} "${CMAKE_MATCH_1}")
    endforeach()
    set(QUALPF_EIGEN_VERSION "${eigen_WORLD}.${eigen_MAJOR}.${eigen_MINOR}")
endif()
set(QUALPF_BOOST_VERSION "unknown")
if(EXISTS "${QUALPF_CORE_BOOST_INCLUDE_DIR}/boost/version.hpp")
    file(READ "${QUALPF_CORE_BOOST_INCLUDE_DIR}/boost/version.hpp" boost_version_header)
    string(REGEX MATCH "#define BOOST_VERSION +([0-9]+)" match "${boost_version_header}")
    set(qualpf_boost_number "${CMAKE_MATCH_1}")
    math(EXPR qualpf_boost_major "${qualpf_boost_number} / 100000")
    math(EXPR qualpf_boost_minor "(${qualpf_boost_number} / 100) % 1000")
    math(EXPR qualpf_boost_patch "${qualpf_boost_number} % 100")
    set(QUALPF_BOOST_VERSION "${qualpf_boost_major}.${qualpf_boost_minor}.${qualpf_boost_patch}")
endif()
set(QUALPF_BUILD_TOOL_VERSION "unknown")
if(EXISTS "${CMAKE_MAKE_PROGRAM}")
    execute_process(COMMAND "${CMAKE_MAKE_PROGRAM}" --version
        OUTPUT_VARIABLE distribution_tool_version OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    string(REGEX MATCH "^[^\r\n]+" distribution_tool_version "${distribution_tool_version}")
    get_filename_component(distribution_tool_name "${CMAKE_MAKE_PROGRAM}" NAME)
    set(QUALPF_BUILD_TOOL_VERSION "${distribution_tool_name} ${distribution_tool_version}")
endif()
set(distribution_arguments --root "${PROJECT_SOURCE_DIR}"
    --write-build-info "${CMAKE_BINARY_DIR}/configured_files/distribution-build.json"
    --compiler "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR})"
    --qt-version "${Qt6_VERSION}" --eigen-version "${QUALPF_EIGEN_VERSION}" --boost-version "${QUALPF_BOOST_VERSION}"
    --cmake-version "${CMAKE_VERSION}" --generator "${CMAKE_GENERATOR}" --build-tool "${QUALPF_BUILD_TOOL_VERSION}")
if(QUALPF_DISTRIBUTION_AUDIT_ONLY)
    list(APPEND distribution_arguments --audit-only)
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${QUALPF_DISTRIBUTION_CHECKER}" ${distribution_arguments}
    RESULT_VARIABLE distribution_status OUTPUT_VARIABLE distribution_output ERROR_VARIABLE distribution_error)
if(NOT distribution_status EQUAL 0)
    message(FATAL_ERROR "Distribution input consistency failed: ${distribution_output}${distribution_error}")
endif()

# Legal materials are ordinary install inputs as well as compiled GUI resources.
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/NOTICE.md"
    "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    "${CMAKE_BINARY_DIR}/configured_files/distribution-build.json"
    DESTINATION "share/${EXECUTABLE_NAME}" COMPONENT "${PROJECT_NAME}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/LICENSES" DESTINATION "share/${EXECUTABLE_NAME}"
    COMPONENT "${PROJECT_NAME}")
install(FILES "${QUALPF_DISTRIBUTION_MANIFEST}" DESTINATION "share/${EXECUTABLE_NAME}/deploy"
    COMPONENT "${PROJECT_NAME}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES "${PROJECT_SOURCE_DIR}/NOTICE.md" DESTINATION "share/doc/${EXECUTABLE_NAME}"
        RENAME copyright COMPONENT "${PROJECT_NAME}")
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        DESTINATION "share/doc/${EXECUTABLE_NAME}" COMPONENT "${PROJECT_NAME}")
endif()

# Carry the actual SDK's notice data, not a claim that generic license texts
# cover all of its embedded dependencies. A missing SDK SBOM remains review work.
get_target_property(distribution_qmake Qt6::qmake IMPORTED_LOCATION)
if(distribution_qmake)
    execute_process(COMMAND "${distribution_qmake}" -query QT_INSTALL_PREFIX
        OUTPUT_VARIABLE distribution_qt_prefix OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(EXISTS "${distribution_qt_prefix}/sbom")
        install(DIRECTORY "${distribution_qt_prefix}/sbom/"
            DESTINATION "share/${EXECUTABLE_NAME}/SDK-NOTICES/Qt"
            COMPONENT "${PROJECT_NAME}" FILES_MATCHING PATTERN "*.spdx")
    endif()
endif()
if(EXISTS "${QUALPF_EIGEN_PREFIX}/share/licenses/Eigen")
    install(DIRECTORY "${QUALPF_EIGEN_PREFIX}/share/licenses/Eigen/"
        DESTINATION "share/${EXECUTABLE_NAME}/SDK-NOTICES/Eigen"
        COMPONENT "${PROJECT_NAME}")
endif()

configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/DistributionPackage.cmake.in"
    "${CMAKE_BINARY_DIR}/configured_files/DistributionPackage.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/DistributionStage.cmake.in"
    "${CMAKE_BINARY_DIR}/configured_files/DistributionStage.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/DistributionArtifacts.cmake.in"
    "${CMAKE_BINARY_DIR}/configured_files/DistributionArtifacts.cmake" @ONLY)
