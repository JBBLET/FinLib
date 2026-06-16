// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>
#include <vector>

#include "finTUI/AppShell.hpp"
#include "finTUI/dataSources/GrpcPortfolioSessionDataSource.hpp"
#include "finTUI/modules/portfolioModule/PortfolioModule.hpp"
#include "ftxui/component/screen_interactive.hpp"

int main(int argc, char** argv) {
    const std::string serverAddr = (argc > 1) ? argv[1] : "localhost:50051";
    auto channel = grpc::CreateChannel(serverAddr, grpc::InsecureChannelCredentials());
    auto sessionSource = std::make_shared<finui::GrpcPortfolioSessionDataSource>(std::move(channel));
    auto screen = ftxui::ScreenInteractive::Fullscreen();

    std::vector<std::shared_ptr<finui::IModule>> modules;
    modules.push_back(std::make_shared<finui::PortfolioModule>(sessionSource, sessionSource, screen));

    finui::AppShell shell(std::move(modules), screen);
    screen.Loop(shell.component());
    return 0;
}
