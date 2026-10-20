# Build-only metadata; the benchmark itself never launches Git.
if(NOT DEFINED QUALPF_SOURCE_DIR OR NOT DEFINED QUALPF_REVISION_HEADER)
    message(FATAL_ERROR "PFSM revision generation requires source and header paths")
endif()

set(pfsm_revision "unavailable")
if(EXISTS "${QUALPF_SOURCE_DIR}/.git")
    # An explicit git-dir handles worktree gitfiles and prevents an archive or
    # invalid repository marker from borrowing an unrelated parent repository.
    execute_process(
        COMMAND git -C "${QUALPF_SOURCE_DIR}" --git-dir=.git rev-parse --verify HEAD
        RESULT_VARIABLE pfsm_git_status
        OUTPUT_VARIABLE pfsm_git_revision
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    string(LENGTH "${pfsm_git_revision}" pfsm_git_revision_length)
    if(pfsm_git_status EQUAL 0 AND pfsm_git_revision MATCHES "^[0-9a-f]+$"
       AND (pfsm_git_revision_length EQUAL 40 OR pfsm_git_revision_length EQUAL 64))
        set(pfsm_revision "${pfsm_git_revision}")
    endif()
endif()

set(pfsm_content "#pragma once\n#define QUALPF_PFSM_REVISION \"${pfsm_revision}\"\n")
set(pfsm_previous "")
if(EXISTS "${QUALPF_REVISION_HEADER}")
    file(READ "${QUALPF_REVISION_HEADER}" pfsm_previous)
endif()
if(NOT pfsm_content STREQUAL pfsm_previous)
    file(WRITE "${QUALPF_REVISION_HEADER}" "${pfsm_content}")
endif()
