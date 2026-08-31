// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/claim_ledger.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr double kMaximumMoneyMillion = 1.0e9;
constexpr double kMaximumAnnualEffectiveRate = 10.0;
constexpr double kAbsoluteTolerance = 1.0e-10;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;

struct Range {
    bool available{true};
    double lower{0.0};
    double upper{0.0};
    bool point_exact{true};
};

struct PeriodAmounts {
    Range buyer_price{};
    Range buyer_direct_cost{};
    Range borrower_gross_proceeds{};
    Range borrower_net_proceeds{};
    Range cash_fee{};
    Range borrower_third_party_cost{};
    Range funded_principal{};
    Range original_issue_discount{};
    Range original_issue_premium{};
    Range capitalized_fee{};
    Range interest_accrual{};
    Range capitalized_interest{};
    Range principal_due{};
    Range interest_due{};
    Range principal_cash{};
    Range interest_cash{};
    Range recovery_principal_cash{};
    Range recovery_interest_cash{};
    Range conversion_principal{};
    Range conversion_interest{};
    Range conversion_units{};
    Range principal_writeoff{};
    Range interest_writeoff{};
    Range guarantee_principal{};
    Range guarantee_interest{};
};

struct ProviderCashAmounts {
    Range principal{};
    Range interest{};
};

struct ProviderTerms {
    bool computable{false};
    double allocation{0.0};
    double coverage{0.0};
    double deductible{0.0};
    double cap{0.0};
    std::size_t lag{0U};
    std::vector<std::string> blockers{};
};

[[nodiscard]] double tolerance(double first, double second) noexcept {
    return kAbsoluteTolerance +
        256.0 * std::numeric_limits<double>::epsilon() *
            std::max({1.0, std::abs(first), std::abs(second)});
}

[[nodiscard]] bool nearly_equal(double first, double second) noexcept {
    return std::abs(first - second) <= tolerance(first, second);
}

[[nodiscard]] bool materially_greater(
    double first, double second) noexcept {
    return first > second + tolerance(first, second);
}

void add_unique(std::vector<std::string>& values, std::string message) {
    if (std::find(values.begin(), values.end(), message) == values.end()) {
        values.push_back(std::move(message));
    }
}

[[nodiscard]] bool ascii_alphanumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool safe_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength ||
        !ascii_alphanumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return ascii_alphanumeric(character) || character == '-' ||
            character == '_' || character == '.';
    });
}

void require_safe_identifier(
    std::string_view value, std::string_view description) {
    if (!safe_identifier(value)) {
        throw std::invalid_argument(
            std::string(description) + " must be a safe bounded identifier");
    }
}

void require_safe_text(std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength ||
        value.front() == ' ' || value.back() == ' ') {
        throw std::invalid_argument(
            std::string(description) + " must be non-empty and bounded");
    }
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                std::string(description) + " contains a control character");
        }
    }
}

void validate_value_shape(
    const ClaimLedgerValue& value, std::string_view description) {
    const bool has_lower = value.lower.has_value();
    const bool has_upper = value.upper.has_value();
    if (value.status == ClaimLedgerValueStatus::Unknown ||
        value.status == ClaimLedgerValueStatus::NotApplicable) {
        if (has_lower || has_upper) {
            throw std::invalid_argument(
                std::string(description) +
                " unknown/not-applicable value cannot carry a number");
        }
        return;
    }
    if (!has_lower || !has_upper || !std::isfinite(*value.lower) ||
        !std::isfinite(*value.upper) || *value.lower > *value.upper) {
        throw std::invalid_argument(
            std::string(description) +
            " known/bounded value requires ordered finite endpoints");
    }
    if (value.status == ClaimLedgerValueStatus::Known &&
        *value.lower != *value.upper) {
        throw std::invalid_argument(
            std::string(description) + " known endpoints must be equal");
    }
    if (value.status == ClaimLedgerValueStatus::Bounded &&
        nearly_equal(*value.lower, *value.upper)) {
        throw std::invalid_argument(
            std::string(description) +
            " equal endpoints must use known status");
    }
}

void validate_non_negative(const ClaimLedgerValue& value,
    std::string_view description, double maximum) {
    validate_value_shape(value, description);
    if (!value.lower.has_value()) {
        return;
    }
    if (*value.lower < 0.0 ||
        (*value.lower == 0.0 && std::signbit(*value.lower)) ||
        *value.upper > maximum) {
        throw std::invalid_argument(
            std::string(description) +
            " must be non-negative and within its guardrail");
    }
}

void validate_annual_effective_rate(const ClaimLedgerValue& value,
    std::string_view description) {
    validate_value_shape(value, description);
    if (!value.lower.has_value()) return;
    if (*value.lower <= -1.0 || *value.upper > kMaximumAnnualEffectiveRate) {
        throw std::invalid_argument(
            std::string(description) +
            " must be greater than -1 and within its upper guardrail");
    }
}

[[nodiscard]] Range unknown_range() noexcept {
    return Range{false, 0.0, 0.0};
}

[[nodiscard]] Range from_value(const ClaimLedgerValue& value) noexcept {
    if (value.status == ClaimLedgerValueStatus::NotApplicable) {
        return unknown_range();
    }
    if (!value.lower.has_value() || !value.upper.has_value()) {
        return unknown_range();
    }
    return Range{true, *value.lower, *value.upper,
        value.status == ClaimLedgerValueStatus::Known};
}

[[nodiscard]] ClaimLedgerValue to_value(const Range& range) {
    if (!range.available) {
        return claim_ledger_unknown();
    }
    if (range.point_exact && range.lower == range.upper) {
        return claim_ledger_known(range.lower);
    }
    if (range.lower == range.upper ||
        nearly_equal(range.lower, range.upper)) {
        return claim_ledger_unknown();
    }
    return claim_ledger_bounded(range.lower, range.upper);
}

[[nodiscard]] Range add(const Range& first, const Range& second) noexcept {
    if (!first.available || !second.available) {
        return unknown_range();
    }
    return Range{
        true, first.lower + second.lower, first.upper + second.upper,
        first.point_exact && second.point_exact};
}

[[nodiscard]] Range scale(const Range& value, double scalar) noexcept {
    if (!value.available) {
        return unknown_range();
    }
    return scalar >= 0.0
        ? Range{true, value.lower * scalar, value.upper * scalar,
              value.point_exact}
        : Range{true, value.upper * scalar, value.lower * scalar,
              value.point_exact};
}

[[nodiscard]] bool exact(const Range& value) noexcept {
    return value.available && value.point_exact &&
        value.lower == value.upper;
}

[[nodiscard]] double exact_value(const Range& value) {
    if (!exact(value)) {
        throw std::logic_error("exact claim-ledger value is unavailable");
    }
    return 0.5 * (value.lower + value.upper);
}

[[nodiscard]] bool known_value(
    const ClaimLedgerValue& value, double& result) noexcept {
    if (value.status != ClaimLedgerValueStatus::Known ||
        !value.lower.has_value() || !value.upper.has_value()) {
        return false;
    }
    result = 0.5 * (*value.lower + *value.upper);
    return true;
}

[[nodiscard]] Range shortfall(
    const Range& due, const Range& prior_cash) noexcept {
    if (!due.available || !prior_cash.available) {
        return unknown_range();
    }
    return Range{true, std::max(0.0, due.lower - prior_cash.upper),
        std::max(0.0, due.upper - prior_cash.lower),
        due.point_exact && prior_cash.point_exact};
}

[[nodiscard]] bool is_guarantee(ClaimLedgerEntryKind kind) noexcept {
    return kind == ClaimLedgerEntryKind::GuaranteePrincipalCash ||
        kind == ClaimLedgerEntryKind::GuaranteeInterestCash;
}

[[nodiscard]] bool is_conversion(ClaimLedgerEntryKind kind) noexcept {
    return kind ==
            ClaimLedgerEntryKind::ConversionPrincipalExtinguishment ||
        kind == ClaimLedgerEntryKind::ConversionInterestExtinguishment ||
        kind == ClaimLedgerEntryKind::ConversionUnits;
}

[[nodiscard]] bool is_funding(ClaimLedgerEntryKind kind) noexcept {
    return kind == ClaimLedgerEntryKind::BuyerPrice ||
        kind == ClaimLedgerEntryKind::BuyerDirectCost ||
        kind == ClaimLedgerEntryKind::BorrowerGrossProceeds ||
        kind == ClaimLedgerEntryKind::BorrowerNetProceeds ||
        kind == ClaimLedgerEntryKind::CashFee ||
        kind == ClaimLedgerEntryKind::BorrowerThirdPartyCost ||
        kind == ClaimLedgerEntryKind::FundedPrincipal ||
        kind == ClaimLedgerEntryKind::OriginalIssueDiscount ||
        kind == ClaimLedgerEntryKind::OriginalIssuePremium ||
        kind == ClaimLedgerEntryKind::CapitalizedFee;
}

[[nodiscard]] Range* amount_for(
    PeriodAmounts& amounts, ClaimLedgerEntryKind kind) noexcept {
    switch (kind) {
    case ClaimLedgerEntryKind::BuyerPrice:
        return &amounts.buyer_price;
    case ClaimLedgerEntryKind::BuyerDirectCost:
        return &amounts.buyer_direct_cost;
    case ClaimLedgerEntryKind::BorrowerGrossProceeds:
        return &amounts.borrower_gross_proceeds;
    case ClaimLedgerEntryKind::BorrowerNetProceeds:
        return &amounts.borrower_net_proceeds;
    case ClaimLedgerEntryKind::CashFee:
        return &amounts.cash_fee;
    case ClaimLedgerEntryKind::BorrowerThirdPartyCost:
        return &amounts.borrower_third_party_cost;
    case ClaimLedgerEntryKind::FundedPrincipal:
        return &amounts.funded_principal;
    case ClaimLedgerEntryKind::OriginalIssueDiscount:
        return &amounts.original_issue_discount;
    case ClaimLedgerEntryKind::OriginalIssuePremium:
        return &amounts.original_issue_premium;
    case ClaimLedgerEntryKind::CapitalizedFee:
        return &amounts.capitalized_fee;
    case ClaimLedgerEntryKind::InterestAccrual:
        return &amounts.interest_accrual;
    case ClaimLedgerEntryKind::CapitalizedInterest:
        return &amounts.capitalized_interest;
    case ClaimLedgerEntryKind::PrincipalDue:
        return &amounts.principal_due;
    case ClaimLedgerEntryKind::InterestDue:
        return &amounts.interest_due;
    case ClaimLedgerEntryKind::PrincipalCash:
        return &amounts.principal_cash;
    case ClaimLedgerEntryKind::InterestCash:
        return &amounts.interest_cash;
    case ClaimLedgerEntryKind::RecoveryPrincipalCash:
        return &amounts.recovery_principal_cash;
    case ClaimLedgerEntryKind::RecoveryInterestCash:
        return &amounts.recovery_interest_cash;
    case ClaimLedgerEntryKind::ConversionPrincipalExtinguishment:
        return &amounts.conversion_principal;
    case ClaimLedgerEntryKind::ConversionInterestExtinguishment:
        return &amounts.conversion_interest;
    case ClaimLedgerEntryKind::ConversionUnits:
        return &amounts.conversion_units;
    case ClaimLedgerEntryKind::PrincipalWriteoff:
        return &amounts.principal_writeoff;
    case ClaimLedgerEntryKind::AccruedInterestWriteoff:
        return &amounts.interest_writeoff;
    case ClaimLedgerEntryKind::GuaranteePrincipalCash:
        return &amounts.guarantee_principal;
    case ClaimLedgerEntryKind::GuaranteeInterestCash:
        return &amounts.guarantee_interest;
    }
    return nullptr;
}

