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


target_link_libraries(finapp_providers
    PUBLIC
        finlib_data
        finapp_core
    PRIVATE
        pybind11::embed
)

# Helper Python scripts live here; the embedded interpreter adds this to sys.path so
# py::module_::import("YFinanceFetcher") resolves YFinanceFetcher.py at runtime.
target_compile_definitions(finapp_providers
    PRIVATE
        FINAPP_PYTHON_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scripts"
)

# Optional: bake a default interpreter chosen at configure time (e.g.
# -DFINAPP_PYTHON=/path/to/venv/bin/python). The FINAPP_PYTHON / VIRTUAL_ENV
# environment variables still take precedence at runtime.
if(FINAPP_PYTHON)
    target_compile_definitions(finapp_providers
        PRIVATE
            FINAPP_PYTHON_DEFAULT="${FINAPP_PYTHON}"
    )
endif()

# --------------------
# Manual end-to-end driver (Returns Features over a live AAPL portfolio)
# --------------------
# Not a unit test — a hand-runnable binary that pushes the Returns Features /
# Returns Transforms computation through the full service loop against live
# yfinance data. Requires Python + yfinance at runtime.
add_executable(finapp_returns_demo
    src/main.cpp
)

target_link_libraries(finapp_returns_demo
    PRIVATE
        finapp_service
        finapp_providers
        finapp_csv_repository
        finlib_analysis
)

