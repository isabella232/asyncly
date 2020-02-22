add_executable(asyncly_benchmarks
  Executor/ExecutorBenchmarksMain.cpp
  Executor/ExecutorWrappersPerformance.cpp
  Executor/ThreadPoolPerformanceTest.cpp)

target_include_directories(asyncly_benchmarks PRIVATE ${PROJECT_SOURCE_DIR}/Source)

target_link_libraries(asyncly_benchmarks asyncly)
target_link_libraries(asyncly_benchmarks Boost::chrono Boost::thread Boost::system)
target_link_libraries(asyncly_benchmarks benchmark::benchmark)

add_test(NAME asyncly_benchmarks COMMAND asyncly_benchmarks)
set_property(TEST asyncly_benchmarks APPEND PROPERTY LABELS Benchmark)
