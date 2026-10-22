# Linux: Desktop integration (icons + .desktop file)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    configure_file(
        ${CMAKE_SOURCE_DIR}/deploy/linux/qualx.appimage.desktop
        ${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME}.appimage.desktop
        @ONLY
    )
    configure_file(
        ${CMAKE_SOURCE_DIR}/deploy/linux/qualx.desktop.in
        ${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME}.desktop
        @ONLY
    )
    # Resolve the actual installation prefix again at install time, including
    # cmake --install --prefix overrides; DESTDIR remains handled by CMake.
    install(CODE "
        set(PROJECT_NAME \"${PROJECT_NAME}\")
        set(PROJECT_VERSION \"${PROJECT_VERSION}\")
        set(EXECUTABLE_NAME \"${EXECUTABLE_NAME}\")
        set(QUALPF_DESCRIPTION \"${QUALPF_DESCRIPTION}\")
        configure_file(\"${CMAKE_SOURCE_DIR}/deploy/linux/qualx.desktop.in\"
                       \"${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME}.desktop\" @ONLY)
        file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/share/applications\"
             TYPE FILE FILES \"${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME}.desktop\")
        " COMPONENT ${COMPONENT_NAME_MAIN})
    foreach(SIZE 16 32 48 256)
        install(FILES ${CMAKE_SOURCE_DIR}/deploy/linux/icons/${SIZE}x${SIZE}/qualpf.png
                DESTINATION share/icons/hicolor/${SIZE}x${SIZE}/apps
                RENAME ${EXECUTABLE_NAME}.png COMPONENT ${COMPONENT_NAME_MAIN})
    endforeach()
    # Registering the app in a user's desktop/icon cache only makes sense
    # for a "make install" done directly on that user's machine — skip it
    # for AppImage builds, where installation happens into a throwaway
    # AppDir (often inside a container with no meaningful $HOME).
    if(NOT BUILD_APPIMAGE)
        file(WRITE ${CMAKE_BINARY_DIR}/install_desktop_linux.cmake "
            set(HOME \"\$ENV{HOME}\")
            set(APPS_DIR \"\${HOME}/.local/share/applications\")
            set(ICON_BASE \"\${HOME}/.local/share/icons/hicolor\")

            file(MAKE_DIRECTORY \"\${APPS_DIR}\")
            file(COPY \"${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME}.desktop\"
                 DESTINATION \"\${APPS_DIR}\")

            foreach(SIZE 16 32 48 256)
                set(SRC \"${CMAKE_SOURCE_DIR}/deploy/linux/icons/\${SIZE}x\${SIZE}/qualpf.png\")
                if(EXISTS \"\${SRC}\")
                    set(DEST \"\${ICON_BASE}/\${SIZE}x\${SIZE}/apps\")
                    file(MAKE_DIRECTORY \"\${DEST}\")
                    configure_file(\"\${SRC}\" \"\${DEST}/${EXECUTABLE_NAME}.png\" COPYONLY)
                endif()
            endforeach()

            execute_process(COMMAND gtk-update-icon-cache -f -t \"\${ICON_BASE}\" ERROR_QUIET)
            execute_process(COMMAND update-desktop-database \"\${APPS_DIR}\" ERROR_QUIET)
            message(STATUS \"Desktop integration installed for user \${HOME}\")
        ")
        install(SCRIPT ${CMAKE_BINARY_DIR}/install_desktop_linux.cmake
                COMPONENT ${COMPONENT_NAME_MAIN})
    endif()
endif()

# Linux: Install Qt libraries and plugins manually (skip for Debian package)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT BUILD_DEB_PACKAGE)
   if(Qt6_FOUND)
   get_target_property(_qt6_qmake_location Qt6::qmake IMPORTED_LOCATION)
   execute_process(
     COMMAND ${_qt6_qmake_location} -query QT_INSTALL_LIBS
     OUTPUT_VARIABLE QT6_LIB_DIR
     OUTPUT_STRIP_TRAILING_WHITESPACE
   )

   # Install Qt libraries to lib directory
   file(GLOB QT6_LIBS
     "${QT6_LIB_DIR}/libQt6*.so*"
     "${QT6_LIB_DIR}/libicu*.so*"
   )
   install(FILES ${QT6_LIBS}
     DESTINATION ${CMAKE_INSTALL_LIBDIR}
     COMPONENT ${COMPONENT_NAME_MAIN}
   )
    endif()
endif()
