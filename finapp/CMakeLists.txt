# finapp/CMakeLists.txt — Finance application (Domain B)
# Depends on FinLib (Domain A) targets: finlib_core, finlib_data, etc.

# --------------------
# Core library
# --------------------

add_library(finapp_core
    src/finance/portfolio/Portfolio.cpp
    src/finance/portfolio/Transaction.cpp
    src/finance/common/Currency.cpp
    src/finance/asset/AssetType.cpp
    src/common/Exception.cpp
)

target_include_directories(finapp_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(finapp_core
    PUBLIC
        finlib_core
    PRIVATE
        stdc++exp
)

# --------------------
# Service library
# --------------------

add_library(finapp_service
    src/service/FXService.cpp
    src/service/AssetService.cpp
    src/service/PortfolioService.cpp
    src/service/analysisService/AssetAnalysisService.cpp
    src/service/analysisService/PortfolioAnalysisService.cpp
    src/service/analysisService/EquityAnalysisService.cpp
    src/finance/analysis/IAssetAnalysis.cpp
    src/finance/analysis/EquityAnalysis.cpp
    src/finance/analysis/PortfolioAnalysis.cpp
    src/finance/analysis/ReturnFeatures.cpp
    src/finance/analysis/ReturnTransforms.cpp
    src/finance/stats/FinanceStats.cpp
)

target_include_directories(finapp_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(finapp_service
    PUBLIC
        finapp_core
        finlib_data
        finlib_session
    PRIVATE
        finlib_analysis
)

# --------------------
# CSV repository library
# --------------------

add_library(finapp_csv_repository
    src/data/repository/implementation/CsvRepository/CSVCashRepository.cpp
    src/data/repository/implementation/CsvRepository/CSVFXRepository.cpp
    src/data/repository/implementation/CsvRepository/CSVEquityRepository.cpp
    src/data/repository/implementation/CsvRepository/CSVPortfolioRepository.cpp
)
target_include_directories(finapp_csv_repository
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(finapp_csv_repository
    PUBLIC
        finapp_core
)

# --------------------
# Importers library
# --------------------

add_library(finapp_importers
    src/data/importers/YahooFinanceImporter.cpp
)

target_include_directories(finapp_importers
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(finapp_importers
    PUBLIC
        finapp_core
        finlib_core
)

# --------------------
# Providers library
# --------------------

add_library(finapp_providers
    src/data/providers/Yfinance/YFinanceProvider.cpp
    src/data/providers/Yfinance/YFinanceEquityProvider.cpp
)

target_include_directories(finapp_providers
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Embed Python helper scripts into the binary at build time so finapp resolves them
# with no on-disk path assumptions (works when consumed as a dependency). CMake reads
# the real .py files and generates a constexpr registry; the .py files stay the source
# of truth. Add future scripts to FINAPP_EMBEDDED_SCRIPTS — no C++ changes needed.
set(FINAPP_EMBEDDED_SCRIPTS
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/YFinanceFetcher.py
    # add future scripts here
)
set(FINAPP_EMBEDDED_ENTRIES "")
foreach(_script ${FINAPP_EMBEDDED_SCRIPTS})
    get_filename_component(_name ${_script} NAME_WE)
    file(READ ${_script} _src)
    string(APPEND FINAPP_EMBEDDED_ENTRIES
        "    {\"${_name}\", R\"PYSRC(\n${_src}\n)PYSRC\"},\n")
endforeach()
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbeddedPythonScripts.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/finapp/EmbeddedPythonScripts.hpp @ONLY)
# Re-run configure (regenerating the header) whenever an embedded script changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${FINAPP_EMBEDDED_SCRIPTS})

target_include_directories(finapp_providers
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Optional: bake a default Python interpreter chosen at configure time
# (e.g. -DFINAPP_PYTHON=/path/to/venv/bin/python). The runtime env vars
# FINAPP_PYTHON / VIRTUAL_ENV still take precedence over this default.
if(FINAPP_PYTHON)
    target_compile_definitions(finapp_providers
        PRIVATE
            FINAPP_PYTHON_DEFAULT="${FINAPP_PYTHON}"
    )
endif()

# --------------------------------------------------------------------------------------
# Validate the target Python interpreter at configure time, so a missing runtime
# dependency (or a version mismatch) surfaces now instead of on first provider call.
# Required packages come from the project's pyproject.toml — the single source of truth.
# --------------------------------------------------------------------------------------
option(FINAPP_REQUIRE_PYTHON_DEPS
    "Fail configuration if the target Python interpreter is missing finapp's required packages" OFF)

# Validate the baked runtime default if given, else the build-time Python (which is the
# runtime fallback when no env override is set).
if(FINAPP_PYTHON)
    set(_finapp_runtime_python "${FINAPP_PYTHON}")
else()
    set(_finapp_runtime_python "${Python3_EXECUTABLE}")
endif()

get_filename_component(_finapp_repo_root "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
set(_finapp_pyproject "${_finapp_repo_root}/pyproject.toml")

if(_finapp_runtime_python AND EXISTS "${_finapp_pyproject}")
    # Warn on a libpython/venv version mismatch: finapp links libpython of Python3_EXECUTABLE,
    # so a runtime interpreter of a different X.Y cannot import that version's site-packages.
    if(FINAPP_PYTHON AND NOT FINAPP_PYTHON STREQUAL Python3_EXECUTABLE)
        execute_process(COMMAND "${Python3_EXECUTABLE}" -c
            "import sys;print('%d.%d' % sys.version_info[:2])"
            OUTPUT_VARIABLE _build_pyver OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        execute_process(COMMAND "${_finapp_runtime_python}" -c
            "import sys;print('%d.%d' % sys.version_info[:2])"
            OUTPUT_VARIABLE _run_pyver OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _run_rc ERROR_QUIET)
        if(_run_rc EQUAL 0 AND _build_pyver AND NOT _build_pyver VERSION_EQUAL _run_pyver)
            message(WARNING
                "finapp: build-time Python is ${_build_pyver} but FINAPP_PYTHON is ${_run_pyver}. "
                "The embedded interpreter links libpython ${_build_pyver}; a different runtime "
                "version will fail to import packages. Point Python3_EXECUTABLE at the same venv.")
        endif()
    endif()

    # Verify the pyproject.toml dependencies are installed in the target interpreter.
    execute_process(
        COMMAND "${_finapp_runtime_python}"
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_python_deps.py"
                "${_finapp_pyproject}"
        OUTPUT_VARIABLE _finapp_missing OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _finapp_dep_rc
        ERROR_VARIABLE _finapp_dep_err)
    if(_finapp_dep_rc EQUAL 1 AND _finapp_missing)
        string(CONCAT _finapp_dep_msg
            "finapp: interpreter '${_finapp_runtime_python}' is missing Python package(s) from "
            "pyproject.toml: ${_finapp_missing}.\n"
            "  Install with:  ${_finapp_runtime_python} -m pip install ${_finapp_missing}")
        if(FINAPP_REQUIRE_PYTHON_DEPS)
            message(FATAL_ERROR "${_finapp_dep_msg}")
        else()
            message(WARNING "${_finapp_dep_msg}")
        endif()
    elseif(_finapp_dep_rc EQUAL 0)
        message(STATUS "finapp: pyproject.toml Python deps satisfied in '${_finapp_runtime_python}'")
    else()
        message(STATUS "finapp: could not verify Python deps (${_finapp_runtime_python}): ${_finapp_dep_err}")
    endif()
endif()

target_link_libraries(finapp_providers
    PUBLIC
        finlib_data
        finapp_core
    PRIVATE
        pybind11::embed
)

