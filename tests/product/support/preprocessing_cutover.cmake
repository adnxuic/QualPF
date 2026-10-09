# These commands clear all product backend / QA variables, even when the caller
# explicitly opts into a fallback. Historical tests above retain their provenance.
function(qualpf_default_acceptance name input output)
    add_test(NAME qualpf.cutover.default.${name}
        COMMAND "${CMAKE_COMMAND}" -E env --unset=QUALPF_PREPROCESSING_BACKEND
        --unset=QUALPF_CPP_PREPROCESSING_DIALOGS --unset=QUALPF_SEARCH_BACKEND
        $<TARGET_FILE:qualpf_product_probe> "${QUALPF_WORKFLOW_DATABASE}" "${input}"
        "${PROJECT_SOURCE_DIR}/share/qualx/files" "${output}" ${ARGN} --expect-default)
    set_tests_properties(qualpf.cutover.default.${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180 LABELS "cutover_default")
endfunction()
add_test(NAME qualpf.cutover.default.startup COMMAND "${CMAKE_COMMAND}" -E env
    --unset=QUALPF_PREPROCESSING_BACKEND --unset=QUALPF_CPP_PREPROCESSING_DIALOGS
    --unset=QUALPF_SEARCH_BACKEND $<TARGET_FILE:qualpf_product_probe> --startup --expect-default)
set_tests_properties(qualpf.cutover.default.startup PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30 LABELS "cutover_default")
if(QUALPF_WORKFLOW_DATABASE AND QUALPF_WORKFLOW_EXAMPLES)
    foreach(sample example1 CPD2 MIXT_5)
        set(input "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat")
        foreach(mode contract96 product94 formats95)
            qualpf_default_acceptance(${mode}.${sample} "${input}"
                "${CMAKE_CURRENT_BINARY_DIR}/cutover-${mode}-${sample}" --${mode})
        endforeach()
        if(NOT sample STREQUAL "CPD2")
            set(prefix "${CMAKE_CURRENT_BINARY_DIR}/cutover-finite-${sample}")
            qualpf_default_acceptance(finite.${sample} "${input}" "${prefix}" --finite96)
            set_tests_properties(qualpf.cutover.default.finite.${sample} PROPERTIES FIXTURES_SETUP cutover_${sample})
            qualpf_default_acceptance(display.${sample} "${prefix}-finite.qxp"
                "${prefix}-display" --manualfix96)
            set_tests_properties(qualpf.cutover.default.display.${sample} PROPERTIES FIXTURES_REQUIRED cutover_${sample})
            foreach(snapshot finite manual)
                qualpf_default_acceptance(reopen.${sample}.${snapshot} "${prefix}-${snapshot}.qxp"
                    "${prefix}-reopen-${snapshot}" --reopen96)
                set_tests_properties(qualpf.cutover.default.reopen.${sample}.${snapshot} PROPERTIES
                    FIXTURES_REQUIRED cutover_${sample})
                add_test(NAME qualpf.cutover.legacy.reopen.${sample}.${snapshot}
                    COMMAND "${CMAKE_COMMAND}" -E env QUALPF_PREPROCESSING_BACKEND=legacy
                    --unset=QUALPF_CPP_PREPROCESSING_DIALOGS --unset=QUALPF_SEARCH_BACKEND
                    $<TARGET_FILE:qualpf_product_probe> "${QUALPF_WORKFLOW_DATABASE}" "${prefix}-${snapshot}.qxp"
                    "${PROJECT_SOURCE_DIR}/share/qualx/files" "${prefix}-legacy-${snapshot}" --count96)
                set_tests_properties(qualpf.cutover.legacy.reopen.${sample}.${snapshot} PROPERTIES
                    ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180 LABELS "cutover_legacy"
                    FIXTURES_REQUIRED cutover_${sample})
            endforeach()
        endif()
    endforeach()
    qualpf_default_acceptance(gui_defaults.example1 "${QUALPF_WORKFLOW_EXAMPLES}/example1.dat"
        "${CMAKE_CURRENT_BINARY_DIR}/cutover-gui-defaults-example1" --finite-default96)
endif()
if(QUALPF_WORKFLOW_DATABASE AND QUALPF_WORKFLOW_PROJECTS)
    foreach(sample example1 CPD2 MIXT_5)
        qualpf_default_acceptance(project.${sample} "${QUALPF_WORKFLOW_PROJECTS}/${sample}.qxp"
            "${CMAKE_CURRENT_BINARY_DIR}/cutover-project-${sample}")
    endforeach()
endif()
