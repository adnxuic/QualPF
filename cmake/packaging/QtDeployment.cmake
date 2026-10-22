# Qt deployment script (must be before install commands to allow overriding)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.5)
    set(_qualpf_deploy_options)
    if(MINGW AND Qt6_VERSION VERSION_GREATER_EQUAL 6.7)
        # The supported Windows platform supplies this system component. Do not
        # redistribute an older SDK copy with separate Microsoft SDK terms.
        list(APPEND _qualpf_deploy_options DEPLOY_TOOL_OPTIONS --no-system-d3d-compiler)
    endif()
    # Generate the deployment script for the target
    qt_generate_deploy_app_script(
        TARGET ${EXECUTABLE_NAME}
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
        ${_qualpf_deploy_options}
    )
endif()

# Execute Qt deployment script (called after custom installations)
# Skip for Debian package as dependencies are managed by apt
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.5 AND NOT BUILD_DEB_PACKAGE)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Match the portable layout written by install_qtconf.cmake below;
        # distribution Qt defaults can otherwise use multiarch plugin paths.
        install(CODE "
            set(QT_DEPLOY_PLUGINS_DIR \"plugins\")
            set(QT_DEPLOY_TRANSLATIONS_DIR \"translations\")
        ")
    endif()
    install(SCRIPT ${deploy_script})
    if(WIN32)
        # Product connections select QSQLITE. These optional SDK drivers have
        # no product owner or bundled client; retain all other deployed files.
        install(CODE "
            file(REMOVE
                \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/plugins/sqldrivers/qsqlmimer.dll\"
                \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/plugins/sqldrivers/qsqlmimerd.dll\"
                \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/plugins/sqldrivers/qsqlpsql.dll\"
                \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/plugins/sqldrivers/qsqlpsqld.dll\"
            )
        ")
    endif()
elseif(NOT BUILD_DEB_PACKAGE)
    # Manual plugin installation for Qt < 6.5 (Linux only)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Get Qt6 plugin directory using qmake query (most reliable method)
        if(Qt6_FOUND)
        get_target_property(_qt6_qmake_location Qt6::qmake IMPORTED_LOCATION)
        execute_process(
            COMMAND ${_qt6_qmake_location} -query QT_INSTALL_PLUGINS
            OUTPUT_VARIABLE QT6_PLUGINS_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        message(STATUS "Qt ${Qt6_VERSION} < 6.5: Installing plugins manually from ${QT6_PLUGINS_DIR}")
        endif()

        # Install essential platforms plugin (required to run the application)
        if(EXISTS "${QT6_PLUGINS_DIR}/platforms")
            install(DIRECTORY "${QT6_PLUGINS_DIR}/platforms"
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
                    COMPONENT ${COMPONENT_NAME_MAIN}
            )
        endif()

        # Install platformthemes for GNOME/GTK integration
        if(EXISTS "${QT6_PLUGINS_DIR}/platformthemes")
            install(DIRECTORY "${QT6_PLUGINS_DIR}/platformthemes"
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
                    COMPONENT ${COMPONENT_NAME_MAIN}
            )
        endif()

        # Install xcbglintegrations for X11/OpenGL support
        if(EXISTS "${QT6_PLUGINS_DIR}/xcbglintegrations")
            install(DIRECTORY "${QT6_PLUGINS_DIR}/xcbglintegrations"
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
                    COMPONENT ${COMPONENT_NAME_MAIN}
            )
        endif()

        # Install imageformats for image support
        if(EXISTS "${QT6_PLUGINS_DIR}/imageformats")
            install(DIRECTORY "${QT6_PLUGINS_DIR}/imageformats"
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
                    COMPONENT ${COMPONENT_NAME_MAIN}
            )
        endif()

        # Install sqldrivers (libqsqlite, ...) — required, qualx links Qt6::Sql
        if(EXISTS "${QT6_PLUGINS_DIR}/sqldrivers")
            install(DIRECTORY "${QT6_PLUGINS_DIR}/sqldrivers"
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
                    COMPONENT ${COMPONENT_NAME_MAIN}
            )
        endif()

        # Install platforminputcontexts for input method support
        if(EXISTS "${QT6_PLUGINS_DIR}/platforminputcontexts")
            install(DIRECTORY "${QT6_PLUGINS_DIR}/platforminputcontexts"
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
                    COMPONENT ${COMPONENT_NAME_MAIN}
            )
        endif()

        # Install wayland plugins if available (for Wayland support)
        if(EXISTS "${QT6_PLUGINS_DIR}/wayland-shell-integration")
            install(DIRECTORY "${QT6_PLUGINS_DIR}/wayland-shell-integration"
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
                    COMPONENT ${COMPONENT_NAME_MAIN}
            )
        endif()

        if(EXISTS "${QT6_PLUGINS_DIR}/wayland-decoration-client")
            install(DIRECTORY "${QT6_PLUGINS_DIR}/wayland-decoration-client"
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
                    COMPONENT ${COMPONENT_NAME_MAIN}
            )
        endif()

        if(EXISTS "${QT6_PLUGINS_DIR}/wayland-graphics-integration-client")
            install(DIRECTORY "${QT6_PLUGINS_DIR}/wayland-graphics-integration-client"
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
                    COMPONENT ${COMPONENT_NAME_MAIN}
            )
        endif()

        # Install Qt translations
        execute_process(
            COMMAND ${_qt6_qmake_location} -query QT_INSTALL_TRANSLATIONS
            OUTPUT_VARIABLE QT6_TRANSLATIONS_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(EXISTS "${QT6_TRANSLATIONS_DIR}")
            install(DIRECTORY "${QT6_TRANSLATIONS_DIR}/"
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/translations
                    COMPONENT ${COMPONENT_NAME_MAIN}
                    FILES_MATCHING PATTERN "*.qm"
            )
        endif()

        # The plugin .so files above are copied verbatim from the system Qt
        # install and keep their original RPATH (baked in for their real
        # system location, e.g. /usr/lib/x86_64-linux-gnu/qt6/plugins/<cat>/).
        # That path does not exist in a relocated/portable install, so at
        # runtime the loader silently falls back to system search paths and
        # can pick up a *different*, ABI-incompatible system Qt install
        # instead of our bundled one. Force their RPATH back to our lib dir.
        find_program(PATCHELF_EXECUTABLE patchelf)
        if(PATCHELF_EXECUTABLE)
            install(CODE "
                file(GLOB_RECURSE _qt_plugin_libs \"\${CMAKE_INSTALL_PREFIX}/plugins/*.so\")
                foreach(_lib \${_qt_plugin_libs})
                    execute_process(COMMAND ${PATCHELF_EXECUTABLE} --set-rpath \"\$ORIGIN/../../lib\" \"\${_lib}\")
                endforeach()
            " COMPONENT ${COMPONENT_NAME_MAIN})
        else()
            message(WARNING "patchelf not found: bundled Qt plugins keep their original system RPATH and may load an incompatible system Qt at runtime. Install patchelf for a portable/AppImage build.")
        endif()
    endif()
endif()

# Install Qt plugins for proper integration (Linux only) - Additional plugins for Qt >= 6.5
# Skip for Debian package
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND Qt6_VERSION VERSION_GREATER_EQUAL 6.5 AND NOT BUILD_DEB_PACKAGE)
    get_target_property(_qt6_qmake_location Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt6_install_prefix "${_qt6_qmake_location}" DIRECTORY)
    get_filename_component(_qt6_install_prefix "${_qt6_install_prefix}" DIRECTORY)
    set(QT6_PLUGINS_DIR "${_qt6_install_prefix}/plugins")

    # Install additional platformthemes for GNOME/GTK integration if not already done by deploy script
    if(EXISTS "${QT6_PLUGINS_DIR}/platformthemes")
        install(DIRECTORY "${QT6_PLUGINS_DIR}/platformthemes"
                DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
                COMPONENT ${COMPONENT_NAME_MAIN}
        )
    endif()
endif()

# Create a custom qt.conf that uses relative paths for portability, so the
# software works regardless of installation prefix (AppImage, /opt install, ...).
# Needed for every Qt6 version, not just >= 6.5: qt_generate_deploy_app_script()
# (which writes its own qt.conf) only exists from Qt 6.5 onward. Without this,
# Qt falls back to its compile-time plugin path, which does not exist outside
# of the machine (or container) the app was built on.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT BUILD_DEB_PACKAGE)
    file(WRITE ${CMAKE_BINARY_DIR}/install_qtconf.cmake
"# Determine relative paths based on installation structure
file(WRITE \"\${CMAKE_INSTALL_PREFIX}/bin/qt.conf\"
\"[Paths]
Prefix = ..
Libraries = lib
Plugins = plugins
Translations = translations
Data = share
\")
message(STATUS \"Installed qt.conf to \${CMAKE_INSTALL_PREFIX}/bin/qt.conf\")
message(STATUS \"Software installed to: \${CMAKE_INSTALL_PREFIX}\")
message(STATUS \"  Executable: \${CMAKE_INSTALL_PREFIX}/bin/${EXECUTABLE_NAME}\")
message(STATUS \"  Libraries: \${CMAKE_INSTALL_PREFIX}/lib\")
message(STATUS \"  Plugins: \${CMAKE_INSTALL_PREFIX}/plugins\")
")
    install(SCRIPT ${CMAKE_BINARY_DIR}/install_qtconf.cmake)
endif()

# Install after Qt deployment so a runtime found on another MinGW's PATH cannot
# replace the compiler-bound files. MSVC and Linux have no such target property.
get_target_property(_qualpf_mingw_runtime ${EXECUTABLE_NAME} QUALPF_MINGW_RUNTIME_FILES)
if(_qualpf_mingw_runtime)
    install(FILES ${_qualpf_mingw_runtime}
        DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT "${COMPONENT_NAME_MAIN}")
endif()

message("Qt6_VERSION: ${Qt6_VERSION}")
