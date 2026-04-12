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
