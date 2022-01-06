add_custom_target(pingpong)

set(BENCHMARK
    pgm
)


set(_benchmark_name ${BENCHMARK}_pingpong_benchmark)
add_executable(${_benchmark_name} ${BENCHMARK}.cpp)
target_link_libraries(${_benchmark_name} PRIVATE charmlite)
target_compile_options(${_benchmark_name} PRIVATE -fno-exceptions -fno-unwind-tables)
add_dependencies(pingpong ${_benchmark_name})
add_charmlite_test(${_benchmark_name})
