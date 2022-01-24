add_custom_target(indexing)

set(TEST
    compile_time
)

set(_test_name ${TEST}_test)
add_executable(${_test_name} ${TEST}.cpp)
target_link_libraries(${_test_name} PRIVATE charmlite)
add_dependencies(indexing ${_test_name})
add_charmlite_test(${_test_name})
