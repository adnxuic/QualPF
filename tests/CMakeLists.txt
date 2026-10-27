# Standalone use is intentionally CXX-only; it never enters the product root's
# Qt product configuration. Neither mode fetches dependencies or enables tests by default.
cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(QualpfContracts LANGUAGES CXX)
    add_subdirectory(../src/core "${CMAKE_CURRENT_BINARY_DIR}/core")
    add_subdirectory(../src/application/session "${CMAKE_CURRENT_BINARY_DIR}/session")
endif()


if(QUALPF_BUILD_PRODUCT_TESTS AND TARGET qualpf_infrastructure)
    add_library(qualpf_legacy_project_test_support STATIC
        support/legacy_qxp/project_codec.cpp support/legacy_qxp/project_v3.cpp support/legacy_qxp/legacy_ports.cpp)
    qualpf_configure_qt_boundary(qualpf_legacy_project_test_support Core Sql Network)
    target_link_libraries(qualpf_legacy_project_test_support PRIVATE qualpf_infrastructure qualpf_application)
    target_include_directories(qualpf_legacy_project_test_support PRIVATE "${PROJECT_SOURCE_DIR}/src")
endif()

if(QUALPF_BUILD_PRODUCT_TESTS OR QUALPF_BUILD_CONTRACT_TESTS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    if(TARGET qualpf_pfsm_benchmark)
        add_test(NAME qualpf.cli.pfsm_benchmark COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/product/cli/pfsm_benchmark.py"
            --runner $<TARGET_FILE:qualpf_pfsm_benchmark> --replay-runner $<TARGET_FILE:qualpf_pfsm_candidate_recall_replay>)
        add_test(NAME qualpf.pfsm.input_adapters COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/unit/tools/pfsm_prepare_test.py")
        add_test(NAME qualpf.pfsm.fast_comparator COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/unit/tools/pfsm_fast_compare_test.py")
        add_test(NAME qualpf.pfsm.fast_validation COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/unit/tools/pfsm_fast_validation_test.py")
    endif()
    add_executable(qualpf_card_replay compatibility/card_replay.cpp)
    target_link_libraries(qualpf_card_replay PRIVATE qualpf_core)
    set_target_properties(qualpf_card_replay PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_executable(qualpf_card_properties unit/core/card_properties.cpp)
    target_link_libraries(qualpf_card_properties PRIVATE qualpf_core)
    set_target_properties(qualpf_card_properties PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.card.properties COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/card_properties.py"
        --runner $<TARGET_FILE:qualpf_card_properties>
        --output "${CMAKE_CURRENT_BINARY_DIR}/card-properties.json")
    add_test(NAME qualpf.card.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/current_transcendental_reference.py" --domain card
        --runner $<TARGET_FILE:qualpf_card_replay> --compiler "${CMAKE_CXX_COMPILER_ID}"
        --output "${CMAKE_CURRENT_BINARY_DIR}/card-differential.json")
    add_test(NAME qualpf.card.acceptance_gate COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/card_acceptance_test.py")
    add_test(NAME qualpf.card.chemistry_registry COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/crystallography/generate_chemistry.py" --check)
    add_executable(qualpf_powder_replay compatibility/powder_replay.cpp)
    target_link_libraries(qualpf_powder_replay PRIVATE qualpf_core)
    set_target_properties(qualpf_powder_replay PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_executable(qualpf_powder_properties unit/core/powder_properties.cpp)
    target_link_libraries(qualpf_powder_properties PRIVATE qualpf_core)
    set_target_properties(qualpf_powder_properties PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.powder.properties COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/powder_properties.py"
        --runner $<TARGET_FILE:qualpf_powder_properties>
        --output "${CMAKE_CURRENT_BINARY_DIR}/powder-properties.json")
    add_test(NAME qualpf.powder.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/current_transcendental_reference.py" --domain powder
        --runner $<TARGET_FILE:qualpf_powder_replay> --compiler "${CMAKE_CXX_COMPILER_ID}"
        --output "${CMAKE_CURRENT_BINARY_DIR}/powder-differential.json")
    add_test(NAME qualpf.powder.protocol_integrity COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/powder_protocol_test.py")
    add_executable(qualpf_scattering_replay compatibility/scattering_replay.cpp)
    target_link_libraries(qualpf_scattering_replay PRIVATE qualpf_core)
    set_target_properties(qualpf_scattering_replay PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_executable(qualpf_scattering_properties unit/core/scattering_properties.cpp)
    target_link_libraries(qualpf_scattering_properties PRIVATE qualpf_core)
    set_target_properties(qualpf_scattering_properties PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME qualpf.scattering.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/current_transcendental_reference.py" --domain scattering
        --runner $<TARGET_FILE:qualpf_scattering_replay> --compiler "${CMAKE_CXX_COMPILER_ID}"
        --output "${CMAKE_CURRENT_BINARY_DIR}/scattering-differential.json")
    add_test(NAME qualpf.scattering.properties COMMAND qualpf_scattering_properties
        "${CMAKE_CURRENT_BINARY_DIR}/scattering-properties.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/phase-sincos-reference.txt")
    add_test(NAME qualpf.scattering.protocol_integrity COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/scattering_protocol_test.py")
    add_test(NAME qualpf.scattering.table_reproducible COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/crystallography/generate_scattering.py" --check)
    add_executable(qualpf_reflection_replay compatibility/reflection_replay.cpp)
    target_link_libraries(qualpf_reflection_replay PRIVATE qualpf_core)
    set_target_properties(qualpf_reflection_replay PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_executable(qualpf_reflection_properties unit/core/reflection_properties.cpp)
    target_link_libraries(qualpf_reflection_properties PRIVATE qualpf_core)
    set_target_properties(qualpf_reflection_properties PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME qualpf.reflection.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/current_transcendental_reference.py" --domain reflection
        --compiler "${CMAKE_CXX_COMPILER_ID}"
        --runner $<TARGET_FILE:qualpf_reflection_replay>
        --output "${CMAKE_CURRENT_BINARY_DIR}/reflection-differential.json")
    add_test(NAME qualpf.reflection.properties COMMAND qualpf_reflection_properties
        "${CMAKE_CURRENT_BINARY_DIR}/reflection-properties.json")
    add_executable(qualpf_symmetry_replay compatibility/symmetry_replay.cpp)
    add_executable(qualpf_symmetry_properties unit/core/symmetry_properties.cpp)
    target_link_libraries(qualpf_symmetry_replay PRIVATE qualpf_core)
    target_link_libraries(qualpf_symmetry_properties PRIVATE qualpf_core)
    set_target_properties(qualpf_symmetry_replay qualpf_symmetry_properties
        PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME qualpf.symmetry.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/symmetry_differential.py"
        --runner $<TARGET_FILE:qualpf_symmetry_replay>
        --output "${CMAKE_CURRENT_BINARY_DIR}/symmetry-differential.json")
    add_test(NAME qualpf.symmetry.properties COMMAND qualpf_symmetry_properties
        "${CMAKE_CURRENT_BINARY_DIR}/symmetry-properties.json")
    add_test(NAME qualpf.symmetry.table_reproducible COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/crystallography/generate_space_groups.py" --check)
    add_executable(qualpf_crystallography_replay compatibility/crystallography_replay.cpp)
    target_link_libraries(qualpf_crystallography_replay PRIVATE qualpf_core)
    add_executable(qualpf_crystallography_properties unit/core/crystallography_properties.cpp)
    target_link_libraries(qualpf_crystallography_properties PRIVATE qualpf_core)
    set_target_properties(qualpf_crystallography_replay qualpf_crystallography_properties
        PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME qualpf.crystallography.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/crystallography_differential.py"
        --runner $<TARGET_FILE:qualpf_crystallography_replay>
        --output "${CMAKE_CURRENT_BINARY_DIR}/crystallography-differential.json"
        --errors "${CMAKE_CURRENT_BINARY_DIR}/crystallography-errors.json")
    add_test(NAME qualpf.crystallography.properties COMMAND qualpf_crystallography_properties
        "${CMAKE_CURRENT_BINARY_DIR}/crystallography-properties.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/unit-cell-formed-angle-reference.txt")
    add_test(NAME qualpf.math.current_reference_protocol COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/current_transcendental_reference_test.py")
    add_executable(qualpf_rounded_transcendentals_properties unit/core/rounded_transcendentals_properties.cpp)
    target_link_libraries(qualpf_rounded_transcendentals_properties PRIVATE qualpf_core)
    set_target_properties(qualpf_rounded_transcendentals_properties PROPERTIES
        AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.math.rounded_transcendentals COMMAND qualpf_rounded_transcendentals_properties
        "${CMAKE_CURRENT_SOURCE_DIR}/data/rounded-transcendentals-reference.txt")
    add_executable(qualpf_pattern_replay compatibility/pattern_replay.cpp)
    target_include_directories(qualpf_pattern_replay PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
    target_link_libraries(qualpf_pattern_replay PRIVATE qualpf_core qualpf_pattern_file)
    set_target_properties(qualpf_pattern_replay PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME qualpf.pattern.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/current_pattern_reference.py"
        --runner $<TARGET_FILE:qualpf_pattern_replay>
        --output "${CMAKE_CURRENT_BINARY_DIR}/pattern-differential.json")
    add_test(NAME qualpf.pattern.abi_reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/current_pattern_reference.py"
        --runner $<TARGET_FILE:qualpf_pattern_replay>
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/data/pattern-abi-reference-manifest.json"
        --output "${CMAKE_CURRENT_BINARY_DIR}/pattern-abi-differential.json")
    add_test(NAME qualpf.pattern.vendor_reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/current_pattern_reference.py"
        --runner $<TARGET_FILE:qualpf_pattern_replay>
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/data/pattern-vendor-reference-manifest.json"
        --output "${CMAKE_CURRENT_BINARY_DIR}/pattern-vendor-differential.json")
    add_test(NAME qualpf.pattern.edge_reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/current_pattern_reference.py"
        --runner $<TARGET_FILE:qualpf_pattern_replay>
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/data/pattern-edge-reference-manifest.json"
        --output "${CMAKE_CURRENT_BINARY_DIR}/pattern-edge-differential.json")
endif()

if(QUALPF_BUILD_PRODUCT_TESTS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME qualpf.distribution.consistency COMMAND "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/validation/check_distribution.py" --self-test)
    add_test(NAME qualpf.distribution.dependency_bundle COMMAND "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/validation/prepare_dependency_bundle.py" --self-test)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        message(FATAL_ERROR "Product tests require the complete root Qt build")
    endif()
    qt_add_resources(QUALPF_ABOUT_TEST_RESOURCES
        "${PROJECT_SOURCE_DIR}/src/gui/resources/resource.qrc"
        "${CMAKE_BINARY_DIR}/configured_files/legal.qrc")
    add_executable(qualpf_about_dialog_tests integration/gui/aboutdialog_test.cpp
        "${PROJECT_SOURCE_DIR}/src/gui/dialogs/aboutdialog.cpp"
        "${PROJECT_SOURCE_DIR}/src/gui/dialogs/aboutdialog.h" ${QUALPF_ABOUT_TEST_RESOURCES})
    qualpf_configure_qt_boundary(qualpf_about_dialog_tests Widgets)
    target_compile_definitions(qualpf_about_dialog_tests PRIVATE
        QUALPF_ABOUT_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
    if(WIN32)
        set(qualpf_about_platform windows)
    else()
        set(qualpf_about_platform offscreen)
    endif()
    foreach(scale 100 150 200)
        if(scale EQUAL 150)
            set(scale_factor 1.5)
        elseif(scale EQUAL 200)
            set(scale_factor 2)
        else()
            set(scale_factor 1)
        endif()
        add_test(NAME qualpf.gui.about.scale${scale} COMMAND qualpf_about_dialog_tests
            "${CMAKE_CURRENT_BINARY_DIR}/distribution/about-${scale}.png")
        set_tests_properties(qualpf.gui.about.scale${scale} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=${qualpf_about_platform};QT_SCALE_FACTOR=${scale_factor}" TIMEOUT 30)
    endforeach()
    add_executable(qualpf_product_probe product/workflow/probe.cpp)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_sources(qualpf_product_probe PRIVATE product/support/pattern_link_guard.cpp)
        target_compile_definitions(qualpf_product_probe PRIVATE QUALPF_PATTERN_LINK_GUARD)
        target_link_options(qualpf_product_probe PRIVATE "-Wl,--wrap=open_diffraction_patt")
        target_link_options(qualpf_product_probe PRIVATE "-Wl,--wrap=legacy_kalpha2_values")
        target_link_options(qualpf_product_probe PRIVATE "-Wl,--wrap=kalpha2_stripping")
    endif()
    add_executable(qualpf_undefined_fom_contract product/support/undefined_fom_contract.cpp)
    qualpf_configure_qt_boundary(qualpf_undefined_fom_contract Widgets Sql PrintSupport Network)
    target_link_libraries(qualpf_undefined_fom_contract PRIVATE
        qualpf_core qualpf_application qualpf_infrastructure qualpf_gui qualpf_qcustomplot
        qualpf_gui_reference  )
    if(WIN32)
        target_link_libraries(qualpf_undefined_fom_contract PRIVATE opengl32)
    endif()
    add_test(NAME qualpf.classic.undefined_product COMMAND qualpf_undefined_fom_contract)
    qualpf_configure_qt_boundary(qualpf_product_probe Widgets Sql PrintSupport Network)
    target_link_libraries(qualpf_product_probe PRIVATE
        qualpf_core qualpf_application qualpf_infrastructure qualpf_gui
        qualpf_qcustomplot qualpf_gui_reference  )
    if(WIN32)
        target_link_libraries(qualpf_product_probe PRIVATE opengl32)
    endif()
    add_test(NAME qualpf.project.codec COMMAND qualpf_product_probe --codec)
    add_test(NAME qualpf.product.startup COMMAND qualpf_product_probe --startup)
    add_test(NAME qualpf.pdf2.lifecycle COMMAND qualpf_product_probe --pdf2-lifecycle
        "${CMAKE_CURRENT_BINARY_DIR}/pdf2-lifecycle.json")
    set_tests_properties(qualpf.pdf2.lifecycle PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    add_test(NAME qualpf.pattern.lifecycle COMMAND qualpf_product_probe --pattern-lifecycle
        "${PROJECT_SOURCE_DIR}/share/qualx/files" "${CMAKE_CURRENT_BINARY_DIR}/pattern-lifecycle.json")
    set_tests_properties(qualpf.pattern.lifecycle PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    foreach(backend cpp legacy)
        add_test(NAME qualpf.kalpha2.lifecycle.${backend} COMMAND "${CMAKE_COMMAND}" -E env
            --unset=QUALPF_PREPROCESSING_BACKEND --unset=QUALPF_SEARCH_BACKEND --unset=QUALPF_CPP_PREPROCESSING_DIALOGS
            QUALPF_KALPHA2_BACKEND=${backend} $<TARGET_FILE:qualpf_product_probe> --kalpha2-lifecycle
            "${PROJECT_SOURCE_DIR}/share/qualx/files" "${CMAKE_CURRENT_BINARY_DIR}/kalpha2-lifecycle-${backend}.json")
        set_tests_properties(qualpf.kalpha2.lifecycle.${backend} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    endforeach()
    set_tests_properties(qualpf.project.codec qualpf.product.startup PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
    set(QUALPF_WORKFLOW_DATABASE "" CACHE FILEPATH "Provenanced four-file database prefix for product acceptance")
    set(QUALPF_WORKFLOW_EXAMPLES "" CACHE PATH "Real measured example directory for product acceptance")
    if(QUALPF_WORKFLOW_DATABASE AND QUALPF_WORKFLOW_EXAMPLES)
        foreach(sample example1 CPD2 MIXT_5)
            add_test(NAME qualpf.kalpha2.workflow.${sample} COMMAND "${CMAKE_COMMAND}" -E env
                --unset=QUALPF_KALPHA2_BACKEND --unset=QUALPF_PREPROCESSING_BACKEND
                --unset=QUALPF_SEARCH_BACKEND --unset=QUALPF_CPP_PREPROCESSING_DIALOGS
                $<TARGET_FILE:qualpf_product_probe> "${QUALPF_WORKFLOW_DATABASE}"
                "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat" "${PROJECT_SOURCE_DIR}/share/qualx/files"
                "${CMAKE_CURRENT_BINARY_DIR}/kalpha2-${sample}" --kalpha2-workflow)
            set_tests_properties(qualpf.kalpha2.workflow.${sample} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                TIMEOUT 180 FIXTURES_SETUP kalpha2_${sample})
            add_test(NAME qualpf.kalpha2.reopen.${sample} COMMAND "${CMAKE_COMMAND}" -E env
                --unset=QUALPF_PREPROCESSING_BACKEND --unset=QUALPF_SEARCH_BACKEND --unset=QUALPF_CPP_PREPROCESSING_DIALOGS
                $<TARGET_FILE:qualpf_product_probe> "${QUALPF_WORKFLOW_DATABASE}"
                "${CMAKE_CURRENT_BINARY_DIR}/kalpha2-${sample}.qxp" "${PROJECT_SOURCE_DIR}/share/qualx/files"
                "${CMAKE_CURRENT_BINARY_DIR}/kalpha2-reopen-${sample}" --reopen96)
            set_tests_properties(qualpf.kalpha2.reopen.${sample} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                TIMEOUT 180 FIXTURES_REQUIRED kalpha2_${sample})
            add_test(NAME qualpf.radiation.cli.${sample} COMMAND qualpf_product_probe
                "${QUALPF_WORKFLOW_DATABASE}" "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat"
                "${PROJECT_SOURCE_DIR}/share/qualx/files" "${CMAKE_CURRENT_BINARY_DIR}/radiation-${sample}" --radiation-cli)
            set_tests_properties(qualpf.radiation.cli.${sample} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
            add_test(NAME qualpf.product.cpp96.undefined.${sample}
                COMMAND "${CMAKE_COMMAND}" -E env --unset=QUALPF_CPP_PREPROCESSING_DIALOGS
                --unset=QUALPF_SEARCH_BACKEND QUALPF_PREPROCESSING_BACKEND=cpp
                $<TARGET_FILE:qualpf_product_probe>
                "${QUALPF_WORKFLOW_DATABASE}" "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat"
                "${PROJECT_SOURCE_DIR}/share/qualx/files" "${CMAKE_CURRENT_BINARY_DIR}/contract96-${sample}" --contract96)
            set_tests_properties(qualpf.product.cpp96.undefined.${sample} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90 LABELS "product_contract;undefined_preprocessing")
            if(NOT sample STREQUAL "CPD2")
                if(sample STREQUAL "example1")
                    add_test(NAME qualpf.product.gui_defaults96.example1
                        COMMAND "${CMAKE_COMMAND}" -E env --unset=QUALPF_CPP_PREPROCESSING_DIALOGS
                        --unset=QUALPF_SEARCH_BACKEND QUALPF_PREPROCESSING_BACKEND=cpp
                        $<TARGET_FILE:qualpf_product_probe>
                        "${QUALPF_WORKFLOW_DATABASE}" "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat"
                        "${PROJECT_SOURCE_DIR}/share/qualx/files"
                        "${CMAKE_CURRENT_BINARY_DIR}/gui-defaults96-example1" --finite-default96)
                    set_tests_properties(qualpf.product.gui_defaults96.example1 PROPERTIES
                        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180 LABELS "search_tolerance_regression;finite_e2e")
                endif()
                set(finite96 "${CMAKE_CURRENT_BINARY_DIR}/finite96-${sample}")
                add_test(NAME qualpf.product.cpp96.finite.${sample}
                    COMMAND "${CMAKE_COMMAND}" -E env --unset=QUALPF_CPP_PREPROCESSING_DIALOGS
                    --unset=QUALPF_SEARCH_BACKEND QUALPF_PREPROCESSING_BACKEND=cpp
                    $<TARGET_FILE:qualpf_product_probe>
                    "${QUALPF_WORKFLOW_DATABASE}" "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat"
                    "${PROJECT_SOURCE_DIR}/share/qualx/files" "${finite96}" --finite96)
                set_tests_properties(qualpf.product.cpp96.finite.${sample} PROPERTIES
                    ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180 LABELS "product_contract;finite_e2e"
                    FIXTURES_SETUP finite96_${sample})
                foreach(snapshot finite manual)
                    add_test(NAME qualpf.product.cpp96.reopen.${sample}.${snapshot}
                        COMMAND "${CMAKE_COMMAND}" -E env --unset=QUALPF_CPP_PREPROCESSING_DIALOGS
                        --unset=QUALPF_SEARCH_BACKEND QUALPF_PREPROCESSING_BACKEND=cpp
                        $<TARGET_FILE:qualpf_product_probe>
                        "${QUALPF_WORKFLOW_DATABASE}" "${finite96}-${snapshot}.qxp"
                        "${PROJECT_SOURCE_DIR}/share/qualx/files" "${finite96}-reopen-${snapshot}" --reopen96)
                    set_tests_properties(qualpf.product.cpp96.reopen.${sample}.${snapshot} PROPERTIES
                        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90 LABELS "product_contract;project_reopen"
                        FIXTURES_REQUIRED finite96_${sample})
                endforeach()
                foreach(backend cpp legacy)
                    add_test(NAME qualpf.product.tolerance96.${sample}.${backend}
                        COMMAND "${CMAKE_COMMAND}" -E env --unset=QUALPF_CPP_PREPROCESSING_DIALOGS
                        QUALPF_SEARCH_BACKEND=${backend} QUALPF_PREPROCESSING_BACKEND=cpp
                        $<TARGET_FILE:qualpf_product_probe>
                        "${QUALPF_WORKFLOW_DATABASE}" "${finite96}-finite.qxp"
                        "${PROJECT_SOURCE_DIR}/share/qualx/files"
                        "${CMAKE_CURRENT_BINARY_DIR}/tolerance96-${sample}-${backend}" --tolerance96)
                    set_tests_properties(qualpf.product.tolerance96.${sample}.${backend} PROPERTIES
                        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180 LABELS "search_tolerance_regression"
                        FIXTURES_REQUIRED finite96_${sample})
                    add_test(NAME qualpf.product.manual96.${sample}.${backend}
                        COMMAND "${CMAKE_COMMAND}" -E env --unset=QUALPF_CPP_PREPROCESSING_DIALOGS
                        --unset=QUALPF_SEARCH_BACKEND QUALPF_PREPROCESSING_BACKEND=${backend}
                        $<TARGET_FILE:qualpf_product_probe>
                        "${QUALPF_WORKFLOW_DATABASE}" "${finite96}-finite.qxp"
                        "${PROJECT_SOURCE_DIR}/share/qualx/files"
                        "${CMAKE_CURRENT_BINARY_DIR}/manual96-${sample}-${backend}" --manualfix96)
                    set_tests_properties(qualpf.product.manual96.${sample}.${backend} PROPERTIES
                        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90 LABELS "manual_acceptance_regression"
                        FIXTURES_REQUIRED finite96_${sample})
                endforeach()
            endif()
            add_test(NAME qualpf.preprocessing.formats95.${sample} COMMAND qualpf_product_probe
                "${QUALPF_WORKFLOW_DATABASE}" "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat"
                "${PROJECT_SOURCE_DIR}/share/qualx/files" "${CMAKE_CURRENT_BINARY_DIR}/formats95-${sample}" --formats95)
            set_tests_properties(qualpf.preprocessing.formats95.${sample} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
            add_test(NAME qualpf.preprocessing.product94.${sample} COMMAND qualpf_product_probe
                "${QUALPF_WORKFLOW_DATABASE}" "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat"
                "${PROJECT_SOURCE_DIR}/share/qualx/files" "${CMAKE_CURRENT_BINARY_DIR}/product94-${sample}" --product94)
            set_tests_properties(qualpf.preprocessing.product94.${sample} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QUALPF_PREPROCESSING_BACKEND=legacy" TIMEOUT 90)
            add_test(NAME qualpf.preprocessing.product93.${sample} COMMAND qualpf_product_probe
                "${QUALPF_WORKFLOW_DATABASE}" "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat"
                "${PROJECT_SOURCE_DIR}/share/qualx/files" "${CMAKE_CURRENT_BINARY_DIR}/product93-${sample}" --product93)
            set_tests_properties(qualpf.preprocessing.product93.${sample} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QUALPF_PREPROCESSING_BACKEND=legacy" TIMEOUT 90)
            add_test(NAME qualpf.preprocessing.cpp_dialogs.${sample} COMMAND qualpf_product_probe
                "${QUALPF_WORKFLOW_DATABASE}" "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat"
                "${PROJECT_SOURCE_DIR}/share/qualx/files" "${CMAKE_CURRENT_BINARY_DIR}/cpp-preprocessing-${sample}" --preprocessing)
            set_tests_properties(qualpf.preprocessing.cpp_dialogs.${sample} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QUALPF_PREPROCESSING_BACKEND=qa;QUALPF_CPP_PREPROCESSING_DIALOGS=1" TIMEOUT 90)
            add_test(NAME qualpf.preprocessing.workflow.${sample} COMMAND qualpf_product_probe
                "${QUALPF_WORKFLOW_DATABASE}" "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat"
                "${PROJECT_SOURCE_DIR}/share/qualx/files" "${CMAKE_CURRENT_BINARY_DIR}/preprocessing-${sample}" --preprocessing)
            set_tests_properties(qualpf.preprocessing.workflow.${sample} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QUALPF_PREPROCESSING_BACKEND=legacy" TIMEOUT 90)
            # Preserve the original runner/assertions as explicitly Legacy historical evidence.
            # Correct atomic rejection is tested above; it is not a failed C++ product contract.
            add_test(NAME qualpf.legacy_historical.${sample} COMMAND "${CMAKE_COMMAND}" -E env
                --unset=QUALPF_CPP_PREPROCESSING_DIALOGS QUALPF_PREPROCESSING_BACKEND=legacy
                $<TARGET_FILE:qualpf_product_probe>
                "${QUALPF_WORKFLOW_DATABASE}" "${QUALPF_WORKFLOW_EXAMPLES}/${sample}.dat"
                "${PROJECT_SOURCE_DIR}/share/qualx/files" "${CMAKE_CURRENT_BINARY_DIR}/${sample}")
            set_tests_properties(qualpf.legacy_historical.${sample} PROPERTIES
                # Exhaustive serial no-strongest evaluation of the real 88k-card database.
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 900 LABELS "legacy_historical")
        endforeach()
    endif()
    set(QUALPF_WORKFLOW_PROJECTS "" CACHE PATH "User-supplied QXP project directory for compatibility validation")
    if(QUALPF_WORKFLOW_DATABASE AND QUALPF_WORKFLOW_PROJECTS)
        foreach(sample example1 CPD2 MIXT_5)
            add_test(NAME qualpf.product.project.${sample} COMMAND qualpf_product_probe
                "${QUALPF_WORKFLOW_DATABASE}" "${QUALPF_WORKFLOW_PROJECTS}/${sample}.qxp"
                "${PROJECT_SOURCE_DIR}/share/qualx/files" "${CMAKE_CURRENT_BINARY_DIR}/supplied-${sample}")
            set_tests_properties(qualpf.product.project.${sample} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
        endforeach()
    endif()
endif()

if(QUALPF_BUILD_PRODUCT_TESTS)
    include(product/support/preprocessing_cutover.cmake)
endif()

option(QUALPF_BUILD_CONTRACT_TESTS "Build model/state/transport contract tests" OFF)
option(QUALPF_BUILD_NUMERICS_TESTS "Build numerical foundation tests" OFF)
option(QUALPF_BUILD_CLASSIC_SEARCH_TESTS "Build classic Search-Match compatibility tests" OFF)
option(QUALPF_BUILD_SEARCH_INTEGRATION_TESTS "Build isolated application search integration tests" OFF)
option(QUALPF_BUILD_PREPROCESSING_TESTS "Build S9 offline preprocessing differential tests" OFF)
if(QUALPF_BUILD_NUMERICS_TESTS OR QUALPF_BUILD_PRODUCT_TESTS)
    enable_testing()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_executable(qualpf_numerical_replay compatibility/numerical_replay.cpp)
    target_link_libraries(qualpf_numerical_replay PRIVATE qualpf_core)
    set_target_properties(qualpf_numerical_replay PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.numerics.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/numerical_reference.py"
        --runner $<TARGET_FILE:qualpf_numerical_replay>
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/data/numerical-fortran/manifest.json"
        --report "${CMAKE_CURRENT_BINARY_DIR}/numerical-differential.json")
endif()
if(QUALPF_BUILD_PREPROCESSING_TESTS)
    add_executable(qualpf_kalpha2_replay compatibility/kalpha2_replay.cpp)
    target_link_libraries(qualpf_kalpha2_replay PRIVATE qualpf_core)
    set_target_properties(qualpf_kalpha2_replay PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME qualpf.kalpha2.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/kalpha2_compare.py"
        --runner $<TARGET_FILE:qualpf_kalpha2_replay>
        --report "${CMAKE_CURRENT_BINARY_DIR}/kalpha2-differential.json")
    enable_testing()
    add_executable(qualpf_preprocessing_replay compatibility/preprocessing_replay.cpp)
    target_link_libraries(qualpf_preprocessing_replay PRIVATE qualpf_core)
    add_executable(qualpf_preprocessing_rounding compatibility/preprocessing_rounding.cpp)
    target_compile_features(qualpf_preprocessing_rounding PRIVATE cxx_std_20)
    set_target_properties(qualpf_preprocessing_rounding PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    if(MSVC)
        target_compile_options(qualpf_preprocessing_rounding PRIVATE /fp:strict)
    else()
        target_compile_options(qualpf_preprocessing_rounding PRIVATE -fno-fast-math -ffp-contract=off)
    endif()
    set_target_properties(qualpf_preprocessing_replay PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME qualpf.preprocessing.product_reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/preprocessing_product_reference.py"
        --runner $<TARGET_FILE:qualpf_preprocessing_replay>
        --report "${CMAKE_CURRENT_BINARY_DIR}/preprocessing-product-differential.json")
    add_test(NAME qualpf.preprocessing.rounding COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/preprocessing_rounding_test.py"
        --runner $<TARGET_FILE:qualpf_preprocessing_rounding>)
    add_test(NAME qualpf.preprocessing.acceptance_gate COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/preprocessing_compare_test.py")
    add_test(NAME qualpf.preprocessing.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/preprocessing_source_snapshot.py"
        --runner $<TARGET_FILE:qualpf_preprocessing_replay>
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/data/preprocessing-fortran/manifest.json"
        --report "${CMAKE_CURRENT_BINARY_DIR}/preprocessing-differential.json")
    add_test(NAME qualpf.preprocessing.export.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/preprocessing_source_snapshot.py"
        --runner $<TARGET_FILE:qualpf_preprocessing_replay>
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/data/preprocessing-export-fortran/manifest.json"
        --report "${CMAKE_CURRENT_BINARY_DIR}/preprocessing-export-differential.json")
endif()
if(QUALPF_BUILD_CONTRACT_TESTS OR QUALPF_BUILD_NUMERICS_TESTS OR QUALPF_BUILD_CLASSIC_SEARCH_TESTS OR QUALPF_BUILD_SEARCH_INTEGRATION_TESTS)
    find_package(GTest CONFIG REQUIRED)
    enable_testing()
endif()

if(QUALPF_BUILD_CLASSIC_SEARCH_TESTS)
    add_executable(qualpf_classic_tests
        unit/core/classic_score_test.cpp
        unit/core/classic_candidates_test.cpp
        unit/core/classic_residual_test.cpp
        compatibility/classic_regression_test.cpp
        support/classic_fixture.h
        support/classic_observation.h
    )
    target_include_directories(qualpf_classic_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
    target_link_libraries(qualpf_classic_tests PRIVATE qualpf_core GTest::gtest_main)
    set_target_properties(qualpf_classic_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.classic COMMAND qualpf_classic_tests)
endif()
if(QUALPF_BUILD_CLASSIC_SEARCH_TESTS OR QUALPF_BUILD_PRODUCT_TESTS)
    add_executable(qualpf_classic_replay compatibility/classic_replay.cpp)
    target_include_directories(qualpf_classic_replay PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
    target_link_libraries(qualpf_classic_replay PRIVATE qualpf_core)
    set_target_properties(qualpf_classic_replay PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME qualpf.classic.synthetic_replay COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/support/classic_reference.py"
        --runner $<TARGET_FILE:qualpf_classic_replay>
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/data/classic-synthetic/manifest.json" --synthetic)
    set(QUALPF_CLASSIC_REFERENCE_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/data/classic-reference-manifest.json"
        CACHE FILEPATH "Reviewed external classic reference manifest")
    add_test(NAME qualpf.classic.reference COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/support/classic_reference.py"
        --runner $<TARGET_FILE:qualpf_classic_replay> --manifest "${QUALPF_CLASSIC_REFERENCE_MANIFEST}")
    set_tests_properties(qualpf.classic.reference PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME qualpf.classic.reference_contract COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/classic_reference_test.py" $<TARGET_FILE:qualpf_classic_replay>)
    add_test(NAME qualpf.classic.fortran_capture COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/support/classic_reference.py"
        --runner $<TARGET_FILE:qualpf_classic_replay>
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/data/classic-fortran/manifest.json")
    add_test(NAME qualpf.classic.phase6_evidence COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/reference_checks/phase6_reference_test.py" $<TARGET_FILE:qualpf_classic_replay>)
endif()
if(QUALPF_BUILD_SEARCH_INTEGRATION_TESTS OR QUALPF_BUILD_CONTRACT_TESTS)
    add_subdirectory(support/snapshot)
endif()
if(QUALPF_BUILD_SEARCH_INTEGRATION_TESTS)
    add_executable(qualpf_search_integration_tests integration/classic_workflow_test.cpp)
    target_include_directories(qualpf_search_integration_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
    target_link_libraries(qualpf_search_integration_tests PRIVATE qualpf_session qualpf_test_snapshot GTest::gtest_main)
    set_target_properties(qualpf_search_integration_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.search.integration COMMAND qualpf_search_integration_tests)
endif()
if(QUALPF_BUILD_CONTRACT_TESTS)
    add_executable(qualpf_contract_tests
        unit/core/core_contract_test.cpp
        unit/application/session_contract_test.cpp
        compatibility/snapshot_codec_test.cpp
        compatibility/regression_contract_test.cpp
        support/regression_case.h
    )
    target_include_directories(qualpf_contract_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
    target_link_libraries(qualpf_contract_tests PRIVATE
        qualpf_core qualpf_session qualpf_test_snapshot GTest::gtest_main
    )
    set_target_properties(qualpf_contract_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.contracts COMMAND qualpf_contract_tests)
endif()

if(QUALPF_BUILD_NUMERICS_TESTS)
    if(QUALPF_BUILD_PREPROCESSING_TESTS)
        add_executable(qualpf_preprocessing_tests unit/core/preprocessing_test.cpp unit/core/kalpha2_test.cpp)
        target_link_libraries(qualpf_preprocessing_tests PRIVATE qualpf_core GTest::gtest_main)
        set_target_properties(qualpf_preprocessing_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
        add_test(NAME qualpf.preprocessing.contracts COMMAND qualpf_preprocessing_tests)
    endif()
    add_executable(qualpf_numerics_tests
        unit/core/numerics_basic_test.cpp
        unit/core/numerics_interpolation_test.cpp
        unit/core/numerics_linear_algebra_test.cpp
        compatibility/numerical_regression_test.cpp
        support/numerical_regression.h
    )
    target_include_directories(qualpf_numerics_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
    target_link_libraries(qualpf_numerics_tests PRIVATE qualpf_core GTest::gtest_main)
    set_target_properties(qualpf_numerics_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.numerics COMMAND qualpf_numerics_tests)
endif()

# Child-directory tests belong to the same long-term suite as product tests.
if(QUALPF_BUILD_PRODUCT_TESTS OR QUALPF_BUILD_PREPROCESSING_TESTS OR QUALPF_BUILD_CLASSIC_SEARCH_TESTS)
    add_executable(qualpf_algorithm_workflow_tests unit/application/algorithm_workflow_test.cpp)
    target_include_directories(qualpf_algorithm_workflow_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
    target_link_libraries(qualpf_algorithm_workflow_tests PRIVATE qualpf_session)
    set_target_properties(qualpf_algorithm_workflow_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.algorithms.product_workflow COMMAND qualpf_algorithm_workflow_tests)
    add_executable(qualpf_core_algorithm_modules unit/core/algorithm_modules_test.cpp)
    target_include_directories(qualpf_core_algorithm_modules PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
    target_link_libraries(qualpf_core_algorithm_modules PRIVATE qualpf_core)
    set_target_properties(qualpf_core_algorithm_modules PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.algorithms.core_modules COMMAND qualpf_core_algorithm_modules)
    add_executable(qualpf_pfsm_core_tests unit/core/pfsm_test.cpp unit/core/pfsm_candidate_recall_test.cpp unit/core/pfsm_integral_test.cpp)
    foreach(kind integral relaxed)
        add_executable(qualpf_pfsm_${kind}_probe unit/core/pfsm_${kind}_probe.cpp)
        target_link_libraries(qualpf_pfsm_${kind}_probe PRIVATE qualpf_core)
        set_target_properties(qualpf_pfsm_${kind}_probe PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    endforeach()
target_include_directories(qualpf_pfsm_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/profile")
    add_executable(qualpf_pfsm_candidate_recall_probe unit/core/pfsm_candidate_recall_probe.cpp)
target_include_directories(qualpf_pfsm_candidate_recall_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/profile")
    target_link_libraries(qualpf_pfsm_candidate_recall_probe PRIVATE qualpf_core)
    set_target_properties(qualpf_pfsm_candidate_recall_probe PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    find_package(Eigen3 3.4 REQUIRED NO_MODULE)
    target_link_libraries(qualpf_pfsm_core_tests PRIVATE qualpf_core Eigen3::Eigen)
    target_compile_definitions(qualpf_pfsm_core_tests PRIVATE EIGEN_DONT_PARALLELIZE EIGEN_DONT_VECTORIZE)
    if(MSVC)
        target_compile_options(qualpf_pfsm_core_tests PRIVATE /fp:strict)
    else()
        target_compile_options(qualpf_pfsm_core_tests PRIVATE -fno-fast-math -ffp-contract=off)
    endif()
    set_target_properties(qualpf_pfsm_core_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.algorithms.pfsm_core COMMAND qualpf_pfsm_core_tests)
    add_executable(qualpf_numerics_runtime_tests unit/core/numerics_runtime_test.cpp)
    target_link_libraries(qualpf_numerics_runtime_tests PRIVATE qualpf_core)
    set_target_properties(qualpf_numerics_runtime_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.numerics.runtime COMMAND qualpf_numerics_runtime_tests)
    add_executable(qualpf_pfsm_application_tests unit/application/pfsm_test.cpp)
    target_include_directories(qualpf_pfsm_application_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/application/session")
    find_package(OpenMP REQUIRED COMPONENTS CXX)
    target_link_libraries(qualpf_pfsm_application_tests PRIVATE qualpf_application OpenMP::OpenMP_CXX)
    set_target_properties(qualpf_pfsm_application_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.algorithms.pfsm_application COMMAND qualpf_pfsm_application_tests)
    add_executable(qualpf_profile_rir_tests unit/core/profile_rir_test.cpp)
    target_link_libraries(qualpf_profile_rir_tests PRIVATE qualpf_core)
    set_target_properties(qualpf_profile_rir_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.quantification.core COMMAND qualpf_profile_rir_tests)
    add_executable(qualpf_quantification_export_tests unit/application/quantification_export_test.cpp)
    target_link_libraries(qualpf_quantification_export_tests PRIVATE qualpf_application)
    set_target_properties(qualpf_quantification_export_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.quantification.export COMMAND qualpf_quantification_export_tests)
    add_executable(qualpf_pfsm_quantification_tests unit/application/pfsm_quantification_test.cpp)
    target_link_libraries(qualpf_pfsm_quantification_tests PRIVATE qualpf_application)
    set_target_properties(qualpf_pfsm_quantification_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.quantification.application COMMAND qualpf_pfsm_quantification_tests)
    add_executable(qualpf_geometric_fwhm_tests unit/core/geometric_fwhm_test.cpp)
    target_link_libraries(qualpf_geometric_fwhm_tests PRIVATE qualpf_core)
    set_target_properties(qualpf_geometric_fwhm_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.algorithms.geometric_fwhm COMMAND qualpf_geometric_fwhm_tests)
    add_executable(qualpf_geometric_fwhm_workflow_tests unit/application/geometric_fwhm_test.cpp)
    target_link_libraries(qualpf_geometric_fwhm_workflow_tests PRIVATE qualpf_session)
    set_target_properties(qualpf_geometric_fwhm_workflow_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.algorithms.geometric_fwhm_workflow COMMAND qualpf_geometric_fwhm_workflow_tests)
    add_executable(qualpf_initial_peak_tests unit/application/initial_peak_search_test.cpp)
    target_link_libraries(qualpf_initial_peak_tests PRIVATE qualpf_session)
    set_target_properties(qualpf_initial_peak_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.application.initial_peak_search COMMAND qualpf_initial_peak_tests)
    add_executable(qualpf_configured_fwhm_tests unit/application/configured_fwhm_test.cpp)
    target_link_libraries(qualpf_configured_fwhm_tests PRIVATE qualpf_session)
    set_target_properties(qualpf_configured_fwhm_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.application.configured_fwhm COMMAND qualpf_configured_fwhm_tests)
    add_executable(qualpf_workflow_tests unit/application/workflow_test.cpp)
    target_link_libraries(qualpf_workflow_tests PRIVATE qualpf_session)
    set_target_properties(qualpf_workflow_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.application.workflows COMMAND qualpf_workflow_tests)
endif()
get_property(qualpf_normal_tests DIRECTORY PROPERTY TESTS)
if(qualpf_normal_tests)
    set_tests_properties(${qualpf_normal_tests} PROPERTIES LABELS normal)
endif()

# Product/property registrations originally declared in the root retain their
# working directory, labels, environment and timeout contracts here.
get_property(qualpf_pre_product_tests DIRECTORY PROPERTY TESTS)
include(product/CMakeLists.txt)
include(Properties.cmake)
get_property(qualpf_all_tests DIRECTORY PROPERTY TESTS)
foreach(test IN LISTS qualpf_all_tests)
    if(NOT test IN_LIST qualpf_pre_product_tests)
        set_tests_properties(${test} PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
        set_property(TEST ${test} APPEND PROPERTY LABELS normal)
    endif()
endforeach()

if(QUALPF_BUILD_PRODUCT_TESTS)
    add_executable(qualpf_search_task_controller_tests integration/gui/search_task_controller_test.cpp
        "${PROJECT_SOURCE_DIR}/src/gui/search/search_task_controller.cpp"
        "${PROJECT_SOURCE_DIR}/src/gui/search/search_task_controller.h")
    target_include_directories(qualpf_search_task_controller_tests PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/support")
    target_link_libraries(qualpf_search_task_controller_tests PRIVATE qualpf_session Qt6::Core)
    set_target_properties(qualpf_search_task_controller_tests PROPERTIES AUTOMOC ON)
    add_test(NAME qualpf.gui.search_tasks COMMAND qualpf_search_task_controller_tests)
    set_tests_properties(qualpf.gui.search_tasks PROPERTIES LABELS normal TIMEOUT 30)
endif()

if(QUALPF_BUILD_PRODUCT_TESTS)
    add_executable(qualpf_platform_process_tests integration/platform/process_services_test.cpp)
    target_include_directories(qualpf_platform_process_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(qualpf_platform_process_tests PRIVATE qualpf_infrastructure qualpf_application Qt6::Core Qt6::Sql)
    set_target_properties(qualpf_platform_process_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.platform.process COMMAND qualpf_platform_process_tests)
    set_tests_properties(qualpf.platform.process PROPERTIES LABELS normal TIMEOUT 30)
endif()

if(QUALPF_BUILD_PRODUCT_TESTS)
    add_executable(qualpf_platform_update_tests integration/platform/update_services_test.cpp)
    target_include_directories(qualpf_platform_update_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(qualpf_platform_update_tests PRIVATE qualpf_update_provider qualpf_application Qt6::Core Qt6::Network)
    set_target_properties(qualpf_platform_update_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.platform.update COMMAND qualpf_platform_update_tests)
    set_tests_properties(qualpf.platform.update PROPERTIES LABELS normal TIMEOUT 30)
endif()

if(QUALPF_BUILD_PRODUCT_TESTS)
    add_executable(qualpf_plot_coordinates_tests unit/application/plot_coordinates_test.cpp)
    target_include_directories(qualpf_plot_coordinates_tests PRIVATE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(qualpf_plot_coordinates_tests PRIVATE qualpf_application)
    set_target_properties(qualpf_plot_coordinates_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME qualpf.plot.coordinates COMMAND qualpf_plot_coordinates_tests)
    set_tests_properties(qualpf.plot.coordinates PROPERTIES LABELS normal TIMEOUT 120)
endif()

# Public CI builds the real product and runs existing controls whose inputs are
# committed or restored from the sealed public reference commit. Private powder
# examples and development-only fixtures remain prerequisites of the full suite.
if(QUALPF_BUILD_PRODUCT_TESTS)
    set(qualpf_public_ci_tests
        qualpf.distribution.consistency qualpf.distribution.dependency_bundle
        qualpf.project.qpf qualpf.project.qpf_protocol qualpf.settings.workflows
        qualpf.gui.about.scale100 qualpf.gui.about.scale150 qualpf.gui.about.scale200
        qualpf.math.rounded_transcendentals qualpf.algorithms.core_modules
        qualpf.algorithms.classic_contracts qualpf.application.workflows
        qualpf.card.chemistry_registry qualpf.scattering.table_reproducible
        qualpf.symmetry.table_reproducible qualpf.platform.process
        qualpf.platform.update qualpf.plot.coordinates)
    foreach(test IN LISTS qualpf_public_ci_tests)
        if(NOT TEST ${test})
            message(FATAL_ERROR "Public CI regression is not registered: ${test}")
        endif()
        set_property(TEST ${test} APPEND PROPERTY LABELS public_ci)
    endforeach()
    add_custom_target(qualpf_public_ci DEPENDS ${EXECUTABLE_NAME}
        qualpf_qpf_project_tests qualpf_workflow_settings_tests qualpf_about_dialog_tests
        qualpf_rounded_transcendentals_properties qualpf_core_algorithm_modules
        qualpf_classic_algorithm_contracts qualpf_workflow_tests
        qualpf_platform_process_tests qualpf_platform_update_tests qualpf_plot_coordinates_tests)
endif()
