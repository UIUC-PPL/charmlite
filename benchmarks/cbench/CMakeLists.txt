add_custom_target(cbench)

set(BENCHMARK
    pgm
)


set(_benchmark_name ${BENCHMARK}_cbench_benchmark)
add_executable(${_benchmark_name} ${BENCHMARK}.cpp)
target_compile_options(${_benchmark_name} PRIVATE -fno-exceptions -fno-unwind-tables)
target_link_libraries(${_benchmark_name} PRIVATE charmlite)
add_dependencies(cbench ${_benchmark_name})
add_charmlite_test(${_benchmark_name})
