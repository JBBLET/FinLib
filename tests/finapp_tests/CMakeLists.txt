add_executable(test_yfinance_provider
    test_yfinance_provider.cpp
)

target_link_libraries(test_yfinance_provider
    PRIVATE
        finapp_providers
        gtest_main
)

add_test(
    NAME YFinanceIntegrationTest
    COMMAND test_yfinance_provider
)

set_tests_properties(
    YFinanceIntegrationTest
    PROPERTIES
        LABELS "integration"
)

# --------------------
# CSV Repository Tests
# --------------------

add_executable(test_csv_equity_repository
    csv_equity_repository_test.cpp
)

target_link_libraries(test_csv_equity_repository
    PRIVATE
        finapp_csv_repository
        gtest_main
)

add_test(NAME CSVEquityRepositoryTest COMMAND test_csv_equity_repository)

add_executable(test_csv_cash_repository
    csv_cash_repository_test.cpp
)

target_link_libraries(test_csv_cash_repository
    PRIVATE
        finapp_csv_repository
        gtest_main
)

add_test(NAME CSVCashRepositoryTest COMMAND test_csv_cash_repository)

add_executable(test_csv_fx_repository
    csv_fx_repository_test.cpp
)

target_link_libraries(test_csv_fx_repository
    PRIVATE
        finapp_csv_repository
        gtest_main
)

add_test(NAME CSVFXRepositoryTest COMMAND test_csv_fx_repository)

add_executable(test_csv_portfolio_repository
    csv_portfolio_repository_test.cpp
)

target_link_libraries(test_csv_portfolio_repository
    PRIVATE
        finapp_csv_repository
        gtest_main
)

add_test(NAME CSVPortfolioRepositoryTest COMMAND test_csv_portfolio_repository)

# --------------------
# Service Tests
# --------------------

add_executable(test_fx_service
    fx_service_test.cpp
)

target_include_directories(test_fx_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(test_fx_service
    PRIVATE
        finapp_service
        finapp_core
        finlib_core
        finlib_data
        gtest_main
)

add_test(NAME FXServiceTest COMMAND test_fx_service)

add_executable(test_asset_service
    asset_service_test.cpp
)

target_include_directories(test_asset_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(test_asset_service
    PRIVATE
        finapp_service
        finapp_core
        finlib_core
        finlib_data
        gtest_main
)

add_test(NAME AssetServiceTest COMMAND test_asset_service)

add_executable(test_portfolio_service
    portfolio_service_test.cpp
)

target_include_directories(test_portfolio_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(test_portfolio_service
    PRIVATE
        finapp_service
        finapp_core
        finlib_core
        finlib_data
        gtest_main
)

add_test(NAME PortfolioServiceTest COMMAND test_portfolio_service)
