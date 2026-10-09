cmake_minimum_required(VERSION 3.20)
project(QualpfApplicationContracts LANGUAGES CXX)
enable_testing()
set(QUALPF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(QUALPF_BUILD_PRODUCT_TESTS OFF)
add_subdirectory("${QUALPF_ROOT}/src/core" core)
# The same complete Application target used by the product. No Qt discovery,
# compile definition, include directory or linked binary enters this harness.
add_subdirectory("${QUALPF_ROOT}/src/application" application)
add_executable(application_boundary application_boundary.cpp)
target_link_libraries(application_boundary PRIVATE qualpf_application)
add_test(NAME application.qtfree COMMAND application_boundary)
