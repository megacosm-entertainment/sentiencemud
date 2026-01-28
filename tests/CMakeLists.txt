cmake_minimum_required(VERSION 3.20)

# Test configuration for Sentience MUD
# This file is included by the main CMakeLists.txt when BUILD_TESTS is enabled

# Find Criterion testing framework
find_package(PkgConfig REQUIRED)
pkg_check_modules(CRITERION REQUIRED criterion)

if(NOT CRITERION_FOUND)
    message(WARNING "Criterion testing framework not found. Tests will be skipped.")
    message(STATUS "To install Criterion: sudo apt install libcriterion-dev")
    return()
endif()

message(STATUS "Found Criterion ${CRITERION_VERSION}")

# Enable testing
enable_testing()

# Test executable for WNUM functionality
add_executable(test_wnum_standalone
    tests/unit/test_wnum_standalone.c
)

# Configure test target
target_include_directories(test_wnum_standalone PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CRITERION_INCLUDE_DIRS}
)

target_compile_options(test_wnum_standalone PRIVATE 
    ${CRITERION_CFLAGS_OTHER}
    -std=gnu99 -Wall -Wextra -O0 -g
)

target_link_libraries(test_wnum_standalone 
    ${CRITERION_LIBRARIES}
)

# Register the test with CTest
add_test(NAME wnum_standalone_tests COMMAND test_wnum_standalone)

# Set test properties
set_tests_properties(wnum_standalone_tests PROPERTIES
    TIMEOUT 30
    LABELS "unit"
)

# Custom target to run all tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_wnum_standalone
    COMMENT "Running unit tests"
)

# Verbose test target
add_custom_target(check-verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS test_wnum_standalone  
    COMMENT "Running unit tests (verbose)"
)