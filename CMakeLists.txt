cmake_minimum_required(VERSION 3.14)

project(FinancialTimeSeriesLib VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Options ---
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF) # Off by default to save time
option(BUILD_PYTHON "Build Python bindings" ON)

# --- Dependencies ---
# Find Eigen, OpenMP, etc. here
include(FetchContent)

# --- Subdirectories ---
add_subdirectory(src) # Always build the library

if(BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/heads/main.zip
    )
    FetchContent_MakeAvailable(googletest)
    add_subdirectory(tests)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
