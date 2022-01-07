add_custom_target(basic)

set(EXAMPLE
    pgm
)

set(_example_name ${EXAMPLE}_basic_example)
add_executable(${_example_name} ${EXAMPLE}.cpp)
target_link_libraries(${_example_name} PRIVATE charmlite)
add_dependencies(basic ${_example_name})
add_charmlite_test(${_example_name})
