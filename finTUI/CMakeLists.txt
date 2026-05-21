add_executable(finTUI_main
    src/main.cpp
    src/GrpcPortfolioDataSource.cpp
    src/uiComponents/TUIFormDialog.cpp
    src/finTuiElements/Dialogs.cpp
)

target_include_directories(finTUI_main PRIVATE include)

target_link_libraries(finTUI_main
    PRIVATE portfolio_service_proto
    PRIVATE grpc++
    PRIVATE ftxui::component
)
