add_library(qualpf_gui_reference OBJECT
    libcomune.cpp libcomune.h libmath.cpp libmath.h libmatrix.h libvector.h locate.cpp nr.h)
target_compile_features(qualpf_gui_reference PUBLIC cxx_std_20)
target_include_directories(qualpf_gui_reference PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(qualpf_gui_reference PRIVATE Qt6::Widgets)