[[nodiscard]] std::vector<PeriodAmounts> aggregate(
    const std::vector<const ClaimLedgerEntry*>& entries,
    std::size_t horizon) {
    std::vector<PeriodAmounts> result(horizon + 1U);
    for (const ClaimLedgerEntry* entry : entries) {
        Range* amount = amount_for(result[entry->period], entry->kind);
        if (amount == nullptr) {
            throw std::logic_error("unknown claim-ledger entry kind");
        }
        *amount = add(*amount, from_value(entry->value));
    }
    return result;
}

[[nodiscard]] bool ranges_overlap(
    const Range& first, const Range& second) noexcept {
    return !first.available || !second.available ||
        (!materially_greater(first.lower, second.upper) &&
            !materially_greater(second.lower, first.upper));
}

void require_overlap(
    const Range& first, const Range& second, std::string_view description) {
    if (!ranges_overlap(first, second)) {
        throw std::invalid_argument(
            std::string(description) + " has no jointly feasible value");
    }
}

void require_common_overlap(std::initializer_list<Range> values,
    std::string_view description) {
    double lower = -std::numeric_limits<double>::infinity();
    double upper = std::numeric_limits<double>::infinity();
    for (const Range& value : values) {
        if (!value.available) return;
        lower = std::max(lower, value.lower);
        upper = std::min(upper, value.upper);
    }
    if (materially_greater(lower, upper)) {
        throw std::invalid_argument(
            std::string(description) + " has no jointly feasible value");
    }
}

[[nodiscard]] std::vector<const ClaimLedgerEntry*> collect_entries(
    const ClaimLedgerConfig& config,
    const std::vector<ClaimLedgerEntry>* scenario, bool decision_only) {
    std::vector<const ClaimLedgerEntry*> result;
    result.reserve(config.common_entries.size() +
        (scenario == nullptr ? 0U : scenario->size()));
    std::unordered_map<std::string, std::size_t> selected_by_fact;
    const auto append = [&](const std::vector<ClaimLedgerEntry>& source) {
        for (const ClaimLedgerEntry& entry : source) {
            if (decision_only &&
                entry.known_at_period > config.decision_period) {
                continue;
            }
            const auto [found, inserted] = selected_by_fact.emplace(
                entry.economic_fact_id, result.size());
            if (inserted) {
                result.push_back(&entry);
                continue;
            }
            const ClaimLedgerEntry* current = result[found->second];
            if (entry.kind != current->kind ||
                entry.provider_claim_id != current->provider_claim_id ||
                entry.event_group_id != current->event_group_id) {
                throw std::invalid_argument(
                    "economic-fact versions cannot change accounting kind, provider, or event group: " +
                    entry.economic_fact_id);
            }
            if (entry.known_at_period == current->known_at_period) {
                throw std::invalid_argument(
                    "economic fact has competing versions at the same information cut: " +
                    entry.economic_fact_id);
            }
            if (entry.known_at_period > current->known_at_period) {
                result[found->second] = &entry;
            }
        }
    };
    append(config.common_entries);
    if (scenario != nullptr) {
        append(*scenario);
    }
    return result;
}

void validate_groups(const std::vector<const ClaimLedgerEntry*>& entries) {
    struct Group {
        std::optional<std::size_t> period{};
        std::map<ClaimLedgerEntryKind, const ClaimLedgerEntry*> kinds{};
    };
    std::unordered_map<std::string, Group> groups;
    for (const ClaimLedgerEntry* entry : entries) {
        if (entry->event_group_id == "none") {
            continue;
        }
        Group& group = groups[entry->event_group_id];
        if (group.period.has_value() && *group.period != entry->period) {
            throw std::invalid_argument(
                "one event group cannot span multiple periods");
        }
        group.period = entry->period;
        if (!group.kinds.emplace(entry->kind, entry).second) {
            throw std::invalid_argument(
                "duplicate entry kind in one event group would double count");
        }
    }
    for (const auto& [group_id, group] : groups) {
        const auto has = [&group](ClaimLedgerEntryKind kind) {
            return group.kinds.contains(kind);
        };
        const bool funding = std::any_of(group.kinds.begin(),
            group.kinds.end(), [](const auto& item) {
                return is_funding(item.first);
            });
        if (funding) {
            constexpr ClaimLedgerEntryKind required[] = {
                ClaimLedgerEntryKind::BuyerPrice,
                ClaimLedgerEntryKind::BuyerDirectCost,
                ClaimLedgerEntryKind::BorrowerGrossProceeds,
                ClaimLedgerEntryKind::BorrowerNetProceeds,
                ClaimLedgerEntryKind::CashFee,
                ClaimLedgerEntryKind::BorrowerThirdPartyCost,
                ClaimLedgerEntryKind::FundedPrincipal,
                ClaimLedgerEntryKind::OriginalIssueDiscount,
                ClaimLedgerEntryKind::OriginalIssuePremium,
                ClaimLedgerEntryKind::CapitalizedFee,
            };
            for (const ClaimLedgerEntryKind kind : required) {
                if (!has(kind)) {
                    throw std::invalid_argument(
                        "funding event " + group_id +
                        " omits an explicit closing field");
                }
            }
            const auto value = [&group](ClaimLedgerEntryKind kind) {
                return from_value(group.kinds.at(kind)->value);
            };
            const Range price = value(ClaimLedgerEntryKind::BuyerPrice);
            const Range gross =
                value(ClaimLedgerEntryKind::BorrowerGrossProceeds);
            const Range funded =
                value(ClaimLedgerEntryKind::FundedPrincipal);
            const Range discount =
                value(ClaimLedgerEntryKind::OriginalIssueDiscount);
            const Range premium =
                value(ClaimLedgerEntryKind::OriginalIssuePremium);
            const Range uses = add(add(
                value(ClaimLedgerEntryKind::BorrowerNetProceeds),
                value(ClaimLedgerEntryKind::CashFee)),
                value(ClaimLedgerEntryKind::BorrowerThirdPartyCost));
            if (materially_greater(discount.upper, 0.0) &&
                materially_greater(premium.upper, 0.0)) {
                throw std::invalid_argument(
                    "one funding event cannot have both an original-issue discount and premium");
            }
            require_overlap(price, gross,
                "buyer price and borrower gross proceeds");
            require_overlap(gross, uses,
                "borrower gross-to-net proceeds identity");
            const Range implied_price = add(
                add(funded, scale(discount, -1.0)), premium);
            require_common_overlap({price, gross, uses, implied_price},
                "funding cash and issue-price equality system");
        }
        const bool conversion = std::any_of(group.kinds.begin(),
            group.kinds.end(), [](const auto& item) {
                return is_conversion(item.first);
            });
        if (conversion &&
            (!has(ClaimLedgerEntryKind::ConversionUnits) ||
                (!has(ClaimLedgerEntryKind::
                         ConversionPrincipalExtinguishment) &&
                    !has(ClaimLedgerEntryKind::
                        ConversionInterestExtinguishment)))) {
            throw std::invalid_argument(
                "conversion event " + group_id +
                " requires units and extinguishment");
        }
        if (conversion) {
            const Range units = from_value(
                group.kinds.at(ClaimLedgerEntryKind::ConversionUnits)->value);
            Range extinguishment{};
            if (has(ClaimLedgerEntryKind::
                    ConversionPrincipalExtinguishment)) {
                extinguishment = add(extinguishment,
                    from_value(group.kinds
                            .at(ClaimLedgerEntryKind::
                                ConversionPrincipalExtinguishment)
                            ->value));
            }
            if (has(ClaimLedgerEntryKind::
                    ConversionInterestExtinguishment)) {
                extinguishment = add(extinguishment,
                    from_value(group.kinds
                            .at(ClaimLedgerEntryKind::
                                ConversionInterestExtinguishment)
                            ->value));
            }
            if (units.available && extinguishment.available) {
                if (materially_greater(extinguishment.lower, 0.0) &&
                    !materially_greater(units.upper, 0.0)) {
                    throw std::invalid_argument(
                        "positive conversion extinguishment requires positive units");
                }
                if (materially_greater(units.lower, 0.0) &&
                    !materially_greater(extinguishment.upper, 0.0)) {
                    throw std::invalid_argument(
                        "positive conversion units require positive extinguishment");
                }
            }
        }
    }
}

[[nodiscard]] bool exact_non_negative(
    const ClaimLedgerValue& value, double& result) noexcept {
    if (!known_value(value, result)) {
        return false;
    }
    return result >= 0.0;
}

[[nodiscard]] ProviderTerms read_provider_terms(
    const ClaimLedgerProviderClaim& provider, bool decision_only,
    std::size_t decision_period) {
    ProviderTerms result;
    if (decision_only && provider.known_at_period > decision_period) {
        result.blockers.emplace_back(
            "provider terms were not known at the decision period");
        return result;
    }
    double lag = 0.0;
    if (!exact_non_negative(
            provider.shortfall_allocation_fraction, result.allocation) ||
        result.allocation > 1.0) {
        result.blockers.emplace_back(
            "shortfall allocation is not exact within [0,1]");
    }
    if (!exact_non_negative(provider.coverage_fraction, result.coverage) ||
        result.coverage > 1.0) {
        result.blockers.emplace_back(
            "coverage fraction is not exact within [0,1]");
    }
    if (!exact_non_negative(provider.deductible_million, result.deductible)) {
        result.blockers.emplace_back("deductible is not exact");
    }
    if (!exact_non_negative(provider.maximum_cash_million, result.cap)) {
        result.blockers.emplace_back("provider cap is not exact");
    }
    if (!exact_non_negative(provider.settlement_lag_periods, lag) ||
        !nearly_equal(lag, std::round(lag)) ||
        lag > static_cast<double>(kClaimLedgerMaximumPeriods)) {
        result.blockers.emplace_back(
            "settlement lag is not an exact supported integer");
    } else {
        result.lag = static_cast<std::size_t>(std::llround(lag));
    }
    if (!provider.covers_principal_due &&
        !provider.covers_interest_due) {
        result.blockers.emplace_back(
            "provider covers neither principal nor interest due");
    }
    if (!provider.payment_right_evidenced) {
        result.blockers.emplace_back("payment right is not evidenced");
    }
    if (!provider.provider_identity_evidenced) {
        result.blockers.emplace_back("provider identity is not evidenced");
    }
    if (!provider.coverage_and_priority_evidenced ||
        !provider.obligation_priority.has_value()) {
        result.blockers.emplace_back(
            "coverage and obligation priority are not evidenced");
    }
    result.computable = result.blockers.empty();
    return result;
}

[[nodiscard]] Range term_range(const ClaimLedgerValue& value,
    std::size_t known_at, bool decision_only,
    std::size_t decision_period) noexcept {
    if (decision_only && known_at > decision_period) {
        return unknown_range();
    }
    return from_value(value);
}

[[nodiscard]] Range reduce_balance(const Range& balance,
    const Range& reduction, std::string_view description,
    std::vector<std::string>& blockers) {
    if (!balance.available || !reduction.available) {
        add_unique(blockers,
            std::string(description) + " is not exactly reconcilable");
        return unknown_range();
    }
    if (materially_greater(reduction.lower, balance.upper)) {
        throw std::invalid_argument(
            std::string(description) + " exceeds the available obligation");
    }
    if (materially_greater(reduction.upper, balance.lower)) {
        add_unique(blockers,
            std::string(description) +
                " may overpay within the stated bounds");
        return unknown_range();
    }
    return Range{true,
        std::max(0.0, balance.lower - reduction.upper),
        std::max(0.0, balance.upper - reduction.lower),
        balance.point_exact && reduction.point_exact};
}

