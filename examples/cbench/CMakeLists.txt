add_custom_target(cbench)

set(EXAMPLE
    pgm
)

set(_example_name ${EXAMPLE}_cbench_example)
add_executable(${_example_name} ${EXAMPLE}.cpp)
target_link_libraries(${_example_name} PRIVATE charmlite)
add_dependencies(cbench ${_example_name})
