add_executable(resampling_test
    time_series_resampling_test.cpp
)

add_executable(operation_test
  time_series_operation_test.cpp
)
target_link_libraries(resampling_test
    PRIVATE
        finlib_core
        gtest_main
)

target_link_libraries(operation_test
    PRIVATE
        finlib_core
        gtest_main
)

add_test(
  NAME TimeSeriesResamplingTest
    COMMAND resampling_test
)

add_test(
  NAME TimeSeriesOperationTest
    COMMAND operation_test
)

set_tests_properties(
    TimeSeriesResamplingTest
    TimeSeriesOperationTest
    PROPERTIES
        LABELS "unit"
      )
