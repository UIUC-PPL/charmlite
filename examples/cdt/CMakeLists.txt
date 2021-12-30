add_custom_target(cdt)

set(EXAMPLE
    pgm
)

set(_example_name ${EXAMPLE}_cdt_example)
add_charmlite_executable(${_example_name} ${EXAMPLE}.cc)
# add_executable(${_example_name} ${EXAMPLE}.cc)
# target_link_libraries(${_example_name} PRIVATE charmlite)
add_dependencies(cdt ${_example_name})