void check_due(const Range& due, const Range& balance,
    std::string_view description, std::vector<std::string>& blockers) {
    if (!due.available || !balance.available) {
        add_unique(blockers,
            std::string(description) + " is not exactly reconcilable");
        return;
    }
    if (materially_greater(due.lower, balance.upper)) {
        throw std::invalid_argument(
            std::string(description) + " exceeds the available obligation");
    }
    if (materially_greater(due.upper, balance.lower)) {
        add_unique(blockers,
            std::string(description) +
                " may exceed the obligation within stated bounds");
    }
}

[[nodiscard]] bool settlement_exact(
    const std::vector<const ClaimLedgerEntry*>& entries) {
    std::unordered_map<std::string,
        std::map<ClaimLedgerEntryKind, Range>> groups;
    for (const ClaimLedgerEntry* entry : entries) {
        if (is_funding(entry->kind)) {
            groups[entry->event_group_id].emplace(
                entry->kind, from_value(entry->value));
        }
    }
    for (const auto& [group_id, values] : groups) {
        static_cast<void>(group_id);
        constexpr ClaimLedgerEntryKind required[] = {
            ClaimLedgerEntryKind::BuyerPrice,
            ClaimLedgerEntryKind::BuyerDirectCost,
            ClaimLedgerEntryKind::BorrowerGrossProceeds,
            ClaimLedgerEntryKind::BorrowerNetProceeds,
            ClaimLedgerEntryKind::CashFee,
            ClaimLedgerEntryKind::BorrowerThirdPartyCost,
            ClaimLedgerEntryKind::FundedPrincipal,
            ClaimLedgerEntryKind::OriginalIssueDiscount,
            ClaimLedgerEntryKind::OriginalIssuePremium,
            ClaimLedgerEntryKind::CapitalizedFee,
        };
        if (std::any_of(std::begin(required), std::end(required),
                [&values](ClaimLedgerEntryKind kind) {
                    return !values.contains(kind);
                })) {
            return false;
        }
        const auto get = [&values](ClaimLedgerEntryKind kind) {
            return values.at(kind);
        };
        const Range price = get(ClaimLedgerEntryKind::BuyerPrice);
        const Range gross =
            get(ClaimLedgerEntryKind::BorrowerGrossProceeds);
        const Range funded = get(ClaimLedgerEntryKind::FundedPrincipal);
        const Range discount =
            get(ClaimLedgerEntryKind::OriginalIssueDiscount);
        const Range premium =
            get(ClaimLedgerEntryKind::OriginalIssuePremium);
        const Range uses = add(add(
            get(ClaimLedgerEntryKind::BorrowerNetProceeds),
            get(ClaimLedgerEntryKind::CashFee)),
            get(ClaimLedgerEntryKind::BorrowerThirdPartyCost));
        const Range buyer_cost =
            get(ClaimLedgerEntryKind::BuyerDirectCost);
        if (!exact(price) || !exact(gross) || !exact(funded) ||
            !exact(discount) || !exact(premium) || !exact(uses) ||
            !exact(buyer_cost) ||
            !nearly_equal(exact_value(price), exact_value(gross)) ||
            !nearly_equal(exact_value(funded),
                exact_value(price) + exact_value(discount) -
                    exact_value(premium)) ||
            !nearly_equal(exact_value(gross), exact_value(uses))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] ClaimLedgerProviderPathResult evaluate_provider(
    const ClaimLedgerProviderClaim& provider, const ProviderTerms& terms,
    const std::vector<ProviderCashAmounts>& provider_cash,
    const std::vector<ClaimLedgerPeriodResult>& periods,
    std::size_t horizon) {
    ClaimLedgerProviderPathResult result;
    result.provider_claim_id = provider.provider_claim_id;
    result.computable = terms.computable;
    result.blockers = terms.blockers;
    result.periods.resize(horizon + 1U);
    Range total_cash{};
    for (std::size_t period = 0U; period <= horizon; ++period) {
        ClaimLedgerProviderPeriodResult& report = result.periods[period];
        report.period = period;
        const Range principal_cash = provider_cash[period].principal;
        const Range interest_cash = provider_cash[period].interest;
        report.guarantee_principal_cash_million = to_value(principal_cash);
        report.guarantee_interest_cash_million = to_value(interest_cash);
        report.guarantee_cash_million =
            to_value(add(principal_cash, interest_cash));
        total_cash = add(total_cash, add(principal_cash, interest_cash));
    }
    result.total_guarantee_cash_million = to_value(total_cash);
    if (!result.computable) {
        for (ClaimLedgerProviderPeriodResult& period : result.periods) {
            period.allocated_principal_shortfall_million =
                claim_ledger_unknown();
            period.allocated_interest_shortfall_million =
                claim_ledger_unknown();
            period.allocated_shortfall_million = claim_ledger_unknown();
            period.principal_claim_generated_million =
                claim_ledger_unknown();
            period.interest_claim_generated_million =
                claim_ledger_unknown();
            period.claim_generated_million = claim_ledger_unknown();
            period.principal_claim_payable_million = claim_ledger_unknown();
            period.interest_claim_payable_million = claim_ledger_unknown();
            period.claim_payable_million = claim_ledger_unknown();
            period.unpaid_principal_payable_claim_million =
                claim_ledger_unknown();
            period.unpaid_interest_payable_claim_million =
                claim_ledger_unknown();
            period.unpaid_payable_claim_million = claim_ledger_unknown();
        }
        result.total_claim_generated_million = claim_ledger_unknown();
        result.terminal_unpaid_payable_claim_million =
            claim_ledger_unknown();
        result.claim_payable_after_horizon_million =
            claim_ledger_unknown();
        return result;
    }

    std::vector<double> principal_payable(horizon + 1U, 0.0);
    std::vector<double> interest_payable(horizon + 1U, 0.0);
    double cumulative_principal_shortfall = 0.0;
    double cumulative_interest_shortfall = 0.0;
    double cumulative_principal_claim = 0.0;
    double cumulative_interest_claim = 0.0;
    double unpaid_principal = 0.0;
    double unpaid_interest = 0.0;
    double after_horizon = 0.0;
    for (std::size_t period = 0U; period <= horizon; ++period) {
        double principal = 0.0;
        double interest = 0.0;
        double principal_cash = 0.0;
        double interest_cash = 0.0;
        double late_principal_resolution = 0.0;
        double late_interest_resolution = 0.0;
        const Range principal_resolution = add(add(add(
            from_value(periods[period].principal_cash_million),
            from_value(periods[period].recovery_principal_cash_million)),
            from_value(periods[period]
                .conversion_principal_extinguishment_million)),
            from_value(periods[period].principal_writeoff_million));
        const Range interest_resolution = add(add(add(add(
            from_value(periods[period].interest_cash_million),
            from_value(periods[period].recovery_interest_cash_million)),
            from_value(periods[period]
                .conversion_interest_extinguishment_million)),
            from_value(periods[period].accrued_interest_writeoff_million)),
            from_value(periods[period].capitalized_interest_million));
        if (!known_value(periods[period]
                    .principal_shortfall_after_borrower_recovery_million,
                principal) ||
            !known_value(periods[period]
                    .interest_shortfall_after_borrower_recovery_million,
                interest) ||
            !known_value(result.periods[period]
                    .guarantee_principal_cash_million,
                principal_cash) ||
            !known_value(result.periods[period]
                    .guarantee_interest_cash_million,
                interest_cash) ||
            !exact_non_negative(
                to_value(principal_resolution), late_principal_resolution) ||
            !exact_non_negative(
                to_value(interest_resolution), late_interest_resolution)) {
            result.computable = false;
            add_unique(result.blockers,
                "provider path contains a non-exact shortfall or cash value");
            break;
        }
        if ((cumulative_principal_shortfall > 0.0 &&
                late_principal_resolution > 0.0) ||
            (cumulative_interest_shortfall > 0.0 &&
                late_interest_resolution > 0.0)) {
            result.computable = false;
            add_unique(result.blockers,
                "late underlying cure, conversion, writeoff, capitalization, or subrogation allocation is unsupported after an earlier allocated due-date shortfall");
            break;
        }
        const double eligible_principal = terms.allocation *
            (provider.covers_principal_due ? principal : 0.0);
        const double eligible_interest = terms.allocation *
            (provider.covers_interest_due ? interest : 0.0);
        const double eligible = eligible_principal + eligible_interest;
        cumulative_principal_shortfall += eligible_principal;
        cumulative_interest_shortfall += eligible_interest;
        const double cumulative_shortfall =
            cumulative_principal_shortfall + cumulative_interest_shortfall;
        const double target = std::min(terms.cap,
            terms.coverage *
                std::max(0.0, cumulative_shortfall - terms.deductible));
        const double cumulative_claim =
            cumulative_principal_claim + cumulative_interest_claim;
        const double generated = std::max(0.0, target - cumulative_claim);
        const double available_principal = std::max(0.0,
            cumulative_principal_shortfall - cumulative_principal_claim);
        const double available_interest = std::max(0.0,
            cumulative_interest_shortfall - cumulative_interest_claim);
        double principal_generated = 0.0;
        double interest_generated = 0.0;
        switch (*provider.obligation_priority) {
        case ClaimLedgerProviderAllocationPriority::PrincipalFirst:
            principal_generated = std::min(generated, available_principal);
            interest_generated =
                std::min(generated - principal_generated, available_interest);
            break;
        case ClaimLedgerProviderAllocationPriority::InterestFirst:
            interest_generated = std::min(generated, available_interest);
            principal_generated =
                std::min(generated - interest_generated, available_principal);
            break;
        case ClaimLedgerProviderAllocationPriority::ProRata: {
            const double available_total =
                available_principal + available_interest;
            if (available_total > 0.0) {
                principal_generated =
                    generated * available_principal / available_total;
                interest_generated = generated - principal_generated;
            }
            break;
        }
        }
        if (!nearly_equal(
                principal_generated + interest_generated, generated)) {
            throw std::logic_error(
                "provider claim cannot be allocated to eligible obligations");
        }
        cumulative_principal_claim += principal_generated;
        cumulative_interest_claim += interest_generated;
        if (period + terms.lag <= horizon) {
            principal_payable[period + terms.lag] += principal_generated;
            interest_payable[period + terms.lag] += interest_generated;
        } else {
            after_horizon += generated;
        }
        unpaid_principal += principal_payable[period];
        unpaid_interest += interest_payable[period];
        if (materially_greater(principal_cash, unpaid_principal)) {
            throw std::invalid_argument(
                "guarantee principal cash exceeds the payable principal provider claim");
        }
        if (materially_greater(interest_cash, unpaid_interest)) {
            throw std::invalid_argument(
                "guarantee interest cash exceeds the payable interest provider claim");
        }
        unpaid_principal =
            std::max(0.0, unpaid_principal - principal_cash);
        unpaid_interest = std::max(0.0, unpaid_interest - interest_cash);
        ClaimLedgerProviderPeriodResult& report = result.periods[period];
        report.allocated_principal_shortfall_million =
            claim_ledger_known(eligible_principal);
        report.allocated_interest_shortfall_million =
            claim_ledger_known(eligible_interest);
        report.allocated_shortfall_million = claim_ledger_known(eligible);
        report.principal_claim_generated_million =
            claim_ledger_known(principal_generated);
        report.interest_claim_generated_million =
            claim_ledger_known(interest_generated);
        report.claim_generated_million = claim_ledger_known(generated);
        report.principal_claim_payable_million =
            claim_ledger_known(principal_payable[period]);
        report.interest_claim_payable_million =
            claim_ledger_known(interest_payable[period]);
        report.claim_payable_million = claim_ledger_known(
            principal_payable[period] + interest_payable[period]);
        report.unpaid_principal_payable_claim_million =
            claim_ledger_known(unpaid_principal);
        report.unpaid_interest_payable_claim_million =
            claim_ledger_known(unpaid_interest);
        report.unpaid_payable_claim_million =
            claim_ledger_known(unpaid_principal + unpaid_interest);
    }
    if (!result.computable) {
        for (ClaimLedgerProviderPeriodResult& period : result.periods) {
            period.allocated_principal_shortfall_million =
                claim_ledger_unknown();
            period.allocated_interest_shortfall_million =
                claim_ledger_unknown();
            period.allocated_shortfall_million = claim_ledger_unknown();
            period.principal_claim_generated_million =
                claim_ledger_unknown();
            period.interest_claim_generated_million =
                claim_ledger_unknown();
            period.claim_generated_million = claim_ledger_unknown();
            period.principal_claim_payable_million = claim_ledger_unknown();
            period.interest_claim_payable_million = claim_ledger_unknown();
            period.claim_payable_million = claim_ledger_unknown();
            period.unpaid_principal_payable_claim_million =
                claim_ledger_unknown();
            period.unpaid_interest_payable_claim_million =
                claim_ledger_unknown();
            period.unpaid_payable_claim_million = claim_ledger_unknown();
        }
        result.total_claim_generated_million = claim_ledger_unknown();
        result.terminal_unpaid_payable_claim_million =
            claim_ledger_unknown();
        result.claim_payable_after_horizon_million =
            claim_ledger_unknown();
        return result;
    }
    result.total_claim_generated_million =
        claim_ledger_known(
            cumulative_principal_claim + cumulative_interest_claim);
    result.terminal_unpaid_payable_claim_million = claim_ledger_known(
        unpaid_principal + unpaid_interest);
    result.claim_payable_after_horizon_million =
        claim_ledger_known(after_horizon);
    return result;
}

[[nodiscard]] Range investor_cash(const PeriodAmounts& amount) noexcept {
    Range result = add(amount.cash_fee,
        add(amount.principal_cash,
            add(amount.interest_cash,
                add(amount.recovery_principal_cash,
                    add(amount.recovery_interest_cash,
                        add(amount.guarantee_principal,
                            amount.guarantee_interest))))));
    result = add(result, scale(amount.buyer_price, -1.0));
    return add(result, scale(amount.buyer_direct_cost, -1.0));
}

[[nodiscard]] Range path_npv(
    const std::vector<ClaimLedgerPeriodResult>& periods,
    const Range& annual_rate, std::size_t periods_per_year) {
    if (!exact(annual_rate)) {
        return unknown_range();
    }
    std::vector<double> lower_cashflows;
    std::vector<double> upper_cashflows;
    lower_cashflows.reserve(periods.size());
    upper_cashflows.reserve(periods.size());
    bool cashflows_point_exact = true;
    for (const ClaimLedgerPeriodResult& period : periods) {
        const Range cash = from_value(period.investor_cashflow_million);
        if (!cash.available) return unknown_range();
        cashflows_point_exact =
            cashflows_point_exact && cash.point_exact;
        lower_cashflows.push_back(cash.lower);
        upper_cashflows.push_back(cash.upper);
    }
    const auto stable_npv = [&](const std::vector<double>& cashflows)
        -> std::optional<double> {
        double maximum_log_term =
            -std::numeric_limits<double>::infinity();
        const double log_rate = std::log1p(exact_value(annual_rate));
        for (std::size_t period = 0U; period < cashflows.size(); ++period) {
            if (cashflows[period] == 0.0) continue;
            maximum_log_term = std::max(maximum_log_term,
                std::log(std::abs(cashflows[period])) -
                    static_cast<double>(period) /
                        static_cast<double>(periods_per_year) * log_rate);
        }
        if (!std::isfinite(maximum_log_term)) return 0.0;
        double scaled_sum = 0.0;
        double correction = 0.0;
        for (std::size_t period = 0U; period < cashflows.size(); ++period) {
            if (cashflows[period] == 0.0) continue;
            const double log_term =
                std::log(std::abs(cashflows[period])) -
                static_cast<double>(period) /
                    static_cast<double>(periods_per_year) * log_rate;
            const double term = std::copysign(
                std::exp(log_term - maximum_log_term), cashflows[period]);
            const double adjusted = term - correction;
            const double next = scaled_sum + adjusted;
            correction = (next - scaled_sum) - adjusted;
            scaled_sum = next;
        }
        if (scaled_sum == 0.0) return 0.0;
        const double result_log =
            maximum_log_term + std::log(std::abs(scaled_sum));
        if (result_log > std::log(std::numeric_limits<double>::max())) {
            return std::nullopt;
        }
        const double value =
            std::copysign(std::exp(result_log), scaled_sum);
        return std::isfinite(value) ? std::optional<double>{value}
                                    : std::nullopt;
    };
    const std::optional<double> lower = stable_npv(lower_cashflows);
    const std::optional<double> upper = stable_npv(upper_cashflows);
    return lower.has_value() && upper.has_value()
        ? Range{true, *lower, *upper, cashflows_point_exact}
        : unknown_range();
}

[[nodiscard]] ClaimLedgerPathResult evaluate_path(
    const ClaimLedgerConfig& config,
    const std::vector<const ClaimLedgerEntry*>& entries,
    bool decision_only) {
    ClaimLedgerPathResult result;
    const std::vector<PeriodAmounts> amounts =
        aggregate(entries, config.horizon_period);
    result.periods.resize(config.horizon_period + 1U);
    result.settlement_reconciled = settlement_exact(entries);
    if (!result.settlement_reconciled) {
        result.blockers.emplace_back(
            "funding settlement is not exact and reconciled");
    }
    Range principal = term_range(config.opening_principal_million,
        config.opening_principal_known_at_period, decision_only,
        config.decision_period);
    Range interest = term_range(config.opening_accrued_interest_million,
        config.opening_accrued_interest_known_at_period, decision_only,
        config.decision_period);
    Range principal_due_outstanding{};
    Range interest_due_outstanding{};
    Range peak_ead{};
    Range peak_principal{};
    Range principal_loss{};
    Range interest_loss{};
    Range total_conversion_units{};
    bool balances_exact = exact(principal) && exact(interest);

    for (std::size_t period = 0U; period <= config.horizon_period; ++period) {
        const PeriodAmounts& amount = amounts[period];
        ClaimLedgerPeriodResult& report = result.periods[period];
        report.period = period;
        report.buyer_price_million = to_value(amount.buyer_price);
        report.buyer_direct_cost_million = to_value(amount.buyer_direct_cost);
        report.borrower_gross_proceeds_million =
            to_value(amount.borrower_gross_proceeds);
        report.borrower_net_proceeds_million =
            to_value(amount.borrower_net_proceeds);
        report.cash_fee_million = to_value(amount.cash_fee);
        report.borrower_third_party_cost_million =
            to_value(amount.borrower_third_party_cost);
        report.opening_principal_million = to_value(principal);
        report.funded_principal_million = to_value(amount.funded_principal);
        report.original_issue_discount_million =
            to_value(amount.original_issue_discount);
        report.original_issue_premium_million =
            to_value(amount.original_issue_premium);
        report.capitalized_fee_million = to_value(amount.capitalized_fee);
        report.capitalized_interest_million =
            to_value(amount.capitalized_interest);
        report.principal_due_million = to_value(amount.principal_due);
        report.principal_cash_million = to_value(amount.principal_cash);
        report.recovery_principal_cash_million =
            to_value(amount.recovery_principal_cash);
        report.guarantee_principal_cash_million =
            to_value(amount.guarantee_principal);
        report.conversion_principal_extinguishment_million =
            to_value(amount.conversion_principal);
        report.principal_writeoff_million =
            to_value(amount.principal_writeoff);
        report.opening_accrued_interest_million = to_value(interest);
        report.interest_accrual_million = to_value(amount.interest_accrual);
        report.interest_due_million = to_value(amount.interest_due);
        report.interest_cash_million = to_value(amount.interest_cash);
        report.recovery_interest_cash_million =
            to_value(amount.recovery_interest_cash);
        report.guarantee_interest_cash_million =
            to_value(amount.guarantee_interest);
        report.conversion_interest_extinguishment_million =
            to_value(amount.conversion_interest);
        report.accrued_interest_writeoff_million =
            to_value(amount.interest_writeoff);
        report.conversion_units = to_value(amount.conversion_units);

        Range interest_before_cap = add(interest, amount.interest_accrual);
        const Range interest_not_already_due = reduce_balance(
            interest_before_cap, interest_due_outstanding,
            "outstanding interest due", result.blockers);
        check_due(amount.interest_due, interest_not_already_due,
            "new interest due", result.blockers);
        interest_due_outstanding =
            add(interest_due_outstanding, amount.interest_due);
        Range interest_before_resolution = reduce_balance(
            interest_before_cap, amount.capitalized_interest,
            "capitalized interest", result.blockers);
        interest_due_outstanding = shortfall(
            interest_due_outstanding, amount.capitalized_interest);
        Range principal_before_resolution = add(add(add(principal,
            amount.funded_principal), amount.capitalized_fee),
            amount.capitalized_interest);
        const Range principal_not_already_due = reduce_balance(
            principal_before_resolution, principal_due_outstanding,
            "outstanding principal due", result.blockers);
        check_due(amount.principal_due, principal_not_already_due,
            "new principal due", result.blockers);
        principal_due_outstanding =
            add(principal_due_outstanding, amount.principal_due);
        if (!peak_principal.available ||
            !principal_before_resolution.available) {
            peak_principal = unknown_range();
        } else {
            peak_principal.lower = std::max(
                peak_principal.lower, principal_before_resolution.lower);
            peak_principal.upper = std::max(
                peak_principal.upper, principal_before_resolution.upper);
            peak_principal.point_exact = peak_principal.point_exact &&
                principal_before_resolution.point_exact;
        }
        const Range ead = add(
            principal_before_resolution, interest_before_resolution);
        if (!peak_ead.available || !ead.available) {
            peak_ead = unknown_range();
        } else {
            peak_ead.lower = std::max(peak_ead.lower, ead.lower);
            peak_ead.upper = std::max(peak_ead.upper, ead.upper);
            peak_ead.point_exact =
                peak_ead.point_exact && ead.point_exact;
        }
        report.ead_before_resolution_million = to_value(ead);
        const Range prior_principal_cash = add(
            amount.principal_cash, amount.recovery_principal_cash);
        const Range prior_interest_cash =
            add(add(amount.interest_cash,
                    amount.recovery_interest_cash),
                amount.capitalized_interest);
        report.principal_shortfall_after_borrower_recovery_million =
            to_value(shortfall(amount.principal_due, prior_principal_cash));
        report.interest_shortfall_after_borrower_recovery_million =
            to_value(shortfall(amount.interest_due, prior_interest_cash));

        principal = reduce_balance(principal_before_resolution,
            amount.principal_cash, "borrower principal cash",
            result.blockers);
        principal_due_outstanding =
            shortfall(principal_due_outstanding, amount.principal_cash);
        principal = reduce_balance(principal,
            amount.recovery_principal_cash, "recovery principal cash",
            result.blockers);
        principal_due_outstanding = shortfall(
            principal_due_outstanding, amount.recovery_principal_cash);
        principal = reduce_balance(principal,
            amount.guarantee_principal, "guarantee principal cash",
            result.blockers);
        principal_due_outstanding = shortfall(
            principal_due_outstanding, amount.guarantee_principal);
        principal = reduce_balance(principal,
            amount.conversion_principal, "principal conversion",
            result.blockers);
        principal_due_outstanding = shortfall(
            principal_due_outstanding, amount.conversion_principal);
        principal = reduce_balance(principal,
            amount.principal_writeoff, "principal writeoff",
            result.blockers);
        principal_due_outstanding = shortfall(
            principal_due_outstanding, amount.principal_writeoff);
        interest = reduce_balance(interest_before_resolution,
            amount.interest_cash, "borrower interest cash",
            result.blockers);
        interest_due_outstanding =
            shortfall(interest_due_outstanding, amount.interest_cash);
        interest = reduce_balance(interest,
            amount.recovery_interest_cash, "recovery interest cash",
            result.blockers);
        interest_due_outstanding = shortfall(
            interest_due_outstanding, amount.recovery_interest_cash);
        interest = reduce_balance(interest,
            amount.guarantee_interest, "guarantee interest cash",
            result.blockers);
        interest_due_outstanding = shortfall(
            interest_due_outstanding, amount.guarantee_interest);
        interest = reduce_balance(interest,
            amount.conversion_interest, "interest conversion",
            result.blockers);
        interest_due_outstanding = shortfall(
            interest_due_outstanding, amount.conversion_interest);
        interest = reduce_balance(interest,
            amount.interest_writeoff, "accrued-interest writeoff",
            result.blockers);
        interest_due_outstanding = shortfall(
            interest_due_outstanding, amount.interest_writeoff);
        report.closing_principal_million = to_value(principal);
        report.closing_accrued_interest_million = to_value(interest);
        report.outstanding_principal_due_million =
            to_value(principal_due_outstanding);
        report.outstanding_interest_due_million =
            to_value(interest_due_outstanding);
        report.investor_cashflow_million = to_value(investor_cash(amount));
        principal_loss = add(principal_loss, amount.principal_writeoff);
        interest_loss = add(interest_loss, amount.interest_writeoff);
        total_conversion_units =
            add(total_conversion_units, amount.conversion_units);
        balances_exact = balances_exact && exact(principal) &&
            exact(interest) && exact(principal_due_outstanding) &&
            exact(interest_due_outstanding) && exact(amount.conversion_units) &&
            exact(ead) && exact(investor_cash(amount));
    }
    result.rollforwards_reconciled = balances_exact;
    if (!result.rollforwards_reconciled) {
        add_unique(result.blockers,
            "principal or accrued-interest rollforward is not exact");
    }

    const bool face_applicable =
        config.contractual_face_amount_million.status !=
            ClaimLedgerValueStatus::NotApplicable;
    if (!face_applicable) {
        result.contractual_face_reconciled = true;
    } else {
        const Range face = term_range(config.contractual_face_amount_million,
            config.face_amount_known_at_period, decision_only,
            config.decision_period);
        if (face.available && peak_principal.available) {
            if (materially_greater(peak_principal.lower, face.upper)) {
                throw std::invalid_argument(
                    "principal exposure exceeds contractual face amount");
            }
            result.contractual_face_reconciled =
                !materially_greater(peak_principal.upper, face.lower);
        }
    }
    if (!result.contractual_face_reconciled) {
        add_unique(result.blockers,
            "contractual face amount is unknown, bounded, or unreconciled");
    }

    Range discount = config.annual_effective_discount_rate.status ==
                ClaimLedgerValueStatus::NotApplicable ||
            !config.discount_rate_known_at_period.has_value()
        ? unknown_range()
        : term_range(config.annual_effective_discount_rate,
              *config.discount_rate_known_at_period, decision_only,
              config.decision_period);
    result.npv_million = to_value(path_npv(
        result.periods, discount, config.periods_per_year));
    result.peak_ead_million = to_value(peak_ead);
    result.terminal_principal_million = to_value(principal);
    result.terminal_accrued_interest_million = to_value(interest);
    result.terminal_total_exposure_million = to_value(add(principal, interest));
    result.total_conversion_units = to_value(total_conversion_units);
    result.principal_loss_million = to_value(principal_loss);
    result.accrued_interest_loss_million = to_value(interest_loss);
    result.total_loss_million = to_value(add(principal_loss, interest_loss));

    double wal_numerator = 0.0;
    double wal_denominator = 0.0;
    bool wal_exact = true;
    for (const ClaimLedgerPeriodResult& period : result.periods) {
        const Range principal_cash = add(add(
            from_value(period.principal_cash_million),
            from_value(period.recovery_principal_cash_million)),
            from_value(period.guarantee_principal_cash_million));
        if (!exact(principal_cash)) {
            wal_exact = false;
            break;
        }
        const double cash = exact_value(principal_cash);
        wal_numerator += static_cast<double>(period.period) * cash;
        wal_denominator += cash;
    }
    result.principal_cash_wal_months = wal_exact && wal_denominator > 0.0
        ? claim_ledger_known(wal_numerator / wal_denominator)
        : (wal_exact ? claim_ledger_not_applicable()
                     : claim_ledger_unknown());
    std::unordered_map<std::string, std::vector<ProviderCashAmounts>>
        provider_cash_index;
    double included_principal_provider_allocation = 0.0;
    double included_interest_provider_allocation = 0.0;
    for (const ClaimLedgerProviderClaim& provider : config.provider_claims) {
        if (decision_only &&
            provider.known_at_period > config.decision_period) {
            continue;
        }
        const Range allocation =
            from_value(provider.shortfall_allocation_fraction);
        if (allocation.available && allocation.lower >= 0.0 &&
            allocation.upper <= 1.0) {
            if (provider.covers_principal_due) {
                included_principal_provider_allocation += allocation.lower;
            }
            if (provider.covers_interest_due) {
                included_interest_provider_allocation += allocation.lower;
            }
        }
    }
    const bool principal_provider_allocations_overlap = materially_greater(
        included_principal_provider_allocation, 1.0);
    const bool interest_provider_allocations_overlap = materially_greater(
        included_interest_provider_allocation, 1.0);
    for (const ClaimLedgerProviderClaim& provider : config.provider_claims) {
        if (decision_only &&
            provider.known_at_period > config.decision_period) {
            continue;
        }
        provider_cash_index.emplace(provider.provider_claim_id,
            std::vector<ProviderCashAmounts>(config.horizon_period + 1U));
    }
    for (const ClaimLedgerEntry* entry : entries) {
        if (!is_guarantee(entry->kind)) continue;
        const auto provider_cash =
            provider_cash_index.find(entry->provider_claim_id);
        if (provider_cash == provider_cash_index.end()) {
            add_unique(result.blockers,
                "guarantee cash references provider terms unavailable at this information cut: " +
                    entry->provider_claim_id);
            continue;
        }
        ProviderCashAmounts& indexed =
            provider_cash->second[entry->period];
        Range& destination =
            entry->kind == ClaimLedgerEntryKind::GuaranteePrincipalCash
            ? indexed.principal
            : indexed.interest;
        destination = add(destination, from_value(entry->value));
    }
    for (const ClaimLedgerProviderClaim& provider : config.provider_claims) {
        if (decision_only &&
            provider.known_at_period > config.decision_period) {
            continue;
        }
        ProviderTerms terms = read_provider_terms(
            provider, decision_only, config.decision_period);
        if ((provider.covers_principal_due &&
                principal_provider_allocations_overlap) ||
            (provider.covers_interest_due &&
                interest_provider_allocations_overlap)) {
            terms.computable = false;
            add_unique(terms.blockers,
                "provider shortfall allocations overlap above 100 percent within a covered obligation category at this information cut");
        }
        result.provider_claims.push_back(evaluate_provider(provider, terms,
            provider_cash_index.at(provider.provider_claim_id),
            result.periods, config.horizon_period));
    }
    result.exact = result.settlement_reconciled &&
        result.rollforwards_reconciled &&
        result.contractual_face_reconciled && result.blockers.empty();
    return result;
}

void validate_config_impl(const ClaimLedgerConfig& config) {
    if (config.model_version != kClaimLedgerModelVersion) {
        throw std::invalid_argument(
            "unsupported claim-ledger model version");
    }
    require_safe_identifier(config.ledger_id, "claim-ledger id");
    require_safe_identifier(config.project_id, "project id");
    require_safe_identifier(config.claim_id, "claim id");
    require_safe_identifier(config.currency_label, "currency label");
    require_safe_text(config.monetary_basis, "monetary basis");
    require_safe_identifier(config.period_unit_label, "period-unit label");
    require_safe_identifier(
        config.conversion_unit_label, "conversion-unit label");
    require_safe_text(config.conversion_unit_basis, "conversion-unit basis");
    if (config.period_unit_label != "month" ||
        config.periods_per_year != 12U) {
        throw std::invalid_argument(
            "claim-ledger v0.1 requires monthly periods and 12 periods per year");
    }
    if (config.horizon_period > kClaimLedgerMaximumPeriods ||
        config.decision_period > config.horizon_period) {
        throw std::invalid_argument(
            "claim-ledger horizon or decision period is invalid");
    }
    const auto check_known_at = [](std::size_t period,
                                    std::string_view description) {
        if (period > kClaimLedgerMaximumPeriods) {
            throw std::invalid_argument(
                std::string(description) +
                " exceeds the evidence-period guardrail");
        }
    };
    check_known_at(config.face_amount_known_at_period,
        "face known-at period");
    check_known_at(config.opening_principal_known_at_period,
        "opening principal known-at period");
    check_known_at(config.opening_accrued_interest_known_at_period,
        "opening interest known-at period");
    if (config.discount_rate_known_at_period.has_value()) {
        check_known_at(*config.discount_rate_known_at_period,
            "discount-rate known-at period");
    }
    validate_non_negative(config.contractual_face_amount_million,
        "contractual face amount", kMaximumMoneyMillion);
    validate_non_negative(config.opening_principal_million,
        "opening principal", kMaximumMoneyMillion);
    validate_non_negative(config.opening_accrued_interest_million,
        "opening accrued interest", kMaximumMoneyMillion);
    if (config.opening_principal_million.status ==
            ClaimLedgerValueStatus::NotApplicable ||
        config.opening_accrued_interest_million.status ==
            ClaimLedgerValueStatus::NotApplicable) {
        throw std::invalid_argument(
            "opening principal and accrued interest require evidenced numeric values or UNKNOWN, not not-applicable");
    }
    validate_annual_effective_rate(config.annual_effective_discount_rate,
        "annual effective discount rate");
    if (config.scenarios.size() > kClaimLedgerMaximumScenarios) {
        throw std::invalid_argument(
            "scenario count exceeds the resource guardrail");
    }
    if (config.provider_claims.size() >
        kClaimLedgerMaximumProviderClaims) {
        throw std::invalid_argument(
            "provider-claim count exceeds the resource guardrail");
    }
    const std::size_t retained_path_count =
        2U * (config.scenarios.size() + 1U);
    const std::size_t period_count = config.horizon_period + 1U;
    if (retained_path_count >
        kClaimLedgerMaximumRetainedPathPeriodCells / period_count) {
        throw std::invalid_argument(
            "scenario-period result product exceeds the resource guardrail");
    }
    const std::size_t retained_path_period_cells =
        retained_path_count * period_count;
    if (!config.provider_claims.empty() &&
        retained_path_period_cells >
            kClaimLedgerMaximumProviderPeriodCells /
                config.provider_claims.size()) {
        throw std::invalid_argument(
            "provider-scenario-period result product exceeds the resource guardrail");
    }

    std::unordered_set<std::string> provider_ids;
    double minimum_principal_allocation = 0.0;
    double minimum_interest_allocation = 0.0;
    for (const ClaimLedgerProviderClaim& provider :
        config.provider_claims) {
        require_safe_identifier(provider.provider_claim_id,
            "provider-claim id");
        require_safe_identifier(provider.provider_id, "provider id");
        require_safe_identifier(provider.source_record_id,
            "provider source-record id");
        if (!provider_ids.insert(provider.provider_claim_id).second) {
            throw std::invalid_argument("duplicate provider-claim id");
        }
        check_known_at(provider.known_at_period,
            "provider known-at period");
        validate_non_negative(provider.shortfall_allocation_fraction,
            "provider shortfall allocation", 1.0);
        validate_non_negative(provider.coverage_fraction,
            "provider coverage fraction", 1.0);
        validate_non_negative(provider.deductible_million,
            "provider deductible", kMaximumMoneyMillion);
        validate_non_negative(provider.maximum_cash_million,
            "provider cap", kMaximumMoneyMillion);
        validate_non_negative(provider.settlement_lag_periods,
            "provider settlement lag",
            static_cast<double>(kClaimLedgerMaximumPeriods));
        if (provider.settlement_lag_periods.lower.has_value()) {
            if (!nearly_equal(*provider.settlement_lag_periods.lower,
                    std::round(*provider.settlement_lag_periods.lower)) ||
                !nearly_equal(*provider.settlement_lag_periods.upper,
                    std::round(*provider.settlement_lag_periods.upper))) {
                throw std::invalid_argument(
                    "provider settlement-lag endpoints must be integers");
            }
        }
        const Range allocation =
            from_value(provider.shortfall_allocation_fraction);
        if (provider.known_at_period <= config.decision_period &&
            allocation.available) {
            if (provider.covers_principal_due) {
                minimum_principal_allocation += allocation.lower;
            }
            if (provider.covers_interest_due) {
                minimum_interest_allocation += allocation.lower;
            }
        }
    }
    if (materially_greater(minimum_principal_allocation, 1.0) ||
        materially_greater(minimum_interest_allocation, 1.0)) {
        throw std::invalid_argument(
            "provider shortfall allocations overlap above 100 percent within a covered obligation category");
    }

    std::unordered_set<std::string> entry_ids;
    std::unordered_set<std::string> covenant_event_ids;
    bool conversion_units_present = false;
    std::size_t entry_count = config.common_entries.size();
    std::size_t covenant_count = config.common_covenant_events.size();
    for (const ClaimLedgerScenario& scenario : config.scenarios) {
        entry_count += scenario.entries.size();
        if (scenario.covenant_events.size() >
            kClaimLedgerMaximumCovenantEvents -
                std::min(covenant_count,
                    kClaimLedgerMaximumCovenantEvents)) {
            throw std::invalid_argument(
                "covenant-event count exceeds the resource guardrail");
        }
        covenant_count += scenario.covenant_events.size();
    }
    if (entry_count > kClaimLedgerMaximumEntries) {
        throw std::invalid_argument(
            "entry count exceeds the resource guardrail");
    }
    if (covenant_count > kClaimLedgerMaximumCovenantEvents) {
        throw std::invalid_argument(
            "covenant-event count exceeds the resource guardrail");
    }
    constexpr std::size_t kEvaluationTraversalFactor = 3U;
    constexpr std::size_t kMaximumBasePathEntryVisits =
        kClaimLedgerMaximumPathEntryVisits / kEvaluationTraversalFactor;
    const std::size_t scenario_path_count = config.scenarios.size() + 1U;
    if (!config.common_entries.empty() &&
        scenario_path_count > kMaximumBasePathEntryVisits /
                config.common_entries.size()) {
        throw std::invalid_argument(
            "common-entry scenario work product exceeds the resource guardrail");
    }
    std::size_t base_path_entry_visits =
        scenario_path_count * config.common_entries.size();
    for (const ClaimLedgerScenario& scenario : config.scenarios) {
        if (scenario.entries.size() >
            kMaximumBasePathEntryVisits - base_path_entry_visits) {
            throw std::invalid_argument(
                "scenario-entry path work exceeds the resource guardrail");
        }
        base_path_entry_visits += scenario.entries.size();
    }
    const auto check_entries = [&](const std::vector<ClaimLedgerEntry>& entries) {
        for (const ClaimLedgerEntry& entry : entries) {
            require_safe_identifier(entry.entry_id, "entry id");
            require_safe_identifier(
                entry.economic_fact_id, "economic-fact id");
            require_safe_identifier(entry.event_group_id, "event-group id");
            require_safe_identifier(entry.source_record_id,
                "entry source-record id");
            require_safe_identifier(entry.provider_claim_id,
                "entry provider-claim id");
            if (entry.period > config.horizon_period) {
                throw std::invalid_argument(
                    "cash-event period follows the analysis horizon");
            }
            check_known_at(entry.known_at_period,
                "entry known-at period");
            validate_non_negative(entry.value, "entry value",
                kMaximumMoneyMillion);
            if (entry.value.status ==
                ClaimLedgerValueStatus::NotApplicable) {
                throw std::invalid_argument(
                    "monetary and conversion ledger entries cannot be not-applicable");
            }
            conversion_units_present = conversion_units_present ||
                entry.kind == ClaimLedgerEntryKind::ConversionUnits;
            if ((is_funding(entry.kind) || is_conversion(entry.kind)) &&
                entry.event_group_id == "none") {
                throw std::invalid_argument(
                    "funding and conversion entries require an event group");
            }
            if (is_guarantee(entry.kind)) {
                if (entry.provider_claim_id == "none" ||
                    !provider_ids.contains(entry.provider_claim_id)) {
                    throw std::invalid_argument(
                        "guarantee cash must reference a declared provider");
                }
            } else if (entry.provider_claim_id != "none") {
                throw std::invalid_argument(
                    "only guarantee cash may reference a provider claim");
            }
            if (!entry_ids.insert(entry.entry_id).second) {
                throw std::invalid_argument("duplicate entry id");
            }
        }
    };
    const auto check_covenant_events =
        [&](const std::vector<ClaimLedgerCovenantEvent>& events) {
            for (const ClaimLedgerCovenantEvent& event : events) {
                require_safe_identifier(event.event_id,
                    "covenant event id");
                require_safe_identifier(event.covenant_id, "covenant id");
                require_safe_identifier(event.source_record_id,
                    "covenant source-record id");
                if (event.period > config.horizon_period) {
                    throw std::invalid_argument(
                        "covenant event period follows the horizon");
                }
                check_known_at(event.known_at_period,
                    "covenant known-at period");
                if (!covenant_event_ids.insert(event.event_id).second) {
                    throw std::invalid_argument(
                        "duplicate covenant event id");
                }
            }
        };
    check_entries(config.common_entries);
    check_covenant_events(config.common_covenant_events);
    std::set<std::pair<std::string, std::size_t>> common_covenant_states;
    for (const ClaimLedgerCovenantEvent& event :
        config.common_covenant_events) {
        if (!common_covenant_states.emplace(
                event.covenant_id, event.period).second) {
            throw std::invalid_argument(
                "one common covenant has conflicting states in one period");
        }
    }

    std::unordered_set<std::string> scenario_ids;
    double minimum_probability = 0.0;
    double maximum_probability = 0.0;
    double exact_probability = 0.0;
    bool all_probability_exact = !config.scenarios.empty();
    bool all_probability_bounded = !config.scenarios.empty();
    for (const ClaimLedgerScenario& scenario : config.scenarios) {
        require_safe_identifier(scenario.scenario_id, "scenario id");
        require_safe_identifier(scenario.probability_source_record_id,
            "scenario probability source-record id");
        require_safe_identifier(scenario.cash_path_status_source_record_id,
            "scenario cash-path-status source-record id");
        if (!scenario_ids.insert(scenario.scenario_id).second) {
            throw std::invalid_argument("duplicate scenario id");
        }
        check_known_at(scenario.probability_known_at_period,
            "probability known-at period");
        check_known_at(scenario.cash_path_status_known_at_period,
            "cash-path status known-at period");
        validate_non_negative(scenario.physical_probability,
            "physical probability", 1.0);
        const Range probability = from_value(scenario.physical_probability);
        if (probability.available) {
            minimum_probability += probability.lower;
            maximum_probability += probability.upper;
        } else {
            all_probability_bounded = false;
        }
        if (scenario.physical_probability.status ==
                ClaimLedgerValueStatus::Known && exact(probability)) {
            exact_probability += exact_value(probability);
        } else {
            all_probability_exact = false;
        }
        check_entries(scenario.entries);
        check_covenant_events(scenario.covenant_events);
        std::set<std::pair<std::string, std::size_t>> scenario_states;
        for (const ClaimLedgerCovenantEvent& event :
            scenario.covenant_events) {
            const auto key = std::make_pair(
                event.covenant_id, event.period);
            if (!scenario_states.insert(key).second ||
                common_covenant_states.contains(key)) {
                throw std::invalid_argument(
                    "one covenant has conflicting states in one scenario period");
            }
        }
        const std::vector<const ClaimLedgerEntry*> entries =
            collect_entries(config, &scenario.entries, false);
        validate_groups(entries);
        static_cast<void>(evaluate_path(config, entries, false));
    }
    const bool conversion_label_not_applicable =
        config.conversion_unit_label == "not-applicable";
    const bool conversion_basis_not_applicable =
        config.conversion_unit_basis == "not-applicable";
    if (conversion_label_not_applicable !=
        conversion_basis_not_applicable) {
        throw std::invalid_argument(
            "conversion unit label and basis must both be applicable or both be not-applicable");
    }
    if (conversion_units_present && conversion_label_not_applicable) {
        throw std::invalid_argument(
            "conversion units require a declared unit label and basis");
    }
    if (!conversion_units_present && !conversion_label_not_applicable) {
        throw std::invalid_argument(
            "conversion unit metadata must be not-applicable when the ledger has no conversion units");
    }
    if (materially_greater(minimum_probability, 1.0)) {
        throw std::invalid_argument(
            "minimum physical probabilities exceed one");
    }
    if (all_probability_bounded &&
        materially_greater(1.0, maximum_probability)) {
        throw std::invalid_argument(
            "maximum physical probabilities cannot reach one");
    }
    if (all_probability_exact && !nearly_equal(exact_probability, 1.0)) {
        throw std::invalid_argument(
            "known mutually exclusive probabilities must sum to one");
    }
    const std::vector<const ClaimLedgerEntry*> common =
        collect_entries(config, nullptr, false);
    validate_groups(common);
    static_cast<void>(evaluate_path(config, common, false));
}

[[nodiscard]] bool has_guarantee_cash(const ClaimLedgerConfig& config,
    bool decision_only) noexcept {
    const auto contains = [&](const std::vector<ClaimLedgerEntry>& entries) {
        return std::any_of(entries.begin(), entries.end(),
            [&](const ClaimLedgerEntry& entry) {
                return is_guarantee(entry.kind) &&
                    (!decision_only || entry.known_at_period <=
                        config.decision_period);
            });
    };
    if (contains(config.common_entries)) {
        return true;
    }
    return std::any_of(config.scenarios.begin(), config.scenarios.end(),
        [&contains](const ClaimLedgerScenario& scenario) {
            return contains(scenario.entries);
        });
}

[[nodiscard]] std::optional<double> cash_npv(
    const std::vector<double>& cashflows, double rate,
    std::size_t periods_per_year) {
    double maximum_log_term =
        -std::numeric_limits<double>::infinity();
    const double log_rate = std::log1p(rate);
    for (std::size_t period = 0U; period < cashflows.size(); ++period) {
        if (cashflows[period] == 0.0) continue;
        maximum_log_term = std::max(maximum_log_term,
            std::log(std::abs(cashflows[period])) -
                static_cast<double>(period) /
                    static_cast<double>(periods_per_year) * log_rate);
    }
    if (!std::isfinite(maximum_log_term)) return 0.0;
    double scaled_sum = 0.0;
    double correction = 0.0;
    for (std::size_t period = 0U; period < cashflows.size(); ++period) {
        if (cashflows[period] == 0.0) continue;
        const double log_term = std::log(std::abs(cashflows[period])) -
            static_cast<double>(period) /
                static_cast<double>(periods_per_year) * log_rate;
        const double term = std::copysign(
            std::exp(log_term - maximum_log_term), cashflows[period]);
        const double adjusted = term - correction;
        const double next = scaled_sum + adjusted;
        correction = (next - scaled_sum) - adjusted;
        scaled_sum = next;
    }
    if (scaled_sum == 0.0) return 0.0;
    const double result_log =
        maximum_log_term + std::log(std::abs(scaled_sum));
    if (result_log > std::log(std::numeric_limits<double>::max())) {
        return std::nullopt;
    }
    const double value = std::copysign(std::exp(result_log), scaled_sum);
    return std::isfinite(value) ? std::optional<double>{value}
                                : std::nullopt;
}

[[nodiscard]] int cash_npv_sign(const std::vector<double>& cashflows,
    double rate, std::size_t periods_per_year) {
    double maximum_log_term =
        -std::numeric_limits<double>::infinity();
    for (std::size_t period = 0U; period < cashflows.size(); ++period) {
        if (cashflows[period] == 0.0) continue;
        const double log_term = std::log(std::abs(cashflows[period])) -
            static_cast<double>(period) /
                static_cast<double>(periods_per_year) *
                std::log1p(rate);
        maximum_log_term = std::max(maximum_log_term, log_term);
    }
    if (!std::isfinite(maximum_log_term)) return 0;

    double scaled_sum = 0.0;
    double correction = 0.0;
    for (std::size_t period = 0U; period < cashflows.size(); ++period) {
        if (cashflows[period] == 0.0) continue;
        const double log_term = std::log(std::abs(cashflows[period])) -
            static_cast<double>(period) /
                static_cast<double>(periods_per_year) *
                std::log1p(rate);
        const double scaled_term = std::copysign(
            std::exp(log_term - maximum_log_term), cashflows[period]);
        const double adjusted = scaled_term - correction;
        const double next = scaled_sum + adjusted;
        correction = (next - scaled_sum) - adjusted;
        scaled_sum = next;
    }
    if (scaled_sum > 0.0) return 1;
    if (scaled_sum < 0.0) return -1;
    return 0;
}

[[nodiscard]] std::optional<double> rate_preimage(
    const std::vector<double>& cashflows, std::size_t periods_per_year,
    std::vector<std::string>& blockers) {
    bool negative = false;
    bool positive = false;
    bool positive_phase = false;
    for (const double cash : cashflows) {
        if (nearly_equal(cash, 0.0)) {
            continue;
        }
        if (cash < 0.0) {
            negative = true;
            if (positive_phase) {
                blockers.emplace_back(
                    "expected cash has more than one sign phase");
                return std::nullopt;
            }
        } else {
            positive = true;
            positive_phase = true;
        }
    }
    if (!negative || !positive) {
        blockers.emplace_back(
            "rate preimage requires negative cash followed by positive cash");
        return std::nullopt;
    }
    constexpr double lower_bound = -0.999999;
    constexpr double upper_bound = kMaximumAnnualEffectiveRate;
    double lower = lower_bound;
    double upper = upper_bound;
    const int lower_sign =
        cash_npv_sign(cashflows, lower, periods_per_year);
    const int upper_sign =
        cash_npv_sign(cashflows, upper, periods_per_year);
    if (lower_sign < 0 || upper_sign > 0) {
        blockers.emplace_back(
            "zero NPV lies outside the supported annual-rate bracket");
        return std::nullopt;
    }
    for (std::size_t iteration = 0U; iteration < 200U; ++iteration) {
        const double middle = 0.5 * (lower + upper);
        if (cash_npv_sign(cashflows, middle, periods_per_year) > 0) {
            lower = middle;
        } else {
            upper = middle;
        }
    }
    return 0.5 * (lower + upper);
}

void set_unknown_expected(
    const ClaimLedgerConfig& config, ClaimLedgerSummary& summary) {
    summary.expected_investor_cashflows_million.assign(
        config.horizon_period + 1U, claim_ledger_unknown());
    summary.expected_ead_million.assign(
        config.horizon_period + 1U, claim_ledger_unknown());
    summary.expected_npv_million = claim_ledger_unknown();
    summary.expected_principal_loss_million = claim_ledger_unknown();
    summary.expected_accrued_interest_loss_million =
        claim_ledger_unknown();
    summary.expected_total_loss_million = claim_ledger_unknown();
    summary.expected_principal_cash_wal_months = claim_ledger_unknown();
    summary.annual_effective_rate_preimage = claim_ledger_unknown();
}

void calculate_expected(const ClaimLedgerConfig& config,
    const std::vector<double>& probabilities, ClaimLedgerSummary& summary) {
    std::vector<double> cash(config.horizon_period + 1U, 0.0);
    std::vector<double> ead(config.horizon_period + 1U, 0.0);
    double principal_loss = 0.0;
    double interest_loss = 0.0;
    double wal_numerator = 0.0;
    double wal_denominator = 0.0;
    for (std::size_t scenario_index = 0U;
         scenario_index < summary.scenarios.size(); ++scenario_index) {
        const double probability = probabilities[scenario_index];
        const ClaimLedgerPathResult& path =
            summary.scenarios[scenario_index].decision_path;
        double path_principal_loss = 0.0;
        double path_interest_loss = 0.0;
        if (!known_value(path.principal_loss_million, path_principal_loss) ||
            !known_value(
                path.accrued_interest_loss_million, path_interest_loss)) {
            throw std::logic_error("ready path lost exact loss metrics");
        }
        principal_loss += probability * path_principal_loss;
        interest_loss += probability * path_interest_loss;
        for (std::size_t period = 0U;
             period <= config.horizon_period; ++period) {
            const ClaimLedgerPeriodResult& row = path.periods[period];
            double path_cash = 0.0;
            double path_ead = 0.0;
            double borrower_principal = 0.0;
            double recovery_principal = 0.0;
            double guarantee_principal = 0.0;
            if (!known_value(row.investor_cashflow_million, path_cash) ||
                !known_value(row.ead_before_resolution_million, path_ead) ||
                !known_value(
                    row.principal_cash_million, borrower_principal) ||
                !known_value(row.recovery_principal_cash_million,
                    recovery_principal) ||
                !known_value(row.guarantee_principal_cash_million,
                    guarantee_principal)) {
                throw std::logic_error("ready path lost exact period metrics");
            }
            cash[period] += probability * path_cash;
            ead[period] += probability * path_ead;
            const double principal_cash = borrower_principal +
                recovery_principal + guarantee_principal;
            wal_numerator += probability *
                static_cast<double>(period) * principal_cash;
            wal_denominator += probability * principal_cash;
        }
    }
    for (const double value : cash) {
        summary.expected_investor_cashflows_million.push_back(
            claim_ledger_known(value));
    }
    for (const double value : ead) {
        summary.expected_ead_million.push_back(claim_ledger_known(value));
    }
    summary.expected_principal_loss_million =
        claim_ledger_known(principal_loss);
    summary.expected_accrued_interest_loss_million =
        claim_ledger_known(interest_loss);
    summary.expected_total_loss_million =
        claim_ledger_known(principal_loss + interest_loss);
    summary.expected_principal_cash_wal_months = wal_denominator > 0.0
        ? claim_ledger_known(wal_numerator / wal_denominator)
        : claim_ledger_not_applicable();
    double discount_rate = 0.0;
    if (config.discount_rate_known_at_period.has_value() &&
        *config.discount_rate_known_at_period <= config.decision_period &&
        known_value(config.annual_effective_discount_rate, discount_rate)) {
        const std::optional<double> npv = cash_npv(
            cash, discount_rate, config.periods_per_year);
        if (npv.has_value()) {
            summary.expected_npv_million = claim_ledger_known(*npv);
            summary.readiness.npv_ready = true;
        } else {
            summary.expected_npv_million = claim_ledger_unknown();
            add_unique(summary.readiness.npv_blockers,
                "discounted cash magnitude exceeds the finite numeric guardrail");
        }
    } else {
        summary.expected_npv_million = claim_ledger_unknown();
        add_unique(summary.readiness.npv_blockers,
            "annual effective discount rate is not exact and ex-ante");
    }
    const std::optional<double> preimage =
        rate_preimage(cash, config.periods_per_year,
            summary.readiness.rate_preimage_blockers);
    if (preimage.has_value()) {
        summary.annual_effective_rate_preimage =
            claim_ledger_known(*preimage);
        summary.readiness.rate_preimage_ready = true;
    } else {
        summary.annual_effective_rate_preimage = claim_ledger_unknown();
    }
}

} // namespace

ClaimLedgerValue claim_ledger_known(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("known claim-ledger value must be finite");
    }
    return ClaimLedgerValue{ClaimLedgerValueStatus::Known, value, value};
}

