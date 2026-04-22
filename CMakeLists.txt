cmake_minimum_required(VERSION 3.20)
project(TestCalcBench LANGUAGES CXX)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(chunk_bench
    bench/chunk_bench_main.cpp
)

target_include_directories(chunk_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/bench
)

if (WIN32)
    target_compile_definitions(chunk_bench PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (MSVC)
    target_compile_options(chunk_bench PRIVATE /O2 /EHsc /W4)
else()
    target_compile_options(chunk_bench PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()

add_executable(expr_method_bench
    bench/expression_method_bench.cpp
)

target_include_directories(expr_method_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/bench
)

if (WIN32)
    target_compile_definitions(expr_method_bench PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (MSVC)
    target_compile_options(expr_method_bench PRIVATE /O2 /EHsc /W4)
else()
    target_compile_options(expr_method_bench PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()

add_executable(expr_unit_tests
    tests/expr_unit_tests.cpp
)

target_include_directories(expr_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/bench
)

if (WIN32)
    target_compile_definitions(expr_unit_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (MSVC)
    target_compile_options(expr_unit_tests PRIVATE /O2 /EHsc /W4)
else()
    target_compile_options(expr_unit_tests PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if (BUILD_TESTING)
    add_test(
        NAME chunk_bench_quick_smoke
        COMMAND chunk_bench --preset quick --iterations 4 --target-ms 20
    )
    add_test(
        NAME expr_method_bench_quick_smoke
        COMMAND expr_method_bench --preset quick
    )
    add_test(
        NAME expr_unit_tests
        COMMAND expr_unit_tests
    )
endif()
