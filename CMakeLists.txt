cmake_minimum_required(VERSION 3.14)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

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

# gRPC + Protobuf via FetchContent
set(FETCHCONTENT_QUIET OFF)
set(gRPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_CSHARP_EXT OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(absl_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(gRPC_INSTALL OFF CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG        v1.64.0
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(gRPC)
include(cmake/GrpcHelpers.cmake)
# --- Subdirectories ---

add_subdirectory(finlib)  # FinLib — Domain A (generic time series)
add_subdirectory(finapp)  # FinApp — Domain B (finance application)
add_subdirectory(finAppImplementation) # Example System using the FinApp into an actual project

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
