cmake_minimum_required(VERSION 3.14)

project(FinancialTimeSeriesLib VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# --- Options ---
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF) # Off by default to save time
option(BUILD_PYTHON "Build Python bindings" ON)

# --- Dependencies ---
# Find Eigen, OpenMP, etc. here
include(FetchContent)

# Eigen Content and Settings
FetchContent_Declare(
  Eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

set(BUILD_TESTING OFF)
set(EIGEN_BUILD_TESTING OFF)
set(EIGEN_MPL2_ONLY ON)
set(EIGEN_BUILD_PKGCONFIG OFF)
set(EIGEN_BUILD_DOC OFF)

# Download and make Eigen available in your project
FetchContent_MakeAvailable(Eigen)

#Pybind11

set(Python3_VIRTUALENV ONLY)
set(Python3_EXECUTABLE "${CMAKE_SOURCE_DIR}/.venv/bin/python")
set(PYBIND11_FINDPYTHON ON)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11
  GIT_TAG        v2.13.1
)

FetchContent_MakeAvailable(pybind11)

# --- Subdirectories ---

add_subdirectory(finlib)  # FinLib — Domain A (generic time series)
add_subdirectory(finapp)  # FinApp — Domain B (finance application)

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
