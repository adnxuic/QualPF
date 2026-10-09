add_library(qualpf_cif_database STATIC
    qt_sql/sql_schema.cpp
    qt_sql/sql_schema.h
    database_access.cpp
    database_access.h
    "${PROJECT_SOURCE_DIR}/src/infrastructure/database/qt_sql/cif_database_writer.cpp"
    "${PROJECT_SOURCE_DIR}/src/infrastructure/database/qt_sql/sql_schema_builder.cpp"
    "${PROJECT_SOURCE_DIR}/src/infrastructure/database/qt_sql/sql_schema_builder.h")
target_compile_features(qualpf_cif_database PUBLIC cxx_std_20)
target_include_directories(qualpf_cif_database PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(qualpf_cif_database PUBLIC Qt6::Core Qt6::Sql qualpf_cif_card)
set_target_properties(qualpf_cif_database PROPERTIES AUTOMOC ON)

include(cif_product.cmake)
if(QUALPF_BUILD_PRODUCT_TESTS)
    include(cif_database_build.cmake)
endif()
