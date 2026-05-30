// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/modules/portfolioModule/Dialogs.hpp"

#include <memory>
#include <utility>

#include "finTUI/modules/portfolioModule/PortfolioModuleTypes.hpp"

namespace finui {

std::shared_ptr<TuiFormDialog> makeCreatePortfolioDialog(CreatePortfolioForm& f, std::function<void()> onSubmit,
                                                         std::function<void()> onCancel) {
    return TuiFormDialogBuilder("Create Portfolio")
        .add(TextDef{"Name", "Portfolio name", &f.name})
        .add(DateDef{"Date", &f.date})
        .add(SelectDef{"Currency", currencies, &f.currencyIdx})
        .setOnCancel("Cancel", std::move(onCancel))
        .setOnConfirm("Create", std::move(onSubmit))
        .build();
}

std::shared_ptr<TuiFormDialog> makeAddTransactionDialog(AddTransactionForm& f, std::function<void()> onSubmit,
                                                        std::function<void()> onCancel) {
    return TuiFormDialogBuilder("Add Transaction")
        .add(SelectDef{"Type", transactionTypes, &f.typeIdx})
        .add(DateDef{"Date", &f.date})
        .add(TextDef{"Ticker", "AAPL / USD", &f.ticker})
        .add(NumberDef{"Qty", "0.00", &f.qty})
        .add(NumberDef{"Price", "0.00", &f.price})
        .add(NumberDef{"Fees", "0.00", &f.fee})
        .setOnCancel("Cancel", std::move(onCancel))
        .setOnConfirm("Add", std::move(onSubmit))
        .build();
}

}  // namespace finui
