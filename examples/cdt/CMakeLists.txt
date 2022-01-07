add_custom_target(cdt)

set(EXAMPLE
    pgm
)

set(_example_name ${EXAMPLE}_cdt_example)
add_executable(${_example_name} ${EXAMPLE}.cpp)
target_link_libraries(${_example_name} PRIVATE charmlite)
add_dependencies(cdt ${_example_name})
add_charmlite_test(${_example_name})
