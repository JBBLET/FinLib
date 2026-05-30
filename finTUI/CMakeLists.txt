add_library(finTUI_uiComponents STATIC
    src/uiComponents/TuiFormDialog.cpp
    src/uiComponents/TuiConfirmationDialog.cpp
    src/uiComponents/TuiTabbedPanel.cpp
    src/uiComponents/TuiTable.cpp
    src/uiComponents/TuiLineChart.cpp
    src/finTuiUtils/FinTuiUtils.cpp
    src/uiComponents/TuiFileDialog.cpp
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
)
target_link_libraries(finTUI_portfolioModule PUBLIC finTUI_uiComponents)

add_executable(finTUI_main
    src/main.cpp
    src/dataSources/GrpcPortfolioDataSource.cpp
)
target_link_libraries(finTUI_main
    PRIVATE finTUI_portfolioModule
    PRIVATE finTUI_appShell
    PRIVATE portfolio_service_proto
    PRIVATE grpc++
)
