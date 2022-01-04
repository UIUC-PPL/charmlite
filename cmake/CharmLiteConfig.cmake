get_filename_component(CharmLite_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)

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

include("${CharmLite_CMAKE_DIR}/CharmLiteTargets.cmake")
