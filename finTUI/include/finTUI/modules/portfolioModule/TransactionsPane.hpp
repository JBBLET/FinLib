// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "finTUI/dataSources/IPortfolioDataSource.hpp"
#include "finTUI/modules/portfolioModule/Dialogs.hpp"
#include "finTUI/modules/portfolioModule/PortfolioModuleTypes.hpp"
#include "finTUI/uiComponents/TuiConfirmationDialog.hpp"
#include "finTUI/uiComponents/TuiFileDialog.hpp"
#include "finTUI/uiComponents/TuiFormDialog.hpp"
#include "finTUI/uiComponents/TuiTable.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

class TransactionsPane {
 public:
    TransactionsPane(std::shared_ptr<IPortfolioDataSource> ds,
                     std::function<void(std::string, bool)> onStatus);
    TransactionsPane(const TransactionsPane&) = delete;
    TransactionsPane(TransactionsPane&&) = delete;
    TransactionsPane& operator=(const TransactionsPane&) = delete;
    TransactionsPane& operator=(TransactionsPane&&) = delete;

    ftxui::Component component();
    void setPortfolio(PortfolioSummary s);
    void onActivated();

 private:
    std::shared_ptr<IPortfolioDataSource> dataSource_;
    std::function<void(std::string, bool)> onStatus_;

    PortfolioSummary summary_;
    std::vector<TransactionRow> transactions_;

    AddTransactionForm addForm_;
    std::string editingTxnId_;

    std::shared_ptr<TuiTable> table_;
    std::shared_ptr<TuiFormDialog> addDialog_;
    std::shared_ptr<TuiFormDialog> editDialog_;
    std::shared_ptr<TuiConfirmationDialog> deleteConfirm_;
    std::shared_ptr<TuiFileDialog> importDialog_;

    int modeAsInt_ = 0;
    bool showAdd_ = false;
    bool showEdit_ = false;
    bool showDelete_ = false;
    bool showImport_ = false;

    ftxui::Component app_;
    ftxui::Component component_;

    void enterNormal_();
    void loadTransactions_();
    void submitAdd_();
    void startEdit_(int idx);
    void submitEdit_();
    void confirmDelete_();
    void submitImport_(const std::string& path);
    ftxui::Element buildContent_() const;
    ftxui::Element renderRoot_() const;
};

}  // namespace finui
