# The six approved importer sources are registered unchanged. Numerical and
# syntax ownership remains independent of Qt and the transitional Legacy product.
find_path(QUALPF_POWDER_BOOST_INCLUDE_DIR boost/multiprecision/cpp_int.hpp
    HINTS ${Boost_INCLUDE_DIRS} "${BOOST_ROOT}" REQUIRED)
add_library(qualpf_powder_cif STATIC powder_cif_importer.cpp binary32.cpp sha256.cpp)
add_library(qualpf::powder_cif ALIAS qualpf_powder_cif)
target_compile_features(qualpf_powder_cif PUBLIC cxx_std_20)
target_include_directories(qualpf_powder_cif PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${QUALPF_POWDER_BOOST_INCLUDE_DIR}")
target_link_libraries(qualpf_powder_cif PUBLIC qualpf_cif_syntax qualpf_core)
set_target_properties(qualpf_powder_cif PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
if(MSVC)
    target_compile_options(qualpf_powder_cif PRIVATE /W4 /fp:strict)
else()
    target_compile_options(qualpf_powder_cif PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
