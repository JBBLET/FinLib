// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/modules/portfolioModule/PortfolioModule.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "finTUI/finTuiUtils/FinTuiUtils.hpp"
#include "finTUI/modules/portfolioModule/PortfolioModuleTypes.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace finui {

// ── Constructor ───────────────────────────────────────────────────────────────

PortfolioModule::PortfolioModule(std::shared_ptr<IPortfolioDataSource> dataSource, ftxui::ScreenInteractive& screen)
    : dataSource_(std::move(dataSource)),
      screen_(screen),
      createDialog_(makeCreatePortfolioDialog(
          createForm_, [this] { submitCreatePortfolio_(); }, [this] { enterNormal_(); })),
      addTransactionDialog_(makeAddTransactionDialog(
          addTxnForm_, [this] { submitAddTransaction_(); }, [this] { enterNormal_(); })),
      editTransactionDialog_(makeAddTransactionDialog(
          addTxnForm_, [this] { submitEditTransaction_(); }, [this] { enterNormal_(); })),
      deletePortfolioConfirm_(std::make_shared<TuiConfirmationDialog>(
          "Delete Portfolio", "", "Delete", [this] { confirmDeletePortfolio_(); }, "Cancel",
          [this] { enterNormal_(); })),
      deleteTransactionConfirm_(std::make_shared<TuiConfirmationDialog>(
          "Delete Transaction", "", "Confirm", [this] { confirmDeleteTransaction_(); }, "Cancel",
          [this] { enterNormal_(); })),
      importDialog_(std::make_shared<TuiFileDialog>(
          ".csv", [this](std::string path) { submitImportTransactions_(path); }, [this] { enterNormal_(); }))
      // Panels
      ,
      txnTable_(std::make_shared<TuiTable>(
          std::vector<ColumnDef>{
              {"", "Date", 13, [this](int i) { return Utils::fmtDate(currentTransactions_[i].timestampMs); }},
              {"", "Type", 12, [this](int i) { return currentTransactions_[i].type; }},
              {"Asset", "Ticker", 10, [this](int i) { return currentTransactions_[i].ticker; }},
              {"Asset", "Qty", 12, [this](int i) { return Utils::fmtMoney(currentTransactions_[i].quantity); }},
              {"Amount", "Price", 12, [this](int i) { return Utils::fmtMoney(currentTransactions_[i].pricePerUnit); }},
              {"Amount", "Fees", 10, [this](int i) { return Utils::fmtMoney(currentTransactions_[i].fees); }},
              {"", "CCY", 5, [this](int i) { return currentTransactions_[i].currency; }},
          },
          [this] { return static_cast<int>(currentTransactions_.size()); })),
      rightPanel_(std::make_shared<TuiTabbedPanel>(std::vector<Tab>{
          {"Overview", '1', ftxui::Renderer([this] { return buildOverviewPanel_(); })},
          {"Transactions", '2', ftxui::Renderer(txnTable_, [this] { return buildTransactionsPanel_(); })},
          {"Chart", '3', ftxui::Renderer([this] { return buildChartPanel_(); })},
      })) {
    chart_.setTitle("Portfolio Value — Last 30 Days");

    // ── TuiTable callbacks ────────────────────────────────────────────────────
    txnTable_->setOnEdit([this](int i) { startEditTransaction_(i); });
    txnTable_->setOnDelete([this](int i) {
        if (i < 0 || i >= static_cast<int>(currentTransactions_.size())) return;
        const auto& txn = currentTransactions_[i];
        deleteTransactionConfirm_->setMessage(txn.type + "  " + txn.ticker + "  " + Utils::fmtMoney(txn.quantity) +
                                              "  " + Utils::fmtDate(txn.timestampMs));
        modeAsInt_ = 5;
        showDeleteTransaction_ = true;
    });
    txnTable_->setOnAdd([this] {
        if (currentSummary_.id.empty()) return;
        addTransactionDialog_->reset();
        addTxnForm_.date = Utils::todayDateString();
        modeAsInt_ = 3;
        showAddTransaction_ = true;
    });
    txnTable_->setOnImport([this] {
        if (currentSummary_.id.empty()) return;
        importDialog_->reset();
        modeAsInt_ = 6;
        showImportTransaction_ = true;
    });

    // ── Tab-change data loading ───────────────────────────────────────────────
    rightPanel_->setOnTabChanged([this](int tab) {
        if (tab == 1 && currentTransactions_.empty() && !currentSummary_.id.empty()) loadTransactions_();
        if (tab == 2 && currentTimeSeries_.values.empty() && !currentSummary_.id.empty()) loadTimeSeries_();
    });

    // ── Portfolio menu ────────────────────────────────────────────────────────
    ftxui::MenuOption menuOpt = ftxui::MenuOption::Vertical();
    menuOpt.on_enter = [this] {
        loadSelectedPortfolio_();
        activePane_ = Pane::Right;
        rightPanel_->TakeFocus();
    };
    portfolioMenu_ = Menu(&menuEntries_, &selectedPortfolio_, menuOpt);

    // ── App component ─────────────────────────────────────────────────────────

    app_ = CatchEvent(
        ftxui::Container::Tab(
            {
                ftxui::Container::Horizontal({portfolioMenu_, rightPanel_}),       // 0 Normal
                ftxui::Maybe(createDialog_, &showCreatePortfolio_),                // 1 CreatePortfolio
                ftxui::Maybe(deletePortfolioConfirm_, &showDeletePortfolio_),      // 2 DeletePortfolio
                ftxui::Maybe(addTransactionDialog_, &showAddTransaction_),         // 3 AddTransaction
                ftxui::Maybe(editTransactionDialog_, &showEditTransaction_),       // 4 EditTransaction
                ftxui::Maybe(deleteTransactionConfirm_, &showDeleteTransaction_),  // 5 DeleteTransaction
                ftxui::Maybe(importDialog_, &showImportTransaction_)               // 6 ImportTransactions
            },
            &modeAsInt_),
        [this](ftxui::Event e) -> bool {
            if (modeAsInt_ != 0) return false;
            if (e == ftxui::Event::Character('r')) {
                loadPortfolioList_();
                if (!portfolioList_.empty()) loadSelectedPortfolio_();
                return true;
            }
            if (e == ftxui::Event::Tab) {
                if (activePane_ == Pane::Left) {
                    activePane_ = Pane::Right;
                    rightPanel_->TakeFocus();
                } else {
                    activePane_ = Pane::Left;
                    portfolioMenu_->TakeFocus();
                }
                return true;
            }
            if (activePane_ == Pane::Left) {
                if (e == ftxui::Event::Character('n')) {
                    createDialog_->reset();
                    modeAsInt_ = 1;
                    showCreatePortfolio_ = true;
                    return true;
                }
                if (e == ftxui::Event::Character('d') && !portfolioList_.empty()) {
                    deletePortfolioConfirm_->setMessage("Delete \"" + portfolioList_[selectedPortfolio_].name + "\"?");
                    modeAsInt_ = 2;
                    showDeletePortfolio_ = true;
                    return true;
                }
                return false;  // arrows fall through to portfolioMenu_
            }
            // ── Right pane: only module-level concerns remain ─────────────────
            if (e == ftxui::Event::Escape) {
                activePane_ = Pane::Left;
                portfolioMenu_->TakeFocus();
                return true;
            }
            return false;  // '1','2','3', ↑↓, 't','e','x','i' fall through to TuiTabbedPanel / TuiTable
        });

    uiComponent_ = Renderer(app_, [this] { return renderRoot_(); });

    loadPortfolioList_();
    if (!portfolioList_.empty()) loadSelectedPortfolio_();
}

