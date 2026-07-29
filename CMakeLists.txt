cmake_minimum_required(VERSION 3.14)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(FinancialTimeSeriesLib VERSION 0.1)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(PROJECT_IS_TOP_LEVEL ON)
    else()
        set(PROJECT_IS_TOP_LEVEL OFF)
    endif()
endif()

if(PROJECT_IS_TOP_LEVEL)
    include(DevBuild)
    finlib_dev_predeps()
endif()

# --------------------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------------------
include(FetchContent)

# Eigen — public dependency of finlib_core.
if(NOT TARGET Eigen3::Eigen)
    set(BUILD_TESTING OFF)
    set(EIGEN_BUILD_TESTING OFF)
    set(EIGEN_MPL2_ONLY ON)
    set(EIGEN_BUILD_PKGCONFIG OFF)
    set(EIGEN_BUILD_DOC OFF)
    FetchContent_Declare(
        Eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(Eigen)
endif()

# CppUtils — header-only CSV reader/writer (target cpp::csv). Pulled from the sibling repo.
if(NOT TARGET cpp::csv)
    # Sibling project under active local development — consume the live source tree directly
    # (SOURCE_DIR uses the working directory as-is, no clone of committed state).
    FetchContent_Declare(
        CppUtils
        SOURCE_DIR /home/jbblet/user/Documents/Projects/C++/CppUtils)
    FetchContent_MakeAvailable(CppUtils)
endif()

# Python + pybind11 — needed by finapp_providers' embedded interpreter.
set(PYBIND11_FINDPYTHON ON)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
if(NOT TARGET pybind11::embed)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11
        GIT_TAG        v2.13.1)
    FetchContent_MakeAvailable(pybind11)
endif()

# --------------------------------------------------------------------------------------
# Library targets — the consumable surface.
# --------------------------------------------------------------------------------------
add_subdirectory(finlib)  # FinLib — Domain A (generic time series)
add_subdirectory(finapp)  # FinApp — Domain B (finance application)

# --------------------------------------------------------------------------------------
# Dev/standalone-only targets (tests, benchmarks), registered after the libraries exist.
# --------------------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
    # Must be at directory scope, before the tests subdirectory is added, so that ctest
    # finds a CTestTestfile.cmake at the root of the build tree.
    if(BUILD_TESTS)
        enable_testing()
    endif()
    finlib_dev_targets()
endif()
