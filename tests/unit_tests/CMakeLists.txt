add_executable(resampling_test
    time_series_resampling_test.cpp
)

add_executable(operation_test
  time_series_operation_test.cpp
)

add_executable(time_series_view_test
  time_series_view_test.cpp
)

add_executable(time_series_stats_test
    time_series_stats_test.cpp
)

add_executable(time_series_analysis_test
    time_series_analysis_test.cpp
)

add_executable(ar_model_test
    ar_model_test.cpp
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

target_link_libraries(time_series_stats_test
    PRIVATE
        finlib_core
        gtest_main
)

target_link_libraries(time_series_analysis_test
    PRIVATE
        finlib_core
        finlib_analysis
        gtest_main
)

target_link_libraries(ar_model_test
    PRIVATE
        finlib_core
        finlib_analysis
        finlib_models
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

add_test(
    NAME TimeSeriesStatsTest
    COMMAND time_series_stats_test
)

add_test(
    NAME TimeSeriesAnalysisTest
    COMMAND time_series_analysis_test
)

add_test(
    NAME ARModelTest
    COMMAND ar_model_test
)

set_tests_properties(
    TimeSeriesResamplingTest
    TimeSeriesOperationTest
    TimeSeriesViewTest
    TimeSeriesStatsTest
    TimeSeriesAnalysisTest
    ARModelTest
    PROPERTIES
        LABELS "unit"
      )