ftxui::Component PortfolioModule::component() { return uiComponent_; }

std::string PortfolioModule::title() const { return "Portfolio"; }

// ── Load helpers ──────────────────────────────────────────────────────────────

void PortfolioModule::enterNormal_() {
    modeAsInt_ = 0;
    showCreatePortfolio_ = showDeletePortfolio_ = showAddTransaction_ = showEditTransaction_ = showDeleteTransaction_ =
        showImportTransaction_ = showImportTransaction_ = false;
}
void PortfolioModule::enterDialog_(int idx, bool& flag) {
    modeAsInt_ = idx;
    flag = true;
}
void PortfolioModule::rebuildMenuEntries_() {
    menuEntries_.clear();
    for (const auto& e : portfolioList_) menuEntries_.push_back(e.name.empty() ? e.id : e.name);
}

void PortfolioModule::loadPortfolioList_() {
    try {
        portfolioList_ = dataSource_->listPortfolios();
        rebuildMenuEntries_();
        selectedPortfolio_ = 0;
        currentSummary_ = {};
        currentTransactions_.clear();
        txnTable_->setRowCount(0);
        currentTimeSeries_ = {};
        chart_.setData({});
        statusMsg_ = portfolioList_.empty() ? "No portfolios found."
                                            : std::to_string(portfolioList_.size()) + " portfolio(s) loaded.";
        statusIsError_ = false;
    } catch (const std::exception& e) {
        statusMsg_ = std::string("Error: ") + e.what();
        statusIsError_ = true;
    }
}

