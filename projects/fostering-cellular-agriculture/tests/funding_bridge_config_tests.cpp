// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/funding_bridge_config.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

class NonCanonicalPunctuation final : public std::numpunct<char> {
protected:
    [[nodiscard]] char do_decimal_point() const override { return ','; }
    [[nodiscard]] std::string do_truename() const override { return "yes"; }
    [[nodiscard]] std::string do_falsename() const override { return "no"; }
};

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expect_invalid_argument(
    const std::function<void()>& operation, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

void expect_runtime_error(
    const std::function<void()>& operation, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::runtime_error&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

[[nodiscard]] cf::FundingBridgeConfig full_config() {
    cf::FundingBridgeConfig config;
    config.scenario_label =
        "strict provider-facility funding parser fixture";
    config.source_note = "synthetic parser roundtrip values only";
    config.synthetic_inputs = true;
    config.funded_at_close_cash_million = 4.25;
    config.funded_at_close_provider_id = "sponsor-a";
    config.funded_at_close_source_record_id = "close-cash-record";
    config.providers = {
        {"sponsor-a", "group-a", 20.0, true, true, true,
            "provider-sponsor-record"},
        {"bank-b", "group-b", 25.0, true, true, false,
            "provider-bank-record"},
        {"takeout-c", "group-c", 30.0, false, false, false,
            "provider-takeout-record"},
    };
    config.callable_facility = {"callable-main", "sponsor-a",
        "callable-terms-record", 6.0, 0U, 18U, 2U,
        "Acquisition and primary funding support", 0.0125, 0.25, 0.08,
        0.02};
    config.warehouse_facility = {"warehouse-main", "bank-b",
        "warehouse-terms-record", 5.0, 0U, 18U, 24U, 1U,
        "Temporary eligible asset funding", 0.75, 0.1, 0.0125, 0.0025,
        0.005};
    config.uncalled_commitment_is_not_cash_or_loss_absorption = true;
    config.acquisition_and_primary_funding_uses_precede_same_month_receipts =
        true;
    config.warehouse_is_external_temporary_debt = true;
    config.warehouse_proceeds_cannot_fund_interest_fees_or_costs = true;
    config.project_receipts_sweep_warehouse_principal_before_investor_cash =
        true;
    config.policy_uses_observed_history_only = true;
    config.no_dynamic_tranche_allocation_or_pricing_is_claimed = true;

    cf::FundingBridgeScenarioPerformance alpha;
    alpha.scenario_id = "path-alpha";
    alpha.capital_call_requests = {
        {"call-early", "callable-main", 0U, 2.0},
        {"call-late", "callable-main", 3U, 3.0},
    };
    alpha.capital_call_outcomes = {
        {"call-early", 2U, cf::FundingSettlementStatus::SettledInFull, 2.0,
            "call-early-settlement"},
        {"call-late", 5U,
            cf::FundingSettlementStatus::FinalPartialSettlement, 2.5,
            "call-late-settlement"},
    };
    alpha.warehouse_draw_requests = {
        {"draw-early", "warehouse-main", 1U, 0.5},
        {"draw-late", "warehouse-main", 4U, 1.25},
    };
    alpha.warehouse_draw_outcomes = {
        {"draw-early", 2U, cf::FundingSettlementStatus::SettledInFull, 0.5,
            "draw-early-settlement"},
        {"draw-late", 5U,
            cf::FundingSettlementStatus::FinalPartialSettlement, 1.0,
            "draw-late-settlement"},
    };
    alpha.supplemental_receipts = {
        {"cost-support-1", "sponsor-a", 0U,
            cf::SupplementalFundingPurpose::CostSupport, 0.25,
            "cost-support-record"},
        {"replenishment-1", "sponsor-a", 3U,
            cf::SupplementalFundingPurpose::ProtectionReplenishment, 0.5,
            "replenishment-record"},
        {"takeout-1", "takeout-c", 8U,
            cf::SupplementalFundingPurpose::SettledTakeout, 1.5,
            "takeout-record"},
    };
    alpha.eligible_basis_movements = {
        {"basis-add-1", 1U, cf::EligibleBasisMovementKind::EligibleAddition,
            2.0, "asset-use-1", "basis-add-record"},
        {"basis-return-1", 2U,
            cf::EligibleBasisMovementKind::PrincipalBasisReturn, 0.25,
            "principal-receipt-1", "basis-return-record"},
        {"basis-disposition-1", 3U,
            cf::EligibleBasisMovementKind::Disposition, 0.1,
            "asset-disposition-1", "basis-disposition-record"},
        {"basis-writeoff-1", 4U, cf::EligibleBasisMovementKind::Writeoff,
            0.2, "asset-writeoff-1", "basis-writeoff-record"},
        {"basis-removal-1", 5U,
            cf::EligibleBasisMovementKind::EligibilityRemoval, 0.15,
            "eligibility-test-1", "basis-removal-record"},
    };
    alpha.protection_absorptions = {
        {"absorption-1", 4U, 0.2, "asset-writeoff-1",
            "absorption-record"},
    };
    alpha.protection_releases = {
        {"release-1", 7U, 0.3, "release-record"},
    };

    cf::FundingBridgeScenarioPerformance beta;
    beta.scenario_id = "path-beta";
    beta.capital_call_requests = {
        {"call-beta", "callable-main", 1U, 1.0},
    };
    beta.capital_call_outcomes = {
        {"call-beta", 3U, cf::FundingSettlementStatus::Failed, 0.0,
            "call-beta-failure"},
    };
    beta.warehouse_draw_requests = {
        {"draw-beta", "warehouse-main", 2U, 0.75},
    };
    beta.warehouse_draw_outcomes = {
        {"draw-beta", 3U, cf::FundingSettlementStatus::SettledInFull, 0.75,
            "draw-beta-settlement"},
    };
    beta.supplemental_receipts = {
        {"cost-support-beta", "sponsor-a", 0U,
            cf::SupplementalFundingPurpose::CostSupport, 0.1,
            "cost-support-beta-record"},
    };
    beta.eligible_basis_movements = {
        {"basis-add-beta", 2U,
            cf::EligibleBasisMovementKind::EligibleAddition, 1.0,
            "asset-use-beta", "basis-add-beta-record"},
    };
    config.scenario_performance = {std::move(alpha), std::move(beta)};
    return config;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error(
            "could not create funding-bridge parser fixture");
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        throw std::runtime_error(
            "could not write funding-bridge parser fixture");
    }
}

[[nodiscard]] std::string normalized_config(
    const cf::FundingBridgeConfig& config) {
    std::ostringstream output;
    cf::print_normalized_funding_bridge_config(output, config);
    return output.str();
}

[[nodiscard]] cf::FundingBridgeConfig parse_text(const std::string& text) {
    std::istringstream input{text};
    return cf::parse_funding_bridge_config(input);
}

void set_value(
    std::string& text, const std::string& key, const std::string& value) {
    const std::string prefix = key + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos) {
        throw std::runtime_error("test fixture key not found: " + key);
    }
    const std::size_t value_begin = position + prefix.size();
    const std::size_t line_end = text.find('\n', value_begin);
    text.replace(value_begin,
        line_end == std::string::npos ? std::string::npos
                                     : line_end - value_begin,
        value);
}

