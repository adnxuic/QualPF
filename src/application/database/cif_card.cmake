# Included by consumers only after the independent CIF importer is available.
add_library(qualpf_cif_card STATIC "${CMAKE_CURRENT_LIST_DIR}/cif_batch.cpp" "${CMAKE_CURRENT_LIST_DIR}/cif_import_service.cpp" "${CMAKE_CURRENT_LIST_DIR}/cif_card_service.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/cif_card_record.cpp")
add_library(qualpf::cif_card ALIAS qualpf_cif_card)
target_include_directories(qualpf_cif_card PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../include")
target_link_libraries(qualpf_cif_card PUBLIC qualpf::cif_import)
target_compile_features(qualpf_cif_card PUBLIC cxx_std_20)
set_target_properties(qualpf_cif_card PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
if(MSVC)
    target_compile_options(qualpf_cif_card PRIVATE /W4 /fp:strict)
else()
    target_compile_options(qualpf_cif_card PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
