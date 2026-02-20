# src/CMakeLists.txt

# --------------------
# Core library
# --------------------
add_library(finlib_core
    core/TimeSeries.cpp
    core/TimeSeriesView.cpp
    utils/time.cpp
)

target_include_directories(finlib_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(finlib_core PUBLIC cxx_std_20)

#add_library(finlib_models
#    models/AR.cpp
#    # models/GARCH.cpp later
#)

#target_link_libraries(finlib_models
#    PUBLIC
#        finlib_core
#)

#target_include_directories(finlib_models
#    PUBLIC
#        ${PROJECT_SOURCE_DIR}/include
#)

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