void remove_key(std::string& text, const std::string& key) {
    const std::string prefix = key + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos) {
        throw std::runtime_error("test fixture key not found: " + key);
    }
    const std::size_t line_end = text.find('\n', position);
    text.erase(position, line_end == std::string::npos
            ? std::string::npos
            : line_end - position + 1U);
}

[[nodiscard]] std::string reversed_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t begin = 0U;
    while (begin < text.size()) {
        const std::size_t end = text.find('\n', begin);
        lines.push_back(text.substr(begin,
            end == std::string::npos ? std::string::npos : end - begin));
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1U;
    }
    std::reverse(lines.begin(), lines.end());
    std::string result;
    for (const std::string& line : lines) {
        result += line;
        result += '\n';
    }
    return result;
}

void test_full_roundtrip_and_output_state(
    const std::filesystem::path& path) {
    const cf::FundingBridgeConfig original = full_config();
    std::ostringstream output;
    const std::locale caller_locale(
        std::locale::classic(), new NonCanonicalPunctuation);
    output.imbue(caller_locale);
    output << std::fixed << std::hex << std::showbase << std::showpoint
           << std::showpos << std::uppercase << std::setprecision(6);
    output.fill('#');
    output.width(37);
    const std::ios_base::fmtflags caller_flags = output.flags();
    cf::print_normalized_funding_bridge_config(output, original);
    const std::string normalized = output.str();

    check(output.precision() == 6 && output.flags() == caller_flags &&
            output.width() == 37 && output.fill() == '#' &&
            output.getloc() == caller_locale,
        "normalized printer restores caller flags, precision, width, fill, and locale");
    check(normalized.starts_with(
              "funding_bridge.model_version=0.1.0\n"
              "funding_bridge.label=strict provider-facility funding parser fixture\n") &&
            normalized.find(
                "funding_bridge.funded_at_close_source_record_id=close-cash-record\n") !=
                std::string::npos &&
            normalized.find("provider.count=3\n") != std::string::npos &&
            normalized.find("provider.3.capacity_evidenced=false\n") !=
                std::string::npos &&
            normalized.find(
                "callable_facility.source_record_id=callable-terms-record\n") !=
                std::string::npos &&
            normalized.find(
                "warehouse_facility.source_record_id=warehouse-terms-record\n") !=
                std::string::npos &&
            normalized.find("warehouse_facility.annual_interest_rate="
                            "0.10000000000000001\n") !=
                std::string::npos &&
            normalized.find(
                "scenario.1.capital_call_outcome.2.status=final-partial-settlement\n") !=
                std::string::npos &&
            normalized.find(
                "scenario.1.supplemental_receipt.3.purpose=settled-takeout\n") !=
                std::string::npos &&
            normalized.find(
                "scenario.1.eligible_basis_movement.5.kind=eligibility-removal\n") !=
                std::string::npos &&
            normalized.find(
                "scenario.1.protection_absorption.1.reference_id=asset-writeoff-1\n") !=
                std::string::npos &&
            normalized.find("scenario.2.protection_release.count=0\n") !=
                std::string::npos,
        "printer emits every provider, facility, scenario collection, enum, and lineage field canonically");

    write_text(path, normalized);
    const cf::FundingBridgeConfig loaded =
        cf::load_funding_bridge_config(path);
    check(loaded.model_version == original.model_version &&
            loaded.funded_at_close_provider_id == "sponsor-a" &&
            loaded.funded_at_close_source_record_id == "close-cash-record" &&
            loaded.providers.size() == 3U &&
            loaded.providers[1].id == "bank-b" &&
            !loaded.providers[1].capacity_evidenced &&
            loaded.callable_facility.source_record_id ==
                "callable-terms-record" &&
            loaded.callable_facility.contractual_expiry_month == 18U &&
            loaded.warehouse_facility.source_record_id ==
                "warehouse-terms-record" &&
            loaded.warehouse_facility.legal_maturity_month == 24U &&
            loaded.scenario_performance.size() == 2U &&
            loaded.scenario_performance[0].capital_call_requests.size() == 2U &&
            loaded.scenario_performance[0].capital_call_outcomes[1].status ==
                cf::FundingSettlementStatus::FinalPartialSettlement &&
            loaded.scenario_performance[0].supplemental_receipts[1].purpose ==
                cf::SupplementalFundingPurpose::ProtectionReplenishment &&
            loaded.scenario_performance[0].eligible_basis_movements[3].kind ==
                cf::EligibleBasisMovementKind::Writeoff &&
            loaded.scenario_performance[0].protection_absorptions.size() == 1U &&
            loaded.scenario_performance[0].protection_releases.size() == 1U,
        "the complete replacement bridge input API round-trips through a file");
    check(normalized_config(loaded) == normalized,
        "load-print normalization is idempotent");

    const cf::FundingBridgeConfig parsed = parse_text(normalized);
    check(normalized_config(parsed) == normalized,
        "stream parsing and file loading implement the same closed schema");
}

