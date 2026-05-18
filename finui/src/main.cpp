// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <ctime>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "finui/GrpcPortfolioDataSource.hpp"

using namespace ftxui;

namespace {

std::string fmtMoney(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << v;
    return oss.str();
}

std::string fmtDate(int64_t ms) {
    auto tp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

Element summaryPanel(const finui::PortfolioSummary& s) {
    if (s.id.empty()) {
        return vbox({
            filler(),
            text("  Select a portfolio and press Enter") | dim,
            filler(),
        });
    }

    // Cash balances
    Elements cashRows;
    for (const auto& c : s.cashBalances) {
        cashRows.push_back(hbox({
            text("  " + c.currency + "  ") | bold,
            text(fmtMoney(c.amount)),
        }));
    }
    if (cashRows.empty()) cashRows.push_back(text("  —") | dim);

    // Positions table
    Elements posRows;
    posRows.push_back(hbox({
        text(" Ticker") | bold | size(WIDTH, EQUAL, 10),
        text("Quantity") | bold | size(WIDTH, EQUAL, 14),
        text("Value") | bold | size(WIDTH, EQUAL, 14),
        text("Weight") | bold | size(WIDTH, EQUAL, 9),
    }));
    posRows.push_back(separator());
    if (s.positions.empty()) {
        posRows.push_back(text("  No positions") | dim);
    } else {
        for (const auto& p : s.positions) {
            posRows.push_back(hbox({
                text(" " + p.ticker) | size(WIDTH, EQUAL, 10),
                text(fmtMoney(p.quantity)) | size(WIDTH, EQUAL, 14),
                text(fmtMoney(p.value)) | size(WIDTH, EQUAL, 14),
                text(fmtMoney(p.weight * 100.0) + "%") | size(WIDTH, EQUAL, 9),
            }));
        }
    }

    return vbox({
        hbox(
            {text(" Total Value  ") | bold, text(fmtMoney(s.totalValue) + " " + s.baseCurrency) | color(Color::Green)}),
        separator(),
        text(" Cash") | bold,
        vbox(std::move(cashRows)),
        separator(),
        text(" Positions") | bold,
        vbox(std::move(posRows)),
    });
}

Element transactionsPanel(const std::vector<finui::TransactionRow>& txns) {
    if (txns.empty()) {
        return vbox({
            filler(),
            text("  No transactions — press Enter to load") | dim,
            filler(),
        });
    }

    Elements rows;
    rows.push_back(hbox({
        text(" Date") | bold | size(WIDTH, EQUAL, 13),
        text("Type") | bold | size(WIDTH, EQUAL, 12),
        text("Ticker") | bold | size(WIDTH, EQUAL, 10),
        text("Qty") | bold | size(WIDTH, EQUAL, 12),
        text("Price") | bold | size(WIDTH, EQUAL, 12),
        text("Fees") | bold | size(WIDTH, EQUAL, 10),
        text("CCY") | bold | size(WIDTH, EQUAL, 5),
    }));
    rows.push_back(separator());
    for (const auto& t : txns) {
        rows.push_back(hbox({
            text(" " + fmtDate(t.timestampMs)) | size(WIDTH, EQUAL, 13),
            text(t.type) | size(WIDTH, EQUAL, 12),
            text(t.ticker) | size(WIDTH, EQUAL, 10),
            text(fmtMoney(t.quantity)) | size(WIDTH, EQUAL, 12),
            text(fmtMoney(t.pricePerUnit)) | size(WIDTH, EQUAL, 12),
            text(fmtMoney(t.fees)) | size(WIDTH, EQUAL, 10),
            text(t.currency) | size(WIDTH, EQUAL, 5),
        }));
    }
    return vbox(std::move(rows)) | vscroll_indicator | frame;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string serverAddr = (argc > 1) ? argv[1] : "localhost:50051";

    auto channel = grpc::CreateChannel(serverAddr, grpc::InsecureChannelCredentials());
    auto dataSource = std::make_shared<finui::GrpcPortfolioDataSource>(std::move(channel));

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    std::vector<std::string> portfolioIds;
    int selectedPortfolio = 0;
    int activeTab = 0;
    finui::PortfolioSummary currentSummary;
    std::vector<finui::TransactionRow> currentTransactions;
    std::string statusMsg;

    auto loadPortfolioList = [&] {
        try {
            portfolioIds = dataSource->listPortfolioIds();
            selectedPortfolio = 0;
            currentSummary = {};
            currentTransactions.clear();
            statusMsg = portfolioIds.empty()
                            ? "No portfolios found."
                            : std::to_string(portfolioIds.size()) + " portfolio(s). Press Enter to load.";
        } catch (const std::exception& e) {
            statusMsg = std::string("Error: ") + e.what();
        }
    };

    auto loadSelectedPortfolio = [&] {
        if (portfolioIds.empty()) return;
        const std::string& id = portfolioIds[selectedPortfolio];
        try {
            currentSummary = dataSource->loadSummary(id);
            currentTransactions.clear();  // invalidate on portfolio switch
            if (activeTab == 1) {
                currentTransactions = dataSource->listTransactions(id);
            }
            statusMsg = "Loaded: " + currentSummary.name;
        } catch (const std::exception& e) {
            statusMsg = std::string("Error loading portfolio: ") + e.what();
        }
    };

    // Initial load
    loadPortfolioList();
    if (!portfolioIds.empty()) loadSelectedPortfolio();

    auto screen = ScreenInteractive::Fullscreen();

    // -------------------------------------------------------------------------
    // Components
    // -------------------------------------------------------------------------
    MenuOption menuOpt = MenuOption::Vertical();
    menuOpt.on_enter = [&] { loadSelectedPortfolio(); };
    auto portfolioMenu = Menu(&portfolioIds, &selectedPortfolio, menuOpt);

    auto app = CatchEvent(portfolioMenu, [&](Event e) {
        if (e == Event::Character('q')) {
            screen.Exit();
            return true;
        }
        if (e == Event::Character('r')) {
            loadPortfolioList();
            if (!portfolioIds.empty()) loadSelectedPortfolio();
            return true;
        }
        if (e == Event::Character('1')) {
            activeTab = 0;
            return true;
        }
        if (e == Event::Character('2')) {
            activeTab = 1;
            // Lazy-load transactions when switching to that tab
            if (currentTransactions.empty() && !currentSummary.id.empty()) {
                try {
                    currentTransactions = dataSource->listTransactions(currentSummary.id);
                } catch (const std::exception& ex) {
                    statusMsg = std::string("Error loading transactions: ") + ex.what();
                }
            }
            return true;
        }
        return false;
    });

    // -------------------------------------------------------------------------
    // Renderer
    // -------------------------------------------------------------------------
    auto renderer = Renderer(app, [&] {
        const std::string tabLabel1 = activeTab == 0 ? "[1:Summary]" : " 1:Summary ";
        const std::string tabLabel2 = activeTab == 1 ? "[2:Transactions]" : " 2:Transactions ";

        Element rightContent = activeTab == 0 ? summaryPanel(currentSummary) : transactionsPanel(currentTransactions);

        return vbox({
            // Title bar
            text(" FinUI  —  Portfolio Manager") | bold | inverted,
            // Main area
            hbox({
                // Left: portfolio list
                vbox({
                    text(" Portfolios") | bold,
                    separator(),
                    portfolioIds.empty() ? (text("  (none)") | dim) : portfolioMenu->Render(),
                }) | size(WIDTH, EQUAL, 26),
                // Divider
                separator(),
                // Right: tab content
                vbox({
                    hbox({
                        text(" "),
                        text(tabLabel1) | (activeTab == 0 ? bold : dim),
                        text("  "),
                        text(tabLabel2) | (activeTab == 1 ? bold : dim),
                    }),
                    separator(),
                    rightContent | flex_grow,
                }) | flex_grow,
            }) | flex_grow,
            // Status bar
            separator(),
            hbox({
                text("  " + statusMsg) | flex_grow,
                text(" ↑↓ nav  Enter load  1/2 tabs  r refresh  q quit ") | dim,
            }),
        });
    });

    screen.Loop(renderer);
    return 0;
}
