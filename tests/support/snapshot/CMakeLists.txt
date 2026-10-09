# Owning transport conversions are test support, built only for real consumers.
add_library(qualpf_test_snapshot STATIC snapshot_codec.cpp snapshot_codec.h)
target_compile_features(qualpf_test_snapshot PUBLIC cxx_std_20)
target_include_directories(qualpf_test_snapshot PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qualpf_test_snapshot PUBLIC qualpf_core)
set_target_properties(qualpf_test_snapshot PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
