# Product integration composes the approved libraries without changing their
# numerical compile flags or standalone validation targets.
add_library(qualpf_cif_product STATIC
    "${CMAKE_CURRENT_LIST_DIR}/cif_database_import_service.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/cif_database_product.cpp")
target_link_libraries(qualpf_cif_product PUBLIC qualpf::cif_card qualpf_cif_database)
target_include_directories(qualpf_cif_product PUBLIC "${CMAKE_CURRENT_LIST_DIR}" PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_features(qualpf_cif_product PUBLIC cxx_std_20)
set_target_properties(qualpf_cif_product PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
