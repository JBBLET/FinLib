add_executable(test_yfinance_provider
    test_yfinance_provider.cpp
)

target_link_libraries(test_yfinance_provider
    PRIVATE
        finlib_data
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
