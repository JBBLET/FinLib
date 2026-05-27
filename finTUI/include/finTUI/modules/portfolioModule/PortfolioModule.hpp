// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "finTUI/dataSources/IPortfolioDataSource.hpp"
#include "finTUI/modules/IModule.hpp"
#include "finTUI/modules/portfolioModule/Dialogs.hpp"
#include "finTUI/modules/portfolioModule/PortfolioModuleTypes.hpp"
#include "finTUI/uiComponents/TuiConfirmationDialog.hpp"
#include "finTUI/uiComponents/TuiFileDialog.hpp"
#include "finTUI/uiComponents/TuiFormDialog.hpp"
#include "finTUI/uiComponents/TuiLineChart.hpp"
#include "finTUI/uiComponents/TuiTabbedPanel.hpp"
#include "finTUI/uiComponents/TuiTable.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

class PortfolioModule : public IModule {
 public:
    PortfolioModule(std::shared_ptr<IPortfolioDataSource> dataSource, ftxui::ScreenInteractive& screen);
    PortfolioModule(const PortfolioModule&) = delete;
    PortfolioModule(PortfolioModule&&) = delete;
    PortfolioModule& operator=(const PortfolioModule&) = delete;
    PortfolioModule& operator=(PortfolioModule&&) = delete;

    ftxui::Component component() override;
    std::string title() const override;

 private:
    // ── External ──────────────────────────────────────────────────────────────
    std::shared_ptr<IPortfolioDataSource> dataSource_;
    ftxui::ScreenInteractive& screen_;

    // ── Mode / pane ───────────────────────────────────────────────────────────
    enum class Pane { Left, Right };
    Pane activePane_ = Pane::Left;
    int modeAsInt_ = 0;

    bool showCreatePortfolio_ = false;
    bool showDeletePortfolio_ = false;
    bool showAddTransaction_ = false;
    bool showEditTransaction_ = false;
    bool showDeleteTransaction_ = false;
    bool showImportTransaction_ = false;

    // ── Data ──────────────────────────────────────────────────────────────────
    std::vector<PortfolioListEntry> portfolioList_;
    std::vector<std::string> menuEntries_;
    int selectedPortfolio_ = 0;
    PortfolioSummary currentSummary_;
    std::vector<TransactionRow> currentTransactions_;
    TimeSeriesData currentTimeSeries_;
    std::string statusMsg_;
    bool        statusIsError_ = false;  // true while the last status message is an error

    // ── Forms — declared before dialogs that bind pointers into them ──────────
    CreatePortfolioForm createForm_;
    AddTransactionForm addTxnForm_;
    std::string editingTxnId_;

    // ── Dialogs — initialized in MIL via factory prvalues (C++17 GCE) ─────────
    std::shared_ptr<TuiFormDialog> createDialog_;
    std::shared_ptr<TuiFormDialog> addTransactionDialog_;
    std::shared_ptr<TuiFormDialog> editTransactionDialog_;
    std::shared_ptr<TuiConfirmationDialog> deletePortfolioConfirm_;
    std::shared_ptr<TuiConfirmationDialog> deleteTransactionConfirm_;
    std::shared_ptr<TuiFileDialog> importDialog_;

    // ── Panels ────────────────────────────────────────────────────────────────
    TuiLineChart chart_;
    std::shared_ptr<TuiTable> txnTable_;          // non-movable: on heap
    std::shared_ptr<TuiTabbedPanel> rightPanel_;  // non-movable: on heap, depends on txnTable_

    // ── FTXUI components — built in constructor body ──────────────────────────
    ftxui::Component portfolioMenu_;
    ftxui::Component app_;
    ftxui::Component uiComponent_;

    // ── Load helpers ──────────────────────────────────────────────────────────
    void enterNormal_();
    void enterDialog_(int idx, bool& flag);
    void rebuildMenuEntries_();
    void loadPortfolioList_();
    void loadSelectedPortfolio_();
    void loadTransactions_();
    void loadTimeSeries_();

    // ── Submit / confirm ──────────────────────────────────────────────────────
    void submitCreatePortfolio_();
    void confirmDeletePortfolio_();
    void submitAddTransaction_();
    void startEditTransaction_(int idx);
    void submitEditTransaction_();
    void confirmDeleteTransaction_();
    void submitImportTransactions_(const std::string& path);

    // ── Render ────────────────────────────────────────────────────────────────
    ftxui::Element buildLeftPanel_() const;
    ftxui::Element buildOverviewPanel_() const;
    ftxui::Element buildTransactionsPanel_() const;
    ftxui::Element buildChartPanel_() const;
    ftxui::Element renderRoot_() const;
};

}  // namespace finui