void PortfolioModule::loadSelectedPortfolio_() {
    if (portfolioList_.empty()) return;
    try {
        currentSummary_ = dataSource_->loadSummary(portfolioList_[selectedPortfolio_].id);
        currentTransactions_.clear();
        txnTable_->setRowCount(0);
        currentTimeSeries_ = {};
        chart_.setData({});
        statusMsg_ = "Loaded: " + currentSummary_.name;
        statusIsError_ = false;
    } catch (const std::exception& e) {
        statusMsg_ = std::string("Error: ") + e.what();
        statusIsError_ = true;
    }
}

void PortfolioModule::loadTransactions_() {
    if (currentSummary_.id.empty()) return;
    try {
        currentTransactions_ = dataSource_->listTransactions(currentSummary_.id);
        txnTable_->setRowCount(static_cast<int>(currentTransactions_.size()));
        statusIsError_ = false;
    } catch (const std::exception& e) {
        statusMsg_ = std::string("Error loading transactions: ") + e.what();
        statusIsError_ = true;
    }
}

void PortfolioModule::loadTimeSeries_() {
    if (currentSummary_.id.empty()) return;
    try {
        const int64_t nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        const int64_t startMs = nowMs - 30LL * 86'400'000LL;
        currentTimeSeries_ = dataSource_->getTimeSeries(currentSummary_.id, startMs, nowMs, 86'400'000LL);
        chart_.setData(currentTimeSeries_.values);
        statusIsError_ = false;
    } catch (const std::exception& e) {
        statusMsg_ = std::string("Error loading chart: ") + e.what();
        statusIsError_ = true;
    }
}

// ── Submit / confirm ──────────────────────────────────────────────────────────

void PortfolioModule::submitCreatePortfolio_() {
    if (!createForm_.name.empty()) {
        try {
            const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count();
            CreatePortfolioParams p;
            p.name = createForm_.name;
            p.currency = currencies[createForm_.currencyIdx];
            p.timestampsMs = createForm_.date.empty() ? 0 : Utils::parseDate(createForm_.date);
            dataSource_->createPortfolio(p);
            loadPortfolioList_();
            statusMsg_ = "Created: " + createForm_.name;
            statusIsError_ = false;
        } catch (const std::exception& ex) {
            statusMsg_ = std::string("Error: ") + ex.what();
            statusIsError_ = true;
        }
    }
    enterNormal_();
}

void PortfolioModule::confirmDeletePortfolio_() {
    if (!portfolioList_.empty()) {
        try {
            dataSource_->deletePortfolio(portfolioList_[selectedPortfolio_].id);
            loadPortfolioList_();
            statusMsg_ = "Portfolio deleted.";
            statusIsError_ = false;
        } catch (const std::exception& ex) {
            statusMsg_ = std::string("Error: ") + ex.what();
            statusIsError_ = true;
        }
    }
    enterNormal_();
}

