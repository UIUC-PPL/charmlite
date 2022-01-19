cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# Enable out of source builds and source changes
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

if(CMAKE_VERSION VERSION_GREATER 3.22.1 OR CMAKE_VERSION VERSION_EQUAL 3.22.1)
    cmake_policy(SET CMP0115 OLD)
endif()

# Make Default Build Type Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        "Release"
        CACHE
        STRING
            "Configuration type (Debug, RelWithDebInfo, Release, MinSizeRel)"
        FORCE)
endif()

# Charmlite depends on a valid Charm installation
find_package(Charm REQUIRED)
message(STATUS "Found Charm++ at ${CHARM_ROOT}")
message(STATUS "Switching to Charm CXX Compiler: ${CHARM_ROOT}/bin/charmc")

set(CMAKE_CXX_COMPILER ${CHARM_CXX_COMPILER})

# Make sure correct compiler is set before starting the project
project(CharmLite CXX)

# Make sure all executables go in bin directory and all libraries go in lib
# directory.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

set(CMAKE_CXX_FLAGS "-language converse++ -Wall -Wextra" CACHE STRING "Additional arguments to charmc" FORCE)

# Charmlite requires a conforming C++11 compiler
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# charmc currently appends -std=c++11 so we need to explicitly pass the
# option again
add_compile_options(-cpp-option -std=c++17)

# Add cmake to module path
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Ensure required facilities work
include(CharmLite_TestCXXFeatures)

# Include the testing framework
include(CharmLite_SetupTests)

# Generate libcharmlite.a
add_subdirectory(src)


# Set-up examples
option(CHARMLITE_ENABLE_EXAMPLES "Enable Building Examples" ON)
if(CHARMLITE_ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

# Set-up tests
option(CHARMLITE_ENABLE_TESTS "Enable Testing Framework" OFF)
if(CHARMLITE_ENABLE_TESTS)
    add_subdirectory(tests)
endif()

if(CHARMLITE_ENABLE_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Configure cmake configs
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/templates/CharmLiteConfig.cmake.in
    "${CMAKE_BINARY_DIR}/lib/cmake/CharmLite/CharmLiteConfig.cmake"
    ESCAPE_QUOTES
    @ONLY)

# Setup Installs
include(GNUInstallDirs)
# Install charmlite library
install(
    TARGETS charmlite
    EXPORT CharmLite
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Install cmake targets to be used by CharmLiteConfig.cmake
install(
    EXPORT CharmLite
    FILE CharmLiteTargets.cmake
    NAMESPACE CharmLite::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CharmLite)

# Install header files
install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp")

# Install config file
install(
    FILES ${CMAKE_BINARY_DIR}/lib/cmake/CharmLite/CharmLiteConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CharmLite
)

# Install charmrun
install(
    PROGRAMS ${CHARM_ROOT}/bin/charmrun
    DESTINATION bin
)