void test_key_order_comments_whitespace_bom_and_locale(
    const std::string& normalized) {
    const std::string bom{"\xEF\xBB\xBF", 3U};
    const std::string permuted = reversed_lines(normalized);
    check(normalized_config(parse_text(permuted)) == normalized,
        "key-line permutation does not change the parsed indexed object");

    std::string crlf;
    for (const char character : normalized) {
        if (character == '\n') {
            crlf += "\r\n";
        } else {
            crlf += character;
        }
    }
    const cf::FundingBridgeConfig parsed = parse_text(
        bom + "# full-line comment\r\n\r\n" + crlf);
    check(parsed.scenario_label == full_config().scenario_label,
        "comments, blank lines, CRLF, and a byte-zero UTF-8 BOM are accepted");

    std::string spaced = normalized;
    set_value(spaced, "warehouse_facility.annual_interest_rate", "  0.1  ");
    set_value(spaced, "funding_bridge.synthetic_inputs", "  true  ");
    std::istringstream localized_input{spaced};
    localized_input.imbue(std::locale(
        std::locale::classic(), new NonCanonicalPunctuation));
    const cf::FundingBridgeConfig locale_parsed =
        cf::parse_funding_bridge_config(localized_input);
    check(locale_parsed.warehouse_facility.annual_interest_rate == 0.1 &&
            locale_parsed.synthetic_inputs,
        "numeric and boolean parsing is independent of the stream locale");

    std::string controlled = normalized;
    set_value(controlled, "funding_bridge.synthetic_inputs", "false");
    check(!parse_text(controlled).synthetic_inputs,
        "the schema records controlled-candidate as well as synthetic provenance");

    std::string misplaced = normalized;
    const std::size_t second_line = misplaced.find('\n');
    misplaced.insert(second_line + 1U, bom);
    expect_invalid_argument([&misplaced] { (void)parse_text(misplaced); },
        "UTF-8 BOM is rejected anywhere except byte zero");
}

