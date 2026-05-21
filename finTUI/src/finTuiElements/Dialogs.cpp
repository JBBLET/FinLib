// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/FinTuiElements/Dialogs.hpp"

#include <utility>

namespace finui {

FormDialog makeCreatePortfolioDialog(CreatePortfolioForm& f, std::function<void()> onSubmit,
                                     std::function<void()> onCancel) {
    std::vector<FieldDef> fields = {
        TextDef{"Name", "Portfolio name", &f.name},
        DateDef{"Date", &f.date},
        SelectDef{"Currency", kCurrencies, &f.currencyIdx},
        SubmitButtonDef{"Cancel", std::move(onCancel), "Create", std::move(onSubmit)},
    };
    return FormDialog("Create Portfolio", std::move(fields));
}

FormDialog makeDeletePortfolioDialog(DeletePortfolioForm& f, std::function<void()> onSubmit,
                                     std::function<void()> onCancel) {
    std::vector<FieldDef> fields = {
        TextDef{"Confirm name", "Type portfolio name to confirm", &f.confirmName},
        SubmitButtonDef{"Cancel", std::move(onCancel), "Delete", std::move(onSubmit)},
    };
    return FormDialog("Delete Portfolio", std::move(fields));
}

FormDialog makeAddTransactionDialog(AddTransactionForm& f, std::function<void()> onSubmit,
                                    std::function<void()> onCancel) {
    std::vector<FieldDef> fields = {
        SelectDef{"Type", kTxnTypes, &f.typeIdx},
        DateDef{"Date", &f.date},
        TextDef{"Ticker", "AAPL / USD", &f.ticker},
        NumberDef{"Qty", "0.00", &f.qty},
        NumberDef{"Price", "0.00", &f.price},
        NumberDef{"Fees", "0.00", &f.fee},
        SubmitButtonDef{"Cancel", std::move(onCancel), "Add", std::move(onSubmit)},
    };
    return FormDialog("Add Transaction", std::move(fields));
}

}  // namespace finui
