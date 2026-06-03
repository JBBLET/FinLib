// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "finTUI/dataSources/IPortfolioDataSource.hpp"
#include "finTUI/modules/IModule.hpp"
#include "finTUI/modules/portfolioModule/ChartPane.hpp"
#include "finTUI/modules/portfolioModule/Dialogs.hpp"
#include "finTUI/modules/portfolioModule/PortfolioModuleTypes.hpp"
#include "finTUI/modules/portfolioModule/TransactionsPane.hpp"
#include "finTUI/uiComponents/TuiConfirmationDialog.hpp"
#include "finTUI/uiComponents/TuiFormDialog.hpp"
#include "finTUI/uiComponents/TuiTabbedPanel.hpp"
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
    int modeAsInt_ = 0;  // 0 = Normal, 1 = CreatePortfolio, 2 = DeletePortfolio

    bool showCreatePortfolio_ = false;
    bool showDeletePortfolio_ = false;

    // ── Data ──────────────────────────────────────────────────────────────────
    std::vector<PortfolioListEntry> portfolioList_;
    std::vector<std::string> menuEntries_;
    int selectedPortfolio_ = 0;
    PortfolioSummary currentSummary_;
    std::string statusMsg_;
    bool statusIsError_ = false;

    // ── Form — declared before dialog that binds pointers into it ────────────
    CreatePortfolioForm createForm_;

    // ── Dialogs ───────────────────────────────────────────────────────────────
    std::shared_ptr<TuiFormDialog> createDialog_;
    std::shared_ptr<TuiConfirmationDialog> deletePortfolioConfirm_;

    // ── Panes ─────────────────────────────────────────────────────────────────
    std::unique_ptr<TransactionsPane> txnPane_;
    std::unique_ptr<ChartPane> chartPane_;
    std::shared_ptr<TuiTabbedPanel> rightPanel_;

    // ── FTXUI components — built in constructor body ──────────────────────────
    ftxui::Component portfolioMenu_;
    ftxui::Component app_;
    ftxui::Component uiComponent_;

    // ── Load helpers ──────────────────────────────────────────────────────────
    void enterNormal_();
    void rebuildMenuEntries_();
    void loadPortfolioList_();
    void loadSelectedPortfolio_();

    // ── Submit / confirm ──────────────────────────────────────────────────────
    void submitCreatePortfolio_();
    void confirmDeletePortfolio_();

    // ── Render ────────────────────────────────────────────────────────────────
    ftxui::Element buildLeftPanel_() const;
    ftxui::Element buildOverviewPanel_() const;
    ftxui::Element renderRoot_() const;
};

}  // namespace finui
