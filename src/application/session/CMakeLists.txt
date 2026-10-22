# Qt-free state management can also be configured by the standalone contract harness.
add_library(qualpf_session STATIC
    session.cpp
    classic_search.cpp
    product_session.cpp
    search_engine.cpp
    classical_workflow.cpp
    pfsm_observations.cpp
    pfsm_candidate_recall_refinement.cpp
    pfsm_candidate_recall_correlation.cpp
    pfsm_candidate_recall_integral.cpp
    pfsm_workflow.cpp
    pfsm_formal_fit.cpp
    pfsm_state.cpp
    pfsm_presentation.cpp
    result_management.cpp
    search_preparation.cpp
    algorithms.cpp
    search_coordinator.cpp
    workspace_use_cases.cpp
    ../project_document.cpp
    reference_database.cpp
    preprocessing_configuration.cpp
    workflow_settings.cpp
    workflow.cpp
    ../preprocessing/kalpha2.cpp
    ../preprocessing/preprocessing_requests.cpp
    ../preprocessing/preprocessing_values.cpp
    ../preprocessing/preprocessing_workflows.cpp
    ../preprocessing/geometric_fwhm.cpp
    ../preprocessing/configured_fwhm.cpp
    ../preprocessing/initial_peak_search.cpp
    ../include/qualpf/application/initial_peak_configuration.h
    ../include/qualpf/application/initial_peak_search.h
    ../include/qualpf/application/configured_fwhm.h
    ../include/qualpf/application/fwhm_configuration.h
    ../include/qualpf/application/geometric_fwhm.h
    ../preprocessing/preprocessing_evaluation.cpp
    ../preprocessing/product_preprocessing_edit.cpp
    ../preprocessing/manual_peaks.cpp
    ../preprocessing/peak_operations.cpp
    ../preprocessing/peak_policy.cpp
    ../preprocessing/range_policy.cpp
    ../preprocessing/peak_coordinates.cpp
    ../include/qualpf/application/classic_search.h
    ../include/qualpf/application/session.h
    ../include/qualpf/application/ports.h
    ../include/qualpf/application/algorithms.h
    ../include/qualpf/application/candidate_query.h
)
target_compile_features(qualpf_session PUBLIC cxx_std_20)
target_include_directories(qualpf_session PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(qualpf_session PUBLIC qualpf_core qualpf_peak_measurement)
find_package(OpenMP REQUIRED COMPONENTS CXX)
target_link_libraries(qualpf_session PRIVATE OpenMP::OpenMP_CXX)
set_target_properties(qualpf_session PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

if(MSVC)
    set_source_files_properties(pfsm_candidate_recall_integral.cpp pfsm_candidate_recall_refinement.cpp pfsm_candidate_recall_correlation.cpp pfsm_observations.cpp pfsm_formal_fit.cpp pfsm_workflow.cpp pfsm_state.cpp pfsm_presentation.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
    set_source_files_properties(../preprocessing/configured_fwhm.cpp ../preprocessing/initial_peak_search.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
    set_source_files_properties(../preprocessing/peak_coordinates.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
else()
    set_source_files_properties(pfsm_candidate_recall_integral.cpp pfsm_candidate_recall_refinement.cpp pfsm_candidate_recall_correlation.cpp pfsm_observations.cpp pfsm_formal_fit.cpp pfsm_workflow.cpp pfsm_state.cpp pfsm_presentation.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
    set_source_files_properties(../preprocessing/configured_fwhm.cpp ../preprocessing/initial_peak_search.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
    set_source_files_properties(../preprocessing/peak_coordinates.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()