ClaimLedgerValue claim_ledger_bounded(double lower, double upper) {
    if (!std::isfinite(lower) || !std::isfinite(upper) || lower >= upper) {
        throw std::invalid_argument(
            "bounded claim-ledger value requires increasing finite endpoints");
    }
    return ClaimLedgerValue{
        ClaimLedgerValueStatus::Bounded, lower, upper};
}

ClaimLedgerValue claim_ledger_unknown() noexcept {
    return ClaimLedgerValue{ClaimLedgerValueStatus::Unknown, {}, {}};
}

ClaimLedgerValue claim_ledger_not_applicable() noexcept {
    return ClaimLedgerValue{
        ClaimLedgerValueStatus::NotApplicable, {}, {}};
}

void validate_claim_ledger_config(const ClaimLedgerConfig& config) {
    validate_config_impl(config);
}

ClaimLedgerSummary evaluate_claim_ledger(const ClaimLedgerConfig& config) {
    validate_config_impl(config);
    ClaimLedgerSummary summary;
    summary.common_decision_path = evaluate_path(config,
        collect_entries(config, nullptr, true), true);
    summary.common_full_path = evaluate_path(config,
        collect_entries(config, nullptr, false), false);
    for (const ClaimLedgerCovenantEvent& event :
        config.common_covenant_events) {
        (event.known_at_period <= config.decision_period
                ? summary.common_decision_covenant_events
                : summary.common_backtest_covenant_events)
            .push_back(event);
    }

    bool expected_ready = !config.scenarios.empty();
    if (config.scenarios.empty()) {
        summary.readiness.expected_cash_blockers.emplace_back(
            "no mutually exclusive physical scenario set is supplied");
    }
    std::vector<double> probabilities;
    probabilities.reserve(config.scenarios.size());
    double probability_sum = 0.0;
    for (const ClaimLedgerScenario& scenario : config.scenarios) {
        ClaimLedgerScenarioResult report;
        report.scenario_id = scenario.scenario_id;
        report.physical_probability = scenario.physical_probability;
        double probability = 0.0;
        report.probability_available_at_decision =
            scenario.probability_known_at_period <= config.decision_period &&
            known_value(scenario.physical_probability, probability);
        if (!report.probability_available_at_decision) {
            expected_ready = false;
            add_unique(summary.readiness.expected_cash_blockers,
                scenario.scenario_id +
                    ": physical probability is not exact and ex-ante");
            probabilities.push_back(0.0);
        } else {
            probabilities.push_back(probability);
            probability_sum += probability;
        }
        const std::vector<const ClaimLedgerEntry*> decision_entries =
            collect_entries(config, &scenario.entries, true);
        for (const ClaimLedgerEntry* entry : decision_entries) {
            report.decision_entry_ids.push_back(entry->entry_id);
        }
        const auto classify_later_entry =
            [&](const ClaimLedgerEntry& entry) {
                if (entry.known_at_period > config.decision_period) {
                    report.backtest_entry_ids.push_back(entry.entry_id);
                }
            };
        for (const ClaimLedgerEntry& entry : config.common_entries) {
            classify_later_entry(entry);
        }
        for (const ClaimLedgerEntry& entry : scenario.entries) {
            classify_later_entry(entry);
        }
        const auto classify_covenant =
            [&](const ClaimLedgerCovenantEvent& event) {
                (event.known_at_period <= config.decision_period
                        ? report.decision_covenant_events
                        : report.backtest_covenant_events)
                    .push_back(event);
            };
        for (const ClaimLedgerCovenantEvent& event :
            scenario.covenant_events) {
            classify_covenant(event);
        }
        report.decision_path =
            evaluate_path(config, decision_entries, true);
        report.full_path = evaluate_path(config,
            collect_entries(config, &scenario.entries, false), false);
        report.complete_resolved_cash_path_at_decision =
            scenario.cash_path_status ==
                ClaimLedgerCashPathStatus::CompleteResolved &&
            scenario.cash_path_status_known_at_period <=
                config.decision_period;
        if (!report.complete_resolved_cash_path_at_decision) {
            expected_ready = false;
            add_unique(summary.readiness.expected_cash_blockers,
                scenario.scenario_id +
                    ": cash path is not explicitly complete, resolved, and ex-ante");
        }
        if (!report.decision_path.exact) {
            expected_ready = false;
            for (const std::string& blocker :
                 report.decision_path.blockers) {
                add_unique(summary.readiness.expected_cash_blockers,
                    scenario.scenario_id + ": " + blocker);
            }
        }
        double terminal_exposure = 0.0;
        if (!known_value(
                report.decision_path.terminal_total_exposure_million,
                terminal_exposure) ||
            !nearly_equal(terminal_exposure, 0.0)) {
            expected_ready = false;
            add_unique(summary.readiness.expected_cash_blockers,
                scenario.scenario_id +
                    ": terminal claim exposure is not exactly zero");
        }
        double conversion_units = 0.0;
        if (!known_value(report.decision_path.total_conversion_units,
                conversion_units) ||
            !nearly_equal(conversion_units, 0.0)) {
            expected_ready = false;
            add_unique(summary.readiness.expected_cash_blockers,
                scenario.scenario_id +
                    ": non-cash conversion units require a downstream realization path");
        }
        if (!report.decision_path.periods.empty()) {
            const ClaimLedgerPeriodResult& terminal =
                report.decision_path.periods.back();
            double principal_due = 0.0;
            double interest_due = 0.0;
            if (!known_value(terminal.outstanding_principal_due_million,
                    principal_due) ||
                !known_value(terminal.outstanding_interest_due_million,
                    interest_due) ||
                !nearly_equal(principal_due, 0.0) ||
                !nearly_equal(interest_due, 0.0)) {
                expected_ready = false;
                add_unique(summary.readiness.expected_cash_blockers,
                    scenario.scenario_id +
                        ": terminal due amounts are not exactly resolved");
            }
        }
        for (const ClaimLedgerProviderPathResult& provider :
             report.decision_path.provider_claims) {
            double unpaid = 0.0;
            double after_horizon = 0.0;
            if (!known_value(
                    provider.terminal_unpaid_payable_claim_million, unpaid) ||
                !known_value(provider.claim_payable_after_horizon_million,
                    after_horizon) ||
                !nearly_equal(unpaid, 0.0) ||
                !nearly_equal(after_horizon, 0.0)) {
                expected_ready = false;
                add_unique(summary.readiness.expected_cash_blockers,
                    scenario.scenario_id + "/" +
                        provider.provider_claim_id +
                        ": provider claim remains unpaid or payable after the horizon");
            }
        }
        summary.scenarios.push_back(std::move(report));
    }
    if (!config.scenarios.empty() &&
        !nearly_equal(probability_sum, 1.0)) {
        expected_ready = false;
        add_unique(summary.readiness.expected_cash_blockers,
            "exact ex-ante physical probabilities do not sum to one");
    }
    if (config.face_amount_known_at_period > config.decision_period ||
        config.contractual_face_amount_million.status !=
            ClaimLedgerValueStatus::Known) {
        expected_ready = false;
        add_unique(summary.readiness.expected_cash_blockers,
            "contractual face amount is not exact and ex-ante");
    }
    if (config.opening_principal_known_at_period > config.decision_period ||
        config.opening_principal_million.status !=
            ClaimLedgerValueStatus::Known ||
        config.opening_accrued_interest_known_at_period >
            config.decision_period ||
        config.opening_accrued_interest_million.status !=
            ClaimLedgerValueStatus::Known) {
        expected_ready = false;
        add_unique(summary.readiness.expected_cash_blockers,
            "opening balances are not exact and ex-ante");
    }

    const bool guarantee_cash_present = has_guarantee_cash(config, true);
    summary.readiness.provider_claim_applicable =
        std::any_of(config.provider_claims.begin(),
            config.provider_claims.end(), [&](const auto& provider) {
                return provider.known_at_period <= config.decision_period;
            }) ||
        guarantee_cash_present;
    bool provider_ready = summary.readiness.provider_claim_applicable;
    double allocated = 0.0;
    for (const ClaimLedgerProviderClaim& provider :
        config.provider_claims) {
        if (provider.known_at_period > config.decision_period) continue;
        const ProviderTerms terms = read_provider_terms(
            provider, true, config.decision_period);
        provider_ready = provider_ready && terms.computable;
        if (terms.computable) {
            allocated += terms.allocation;
        }
        for (const std::string& blocker : terms.blockers) {
            add_unique(summary.readiness.provider_claim_blockers,
                provider.provider_claim_id + ": " + blocker);
        }
    }
    if (guarantee_cash_present && allocated <= 0.0) {
        provider_ready = false;
        add_unique(summary.readiness.provider_claim_blockers,
            "guarantee cash has no positive allocated provider coverage");
    }
    const auto inspect_provider_paths =
        [&](std::string_view path_id, const ClaimLedgerPathResult& path) {
            for (const ClaimLedgerProviderPathResult& provider :
                path.provider_claims) {
                if (!provider.computable) {
                    provider_ready = false;
                    for (const std::string& blocker : provider.blockers) {
                        add_unique(summary.readiness.provider_claim_blockers,
                            std::string(path_id) + "/" +
                                provider.provider_claim_id + ": " + blocker);
                    }
                }
            }
        };
    if (summary.scenarios.empty()) {
        inspect_provider_paths("common", summary.common_decision_path);
    } else {
        for (const ClaimLedgerScenarioResult& scenario : summary.scenarios) {
            inspect_provider_paths(
                scenario.scenario_id, scenario.decision_path);
        }
    }
    summary.readiness.provider_claim_ready = provider_ready;
    if (guarantee_cash_present && !provider_ready) {
        expected_ready = false;
        add_unique(summary.readiness.expected_cash_blockers,
            "guarantee cash depends on an uncomputable provider claim");
    }
    summary.readiness.expected_cash_ready = expected_ready;
    if (expected_ready) {
        calculate_expected(config, probabilities, summary);
    } else {
        set_unknown_expected(config, summary);
        summary.readiness.rate_preimage_blockers.emplace_back(
            "expected-cash ledger is not ready");
        summary.readiness.npv_blockers.emplace_back(
            "expected-cash ledger is not ready");
    }
    return summary;
}