void PortfolioModule::submitAddTransaction_() {
    if (!addTxnForm_.ticker.empty() && !addTxnForm_.qty.empty()) {
        try {
            AddTransactionParams p;
            p.timestampMs = addTxnForm_.date.empty() ? 0 : Utils::parseDate(addTxnForm_.date);
            p.type = transactionTypes[addTxnForm_.typeIdx];
            p.ticker = addTxnForm_.ticker;
            p.quantity = std::stod(addTxnForm_.qty);
            p.pricePerUnit = addTxnForm_.price.empty() ? 0.0 : std::stod(addTxnForm_.price);
            p.fees = addTxnForm_.fee.empty() ? 0.0 : std::stod(addTxnForm_.fee);
            p.currency = "USD";
            dataSource_->addTransaction(currentSummary_.id, p);
            currentTransactions_.clear();
            txnTable_->setRowCount(0);
            loadTransactions_();
            statusMsg_ = "Transaction added.";
            statusIsError_ = false;
        } catch (const std::exception& ex) {
            statusMsg_ = std::string("Error: ") + ex.what();
            statusIsError_ = true;
        }
    }
    enterNormal_();
}

void PortfolioModule::startEditTransaction_(int idx) {
    if (idx < 0 || idx >= static_cast<int>(currentTransactions_.size())) return;
    const auto& txn = currentTransactions_[idx];

    editTransactionDialog_->reset();

    addTxnForm_.typeIdx = 0;
    for (int i = 0; i < static_cast<int>(transactionTypes.size()); ++i) {
        if (transactionTypes[i] == txn.type) {
            addTxnForm_.typeIdx = i;
            break;
        }
    }
    addTxnForm_.date = Utils::fmtDate(txn.timestampMs);
    addTxnForm_.ticker = txn.ticker;

    // Format doubles without trailing zeros for editable fields
    auto fmtNum = [](double v) {
        std::string s = std::to_string(v);
        const auto dot = s.find('.');
        if (dot != std::string::npos) {
            s.erase(s.find_last_not_of('0') + 1);
            if (s.back() == '.') s.pop_back();
        }
        return s;
    };
    addTxnForm_.qty = fmtNum(txn.quantity);
    addTxnForm_.price = fmtNum(txn.pricePerUnit);
    addTxnForm_.fee = fmtNum(txn.fees);

    editingTxnId_ = txn.id;
    modeAsInt_ = 4;
    showEditTransaction_ = true;
}

void PortfolioModule::submitEditTransaction_() {
    if (!addTxnForm_.ticker.empty() && !addTxnForm_.qty.empty()) {
        try {
            dataSource_->deleteTransaction(currentSummary_.id, editingTxnId_);
            AddTransactionParams p;
            p.timestampMs = addTxnForm_.date.empty() ? 0 : Utils::parseDate(addTxnForm_.date);
            p.type = transactionTypes[addTxnForm_.typeIdx];
            p.ticker = addTxnForm_.ticker;
            p.quantity = std::stod(addTxnForm_.qty);
            p.pricePerUnit = addTxnForm_.price.empty() ? 0.0 : std::stod(addTxnForm_.price);
            p.fees = addTxnForm_.fee.empty() ? 0.0 : std::stod(addTxnForm_.fee);
            p.currency = "USD";
            dataSource_->addTransaction(currentSummary_.id, p);
            currentTransactions_.clear();
            txnTable_->setRowCount(0);
            loadTransactions_();
            statusMsg_ = "Transaction updated.";
            statusIsError_ = false;
        } catch (const std::exception& ex) {
            statusMsg_ = std::string("Error: ") + ex.what();
            statusIsError_ = true;
        }
    }
    enterNormal_();
}

void PortfolioModule::confirmDeleteTransaction_() {
    if (!currentTransactions_.empty()) {
        try {
            const auto& txn = currentTransactions_[txnTable_->selectedRow()];
            dataSource_->deleteTransaction(currentSummary_.id, txn.id);
            currentTransactions_.clear();
            txnTable_->setRowCount(0);
            loadTransactions_();
            statusMsg_ = "Transaction deleted.";
            statusIsError_ = false;
        } catch (const std::exception& ex) {
            statusMsg_ = std::string("Error: ") + ex.what();
            statusIsError_ = true;
        }
    }
    enterNormal_();
}

void PortfolioModule::submitImportTransactions_(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) throw std::runtime_error("Cannot open: " + path);
        std::ostringstream oss;
        oss << file.rdbuf();
        dataSource_->importCsv(currentSummary_.id, oss.str());
        currentTransactions_.clear();
        txnTable_->setRowCount(0);
        statusMsg_ = "Imported: " + std::filesystem::path(path).filename().string();
        statusIsError_ = false;
    } catch (const std::exception& ex) {
        statusMsg_ = std::string("Import error: ") + ex.what();
        statusIsError_ = true;
    }
    enterNormal_();
}

