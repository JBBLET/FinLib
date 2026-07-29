# cmake/DevBuild.cmake — standalone development build configuration.

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
# enable_testing() is NOT called here — it sets a variable, which this function scope would
# discard, leaving the top of the build tree without a CTestTestfile.cmake. The caller runs
# it at directory scope instead.
function(finlib_dev_targets)
    if(BUILD_TESTS)
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
