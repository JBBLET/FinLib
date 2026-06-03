// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/modules/portfolioModule/TransactionsPane.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "finTUI/finTuiUtils/PortfolioUtils.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

TransactionsPane::TransactionsPane(std::shared_ptr<IPortfolioDataSource> ds,
                                   std::function<void(std::string, bool)> onStatus)
    : dataSource_(std::move(ds)), onStatus_(std::move(onStatus)) {
    table_ = std::make_shared<TuiTable>(
        std::vector<ColumnDef>{
            {"", "Date", 13, [this](int i) { return utils::PortfolioUtils::fmtDate(transactions_[i].timestampMs); }},
            {"", "Type", 12, [this](int i) { return transactions_[i].type; }},
            {"Asset", "Ticker", 10, [this](int i) { return transactions_[i].ticker; }},
            {"Asset", "Qty", 12,
             [this](int i) { return utils::PortfolioUtils::fmtNumber(transactions_[i].quantity); }},
            {"Amount", "Price", 12,
             [this](int i) { return utils::PortfolioUtils::fmtNumber(transactions_[i].pricePerUnit); }},
            {"Amount", "Fees", 10,
             [this](int i) { return utils::PortfolioUtils::fmtNumber(transactions_[i].fees); }},
            {"", "CCY", 5, [this](int i) { return transactions_[i].currency; }},
        },
        [this] { return static_cast<int>(transactions_.size()); });

    addDialog_ = makeAddTransactionDialog(addForm_, [this] { submitAdd_(); }, [this] { enterNormal_(); });
    editDialog_ = makeAddTransactionDialog(addForm_, [this] { submitEdit_(); }, [this] { enterNormal_(); });
    deleteConfirm_ = std::make_shared<TuiConfirmationDialog>("Delete Transaction", "", "Confirm",
                                                             [this] { confirmDelete_(); }, "Cancel",
                                                             [this] { enterNormal_(); });
    importDialog_ = std::make_shared<TuiFileDialog>(".csv", [this](std::string path) { submitImport_(path); },
                                                    [this] { enterNormal_(); });

    table_->setOnEdit([this](int i) { startEdit_(i); });
    table_->setOnDelete([this](int i) {
        if (i < 0 || i >= static_cast<int>(transactions_.size())) return;
        const auto& txn = transactions_[i];
        deleteConfirm_->setMessage(txn.type + "  " + txn.ticker + "  " +
                                   utils::PortfolioUtils::fmtNumber(txn.quantity) + "  " +
                                   utils::PortfolioUtils::fmtDate(txn.timestampMs));
        modeAsInt_ = 3;
        showDelete_ = true;
    });
    table_->setOnAdd([this] {
        if (summary_.id.empty()) return;
        addDialog_->reset();
        addForm_.date = utils::PortfolioUtils::todayDateString();
        modeAsInt_ = 1;
        showAdd_ = true;
    });
    table_->setOnImport([this] {
        if (summary_.id.empty()) return;
        importDialog_->reset();
        modeAsInt_ = 4;
        showImport_ = true;
    });

    app_ = ftxui::Container::Tab(
        {
            table_,                                         // 0 Normal
            ftxui::Maybe(addDialog_, &showAdd_),            // 1 AddTransaction
            ftxui::Maybe(editDialog_, &showEdit_),          // 2 EditTransaction
            ftxui::Maybe(deleteConfirm_, &showDelete_),     // 3 DeleteTransaction
            ftxui::Maybe(importDialog_, &showImport_),      // 4 ImportTransactions
        },
        &modeAsInt_);
    component_ = ftxui::Renderer(app_, [this] { return renderRoot_(); });
}

ftxui::Component TransactionsPane::component() { return component_; }

void TransactionsPane::setPortfolio(PortfolioSummary s) {
    summary_ = std::move(s);
    transactions_.clear();
    table_->setRowCount(0);
}

void TransactionsPane::onActivated() {
    if (transactions_.empty() && !summary_.id.empty()) loadTransactions_();
}

void TransactionsPane::enterNormal_() {
    modeAsInt_ = 0;
    showAdd_ = showEdit_ = showDelete_ = showImport_ = false;
}

void TransactionsPane::loadTransactions_() {
    try {
        transactions_ = dataSource_->listTransactions(summary_.id);
        table_->setRowCount(static_cast<int>(transactions_.size()));
    } catch (const std::exception& e) {
        if (onStatus_) onStatus_(std::string("Error loading transactions: ") + e.what(), true);
    }
}

