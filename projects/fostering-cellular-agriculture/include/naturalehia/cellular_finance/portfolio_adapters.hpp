// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/portfolio.hpp>
#include <naturalehia/cellular_finance/staged_capital.hpp>

#include <string>
#include <vector>

namespace naturalehia::cellular_finance {

struct StagedCompletionSourceAllocation {
    std::string case_id{};
    // Preserved verbatim across scenarios so a declared payer/offtaker can be
    // tracked consistently. The identifier is analyst input, not evidence
    // that the counterparty exists, will pay, or is creditworthy.
    std::string cash_source_id{};
    PortfolioCashSource source{PortfolioCashSource::Commercial};
    double amount_million{0.0};
};

// Converts the actual configured staged-capital paths, including provider
// nonperformance. Allocations must exhaust each positive completed-path
// provider repayment and may split it across multiple explicitly identified
// sources, including multiple counterparties in one taxonomy. A source id
// must retain one taxonomy across the returned distribution. Because the
// staged model supplies no source seniority, funded principal and PIK return
// are allocated pro rata across those sources. This does not use the staged
// model's separate all-provider-performs fee-replay sensitivity. It preserves
// dated provider net cash and principal loss;
// unpaid contractual PIK remains claim writeoff rather than being relabeled
// as cash-principal loss. Case ids become scenario ids, but the adapter leaves
// factor tags empty rather than inventing common-factor exposure from model
// provenance or an evaluated outcome. Non-completion repayment retains the
// staged model's aggregate recovery boundary under a generated project
// recovery id; this v0.1 adapter does not infer recovery-counterparty detail.
[[nodiscard]] PortfolioConfig adapt_staged_capital_to_portfolio(
    const StagedCapitalConfig& staged, std::string project_id,
    ProjectStage stage,
    const std::vector<StagedCompletionSourceAllocation>&
        completion_source_allocations);

} // namespace naturalehia::cellular_finance
