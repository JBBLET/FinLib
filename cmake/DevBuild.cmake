# cmake/DevBuild.cmake — standalone development build configuration.
#
# Included ONLY when FinLib is the top-level project (see PROJECT_IS_TOP_LEVEL in the
# root CMakeLists.txt). A consumer that pulls FinLib via add_subdirectory / FetchContent
# never runs any of this: it gets the library targets only and controls Python + testing
# itself. Keep everything dev/standalone-specific here so the consumable surface stays clean.

# Dev defaults applied BEFORE dependency resolution — they steer Python discovery and
# declare the test/benchmark options, so they must run before find_package(Python3) and
# the library subdirectories.
function(finlib_dev_predeps)
    set(_venv "${CMAKE_SOURCE_DIR}/.venv/bin/python")
    if(EXISTS "${_venv}")
        # Build-time: resolve Python from the in-repo virtualenv (unless the dev overrode it).
        if(NOT Python3_EXECUTABLE)
            set(Python3_VIRTUALENV ONLY PARENT_SCOPE)
            set(Python3_EXECUTABLE "${_venv}" PARENT_SCOPE)
        endif()
        # Runtime: make the same interpreter finapp's baked default (env vars still override).
        if(NOT FINAPP_PYTHON)
            set(FINAPP_PYTHON "${_venv}" CACHE FILEPATH "Default Python interpreter for finapp")
        endif()
    endif()
    option(BUILD_TESTS "Build unit tests" ON)
    option(BUILD_BENCHMARKS "Build benchmarks" OFF)
endfunction()

# Dev targets registered AFTER the library targets exist (tests link finlib_*/finapp_*).
function(finlib_dev_targets)
    if(BUILD_TESTS)
        enable_testing()
        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/heads/main.zip
        )
        FetchContent_MakeAvailable(googletest)
        add_subdirectory(${CMAKE_SOURCE_DIR}/tests ${CMAKE_BINARY_DIR}/tests)
    endif()
    if(BUILD_BENCHMARKS)
        add_subdirectory(${CMAKE_SOURCE_DIR}/benchmarks ${CMAKE_BINARY_DIR}/benchmarks)
    endif()
endfunction()
