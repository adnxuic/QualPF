# Qt records its deployment tool while loading package dependencies. Select it
# before find_package(Qt6), including on a first portable-build configuration.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(QUALPF_DEPLOY_PATCHELF_EXECUTABLE patchelf)
    if(QUALPF_DEPLOY_PATCHELF_EXECUTABLE)
        set(QT_DEPLOY_USE_PATCHELF ON)
    endif()
endif()
