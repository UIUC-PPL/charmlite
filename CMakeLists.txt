cmake_minimum_required(VERSION 3.16)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

project(CharmLite CXX)

# Make sure all executables go in bin directory and all libraries go in lib
# directory.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

# Make Default Build Type Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        "Release"
        CACHE
        STRING
            "Configuration type (Debug, RelWithDebInfo, Release, MinSizeRel)"
        FORCE)
endif()

# Replace this with find module system
set(CHARMC_PATH $ENV{CHARM_HOME})
if(CHARMC_PATH)
    message(STATUS "Charm compiler found at: ${CHARMC_PATH}")
else(CHARMC_PATH)
    message(FATAL_ERROR "Charm compiler not found, please update the environment variable CHARM_HOME to the right location.")
endif(CHARMC_PATH)

set(CMAKE_CXX_COMPILER "${CHARMC_PATH}/bin/charmc")
set(CMAKE_CXX_FLAGS "-language converse++" CACHE STRING "Additional arguments to charmc" FORCE)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(src)


# Set-up examples
option(CHARMLITE_ENABLE_EXAMPLES "Enable Building Examples" ON)
if(CHARMLITE_ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

# Set-up tests
option(CHARMLITE_ENABLE_TESTS "Enable Testing Framework" OFF)
if(CHARMLITE_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Set-up benchmarks
option(CHARMLITE_ENABLE_BENCHMARKS "Enable Benchmarking Framework" OFF)
if(CHARMLITE_ENABLE_BENCHMARKS)
    enable_testing()
    add_subdirectory(benchmarks)
endif()

# Setup Installs

# Install charmlite library
install(
    TARGETS charmlite
    EXPORT CharmLite
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib)

# Install cmake targets to be used by CharmLiteConfig.cmake
install(
    EXPORT CharmLite
    FILE CharmLiteTargets.cmake
    NAMESPACE CharmLite::
    DESTINATION lib/cmake/CharmLite)

# Install header files
install(
    DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hh")

# Install config file
install(
    FILES cmake/CharmLiteConfig.cmake
    DESTINATION lib/cmake/CharmLite
)
