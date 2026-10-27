# QPF decoding uses QCborStreamReader::readAndAppendToString (Qt 6.7).
find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets Sql PrintSupport Network)

# Add Qt library path to RPATH for Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(Qt6_FOUND)
        get_target_property(_qt6_core_location Qt6::Core LOCATION)
        get_filename_component(_qt6_lib_dir "${_qt6_core_location}" DIRECTORY)
        list(APPEND CMAKE_INSTALL_RPATH "${_qt6_lib_dir}")
        list(APPEND CMAKE_BUILD_RPATH "${_qt6_lib_dir}")
    endif()
endif()
qt_standard_project_setup()
