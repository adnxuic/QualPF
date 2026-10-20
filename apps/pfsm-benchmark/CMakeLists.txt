add_executable(qualpf_pfsm_benchmark main.cpp candidate_recall_diagnostics.cpp)
qualpf_configure_qt_boundary(qualpf_pfsm_benchmark Core Sql)
target_link_libraries(qualpf_pfsm_benchmark PRIVATE qualpf_application qualpf_infrastructure)
set_target_properties(qualpf_pfsm_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(WIN32)
    target_link_libraries(qualpf_pfsm_benchmark PRIVATE psapi)
endif()
set(pfsm_revision_header "${CMAKE_CURRENT_BINARY_DIR}/pfsm_build_revision.h")
# Check at build time: a commit can advance a branch ref without changing HEAD
# or causing CMake to configure again. The helper preserves an unchanged header.
add_custom_target(qualpf_pfsm_revision
    COMMAND "${CMAKE_COMMAND}"
        "-DQUALPF_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DQUALPF_REVISION_HEADER=${pfsm_revision_header}"
        -P "${PROJECT_SOURCE_DIR}/cmake/UpdatePfsmRevision.cmake"
    BYPRODUCTS "${pfsm_revision_header}"
    COMMENT "Refreshing PFSM build revision"
    VERBATIM)
add_dependencies(qualpf_pfsm_benchmark qualpf_pfsm_revision)
target_sources(qualpf_pfsm_benchmark PRIVATE "${pfsm_revision_header}")
target_include_directories(qualpf_pfsm_benchmark PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(qualpf_pfsm_benchmark PRIVATE
    QUALPF_PFSM_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}")

# Validation replay consumes frozen physical state without invoking a formal fit.
add_executable(qualpf_pfsm_candidate_recall_replay replay.cpp candidate_recall_diagnostics.cpp)
qualpf_configure_qt_boundary(qualpf_pfsm_candidate_recall_replay Core Sql)
find_package(OpenMP REQUIRED COMPONENTS CXX)
target_link_libraries(qualpf_pfsm_candidate_recall_replay PRIVATE qualpf_application qualpf_infrastructure OpenMP::OpenMP_CXX)
set_target_properties(qualpf_pfsm_candidate_recall_replay PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_dependencies(qualpf_pfsm_candidate_recall_replay qualpf_pfsm_revision)
target_sources(qualpf_pfsm_candidate_recall_replay PRIVATE "${pfsm_revision_header}")
target_include_directories(qualpf_pfsm_candidate_recall_replay PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(qualpf_pfsm_candidate_recall_replay PRIVATE /fp:strict)
else()
    target_compile_options(qualpf_pfsm_candidate_recall_replay PRIVATE -fno-fast-math -ffp-contract=off)
endif()