// ── Render ────────────────────────────────────────────────────────────────────

ftxui::Element PortfolioModule::buildLeftPanel_() const {
    const bool focused = (activePane_ == Pane::Left);

    ftxui::Elements rows;
    for (int i = 0; i < static_cast<int>(portfolioList_.size()); ++i) {
        const auto& e = portfolioList_[i];
        auto row = ftxui::text(" " + (e.name.empty() ? e.id : e.name) + " ");
        rows.push_back(i == selectedPortfolio_ ? row | ftxui::inverted : row);
    }
    if (rows.empty()) rows.push_back(ftxui::text("  No portfolios") | ftxui::dim);

    ftxui::Element hint = focused
                              ? ftxui::hbox({ftxui::text(" n") | ftxui::bold | color(ftxui::Color::Green),
                                             ftxui::text(" New  "),
                                             ftxui::text("d") | ftxui::bold | color(ftxui::Color::Red),
                                             ftxui::text(" Del  "),
                                             ftxui::text("r") | ftxui::bold,
                                             ftxui::text(" Reload  "),
                                             ftxui::text("Enter") | ftxui::bold,
                                             ftxui::text(" →")}) |
                                    ftxui::dim
                              : ftxui::hbox({ftxui::text(" Tab / Esc") | ftxui::bold, ftxui::text(" ←")}) | ftxui::dim;

    return window(focused ? ftxui::text(" Portfolios ") | ftxui::bold : ftxui::text(" Portfolios ") | ftxui::dim,
                  ftxui::vbox(
                      {ftxui::vbox(std::move(rows)) | ftxui::flex_grow, ftxui::filler(), ftxui::separator(), hint})) |
           size(ftxui::WIDTH, ftxui::EQUAL, 32);
}

ftxui::Element PortfolioModule::buildOverviewPanel_() const {
    const auto& s = currentSummary_;
    if (s.id.empty()) {
        return ftxui::vbox(
            {ftxui::filler(), ftxui::text("  Select a portfolio and press Enter") | ftxui::dim, ftxui::filler()});
    }

    ftxui::Elements cashRows;
    for (const auto& c : s.cashBalances) {
        cashRows.push_back(ftxui::hbox({
            ftxui::text("  " + c.currency + ": ") | ftxui::bold | size(ftxui::WIDTH, ftxui::EQUAL, 8),
            ftxui::text(Utils::fmtMoney(c.amount)),
        }));
    }
    if (cashRows.empty()) cashRows.push_back(ftxui::text("  —") | ftxui::dim);

    ftxui::Elements posRows;
    posRows.push_back(ftxui::hbox({
        ftxui::text(" Ticker") | ftxui::bold | size(ftxui::WIDTH, ftxui::EQUAL, 10),
        ftxui::text("Quantity") | ftxui::bold | size(ftxui::WIDTH, ftxui::EQUAL, 14),
        ftxui::text("Value") | ftxui::bold | size(ftxui::WIDTH, ftxui::EQUAL, 14),
        ftxui::text("Weight") | ftxui::bold | size(ftxui::WIDTH, ftxui::EQUAL, 9),
    }));
    posRows.push_back(ftxui::separator());
    if (s.positions.empty()) {
        posRows.push_back(ftxui::text("  No positions") | ftxui::dim);
    } else {
        for (const auto& p : s.positions) {
            posRows.push_back(ftxui::hbox({
                ftxui::text(" " + p.ticker) | size(ftxui::WIDTH, ftxui::EQUAL, 10),
                ftxui::text(Utils::fmtMoney(p.quantity)) | size(ftxui::WIDTH, ftxui::EQUAL, 14),
                ftxui::text(Utils::fmtMoney(p.value)) | size(ftxui::WIDTH, ftxui::EQUAL, 14),
                ftxui::text(Utils::fmtMoney(p.weight * 100.0) + "%") | size(ftxui::WIDTH, ftxui::EQUAL, 9),
            }));
        }
    }

    return ftxui::vbox({
        ftxui::hbox({ftxui::text(" Total Value: ") | ftxui::bold,
                     ftxui::text(Utils::fmtMoney(s.totalValue) + " " + s.baseCurrency) | color(ftxui::Color::Green)}),
        ftxui::separator(),
        ftxui::text(" Cash") | ftxui::bold,
        vbox(std::move(cashRows)),
        ftxui::separator(),
        ftxui::text(" Positions") | ftxui::bold,
        ftxui::vbox(std::move(posRows)) | ftxui::vscroll_indicator | ftxui::frame | ftxui::flex_grow,
        ftxui::separator(),
        ftxui::text(" t Add transaction  r Reload") | ftxui::dim,
    });
}