void test_strict_key_set(const std::string& normalized) {
    expect_invalid_argument(
        [&normalized] { (void)parse_text(normalized + "unknown.field=1\n"); },
        "unknown keys are rejected");
    expect_invalid_argument(
        [&normalized] {
            (void)parse_text(normalized + "provider.count=3\n");
        },
        "duplicate keys are rejected at read time");

    std::string invalid = normalized;
    remove_key(invalid,
        "scenario.1.protection_absorption.1.source_record_id");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing nested keys are rejected");

    invalid = normalized;
    remove_key(invalid, "callable_facility.permitted_purpose");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing facility keys are rejected");

    invalid = normalized;
    remove_key(invalid, "callable_facility.source_record_id");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing callable-facility source lineage is rejected");

    invalid = normalized;
    remove_key(invalid, "warehouse_facility.source_record_id");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing warehouse-facility source lineage is rejected");

    invalid = normalized;
    set_value(invalid, "scenario.1.eligible_basis_movement.count", "4");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "declared counts that leave extra indexed keys are rejected");

    expect_invalid_argument(
        [&normalized] {
            (void)parse_text(normalized + "provider.0.id=zero-index\n");
        },
        "zero-based indexed keys are outside the one-based grammar");
    expect_invalid_argument(
        [] { (void)parse_text("funding_bridge.model_version 0.1.0\n"); },
        "non-key-value lines are rejected");
    expect_invalid_argument([] { (void)parse_text(""); },
        "an empty configuration is rejected");
}

