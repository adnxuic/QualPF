# The accepted source resources retain their original path and bytes. Build and
# installed products expose only the current application resource identity.
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/share/qualx/
     DESTINATION ${CMAKE_BINARY_DIR}/share/${EXECUTABLE_NAME})

# Install the executable into "${CMAKE_INSTALL_PREFIX}/bin
set(COMPONENT_NAME_MAIN "${PROJECT_NAME}")
install(TARGETS ${EXECUTABLE_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${COMPONENT_NAME_MAIN})
# Historical example rights are unresolved. Development keeps the originals;
# delivery carries only the required resource files, not those optional examples.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/share/qualx/files/
    DESTINATION share/${EXECUTABLE_NAME}/files COMPONENT ${COMPONENT_NAME_MAIN})
# install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/DB DESTINATION . COMPONENT ${COMPONENT_NAME_MAIN})

# Include a generated manual only when it matches the current documentation.
# An older local PDF remains untouched; HTML Help opens the current repository guide.
set(QUALPF_MANUAL_PROOF "${PROJECT_SOURCE_DIR}/build/documentation/qualpf_manual-provenance.json")
if(EXISTS "${PROJECT_SOURCE_DIR}/docs/qualpf_manual.pdf")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/validation/check_distribution.py" --root "${PROJECT_SOURCE_DIR}"
        --check-manual --manual-file "${PROJECT_SOURCE_DIR}/docs/qualpf_manual.pdf"
        --manual-proof "${QUALPF_MANUAL_PROOF}"
        RESULT_VARIABLE manual_status OUTPUT_QUIET ERROR_QUIET)
    if(manual_status EQUAL 0)
        install(FILES "${PROJECT_SOURCE_DIR}/docs/qualpf_manual.pdf"
            DESTINATION "share/${EXECUTABLE_NAME}/docs" COMPONENT "${COMPONENT_NAME_MAIN}")
        install(FILES "${QUALPF_MANUAL_PROOF}" DESTINATION "share/${EXECUTABLE_NAME}/docs"
            RENAME qualpf_manual.provenance.json COMPONENT "${COMPONENT_NAME_MAIN}")
    else()
        message(STATUS "Skipping local PDF manual without current source provenance; regenerate with docs/make_pdf.sh before configuration")
    endif()
endif()