ftxui::Element PortfolioModule::buildTransactionsPanel_() const {
    const bool hasData = !currentTransactions_.empty();
    return ftxui::vbox({
        txnTable_->Render(),
        ftxui::separator(),
        ftxui::text(hasData ? " t Add   e Edit   x Delete   i Import   ↑↓ Navigate" : " t Add   i Import") | ftxui::dim,
    });
}

ftxui::Element PortfolioModule::buildChartPanel_() const {
    return ftxui::vbox({
        chart_.render(),
        ftxui::separator(),
        ftxui::text(" r Reload") | ftxui::dim,
    });
}

ftxui::Element PortfolioModule::renderRoot_() const {
    // Status message: red + bold on errors, dim on info.
    // When the message is long (> 80 chars) and it is an error, show a second
    // wrapped line so the full text is readable regardless of terminal width.
    constexpr int kStatusInlineMax = 80;
    const bool longError = statusIsError_ && static_cast<int>(statusMsg_.size()) > kStatusInlineMax;

    const std::string inlineMsg = longError ? statusMsg_.substr(0, kStatusInlineMax) + "…" : statusMsg_;

    auto msgElement = statusIsError_
                          ? (ftxui::text(inlineMsg) | ftxui::color(ftxui::Color::Red) | ftxui::bold | ftxui::flex_grow)
                          : (ftxui::text(inlineMsg) | ftxui::dim | ftxui::flex_grow);

    auto statusBar = ftxui::hbox({
        ftxui::text(" finapp") | ftxui::bold,
        ftxui::text("  ·  " + std::to_string(portfolioList_.size()) + " portfolios  ·  ") | ftxui::dim,
        msgElement,
        ftxui::text(currentSummary_.name.empty() ? "" : "  " + currentSummary_.name + "  ") | ftxui::inverted,
        ftxui::text("  [q] quit  ") | ftxui::dim,
    });

    ftxui::Element content = ftxui::hbox({buildLeftPanel_(), rightPanel_->Render() | ftxui::flex_grow});

    // When there is a long error, add a second line below the status bar that
    // wraps the full message so nothing is hidden.
    ftxui::Elements bottomRows;
    bottomRows.push_back(ftxui::separator());
    bottomRows.push_back(statusBar);
    if (longError)
        bottomRows.push_back(ftxui::paragraph(statusMsg_) | ftxui::color(ftxui::Color::Red) |
                             size(ftxui::HEIGHT, ftxui::LESS_THAN, 4));

    ftxui::Element root = ftxui::vbox({content | ftxui::flex_grow, ftxui::vbox(std::move(bottomRows))});

    if (showCreatePortfolio_)
        root = ftxui::dbox({root, createDialog_->Render() | ftxui::clear_under | ftxui::center});
    else if (showDeletePortfolio_)
        root = ftxui::dbox({root, deletePortfolioConfirm_->Render() | ftxui::clear_under | ftxui::center});
    else if (showAddTransaction_)
        root = ftxui::dbox({root, addTransactionDialog_->Render() | ftxui::clear_under | ftxui::center});
    else if (showEditTransaction_)
        root = ftxui::dbox({root, addTransactionDialog_->Render() | ftxui::clear_under | ftxui::center});
    else if (showDeleteTransaction_)
        root = ftxui::dbox({root, deleteTransactionConfirm_->Render() | ftxui::clear_under | ftxui::center});
    else if (showImportTransaction_)
        root = ftxui::dbox({root, importDialog_->Render() | ftxui::clear_under | ftxui::center});
    return root;
}

}  // namespace finui
