enable_testing()

# Benchmark related Flags
set(CHARMLITE_BENCHMARK_PE 2 CACHE STRING "Commandline PE argument to Benchmarks")
set(CHARMLITE_BENCHMARK_PPN 0 CACHE STRING "Commandline PPN argument to SMP runs")
if(${CHARM_WITH_SMP})
    message(STATUS "Charm++ installed with SMP. Setting +ppn defaults.")
    set(CHARMLITE_BENCHMARK_PPN 2)
endif()

# Set-up benchmarks
option(CHARMLITE_ENABLE_BENCHMARKS "Enable Benchmarking Framework" OFF)

set(CHARMLITE_TEST_FLAGS "+p1")
set(CHARMLITE_PARALLEL_TEST_FLAGS "+p${CHARMLITE_BENCHMARK_PE}")

set(CHARMRUN_LOCAL_FLAGS)
if(${CHARM_WITH_NETWORK} STREQUAL "netlrts")
    set(CHARMRUN_LOCAL_FLAGS "++local")
    set(CHARMRUN_PPN_FLAG "++ppn")
else()
    set(CHARMRUN_PPN_FLAG "+ppn")
endif()

if(${CHARM_WITH_SMP})
    set(CHARMLITE_TEST_FLAGS
        ${CHARMLITE_TEST_FLAGS}
        "${CHARMRUN_PPN_FLAG} 1")
    set(CHARMLITE_PARALLEL_TEST_FLAGS
        ${CHARMLITE_PARALLEL_TEST_FLAGS}
        "${CHARMRUN_PPN_FLAG} ${CHARMLITE_BENCHMARK_PPN}"
        "+CmiSleepOnIdle")
endif()

# Utility function to set up the same test with different arguments
function(add_charmlite_test test_name)
    # Parse arguments
    cmake_parse_arguments(
        ${test_name} "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN}
    )

    add_test(
        NAME ${test_name}
        COMMAND ${CHARM_CHARMRUN_PATH} ${CHARMRUN_LOCAL_FLAGS} ${CMAKE_BINARY_DIR}/bin/${test_name} ${CHARMLITE_TEST_FLAGS} ${ARGN})
    
    add_test(
        NAME ${test_name}_pe2
        COMMAND ${CHARM_CHARMRUN_PATH} ${CHARMRUN_LOCAL_FLAGS} ${CMAKE_BINARY_DIR}/bin/${test_name} ${CHARMLITE_PARALLEL_TEST_FLAGS} ${ARGN})
endfunction()
