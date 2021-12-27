add_custom_target(basic)

set(EXAMPLE
    pgm
)

set(_example_name ${EXAMPLE}_basic_example)
add_executable(${_example_name} ${EXAMPLE}.cc)
target_link_libraries(${_example_name} PRIVATE charmlite)
add_dependencies(basic ${_example_name})
