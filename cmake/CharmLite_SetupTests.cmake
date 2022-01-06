enable_testing()

set(CHARMLITE_TEST_FLAGS "")
if(${CHARM_WITH_NETWORK} STREQUAL "netlrts")
    set(CHARMLITE_TEST_FLAGS "++local")
endif()

# Utility function to set up the same test with different arguments
function(add_charmlite_test test_name)
    # Parse arguments
    cmake_parse_arguments(
        ${test_name} "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN}
    )

    add_test(
        NAME ${test_name}
        COMMAND ${CHARM_CHARMRUN_PATH} ${CHARMLITE_TEST_FLAGS} ${CMAKE_BINARY_DIR}/bin/${test_name} ${ARGN})
    
    add_test(
        NAME ${test_name}_pe2
        COMMAND ${CHARM_CHARMRUN_PATH} ${CHARMLITE_TEST_FLAGS} ${CMAKE_BINARY_DIR}/bin/${test_name} +p2 ${ARGN})
endfunction()
