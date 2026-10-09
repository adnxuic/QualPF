add_subdirectory(io/pattern)
add_subdirectory(database)

add_library(qualpf_infrastructure OBJECT
    database/database_build_provider.cpp
    io/product_files.cpp
    platform/platform_services.cpp
    settings/database_catalog_store.cpp
    settings/preference_store.cpp
    io/atomic_export_target.cpp
    io/atomic_export_target.h
    serialization/project_codec.cpp
    serialization/project_codec.h
    serialization/quantification.h
    database/qt_sql/candidate_reader_factory.cpp
    database/qt_sql/candidate_reader_factory.h
    settings/database_settings.cpp
    settings/database_settings.h
    settings/search_settings.cpp
    settings/search_settings.h
    io/powder_source_reader.cpp
    io/powder_source_reader.h
    io/pattern_source_reader.cpp
    io/pattern_source_reader.h
    database/searchutil.cpp
    database/searchutil.h
    serialization/preprocessing_config.cpp
    serialization/preprocessing_config.h
    serialization/powder_cif_provenance.cpp
    serialization/powder_cif_provenance.h
    database/qt_sql/sql_connection.cpp
    database/qt_sql/sql_connection.h
    database/qt_sql/sql_candidate_query.cpp
    database/qt_sql/sql_candidate_query.h
    database/qt_sql/sql_reference_repository.cpp
    database/qt_sql/sql_reference_repository.h
    database/qt_sql/pdf2_record_mapper.cpp
    database/qt_sql/pdf2_record_mapper.h
    database/qt_sql/pdf2_database_writer.cpp
    database/qt_sql/pdf2_database_writer.h
    diagnostics/scopedtimer.h
    io/project_peaks.cpp
    io/project_peaks.h
    io/pdf2/pdf2reader.cpp
    io/pdf2/pdf2reader.h
    resources/fileutils.cpp
    resources/fileutils.h
    settings/settings_migration.cpp
    settings/settings_migration.h
    settings/progkeysettings.h
)
qualpf_configure_qt_boundary(qualpf_infrastructure Core Sql)
target_link_libraries(qualpf_infrastructure PRIVATE qualpf_core)
target_link_libraries(qualpf_infrastructure PUBLIC qualpf_cif_database)

target_link_libraries(qualpf_infrastructure PUBLIC qualpf_application)

target_link_libraries(qualpf_infrastructure PUBLIC qualpf_pattern_file)

# Network transport is an explicit Qt Core/Network adapter.
add_library(qualpf_update_provider STATIC network/update_service.cpp)
qualpf_configure_qt_boundary(qualpf_update_provider Core Network)
target_link_libraries(qualpf_update_provider PUBLIC qualpf_application)
target_link_libraries(qualpf_infrastructure PUBLIC qualpf_update_provider)

target_link_libraries(qualpf_infrastructure PUBLIC qualpf_cif_product)
