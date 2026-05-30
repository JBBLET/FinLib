// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <functional>
#include <memory>
#include <string>

#include "finTUI/uiComponents/TuiFormDialog.hpp"

namespace finui {

struct CreatePortfolioForm {
    std::string name;
    std::string date;
    int currencyIdx = 0;
};

struct AddTransactionForm {
    std::string date;
    std::string ticker;
    std::string qty;
    std::string price;
    std::string fee;
    int typeIdx = 0;
};

std::shared_ptr<TuiFormDialog> makeCreatePortfolioDialog(CreatePortfolioForm& f,
                                                          std::function<void()> onSubmit,
                                                          std::function<void()> onCancel);

std::shared_ptr<TuiFormDialog> makeAddTransactionDialog(AddTransactionForm& f,
                                                        std::function<void()> onSubmit,
                                                        std::function<void()> onCancel);

}  // namespace finui
