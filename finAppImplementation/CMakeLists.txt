add_grpc_service(portfolio_service_proto
    PROTO "${CMAKE_CURRENT_SOURCE_DIR}/protoFiles/portfolio.proto"
)

# Header-only converter library — bridges finance:: C++ types and finapp_rpc:: proto types.
add_library(portfolio_converters INTERFACE)
target_include_directories(portfolio_converters INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(portfolio_converters INTERFACE portfolio_service_proto finapp_service)

# gRPC service method implementations — depends on converters + grpc++.
add_library(portfolio_grpc_service_impl STATIC src/PortfolioGrpcServiceImpl.cpp)
target_link_libraries(portfolio_grpc_service_impl
    PUBLIC portfolio_converters
    PUBLIC grpc++
)

# Server entry point: wires CSV repos, YFinance providers, and starts the gRPC listener.
add_executable(finapp_runner src/main.cpp)
target_link_libraries(finapp_runner
    PRIVATE portfolio_grpc_service_impl
    PRIVATE finapp_csv_repository
    PRIVATE finapp_providers
    PRIVATE finapp_importers
    PRIVATE grpc++
)
