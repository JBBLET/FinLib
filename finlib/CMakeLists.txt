# finlib/CMakeLists.txt — Domain A (generic time series library)

# --------------------
# Core library
# --------------------

add_library(finlib_core
    src/core/TimeSeries.cpp
    src/core/TimeSeriesView.cpp
    src/core/StatsCore.cpp
    src/utils/TimeUtils.cpp
)

target_include_directories(finlib_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(finlib_core
    PUBLIC
        Eigen3::Eigen
)
target_compile_features(finlib_core PUBLIC cxx_std_20)

# --------------------
# Analysis library
# --------------------

add_library(finlib_analysis
    src/analysis/TimeSeriesAnalysis.cpp
)

target_include_directories(finlib_analysis
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(finlib_analysis
    PUBLIC
        finlib_core
        Eigen3::Eigen
)

# --------------------
# Data library
# --------------------

add_library(finlib_data
    src/data/CSVRepository.cpp
    src/data/TimeSeriesService.cpp
)

target_link_libraries(finlib_data
    PUBLIC
        finlib_core
)

target_include_directories(finlib_data
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --------------------
# Models library
# --------------------

add_library(finlib_models
    src/models/timeseries/regression/ARModel.cpp
    src/models/interfaces/EvaluationResult.cpp
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
    src/session/ModelSession.cpp
    src/utils/LoggerUtils.cpp
)

target_link_libraries(finlib_session
    PUBLIC
        finlib_models
)

target_include_directories(finlib_session
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
