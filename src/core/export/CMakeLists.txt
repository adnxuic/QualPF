# The unchanged Phase24A sources and strict compilation contract are shared by
# production and CXX-only cross-compiler integration checks.
add_library(qualpf_export_coordinates STATIC binary32_coordinates.cpp)
add_library(qualpf_pattern_formatter STATIC pattern_export.cpp)
foreach(target qualpf_export_coordinates qualpf_pattern_formatter)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    if(MSVC)
        target_compile_options(${target} PRIVATE /fp:strict)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -fno-fast-math -ffp-contract=off)
    endif()
endforeach()
