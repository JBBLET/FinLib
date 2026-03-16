# src/CMakeLists.txt

# --------------------
# Core library
# --------------------

add_library(finlib_core
    core/TimeSeries.cpp
    core/TimeSeriesView.cpp
    core/stats.cpp
    utils/time.cpp
)

target_include_directories(finlib_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
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
    analysis/TimeSeriesAnalysis.cpp
)

target_include_directories(finlib_analysis
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
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
    data/YFinanceProvider.cpp
)

target_link_libraries(finlib_data
    PUBLIC
        finlib_core
)

target_include_directories(finlib_data
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)


# --------------------
# Models library
# --------------------

add_library(finlib_models
    models/BaseModel.cpp
)

target_link_libraries(finlib_models
    PUBLIC
        finlib_core
        finlib_analysis
)

target_include_directories(finlib_models
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
