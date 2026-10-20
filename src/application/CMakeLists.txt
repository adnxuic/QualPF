add_subdirectory(database)
add_subdirectory(session)
add_library(qualpf_application STATIC
    domain_inputs.cpp
    plot_coordinates.cpp
    database_build.cpp
    product_files.cpp
    update_service.cpp
    platform_services.cpp
    import/powder_cif_import_service.cpp
    include/qualpf/application/import/powder_cif_import_service.h
    export/session_export.cpp
    include/qualpf/application/export/session_export.h
    presentation/quantification.cpp
    include/qualpf/application/presentation/quantification.h
    import/powder_cif_provenance.cpp
    include/qualpf/application/import/powder_cif_provenance.h
    export/pattern_export_input.cpp
    include/qualpf/application/export/pattern_export_input.h
    export/atomic_export_writer.cpp
    export/quantification_export.cpp
    include/qualpf/application/export/quantification_export.h
    include/qualpf/application/export/atomic_export_writer.h
    include/qualpf/application/presentation/plot_snapshot.h
    presentation/plot_projection.cpp
    state/presentation_workspace.cpp
    include/qualpf/application/state/presentation_workspace.h
    include/qualpf/application/startup/application_initialization_service.h
)
target_compile_features(qualpf_application PUBLIC cxx_std_20)
target_include_directories(qualpf_application PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(qualpf_application PUBLIC qualpf_session qualpf_cif_card qualpf_powder_cif qualpf_export_coordinates qualpf_pattern_formatter)
set_target_properties(qualpf_application PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
if(MSVC)
    target_compile_options(qualpf_application PRIVATE /fp:strict)
else()
    target_compile_options(qualpf_application PRIVATE -fno-fast-math -ffp-contract=off)
endif()

target_link_libraries(qualpf_application PUBLIC qualpf_plot_coordinates)
