# The accepted source resources retain their original path and bytes. Build and
# installed products expose only the current application resource identity.
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/share/qualx/
     DESTINATION ${CMAKE_BINARY_DIR}/share/${EXECUTABLE_NAME})

# Install the executable into "${CMAKE_INSTALL_PREFIX}/bin
set(COMPONENT_NAME_MAIN "${PROJECT_NAME}")
install(TARGETS ${EXECUTABLE_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${COMPONENT_NAME_MAIN})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/share/qualx/ DESTINATION share/${EXECUTABLE_NAME} COMPONENT ${COMPONENT_NAME_MAIN})
# install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DB DESTINATION . COMPONENT ${COMPONENT_NAME_MAIN})

# PDF manual (generated file — build with 'bash docs/make_pdf.sh' first).
# The HTML documentation is no longer bundled: Help > Documentation (HTML)
# opens the version published on GitHub Pages instead.
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/docs/qualpf_manual.pdf
        DESTINATION share/${EXECUTABLE_NAME}/docs
        COMPONENT ${COMPONENT_NAME_MAIN}
        OPTIONAL)
