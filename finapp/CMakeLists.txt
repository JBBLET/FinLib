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
# Providers library
# --------------------

add_library(finapp_providers
    src/data/providers/YFinanceProvider.cpp
)

target_include_directories(finapp_providers
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(finapp_providers
    PUBLIC
        finlib_data
)

# --------------------
# CSV repository library
# --------------------

add_library(finapp_csv_repository
    src/data/repository/implementation/CsvRepository/CSVCashRepository.cpp
    src/data/repository/implementation/CsvRepository/CSVFXRepository.cpp
    src/data/repository/implementation/CsvRepository/CSVEquityRepository.cpp
)
target_include_directories(finapp_csv_repository
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(finapp_csv_repository
    PUBLIC
        finapp_core
)
