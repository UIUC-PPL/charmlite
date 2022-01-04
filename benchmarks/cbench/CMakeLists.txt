add_custom_target(cbench)

set(BENCHMARK
    pgm
)


set(_benchmark_name ${BENCHMARK}_cbench_benchmark)
add_executable(${_benchmark_name} ${BENCHMARK}.cpp)
target_link_libraries(${_benchmark_name} PRIVATE charmlite)
add_dependencies(cbench ${_benchmark_name})

add_test(
    NAME ${_benchmark_name}
    COMMAND ${BENCHMARK_COMMAND} ${CHARMC_PATH}/bin/charmrun +p${CHARMLITE_BENCHMARK_PE} ${PROJECT_BINARY_DIR}/bin/${_benchmark_name})
