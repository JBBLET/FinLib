add_library(finTUI_uiComponents STATIC
    src/uiComponents/TuiFormDialog.cpp
    src/uiComponents/TuiConfirmationDialog.cpp
    src/uiComponents/TuiTabbedPanel.cpp
    src/uiComponents/TuiTable.cpp
    src/uiComponents/TuiLineChart.cpp
    src/uiComponents/TuiFileDialog.cpp
    src/finTuiUtils/FinTuiUtils.cpp
    src/finTuiUtils/DateUtils.cpp
    src/finTuiUtils/FormatUtils.cpp
    src/finTuiUtils/TuiChartUtils.cpp
)
target_include_directories(finTUI_uiComponents PUBLIC include)
target_link_libraries(finTUI_uiComponents PUBLIC ftxui::component)

add_library(finTUI_appShell STATIC
    src/AppShell.cpp
)
target_include_directories(finTUI_appShell PUBLIC include)
target_link_libraries(finTUI_appShell PUBLIC ftxui::component)

add_library(finTUI_portfolioModule STATIC
    src/modules/portfolioModule/PortfolioModule.cpp
    src/modules/portfolioModule/Dialogs.cpp
    src/modules/portfolioModule/TransactionsPane.cpp
    src/modules/portfolioModule/ChartPane.cpp
    src/finTuiUtils/CurrencyUtils.cpp
)
target_link_libraries(finTUI_portfolioModule
    PUBLIC finTUI_uiComponents
    PUBLIC portfolio_service_proto
)

add_library(finTUI_grpcDataSources STATIC
    src/dataSources/GrpcPortfolioDataSource.cpp
)
target_link_libraries(finTUI_grpcDataSources
    PUBLIC finTUI_portfolioModule
    PUBLIC portfolio_service_proto
    PUBLIC grpc++
)

add_executable(finTUI_main src/main.cpp)
target_link_libraries(finTUI_main
    PRIVATE finTUI_grpcDataSources
    PRIVATE finTUI_appShell
)
