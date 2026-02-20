add_executable(resampling_test
    time_series_resampling_test.cpp
)

add_executable(operation_test
  time_series_operation_test.cpp
)

add_executable(time_series_view_test
  time_series_view_test.cpp
)

target_link_libraries(time_series_view_test
    PRIVATE
        finlib_core
        gtest_main
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

add_test(
  NAME TimeSeriesViewTest
    COMMAND time_series_view_test
)

set_tests_properties(
    TimeSeriesResamplingTest
    TimeSeriesOperationTest
    TimeSeriesViewTest
    PROPERTIES
        LABELS "unit"
      )