void TransactionsPane::submitAdd_() {
    if (!addForm_.ticker.empty() && !addForm_.qty.empty()) {
        try {
            AddTransactionParams p;
            p.timestampMs = addForm_.date.empty() ? 0 : utils::PortfolioUtils::parseDate(addForm_.date);
            p.type = transactionTypes[addForm_.typeIdx];
            p.ticker = addForm_.ticker;
            p.quantity = std::stod(addForm_.qty);
            p.pricePerUnit = addForm_.price.empty() ? 0.0 : std::stod(addForm_.price);
            p.fees = addForm_.fee.empty() ? 0.0 : std::stod(addForm_.fee);
            p.currency = "USD";
            dataSource_->addTransaction(summary_.id, p);
            transactions_.clear();
            table_->setRowCount(0);
            loadTransactions_();
            if (onStatus_) onStatus_("Transaction added.", false);
        } catch (const std::exception& ex) {
            if (onStatus_) onStatus_(std::string("Error: ") + ex.what(), true);
        }
    }
    enterNormal_();
}

void TransactionsPane::startEdit_(int idx) {
    if (idx < 0 || idx >= static_cast<int>(transactions_.size())) return;
    const auto& txn = transactions_[idx];

    editDialog_->reset();

    addForm_.typeIdx = 0;
    for (int i = 0; i < static_cast<int>(transactionTypes.size()); ++i) {
        if (transactionTypes[i] == txn.type) {
            addForm_.typeIdx = i;
            break;
        }
    }
    addForm_.date = utils::PortfolioUtils::fmtDate(txn.timestampMs);
    addForm_.ticker = txn.ticker;

    auto fmtNum = [](double v) {
        std::string s = std::to_string(v);
        const auto dot = s.find('.');
        if (dot != std::string::npos) {
            s.erase(s.find_last_not_of('0') + 1);
            if (s.back() == '.') s.pop_back();
        }
        return s;
    };
    addForm_.qty = fmtNum(txn.quantity);
    addForm_.price = fmtNum(txn.pricePerUnit);
    addForm_.fee = fmtNum(txn.fees);

    editingTxnId_ = txn.id;
    modeAsInt_ = 2;
    showEdit_ = true;
}

void TransactionsPane::submitEdit_() {
    if (!addForm_.ticker.empty() && !addForm_.qty.empty()) {
        try {
            dataSource_->deleteTransaction(summary_.id, editingTxnId_);
            AddTransactionParams p;
            p.timestampMs = addForm_.date.empty() ? 0 : utils::PortfolioUtils::parseDate(addForm_.date);
            p.type = transactionTypes[addForm_.typeIdx];
            p.ticker = addForm_.ticker;
            p.quantity = std::stod(addForm_.qty);
            p.pricePerUnit = addForm_.price.empty() ? 0.0 : std::stod(addForm_.price);
            p.fees = addForm_.fee.empty() ? 0.0 : std::stod(addForm_.fee);
            p.currency = "USD";
            dataSource_->addTransaction(summary_.id, p);
            transactions_.clear();
            table_->setRowCount(0);
            loadTransactions_();
            if (onStatus_) onStatus_("Transaction updated.", false);
        } catch (const std::exception& ex) {
            if (onStatus_) onStatus_(std::string("Error: ") + ex.what(), true);
        }
    }
    enterNormal_();
}

void TransactionsPane::confirmDelete_() {
    if (!transactions_.empty()) {
        try {
            const auto& txn = transactions_[table_->selectedRow()];
            dataSource_->deleteTransaction(summary_.id, txn.id);
            transactions_.clear();
            table_->setRowCount(0);
            loadTransactions_();
            if (onStatus_) onStatus_("Transaction deleted.", false);
        } catch (const std::exception& ex) {
            if (onStatus_) onStatus_(std::string("Error: ") + ex.what(), true);
        }
    }
    enterNormal_();
}

void TransactionsPane::submitImport_(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) throw std::runtime_error("Cannot open: " + path);
        std::ostringstream oss;
        oss << file.rdbuf();
        dataSource_->importCsv(summary_.id, oss.str());
        transactions_.clear();
        table_->setRowCount(0);
        loadTransactions_();
        if (onStatus_) onStatus_("Imported: " + std::filesystem::path(path).filename().string(), false);
    } catch (const std::exception& ex) {
        if (onStatus_) onStatus_(std::string("Import error: ") + ex.what(), true);
    }
    enterNormal_();
}

ftxui::Element TransactionsPane::buildContent_() const {
    const bool hasData = !transactions_.empty();
    return ftxui::vbox({
        table_->Render(),
        ftxui::separator(),
        ftxui::text(hasData ? " t Add   e Edit   x Delete   i Import   ↑↓ Navigate" : " t Add   i Import") |
            ftxui::dim,
    });
}

ftxui::Element TransactionsPane::renderRoot_() const {
    ftxui::Element content = buildContent_();
    if (showAdd_)
        content = ftxui::dbox({content, addDialog_->Render() | ftxui::clear_under | ftxui::center});
    else if (showEdit_)
        content = ftxui::dbox({content, editDialog_->Render() | ftxui::clear_under | ftxui::center});
    else if (showDelete_)
        content = ftxui::dbox({content, deleteConfirm_->Render() | ftxui::clear_under | ftxui::center});
    else if (showImport_)
        content = ftxui::dbox({content, importDialog_->Render() | ftxui::clear_under | ftxui::center});
    return content;
}

}  // namespace finui
