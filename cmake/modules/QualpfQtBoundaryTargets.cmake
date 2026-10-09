# Only for existing Qt-coupled C++ modules; never apply to qualpf_core.
function(qualpf_configure_qt_boundary target)
    target_include_directories(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_SOURCE_DIR}/src/application/include"
        "${CMAKE_BINARY_DIR}/configured_files/include"
    )
    target_compile_definitions(${target} PRIVATE USE_CONFIG_H)
    # Boundary targets declare their Qt modules; data providers do not inherit GUI libraries.
    set(qualpf_qt_modules ${ARGN})
    if(NOT qualpf_qt_modules)
        message(FATAL_ERROR "${target} must explicitly declare its Qt modules")
    endif()
    foreach(module IN LISTS qualpf_qt_modules)
        target_link_libraries(${target} PRIVATE Qt6::${module})
    endforeach()
endfunction()
