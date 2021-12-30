add_custom_target(pingpong)

set(EXAMPLE
    pgm
)

set(_example_name ${EXAMPLE}_pingpong_example)
add_charmlite_executable(${_example_name} ${EXAMPLE}.cc)
# add_executable(${_example_name} ${EXAMPLE}.cc)
# target_link_libraries(${_example_name} PRIVATE charmlite)
add_dependencies(pingpong ${_example_name})
