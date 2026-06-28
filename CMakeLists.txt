cmake_minimum_required(VERSION 3.20)

project(DSPmath LANGUAGES CXX)

add_library(DSPmath INTERFACE)
target_include_directories(DSPmath INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(DSPmath INTERFACE cxx_std_20)

enable_testing()

add_executable(compare_std tests/compare_std.cpp)
target_link_libraries(compare_std PRIVATE DSPmath)

add_test(NAME compare_std COMMAND compare_std)
