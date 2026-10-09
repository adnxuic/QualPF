# Qt-free state management can also be configured by the standalone contract harness.
add_library(qualpf_session STATIC
    session.cpp
    classic_search.cpp
    product_session.cpp
    search_engine.cpp
    search_coordinator.cpp
    workspace_use_cases.cpp
    reference_database.cpp
    preprocessing_configuration.cpp
    ../preprocessing/kalpha2.cpp
    ../preprocessing/preprocessing_requests.cpp
    ../preprocessing/preprocessing_values.cpp
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
)
target_compile_features(qualpf_session PUBLIC cxx_std_20)
target_include_directories(qualpf_session PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(qualpf_session PUBLIC qualpf_core qualpf_peak_measurement)
set_target_properties(qualpf_session PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

if(MSVC)
    set_source_files_properties(../preprocessing/peak_coordinates.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
else()
    set_source_files_properties(../preprocessing/peak_coordinates.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()