void test_malformed_values_and_enum_tokens(const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid, "warehouse_facility.committed_limit_million", "0x");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "malformed decimal amounts are rejected");

    for (const std::string_view nonfinite : {"nan", "inf", "-inf"}) {
        invalid = normalized;
        set_value(invalid, "warehouse_facility.annual_interest_rate",
            std::string(nonfinite));
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "non-finite decimal values are rejected");
    }

    invalid = normalized;
    set_value(invalid,
        "scenario.1.capital_call_request.1.notice_month", "-1");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "negative months are rejected");

    invalid = normalized;
    set_value(invalid, "warehouse_facility.legal_maturity_month",
        "18446744073709551616");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "integer conversion overflow is rejected");

    invalid = normalized;
    set_value(invalid, "provider.1.identity_evidenced", "yes");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "non-canonical booleans are rejected");

    invalid = normalized;
    set_value(invalid,
        "scenario.1.capital_call_outcome.1.status", "settled");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "settlement status is a closed kebab-case enum");

    invalid = normalized;
    set_value(invalid,
        "scenario.1.supplemental_receipt.1.purpose", "support");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "supplemental purpose is a closed kebab-case enum");

    invalid = normalized;
    set_value(invalid,
        "scenario.1.eligible_basis_movement.1.kind", "addition");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "eligible-basis movement kind is a closed kebab-case enum");
}

void test_text_identifiers_and_all_assertions(
    const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid, "funding_bridge.model_version", "9.9.9");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "unsupported model versions are rejected");

    const std::array<std::string_view, 7U> assertion_keys{
        "funding_bridge.uncalled_commitment_is_not_cash_or_loss_absorption",
        "funding_bridge.acquisition_and_primary_funding_uses_precede_same_month_receipts",
        "funding_bridge.warehouse_is_external_temporary_debt",
        "funding_bridge.warehouse_proceeds_cannot_fund_interest_fees_or_costs",
        "funding_bridge.project_receipts_sweep_warehouse_principal_before_investor_cash",
        "funding_bridge.policy_uses_observed_history_only",
        "funding_bridge.no_dynamic_tranche_allocation_or_pricing_is_claimed",
    };
    for (const std::string_view key : assertion_keys) {
        invalid = normalized;
        set_value(invalid, std::string(key), "false");
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "each of the seven accounting and policy assertions must be explicitly true");
    }

    invalid = normalized;
    set_value(invalid, "funding_bridge.label", " padded");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "free-form text with surrounding whitespace is rejected rather than normalized");

    invalid = normalized;
    set_value(invalid, "warehouse_facility.permitted_purpose",
        std::string(1'025U, 'x'));
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "free-form text length is bounded");

    invalid = normalized;
    set_value(invalid, "funding_bridge.source_note", "safe\x01unsafe");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "control characters in free-form text are rejected");

    invalid = normalized;
    set_value(invalid, "funding_bridge.funded_at_close_source_record_id",
        "unsafe/id");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "funded-at-close lineage is a safe identifier");

    invalid = normalized;
    set_value(invalid, "callable_facility.source_record_id", "unsafe/id");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "callable-facility lineage is a safe identifier");

    invalid = normalized;
    set_value(invalid, "warehouse_facility.source_record_id", "unsafe/id");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "warehouse-facility lineage is a safe identifier");

    invalid = normalized;
    set_value(invalid, "provider.1.id", "-unsafe");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "provider ids begin with an ASCII alphanumeric character");

    invalid = normalized;
    set_value(invalid, "scenario.1.id", "unsafe/path");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "scenario ids reject unsafe punctuation");

    invalid = normalized;
    set_value(invalid, "scenario.1.capital_call_request.1.id",
        std::string(129U, 'a'));
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "request identifiers are length-bounded");

    invalid = normalized;
    set_value(invalid,
        "scenario.1.eligible_basis_movement.1.reference_id", "bad ref");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "event reference identifiers reject whitespace");
}

