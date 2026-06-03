// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/modules/portfolioModule/PortfolioModule.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "finTUI/finTuiUtils/PortfolioUtils.hpp"
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
      deletePortfolioConfirm_(std::make_shared<TuiConfirmationDialog>(
          "Delete Portfolio", "", "Delete", [this] { confirmDeletePortfolio_(); }, "Cancel",
          [this] { enterNormal_(); })) {
    // ── Panes ─────────────────────────────────────────────────────────────────
    txnPane_ = std::make_unique<TransactionsPane>(
        dataSource_, [this](std::string m, bool e) { statusMsg_ = std::move(m); statusIsError_ = e; });
    chartPane_ = std::make_unique<ChartPane>(
        dataSource_, [this](std::string m, bool e) { statusMsg_ = std::move(m); statusIsError_ = e; });

    // ── Right panel ───────────────────────────────────────────────────────────
    rightPanel_ = std::make_shared<TuiTabbedPanel>(std::vector<Tab>{
        {"Overview", '1', ftxui::Renderer([this] { return buildOverviewPanel_(); })},
        {"Transactions", '2', txnPane_->component()},
        {"Chart", '3', chartPane_->component()},
    });

    rightPanel_->setOnTabChanged([this](int tab) {
        if (tab == 1) txnPane_->onActivated();
        if (tab == 2) chartPane_->onActivated();
    });

    // ── Portfolio menu ────────────────────────────────────────────────────────
    ftxui::MenuOption menuOpt = ftxui::MenuOption::Vertical();
    menuOpt.on_change = [this] {
        rightPanel_->setActiveTab(0);
        loadSelectedPortfolio_();
    };
    menuOpt.on_enter = [this] {
        activePane_ = Pane::Right;
        rightPanel_->TakeFocus();
    };
    portfolioMenu_ = Menu(&menuEntries_, &selectedPortfolio_, menuOpt);

    // ── App component ─────────────────────────────────────────────────────────
    app_ = CatchEvent(
        ftxui::Container::Tab(
            {
                ftxui::Container::Horizontal({portfolioMenu_, rightPanel_}),  // 0 Normal
                ftxui::Maybe(createDialog_, &showCreatePortfolio_),            // 1 CreatePortfolio
                ftxui::Maybe(deletePortfolioConfirm_, &showDeletePortfolio_),  // 2 DeletePortfolio
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
                    rightPanel_->setActiveTab(0);
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
            // ── Right pane: only module-level concerns ────────────────────────
            if (e == ftxui::Event::Escape) {
                activePane_ = Pane::Left;
                rightPanel_->setActiveTab(0);
                portfolioMenu_->TakeFocus();
                return true;
            }
            return false;  // '1','2','3', ↑↓, 't','e','x','i' fall through
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
    showCreatePortfolio_ = showDeletePortfolio_ = false;
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
        txnPane_->setPortfolio({});
        chartPane_->setPortfolio({});
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
        txnPane_->setPortfolio(currentSummary_);
        chartPane_->setPortfolio(currentSummary_);
        statusMsg_ = "Loaded: " + currentSummary_.name;
        statusIsError_ = false;
    } catch (const std::exception& e) {
        statusMsg_ = std::string("Error: ") + e.what();
        statusIsError_ = true;
    }
}

// ── Submit / confirm ──────────────────────────────────────────────────────────

void PortfolioModule::submitCreatePortfolio_() {
    if (!createForm_.name.empty()) {
        try {
            CreatePortfolioParams p;
            p.name = createForm_.name;
            p.currency = currencies[createForm_.currencyIdx];
            p.timestampsMs = createForm_.date.empty() ? 0 : utils::PortfolioUtils::parseDate(createForm_.date);
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
            ftxui::text(utils::PortfolioUtils::fmtNumber(c.amount)),
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
                ftxui::text(utils::PortfolioUtils::fmtNumber(p.quantity)) | size(ftxui::WIDTH, ftxui::EQUAL, 14),
                ftxui::text(utils::PortfolioUtils::fmtNumber(p.value)) | size(ftxui::WIDTH, ftxui::EQUAL, 14),
                ftxui::text(utils::PortfolioUtils::percentDisplay(p.weight)) | size(ftxui::WIDTH, ftxui::EQUAL, 9),
            }));
        }
    }

    return ftxui::vbox({
        ftxui::hbox({ftxui::text(" Total Value: ") | ftxui::bold,
                     ftxui::text(utils::PortfolioUtils::currencyDisplay(
                         s.totalValue, utils::PortfolioUtils::currencyFromString(s.baseCurrency))) |
                         color(ftxui::Color::Green)}),
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

ftxui::Element PortfolioModule::renderRoot_() const {
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
    return root;
}

}  // namespace finui
