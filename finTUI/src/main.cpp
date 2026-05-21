// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <grpcpp/grpcpp.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "finTUI/FinTuiElements/Dialogs.hpp"
#include "finTUI/dataSources/GrpcPortfolioDataSource.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

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

int64_t parseDate(const std::string& s) {
    std::tm tm{};
    std::istringstream ss(s);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (ss.fail()) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }
    time_t t = timegm(&tm);
    return static_cast<int64_t>(t) * 1000LL;
}

std::vector<std::string> asciiLineChart(const std::vector<double>& values, int w, int h) {
    std::vector<std::string> grid(h, std::string(w, ' '));
    if (values.empty() || w <= 0 || h <= 0) return grid;

    double minVal = *std::min_element(values.begin(), values.end());
    double maxVal = *std::max_element(values.begin(), values.end());
    if (maxVal <= minVal) maxVal = minVal + 1.0;

    auto sample = [&](int col) -> double {
        if ((int)values.size() == 1) return values[0];
        size_t idx = static_cast<size_t>(static_cast<double>(values.size() - 1) * col / (w - 1));
        return values[idx];
    };

    auto toRow = [&](double v) -> int {
        int row = static_cast<int>((1.0 - (v - minVal) / (maxVal - minVal)) * (h - 1) + 0.5);
        return std::max(0, std::min(h - 1, row));
    };

    for (int x = 0; x < w; ++x) {
        const int row = toRow(sample(x));
        if (x == 0) {
            grid[row][x] = '*';
        } else {
            const int prev = toRow(sample(x - 1));
            if (row == prev) {
                grid[row][x] = '-';
            } else if (row < prev) {
                grid[row][x] = '/';
                for (int r = row + 1; r < prev; ++r) grid[r][x] = '|';
            } else {
                for (int r = prev + 1; r < row; ++r) grid[r][x] = '|';
                grid[row][x] = '\\';
            }
        }
    }
    return grid;
}

// ─── Panel builders ───────────────────────────────────────────────────────────

Element buildLeftPanel(const std::vector<finui::PortfolioListEntry>& list, int selected) {
    Elements rows;
    for (int i = 0; i < (int)list.size(); ++i) {
        const auto& entry = list[i];
        const std::string label = " " + (entry.name.empty() ? entry.id : entry.name) + " ";
        auto row = text(label);
        rows.push_back(i == selected ? row | inverted : row);
    }
    if (rows.empty()) rows.push_back(text("  No portfolios") | dim);

    return window(text(" Portfolios "),
                  vbox({
                      vbox(std::move(rows)) | flex_grow,
                      filler(),
                      separator(),
                      hbox({
                          text(" n") | bold | color(Color::Green),
                          text(" New  "),
                          text("d") | bold | color(Color::Red),
                          text(" Del  "),
                          text("r") | bold,
                          text(" Reload "),
                      }) | dim,
                  })) |
           size(WIDTH, EQUAL, 32);
}

Element buildOverviewPanel(const finui::PortfolioSummary& s) {
    if (s.id.empty()) {
        return vbox({filler(), text("  Select a portfolio and press Enter") | dim, filler()});
    }

    Elements cashRows;
    for (const auto& c : s.cashBalances) {
        cashRows.push_back(hbox({
            text("  " + c.currency + ": ") | bold | size(WIDTH, EQUAL, 8),
            text(fmtMoney(c.amount)),
        }));
    }
    if (cashRows.empty()) cashRows.push_back(text("  —") | dim);

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
            {text(" Total Value: ") | bold, text(fmtMoney(s.totalValue) + " " + s.baseCurrency) | color(Color::Green)}),
        separator(),
        text(" Cash") | bold,
        vbox(std::move(cashRows)),
        separator(),
        text(" Positions") | bold,
        vbox(std::move(posRows)) | vscroll_indicator | frame | flex_grow,
        separator(),
        text(" 1 overview  2 transactions  3 chart") | dim,
    });
}

