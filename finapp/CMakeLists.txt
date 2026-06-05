# finapp/CMakeLists.txt — Finance application (Domain B)
# Depends on FinLib (Domain A) targets: finlib_core, finlib_data, etc.

# --------------------
# Core library
# --------------------

add_library(finapp_core
    src/finance/portfolio/Portfolio.cpp
)

target_include_directories(finapp_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


# --------------------
# Service library
# --------------------

add_library(finapp_service
    src/service/FXService.cpp
    src/service/AssetService.cpp
    src/service/PortfolioService.cpp
)

target_include_directories(finapp_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(finapp_service
    PUBLIC
        finapp_core
        finlib_data
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