void test_intrinsic_numeric_ranges(const std::string& normalized) {
    struct InvalidValue {
        std::string_view key;
        std::string_view value;
        std::string_view message;
    };
    const std::array<InvalidValue, 15U> invalid_values{{
        {"funding_bridge.funded_at_close_cash_million", "-1",
            "negative funded-at-close cash is rejected"},
        {"provider.1.declared_capacity_million", "1000000.01",
            "provider capacity above the monetary bound is rejected"},
        {"callable_facility.commitment_million", "-0.1",
            "negative callable commitment is rejected"},
        {"callable_facility.liquidity_reserve_fraction", "1.01",
            "callable reserve fraction is confined to [0,1]"},
        {"callable_facility.annual_liquidity_hurdle_rate", "10.01",
            "callable annual rates are bounded"},
        {"warehouse_facility.collateral_advance_rate", "1.01",
            "warehouse advance rate is confined to [0,1]"},
        {"warehouse_facility.annual_interest_rate", "10.01",
            "warehouse annual rates are bounded"},
        {"warehouse_facility.advance_fee_rate", "1.01",
            "warehouse one-time fee fractions are bounded"},
        {"scenario.1.capital_call_request.1.requested_million", "0",
            "capital-call requests must be positive"},
        {"scenario.1.warehouse_draw_request.1.requested_million", "0",
            "warehouse-draw requests must be positive"},
        {"scenario.1.capital_call_outcome.1.actual_cash_million", "-0.1",
            "settlement cash cannot be negative"},
        {"scenario.1.supplemental_receipt.1.actual_cash_million", "0",
            "supplemental receipts must be positive"},
        {"scenario.1.eligible_basis_movement.1.amount_million", "0",
            "eligible-basis movements must be positive"},
        {"scenario.1.protection_absorption.1.amount_million", "0",
            "protection absorptions must be positive"},
        {"scenario.1.protection_release.1.amount_million", "0",
            "protection releases must be positive"},
    }};
    for (const InvalidValue& value : invalid_values) {
        std::string invalid = normalized;
        set_value(invalid, std::string(value.key), std::string(value.value));
        expect_invalid_argument(
            [&invalid] { (void)parse_text(invalid); }, value.message);
    }
}

void test_relational_validation_is_deferred(const std::string& normalized) {
    std::string deferred = normalized;
    set_value(deferred, "callable_facility.availability_start_month", "20");
    set_value(deferred, "callable_facility.contractual_expiry_month", "1");
    set_value(deferred, "warehouse_facility.availability_end_month", "30");
    set_value(deferred, "warehouse_facility.legal_maturity_month", "2");
    set_value(deferred, "provider.1.declared_capacity_million", "0.01");
    set_value(deferred,
        "scenario.1.capital_call_request.1.facility_id",
        "unknown-callable");
    set_value(deferred,
        "scenario.1.capital_call_request.2.id", "call-early");
    set_value(deferred,
        "scenario.1.capital_call_outcome.1.request_id", "unknown-request");
    set_value(deferred,
        "scenario.1.capital_call_outcome.1.actual_cash_million", "0");
    const cf::FundingBridgeConfig parsed = parse_text(deferred);
    check(parsed.callable_facility.availability_start_month == 20U &&
            parsed.callable_facility.contractual_expiry_month == 1U &&
            parsed.scenario_performance[0].capital_call_requests[0]
                    .facility_id == "unknown-callable" &&
            parsed.scenario_performance[0].capital_call_requests[1].id ==
                "call-early" &&
            parsed.scenario_performance[0].capital_call_outcomes[0]
                    .actual_cash_million == 0.0,
        "parser accepts syntactically valid cross-record and economic inconsistencies for engine validation");
}

