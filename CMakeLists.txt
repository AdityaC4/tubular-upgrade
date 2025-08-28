cmake_minimum_required(VERSION 3.16)

# Project setup
project(Tubular VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define the executable
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    src
    src/backend
    src/core  
    src/frontend
    src/middle_end
)

# Common compiler flags (equivalent to CFLAGS_all)
target_compile_options(${PROJECT_NAME} PRIVATE 
    -Wall 
    -Wextra
)

# Key files that trigger recompilation (equivalent to KEY_FILES)
set(KEY_FILES src/lexer.hpp)

# Make the executable depend on key files for proper rebuilds
set_target_properties(${PROJECT_NAME} PROPERTIES 
    ADDITIONAL_CLEAN_FILES "${KEY_FILES}"
)

# Default build type (equivalent to default target with -O3 -DNDEBUG)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Configure build types
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g")

# Custom build type: Grumpy (equivalent to grumpy target)
set(CMAKE_CXX_FLAGS_GRUMPY "-pedantic -Wconversion -Weffc++")
set(CMAKE_C_FLAGS_GRUMPY "-pedantic -Wconversion -Weffc++")

# Add Grumpy to the list of valid build types
get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(isMultiConfig)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES};Grumpy" CACHE STRING "" FORCE)
else()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo" "Grumpy")
endif()

# Custom target for tests (equivalent to tests target)
add_custom_target(tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running standard tests..."
    COMMAND cd tests && ./run_tests.sh
    COMMAND ${CMAKE_COMMAND} -E echo "Running loop unrolling tests..."
    COMMAND cd tests/loop-unrolling && ./run_unroll_tests.sh
    COMMAND ${CMAKE_COMMAND} -E echo "All tests completed."
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running complete test suite including loop unrolling tests"
)

# Custom clean target to match original Makefile behavior exactly
add_custom_target(clean-all
    COMMAND rm -f ${PROJECT_NAME} *.o tests/test-??.wasm tests/test-??.wat tests/P3-test-??.wasm tests/P3-test-??.wat
    COMMAND rm -f tests/loop-unrolling/ultra-??-unroll*.wasm tests/loop-unrolling/ultra-??-unroll*.wat
    COMMAND rm -rf ${PROJECT_NAME}.dSYM
    COMMAND rm -rf tests/loop-unrolling/results
    COMMAND rm -f build/${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Cleaning all files including loop unrolling test results and binary"
)

# Clean only test files
add_custom_target(clean-test
    COMMAND rm -f tests/test-??.wasm tests/test-??.wat tests/P3-test-??.wasm tests/P3-test-??.wat
    COMMAND rm -f tests/loop-unrolling/ultra-??-unroll*.wasm tests/loop-unrolling/ultra-??-unroll*.wat
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Cleaning all test files including loop unrolling tests"
)

# Clean only loop unrolling test results
add_custom_target(clean-unroll
    COMMAND rm -f tests/loop-unrolling/ultra-??-unroll*.wasm tests/loop-unrolling/ultra-??-unroll*.wat
    COMMAND rm -rf tests/loop-unrolling/results
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Cleaning loop unrolling test files and results"
)

# Print configuration information (equivalent to print-% debugging)
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Source file: ${PROJECT_NAME}.cpp")
message(STATUS "Key files: ${KEY_FILES}")
