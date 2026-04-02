# finapp/CMakeLists.txt — Finance application (Domain B)
# Depends on FinLib (Domain A) targets: finlib_core, finlib_data, etc.

# --------------------
# Providers library
# --------------------

add_library(finapp_providers
    src/data/providers/YFinanceProvider.cpp
)

target_include_directories(finapp_providers
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(finapp_providers
    PUBLIC
        finlib_data
)
