add_custom_target(hello)

set(EXAMPLE
    hello
)

set(_example_name ${EXAMPLE}_example)
add_executable(${_example_name} ${EXAMPLE}.cpp)
target_link_libraries(${_example_name} PRIVATE charmlite)
add_dependencies(hello ${_example_name})
add_charmlite_test(${_example_name})
