add_executable(finui_main
    src/main.cpp
    src/GrpcPortfolioDataSource.cpp
)

target_include_directories(finui_main PRIVATE include)

target_link_libraries(finui_main
    PRIVATE portfolio_service_proto
    PRIVATE grpc++
    PRIVATE ftxui::component
)
