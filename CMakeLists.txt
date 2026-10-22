cmake_minimum_required(VERSION 3.20)

get_filename_component(ProjectId ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" ProjectId ${ProjectId})
project(QualPF VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "QualPF supports Windows and Linux.")
endif()
set(EXECUTABLE_NAME "qualpf")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include(cmake/packaging/ProductIdentity.cmake)
include(QualpfBuildPolicy)
include(cmake/packaging/QtDeploymentOptions.cmake)
include(QualpfQtDependencies)
include(QualpfOptions)

include(QualpfGeneratedFiles)
include(QualpfQtBoundaryTargets)
add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(apps/qualpf)
add_subdirectory(apps/pfsm-benchmark)
add_subdirectory(tests)

include(cmake/packaging/Install.cmake)
include(cmake/packaging/Distribution.cmake)
include(cmake/packaging/Linux.cmake)
include(cmake/packaging/QtDeployment.cmake)
include(cmake/packaging/CPack.cmake)

include(QualpfStrictBoundaries)
qualpf_enable_strict_boundaries()
