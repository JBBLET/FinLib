add_executable(test_yfinance_provider
    test_yfinance_provider.cpp
)

target_link_libraries(test_yfinance_provider
    PRIVATE
        finlib_data
        gtest_main
)

add_executable(csv_repository_test
    csv_repository_test.cpp
)

target_link_libraries(csv_repository_test
    PRIVATE
        finlib_data
        finlib_core
        gtest_main
)

add_test(
    NAME YFinanceIntegrationTest
    COMMAND test_yfinance_provider
)

add_test(
    NAME CSVRepositoryTest
    COMMAND csv_repository_test
)

set_tests_properties(
    YFinanceIntegrationTest
    CSVRepositoryTest
    PROPERTIES
        LABELS "integration"
)
