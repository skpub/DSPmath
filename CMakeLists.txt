cmake_minimum_required(VERSION 3.20)

project(DSPmath LANGUAGES CXX)

add_library(DSPmath INTERFACE)
target_include_directories(DSPmath INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(DSPmath INTERFACE cxx_std_20)

enable_testing()

add_executable(compare_std tests/compare_std.cpp)
target_link_libraries(compare_std PRIVATE DSPmath)

add_test(NAME compare_std COMMAND compare_std)

add_executable(optimization_accuracy tests/optimization_accuracy.cpp)
target_link_libraries(optimization_accuracy PRIVATE DSPmath)
add_test(NAME optimization_accuracy COMMAND optimization_accuracy)

option(DSPMATH_BUILD_BENCHMARKS "Build the independent/dependent-input optimization benchmark" OFF)
if(DSPMATH_BUILD_BENCHMARKS)
    add_executable(optimization_benchmark tests/optimization_benchmark.cpp)
    target_link_libraries(optimization_benchmark PRIVATE DSPmath)
    set(DSPMATH_BENCHMARK_HEADER "" CACHE FILEPATH "Optional saved header for baseline measurements")
    if(DSPMATH_BENCHMARK_HEADER)
        target_compile_definitions(optimization_benchmark PRIVATE
            DSPMATH_BENCHMARK_HEADER="${DSPMATH_BENCHMARK_HEADER}")
    endif()
endif()