std::string_view to_string(ClaimLedgerValueStatus status) noexcept {
    switch (status) {
    case ClaimLedgerValueStatus::Known:
        return "known";
    case ClaimLedgerValueStatus::Bounded:
        return "bounded";
    case ClaimLedgerValueStatus::Unknown:
        return "unknown";
    case ClaimLedgerValueStatus::NotApplicable:
        return "not-applicable";
    }
    return "unknown";
}

std::string_view to_string(ClaimLedgerEntryKind kind) noexcept {
    switch (kind) {
    case ClaimLedgerEntryKind::BuyerPrice:
        return "buyer-price";
    case ClaimLedgerEntryKind::BuyerDirectCost:
        return "buyer-direct-cost";
    case ClaimLedgerEntryKind::BorrowerGrossProceeds:
        return "borrower-gross-proceeds";
    case ClaimLedgerEntryKind::BorrowerNetProceeds:
        return "borrower-net-proceeds";
    case ClaimLedgerEntryKind::CashFee:
        return "cash-fee";
    case ClaimLedgerEntryKind::BorrowerThirdPartyCost:
        return "borrower-third-party-cost";
    case ClaimLedgerEntryKind::FundedPrincipal:
        return "funded-principal";
    case ClaimLedgerEntryKind::OriginalIssueDiscount:
        return "original-issue-discount";
    case ClaimLedgerEntryKind::OriginalIssuePremium:
        return "original-issue-premium";
    case ClaimLedgerEntryKind::CapitalizedFee:
        return "capitalized-fee";
    case ClaimLedgerEntryKind::InterestAccrual:
        return "interest-accrual";
    case ClaimLedgerEntryKind::CapitalizedInterest:
        return "capitalized-interest";
    case ClaimLedgerEntryKind::PrincipalDue:
        return "principal-due";
    case ClaimLedgerEntryKind::InterestDue:
        return "interest-due";
    case ClaimLedgerEntryKind::PrincipalCash:
        return "principal-cash";
    case ClaimLedgerEntryKind::InterestCash:
        return "interest-cash";
    case ClaimLedgerEntryKind::RecoveryPrincipalCash:
        return "recovery-principal-cash";
    case ClaimLedgerEntryKind::RecoveryInterestCash:
        return "recovery-interest-cash";
    case ClaimLedgerEntryKind::ConversionPrincipalExtinguishment:
        return "conversion-principal-extinguishment";
    case ClaimLedgerEntryKind::ConversionInterestExtinguishment:
        return "conversion-interest-extinguishment";
    case ClaimLedgerEntryKind::ConversionUnits:
        return "conversion-units";
    case ClaimLedgerEntryKind::PrincipalWriteoff:
        return "principal-writeoff";
    case ClaimLedgerEntryKind::AccruedInterestWriteoff:
        return "accrued-interest-writeoff";
    case ClaimLedgerEntryKind::GuaranteePrincipalCash:
        return "guarantee-principal-cash";
    case ClaimLedgerEntryKind::GuaranteeInterestCash:
        return "guarantee-interest-cash";
    }
    return "unknown";
}

