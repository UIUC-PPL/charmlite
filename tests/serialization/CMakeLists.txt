add_custom_target(serialization)

set(TESTS
    broadcast
    # insert
    # send
)

foreach(TEST ${TESTS})
    set(_test_name ${TEST}_test)
    add_executable(${_test_name} ${TEST}.cpp)
    target_link_libraries(${_test_name} PRIVATE charmlite)
    add_dependencies(serialization ${_test_name})
    add_charmlite_test(${_test_name})    
endforeach()

