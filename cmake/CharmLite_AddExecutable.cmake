# A hack to enforce charmlite to link prior to object file
function(add_charmlite_executable name)
    # Parse arguments
    cmake_parse_arguments(
        ${name} "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN}
    )

    # message(STATUS "Name: ${name}; Sources: ${ARGN}")

    add_executable(${name} ${ARGN})
    target_include_directories(${name}
        PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_link_options(${name} PUBLIC "${PROJECT_BINARY_DIR}/src/CMakeFiles/charmlite.dir/core.cc.o")
endfunction()