void test_count_and_file_resource_guards(
    const std::filesystem::path& path, const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid, "provider.count", "0");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "zero providers are rejected");

    invalid = normalized;
    set_value(invalid, "provider.count", "10001");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "provider count is bounded before schema expansion");

    invalid = normalized;
    set_value(invalid, "scenario.count", "0");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "zero scenarios are rejected");

    invalid = normalized;
    set_value(invalid, "scenario.count", "10001");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "scenario count is bounded before schema expansion");

    const std::array<std::string_view, 8U> collections{
        "capital_call_request", "capital_call_outcome",
        "warehouse_draw_request", "warehouse_draw_outcome",
        "supplemental_receipt", "eligible_basis_movement",
        "protection_absorption", "protection_release",
    };
    for (const std::string_view collection : collections) {
        invalid = normalized;
        set_value(invalid,
            "scenario.1." + std::string(collection) + ".count", "100001");
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "each per-scenario record collection is bounded before expansion");
    }

    cf::FundingBridgeConfig three_scenarios = full_config();
    three_scenarios.scenario_performance.push_back(
        three_scenarios.scenario_performance.back());
    const std::string three_normalized = normalized_config(three_scenarios);
    invalid = three_normalized;
    for (std::size_t scenario = 1U; scenario <= 3U; ++scenario) {
        for (const std::string_view collection : collections) {
            set_value(invalid, "scenario." + std::to_string(scenario) + "." +
                    std::string(collection) + ".count",
                "100000");
        }
    }
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "aggregate performance records are bounded before schema expansion");

    invalid = normalized;
    for (const std::string_view collection : collections) {
        set_value(invalid,
            "scenario.1." + std::string(collection) + ".count", "100000");
    }
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "expanded closed-key population is independently resource-bounded");

    std::string long_line = normalized;
    set_value(long_line, "funding_bridge.source_note",
        std::string(4'097U, 'x'));
    expect_invalid_argument([&long_line] { (void)parse_text(long_line); },
        "4096-byte line guard is enforced while reading");

    write_text(path, "x");
    std::filesystem::resize_file(path, 16U * 1024U * 1024U + 1U);
    expect_invalid_argument(
        [&path] { (void)cf::load_funding_bridge_config(path); },
        "16 MiB file guard is enforced before parsing");

    const std::filesystem::path missing = path.string() + ".missing";
    (void)std::filesystem::remove(missing);
    expect_runtime_error(
        [&missing] { (void)cf::load_funding_bridge_config(missing); },
        "an unreadable path is reported as a runtime error");
}

void test_printer_rejects_intrinsically_invalid_config() {
    cf::FundingBridgeConfig invalid = full_config();
    invalid.policy_uses_observed_history_only = false;
    std::ostringstream output;
    expect_invalid_argument(
        [&output, &invalid] {
            cf::print_normalized_funding_bridge_config(output, invalid);
        },
        "printer rejects false required assertions");
    check(output.str().empty(),
        "printer emits no partial output when intrinsic validation fails");

    invalid = full_config();
    invalid.warehouse_facility.annual_interest_rate =
        std::numeric_limits<double>::infinity();
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects non-finite terms");

    invalid = full_config();
    invalid.scenario_label = " padded";
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects text that would not round-trip after trimming");

    invalid = full_config();
    invalid.providers.clear();
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects an empty provider set");

    invalid = full_config();
    invalid.scenario_performance.clear();
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects an empty performance set");

    invalid = full_config();
    invalid.scenario_performance[0].capital_call_outcomes[0].status =
        static_cast<cf::FundingSettlementStatus>(255U);
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects invalid in-memory enum discriminants");

    invalid = full_config();
    invalid.scenario_performance[0].protection_absorptions[0]
        .amount_million = 0.0;
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects non-positive event amounts");
}

} // namespace

int main() {
    const std::filesystem::path path = std::filesystem::current_path() /
        "funding-bridge-config-parser-test.cfg";
    try {
        const std::string normalized = normalized_config(full_config());
        test_full_roundtrip_and_output_state(path);
        test_key_order_comments_whitespace_bom_and_locale(normalized);
        test_strict_key_set(normalized);
        test_malformed_values_and_enum_tokens(normalized);
        test_text_identifiers_and_all_assertions(normalized);
        test_intrinsic_numeric_ranges(normalized);
        test_relational_validation_is_deferred(normalized);
        test_count_and_file_resource_guards(path, normalized);
        test_printer_rejects_intrinsically_invalid_config();
        (void)std::filesystem::remove(path);
    } catch (const std::exception& error) {
        (void)std::filesystem::remove(path);
        std::cerr << "FAIL: unexpected test exception: " << error.what()
                  << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << failures << " funding-bridge-config test(s) failed\n";
        return 1;
    }
    std::cout << "all funding-bridge-config tests passed\n";
    return 0;
}
