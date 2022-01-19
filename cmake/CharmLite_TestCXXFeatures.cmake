set(CHARMLITE_INTERNAL_CXX_TESTS ${CMAKE_SOURCE_DIR}/cmake/tests)

set(INTERNAL_TESTS
    ctad
    template_auto
)

message(STATUS "Testing C++17 compliance")
foreach(INTERNAL_TEST ${INTERNAL_TESTS})
    set(INTERNAL_TEST_NAME ${INTERNAL_TEST}.cpp)
    try_compile(
        RESULT_VAR
        ${CMAKE_BINARY_DIR}/internal_tests
        ${CHARMLITE_INTERNAL_CXX_TESTS}/${INTERNAL_TEST_NAME}
        COMPILE_DEFINITIONS "-c++-option -std=c++17"
        OUTPUT_VARIABLE TRY_COMPILE_OUTPUT)
    
    if(${RESULT_VAR} STREQUAL "FALSE")
        message(STATUS "${RESULT_VAR} ${TRY_COMPILE_OUTPUT}")
        message(FATAL_ERROR "CharmLite requires C++17 compliant compiler.")
    endif()
endforeach()
