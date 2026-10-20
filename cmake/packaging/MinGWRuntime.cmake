include_guard(GLOBAL)

# Ask the configured compiler, never PATH, for its matching runtime. In
# particular Qt's deployment tool does not discover PFSM's libgomp dependency.
function(qualpf_collect_mingw_runtime output)
    set(${output} "" PARENT_SCOPE)
    if(NOT WIN32 OR NOT MINGW)
        return()
    endif()
    set(runtime_files)
    foreach(name libgomp-1.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" "-print-file-name=${name}"
            RESULT_VARIABLE status
            OUTPUT_VARIABLE path
            ERROR_VARIABLE error
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT status STREQUAL "0" OR NOT IS_ABSOLUTE "${path}" OR
           NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
            message(FATAL_ERROR
                "Cannot locate ${name} from the configured MinGW compiler "
                "${CMAKE_CXX_COMPILER}: ${path} ${error}")
        endif()
        get_filename_component(path "${path}" REALPATH)
        list(APPEND runtime_files "${path}")
    endforeach()
    set(${output} "${runtime_files}" PARENT_SCOPE)
endfunction()

function(qualpf_stage_mingw_runtime target)
    qualpf_collect_mingw_runtime(runtime_files)
    if(NOT runtime_files)
        return()
    endif()
    # Run on incremental builds too, so removing a deployed DLL is recoverable
    # without forcing a product relink. copy_if_different preserves timestamps.
    add_custom_target(${target}_mingw_runtime
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${target}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            ${runtime_files} "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Staging matching MinGW and OpenMP runtime DLLs"
        VERBATIM)
    add_dependencies(${target} ${target}_mingw_runtime)
    set_property(TARGET ${target} PROPERTY QUALPF_MINGW_RUNTIME_FILES "${runtime_files}")
endfunction()
