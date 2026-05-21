// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "finTUI/uiComponents/TUIFormDialog.hpp"

namespace finui {

inline const std::vector<std::string> kCurrencies = {"USD", "EUR", "GBP", "JPY", "KRW", "CAD"};
inline const std::vector<std::string> kTxnTypes   = {"BUY", "SELL", "DEPOSIT", "WITHDRAWAL", "DIVIDEND", "SPLIT"};

struct CreatePortfolioForm {
    std::string name;
    std::string date;
    int         currencyIdx = 0;
};

struct DeletePortfolioForm {
    std::string confirmName;
};

struct AddTransactionForm {
    std::string date;
    std::string ticker;
    std::string qty;
    std::string price;
    std::string fee;
    int         typeIdx = 0;
};

FormDialog makeCreatePortfolioDialog(CreatePortfolioForm& f,
                                     std::function<void()> onSubmit,
                                     std::function<void()> onCancel);

FormDialog makeDeletePortfolioDialog(DeletePortfolioForm& f,
                                     std::function<void()> onSubmit,
                                     std::function<void()> onCancel);

FormDialog makeAddTransactionDialog(AddTransactionForm& f,
                                    std::function<void()> onSubmit,
                                    std::function<void()> onCancel);

}  // namespace finui
