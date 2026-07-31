# finlib/CMakeLists.txt — Domain A (generic time series library)

# --------------------
# Core library
# --------------------

add_library(finlib_core
    src/core/TimeSeries.cpp
    src/core/TimeSeriesView.cpp
    src/core/Resampling.cpp
    src/core/StatsCore.cpp
    src/utils/TimeUtils.cpp
    src/utils/TimeSeriesUtils.cpp
)

target_include_directories(finlib_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(finlib_core
    PUBLIC
        Eigen3::Eigen
        cpp::utils
)

# --------------------
# Analysis library
# --------------------

add_library(finlib_analysis
    src/analysis/seriesAnalysis/TimeSeriesAnalysis.cpp
    src/analysis/simulation/Distribution.cpp
)

target_include_directories(finlib_analysis
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(finlib_analysis
    PUBLIC
        finlib_core
)

# --------------------
# Data library
# --------------------

add_library(finlib_data
    src/data/CSVRepository.cpp
    src/data/TimeSeriesService.cpp
    src/data/TimeRange.cpp
)

target_link_libraries(finlib_data
    PUBLIC
        finlib_core
    PRIVATE
        cpp::csv
)

target_include_directories(finlib_data
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --------------------
# Models library
# --------------------

add_library(finlib_models
    src/analysis/models/timeseries/regression/ARModel.cpp
    src/analysis/models/interfaces/EvaluationResult.cpp
)

target_link_libraries(finlib_models
    PUBLIC
        finlib_analysis
)

target_include_directories(finlib_models
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --------------------
# Session library
# --------------------

add_library(finlib_session
    src/analysis/session/ModelSession.cpp
    src/analysis/session/TimeSeriesSession.cpp
    src/analysis/session/MultiTimeSeriesSession.cpp
)

target_link_libraries(finlib_session
    PUBLIC
        finlib_models
        finlib_data
)

target_include_directories(finlib_session
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
