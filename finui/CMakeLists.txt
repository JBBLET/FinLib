add_executable(finui_main src/main.cpp)

target_link_libraries(finui_main
    PRIVATE portfolio_service_proto
    PRIVATE grpc++
)