std::string_view to_string(ClaimLedgerCovenantState state) noexcept {
    switch (state) {
    case ClaimLedgerCovenantState::Pass:
        return "pass";
    case ClaimLedgerCovenantState::Breach:
        return "breach";
    case ClaimLedgerCovenantState::BreachWithCure:
        return "breach-with-cure";
    case ClaimLedgerCovenantState::BreachWithWaiver:
        return "breach-with-waiver";
    case ClaimLedgerCovenantState::BreachWithNonExerciseConsent:
        return "breach-with-non-exercise-consent";
    case ClaimLedgerCovenantState::Default:
        return "default";
    case ClaimLedgerCovenantState::Acceleration:
        return "acceleration";
    }
    return "unknown";
}

std::string_view to_string(
    ClaimLedgerProviderAllocationPriority priority) noexcept {
    switch (priority) {
    case ClaimLedgerProviderAllocationPriority::PrincipalFirst:
        return "principal-first";
    case ClaimLedgerProviderAllocationPriority::InterestFirst:
        return "interest-first";
    case ClaimLedgerProviderAllocationPriority::ProRata:
        return "pro-rata";
    }
    return "unknown";
}

std::string_view to_string(ClaimLedgerCashPathStatus status) noexcept {
    switch (status) {
    case ClaimLedgerCashPathStatus::Incomplete:
        return "incomplete";
    case ClaimLedgerCashPathStatus::CompleteResolved:
        return "complete-resolved";
    }
    return "unknown";
}

} // namespace naturalehia::cellular_finance
