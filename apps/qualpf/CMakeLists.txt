qt_add_executable(${EXECUTABLE_NAME}
    main.cpp
    commandline.cpp
    commandline.h
)
qualpf_configure_qt_boundary(${EXECUTABLE_NAME} Widgets Sql PrintSupport)

# OBJECT libraries retain every product translation unit; no static
# archive ordering or dead stripping is introduced during this organization pass.
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    qualpf_core
    qualpf_application
    qualpf_infrastructure
    qualpf_gui
    qualpf_qcustomplot
    qualpf_gui_reference

    )

# Keep the existing build-tree executable path for deployment scripts
# and share-directory lookup, despite moving the application sources.
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src"
)

include("${PROJECT_SOURCE_DIR}/cmake/packaging/MinGWRuntime.cmake")
qualpf_stage_mingw_runtime(${EXECUTABLE_NAME})
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    "${PROJECT_SOURCE_DIR}/src/gui/resources/images"
    "${PROJECT_SOURCE_DIR}/src/gui/resources/branding"
)

if (WIN32)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE opengl32)
endif()

set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
)

#Setting the Application Icon
if (WIN32)
    set(app_icon_resource_windows "${PROJECT_SOURCE_DIR}/src/gui/resources/branding/qualpf.rc")
    target_sources(${EXECUTABLE_NAME} PRIVATE ${app_icon_resource_windows})
    set_property(SOURCE ${app_icon_resource_windows} APPEND PROPERTY OBJECT_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/gui/resources/branding/qualpf.ico")
endif()