Element buildTransactionsPanel(const std::vector<finui::TransactionRow>& txns, int selected) {
    if (txns.empty()) {
        return vbox({
            filler(),
            text("  No transactions") | dim,
            filler(),
            separator(),
            text(" t Add   ↑↓ Navigate") | dim,
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
    for (int i = 0; i < (int)txns.size(); ++i) {
        const auto& t = txns[i];
        auto row = hbox({
            text(" " + fmtDate(t.timestampMs)) | size(WIDTH, EQUAL, 13),
            text(t.type) | size(WIDTH, EQUAL, 12),
            text(t.ticker) | size(WIDTH, EQUAL, 10),
            text(fmtMoney(t.quantity)) | size(WIDTH, EQUAL, 12),
            text(fmtMoney(t.pricePerUnit)) | size(WIDTH, EQUAL, 12),
            text(fmtMoney(t.fees)) | size(WIDTH, EQUAL, 10),
            text(t.currency) | size(WIDTH, EQUAL, 5),
        });
        rows.push_back(i == selected ? row | inverted : row);
    }

    return vbox({
        vbox(std::move(rows)) | vscroll_indicator | frame | flex_grow,
        separator(),
        text(" t Add   x Delete   ↑↓ Navigate") | dim,
    });
}

Element buildChartPanel(const finui::TimeSeriesData& ts) {
    if (ts.values.empty()) {
        return vbox({
            filler(),
            text("  No chart data — press 3 to load") | dim,
            filler(),
        });
    }

    constexpr int kW = 68;
    constexpr int kH = 12;
    const auto lines = asciiLineChart(ts.values, kW, kH);

    const double minVal = *std::min_element(ts.values.begin(), ts.values.end());
    const double maxVal = *std::max_element(ts.values.begin(), ts.values.end());

    Elements chartRows;
    for (int row = 0; row < kH; ++row) {
        std::string labelStr;
        if (row == 0 || row == kH / 2 || row == kH - 1) {
            const double v = maxVal - (maxVal - minVal) * row / (kH - 1);
            labelStr = fmtMoney(v);
            if ((int)labelStr.size() > 10) labelStr = labelStr.substr(0, 10);
        }
        std::string padding(10 - (int)labelStr.size(), ' ');
        chartRows.push_back(hbox({
            text(padding + labelStr) | size(WIDTH, EQUAL, 10),
            text("│"),
            text(row < (int)lines.size() ? lines[row] : std::string(kW, ' ')),
        }));
    }
    std::string xAxis(kW, '-');
    chartRows.push_back(hbox({
        text("          ") | size(WIDTH, EQUAL, 10),
        text("└"),
        text(xAxis),
    }));

    return vbox({
        text(" Portfolio Value — Last 30 Days") | bold,
        separator(),
        vbox(std::move(chartRows)),
        filler(),
        separator(),
        text(" r Reload   1 overview   2 transactions") | dim,
    });
}

Element buildDeleteTxnModal(const finui::TransactionRow& txn) {
    return window(text(" Delete Transaction "),
                  vbox({
                      text("  Deleting transaction:") | bold,
                      hbox({text("  Date:   "), text(fmtDate(txn.timestampMs))}),
                      hbox({text("  Type:   "), text(txn.type)}),
                      hbox({text("  Ticker: "), text(txn.ticker)}),
                      hbox({text("  Qty:    "), text(fmtMoney(txn.quantity))}),
                      separator(),
                      text("  y / Enter confirm   Esc cancel  ") | dim,
                  })) |
           size(WIDTH, EQUAL, 54);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string serverAddr = (argc > 1) ? argv[1] : "localhost:50051";
    auto channel = grpc::CreateChannel(serverAddr, grpc::InsecureChannelCredentials());
    auto dataSource = std::make_shared<finui::GrpcPortfolioDataSource>(std::move(channel));

    // ─── State ────────────────────────────────────────────────────────────────
    enum class AppMode { Normal, CreatePortfolio, DeletePortfolio, AddTransaction, DeleteTransaction };

    std::vector<finui::PortfolioListEntry> portfolioList;
    std::vector<std::string> portfolioMenuEntries;
    int selectedPortfolio = 0;
    int selectedTransaction = 0;
    int activeTab = 0;
    AppMode mode = AppMode::Normal;
    int modeAsInt = 0;

    finui::PortfolioSummary currentSummary;
    std::vector<finui::TransactionRow> currentTransactions;
    finui::TimeSeriesData currentTimeSeries;
    std::string statusMsg;

    // ─── Form state ───────────────────────────────────────────────────────────
    finui::CreatePortfolioForm createForm;
    finui::DeletePortfolioForm deleteForm;
    finui::AddTransactionForm addTxnForm;

    // ─── Load helpers ─────────────────────────────────────────────────────────
    auto enterNormal = [&] {
        mode = AppMode::Normal;
        modeAsInt = 0;
    };

    auto rebuildMenuEntries = [&] {
        portfolioMenuEntries.clear();
        for (const auto& e : portfolioList) portfolioMenuEntries.push_back(e.name.empty() ? e.id : e.name);
    };

    auto loadPortfolioList = [&] {
        try {
            portfolioList = dataSource->listPortfolios();
            rebuildMenuEntries();
            selectedPortfolio = 0;
            currentSummary = {};
            currentTransactions.clear();
            currentTimeSeries = {};
            statusMsg = portfolioList.empty() ? "No portfolios found."
                                              : std::to_string(portfolioList.size()) + " portfolio(s) loaded.";
        } catch (const std::exception& e) {
            statusMsg = std::string("Error: ") + e.what();
        }
    };

    auto loadSelectedPortfolio = [&] {
        if (portfolioList.empty()) return;
        try {
            currentSummary = dataSource->loadSummary(portfolioList[selectedPortfolio].id);
            currentTransactions.clear();
            currentTimeSeries = {};
            statusMsg = "Loaded: " + currentSummary.name;
        } catch (const std::exception& e) {
            statusMsg = std::string("Error: ") + e.what();
        }
    };

    auto loadTransactions = [&] {
        if (currentSummary.id.empty()) return;
        try {
            currentTransactions = dataSource->listTransactions(currentSummary.id);
            selectedTransaction = 0;
        } catch (const std::exception& e) {
            statusMsg = std::string("Error loading transactions: ") + e.what();
        }
    };

    auto loadTimeSeries = [&] {
        if (currentSummary.id.empty()) return;
        try {
            const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count();
            const int64_t startMs = nowMs - 30LL * 86'400'000LL;
            currentTimeSeries = dataSource->getTimeSeries(currentSummary.id, startMs, nowMs, 86'400'000LL);
        } catch (const std::exception& e) {
            statusMsg = std::string("Error loading chart: ") + e.what();
        }
    };

    // Initial load
    loadPortfolioList();
    if (!portfolioList.empty()) loadSelectedPortfolio();

    auto screen = ScreenInteractive::Fullscreen();

    // ─── Submit callbacks ─────────────────────────────────────────────────────
    auto submitCreatePortfolio = [&] {
        if (!createForm.name.empty()) {
            try {
                const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
                finui::CreatePortfolioParams p;
                p.name = createForm.name;
                p.currency = finui::kCurrencies[createForm.currencyIdx];
                p.timestampMs = createForm.date.empty() ? nowMs : parseDate(createForm.date);
                dataSource->createPortfolio(p);
                loadPortfolioList();
                statusMsg = "Created: " + createForm.name;
            } catch (const std::exception& ex) {
                statusMsg = std::string("Error: ") + ex.what();
            }
        }
        enterNormal();
    };

    auto submitDeletePortfolio = [&] {
        if (!portfolioList.empty() && deleteForm.confirmName == portfolioList[selectedPortfolio].name) {
            try {
                dataSource->deletePortfolio(portfolioList[selectedPortfolio].id);
                loadPortfolioList();
                statusMsg = "Portfolio deleted.";
            } catch (const std::exception& ex) {
                statusMsg = std::string("Error: ") + ex.what();
            }
        } else {
            statusMsg = "Name mismatch — portfolio not deleted.";
        }
        enterNormal();
    };

    auto submitAddTransaction = [&] {
        if (!addTxnForm.ticker.empty() && !addTxnForm.qty.empty()) {
            try {
                finui::AddTransactionParams p;
                p.timestampMs = addTxnForm.date.empty() ? 0 : parseDate(addTxnForm.date);
                p.type = finui::kTxnTypes[addTxnForm.typeIdx];
                p.ticker = addTxnForm.ticker;
                p.quantity = std::stod(addTxnForm.qty);
                p.pricePerUnit = addTxnForm.price.empty() ? 0.0 : std::stod(addTxnForm.price);
                p.fees = addTxnForm.fee.empty() ? 0.0 : std::stod(addTxnForm.fee);
                p.currency = "USD";
                dataSource->addTransaction(currentSummary.id, p);
                currentTransactions.clear();
                statusMsg = "Transaction added.";
            } catch (const std::exception& ex) {
                statusMsg = std::string("Error: ") + ex.what();
            }
        }
        enterNormal();
    };

    // ─── Dialogs ──────────────────────────────────────────────────────────────
    auto createDialog = finui::makeCreatePortfolioDialog(createForm, submitCreatePortfolio, enterNormal);
    auto deleteDialog = finui::makeDeletePortfolioDialog(deleteForm, submitDeletePortfolio, enterNormal);
    auto addTxnDialog = finui::makeAddTransactionDialog(addTxnForm, submitAddTransaction, enterNormal);

    auto createInputComp = createDialog.inputComponent();
    auto deleteInputComp = deleteDialog.inputComponent();
    auto addTxnInputComp = addTxnDialog.inputComponent();

    // ─── Portfolio menu ───────────────────────────────────────────────────────
    MenuOption menuOpt = MenuOption::Vertical();
    menuOpt.on_enter = [&] { loadSelectedPortfolio(); };
    auto portfolioMenu = Menu(&portfolioMenuEntries, &selectedPortfolio, menuOpt);

    // ─── App component ────────────────────────────────────────────────────────
    auto app = CatchEvent(Container::Tab(
                              {
                                  portfolioMenu,
                                  createInputComp,
                                  deleteInputComp,
                                  addTxnInputComp,
                                  Container::Horizontal({}),  // DeleteTransaction: no inputs
                              },
                              &modeAsInt),
                          [&](Event e) -> bool {
                              // ── Normal ──────────────────────────────────────────────────────
                              if (mode == AppMode::Normal) {
                                  if (e == Event::Character('q')) {
                                      screen.Exit();
                                      return true;
                                  }
                                  if (e == Event::Character('r')) {
                                      loadPortfolioList();
                                      if (!portfolioList.empty()) loadSelectedPortfolio();
                                      return true;
                                  }
                                  if (e == Event::Character('1')) {
                                      activeTab = 0;
                                      return true;
                                  }
                                  if (e == Event::Character('2')) {
                                      activeTab = 1;
                                      if (currentTransactions.empty() && !currentSummary.id.empty()) loadTransactions();
                                      return true;
                                  }
                                  if (e == Event::Character('3')) {
                                      activeTab = 2;
                                      if (currentTimeSeries.values.empty() && !currentSummary.id.empty())
                                          loadTimeSeries();
                                      return true;
                                  }
                                  if (e == Event::ArrowUp && activeTab == 1) {
                                      if (selectedTransaction > 0) --selectedTransaction;
                                      return true;
                                  }
                                  if (e == Event::ArrowDown && activeTab == 1) {
                                      if (selectedTransaction < (int)currentTransactions.size() - 1)
                                          ++selectedTransaction;
                                      return true;
                                  }
                                  if (e == Event::Character('n')) {
                                      createDialog.reset();
                                      mode = AppMode::CreatePortfolio;
                                      modeAsInt = 1;
                                      return true;
                                  }
                                  if (e == Event::Character('d') && !portfolioList.empty()) {
                                      deleteDialog.reset();
                                      mode = AppMode::DeletePortfolio;
                                      modeAsInt = 2;
                                      return true;
                                  }
                                  if (e == Event::Character('t') && !currentSummary.id.empty()) {
                                      addTxnDialog.reset();
                                      std::time_t t =
                                          std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                                      std::tm tm{};
                                      gmtime_r(&t, &tm);
                                      std::ostringstream oss;
                                      oss << std::put_time(&tm, "%Y-%m-%d");
                                      addTxnForm.date = oss.str();
                                      mode = AppMode::AddTransaction;
                                      modeAsInt = 3;
                                      return true;
                                  }
                                  if (e == Event::Character('x') && activeTab == 1 && !currentTransactions.empty()) {
                                      mode = AppMode::DeleteTransaction;
                                      modeAsInt = 4;
                                      return true;
                                  }
                                  return false;
                              }

                              // ── Modal modes: Escape always cancels ──────────────────────────
                              if (e == Event::Escape) {
                                  enterNormal();
                                  return true;
                              }

                              // ── Delete Transaction: no form inputs, handle confirm directly ─
                              if (mode == AppMode::DeleteTransaction) {
                                  if (e == Event::Character('y') || e == Event::Return) {
                                      if (!currentTransactions.empty()) {
                                          try {
                                              const auto& txn = currentTransactions[selectedTransaction];
                                              dataSource->deleteTransaction(currentSummary.id, txn.id);
                                              currentTransactions.clear();
                                              statusMsg = "Transaction deleted.";
                                          } catch (const std::exception& ex) {
                                              statusMsg = std::string("Error: ") + ex.what();
                                          }
                                      }
                                      enterNormal();
                                      return true;
                                  }
                                  return false;
                              }

                              return false;
                          });

    // ─── Renderer ─────────────────────────────────────────────────────────────
    auto renderer = Renderer(app, [&] {
        std::string modeLabel;
        switch (mode) {
            case AppMode::Normal:
                modeLabel = "NORMAL";
                break;
            case AppMode::CreatePortfolio:
                modeLabel = "INSERT";
                break;
            case AppMode::DeletePortfolio:
                modeLabel = "CONFIRM";
                break;
            case AppMode::AddTransaction:
                modeLabel = "INSERT";
                break;
            case AppMode::DeleteTransaction:
                modeLabel = "CONFIRM";
                break;
        }

        Element rightContent;
        switch (activeTab) {
            case 1:
                rightContent = buildTransactionsPanel(currentTransactions, selectedTransaction);
                break;
            case 2:
                rightContent = buildChartPanel(currentTimeSeries);
                break;
            default:
                rightContent = buildOverviewPanel(currentSummary);
                break;
        }

        auto tabBar = hbox({
            text(activeTab == 0 ? " [1] overview " : "  1  overview ") | (activeTab == 0 ? bold : dim),
            text(activeTab == 1 ? " [2] transactions " : "  2  transactions ") | (activeTab == 1 ? bold : dim),
            text(activeTab == 2 ? " [3] chart " : "  3  chart ") | (activeTab == 2 ? bold : dim),
            filler(),
            text(currentSummary.name.empty() ? "" : "  " + currentSummary.name + "  ") | inverted,
        });

        auto rightPanel = window(hbox({tabBar}), rightContent | flex_grow);

        auto statusBar = hbox({
            text(" finapp") | bold,
            text("  ·  " + std::to_string(portfolioList.size()) + " portfolios  ·  ") | dim,
            text(statusMsg) | dim | flex_grow,
            text("  [q] quit  [" + modeLabel + "]  ") | dim,
        });

        Element root = vbox({
            hbox({buildLeftPanel(portfolioList, selectedPortfolio), rightPanel | flex_grow}) | flex_grow,
            separator(),
            statusBar,
        });

        if (mode == AppMode::CreatePortfolio)
            root = dbox({root, createDialog.render() | clear_under | center});
        else if (mode == AppMode::DeletePortfolio && !portfolioList.empty())
            root = dbox({root, deleteDialog.render() | clear_under | center});
        else if (mode == AppMode::AddTransaction)
            root = dbox({root, addTxnDialog.render() | clear_under | center});
        else if (mode == AppMode::DeleteTransaction && !currentTransactions.empty())
            root = dbox({root, buildDeleteTxnModal(currentTransactions[selectedTransaction]) | clear_under | center});

        return root;
    });

    screen.Loop(renderer);
    return 0;
}
