// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_capital_mobilization_frontier_config.hpp>

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace cf = naturalehia::cellular_finance;

namespace {

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

[[nodiscard]] std::string valid_config() {
    return
        "frontier.model_version=0.1.0\n"
        "frontier.label=Finite two-claim frontier parser test\n"
        "frontier.source_note=Unvalidated synthetic parser terms only\n"
        "frontier.synthetic_inputs=true\n"
        "frontier.catalytic_claim_id=catalytic-first-loss\n"
        "frontier.market_claim_id=market-priority\n"
        "frontier.market_priority_nonprincipal_cap_million=1\n"
        "frontier.catalytic_annual_physical_hurdle_rate=0\n"
        "frontier.market_annual_physical_hurdle_rate=0\n"
        "frontier.catalytic_target_npv_million=0\n"
        "participation_grid.count=2\n"
        "participation_grid.1.fraction=0.9678571428571429\n"
        "participation_grid.2.fraction=0.8928571428571429\n"
        "catalytic_first_loss_grid.count=4\n"
        "catalytic_first_loss_grid.1.amount_million=14\n"
        "catalytic_first_loss_grid.2.amount_million=8.181818181818182\n"
        "catalytic_first_loss_grid.3.amount_million=16\n"
        "catalytic_first_loss_grid.4.amount_million=12\n"
        "mandate.minimum_robust_aggregate_npv_million=0\n"
        "mandate.minimum_market_robust_npv_margin_fraction=0\n"
        "mandate.maximum_market_expected_loss_fraction=0.05\n"
        "mandate.maximum_market_principal_loss_es95_fraction=0.5\n"
        "mandate.maximum_market_principal_loss_es99_fraction=0.5\n"
        "mandate.maximum_market_principal_impairment_probability=0.1\n"
        "mandate.maximum_market_negative_npv_probability=0.1\n"
        "mandate.maximum_market_npv_shortfall_es95_fraction=0.51\n"
        "mandate.maximum_market_npv_shortfall_es99_fraction=0.51\n"
        "mandate.maximum_market_wal_years=none\n"
        "mandate.maximum_catalytic_first_loss_million=14\n"
        "mandate.maximum_catalytic_npv_concession_million=0.42\n";
}

[[nodiscard]] cf::RobustCapitalMobilizationFrontierConfig parse(
    std::string text) {
    std::istringstream input(std::move(text));
    return cf::parse_robust_capital_mobilization_frontier_config(input);
}

void replace_once(std::string& text, std::string_view old_value,
    std::string_view new_value) {
    const std::size_t position = text.find(old_value);
    if (position == std::string::npos) {
        throw std::logic_error("test replacement source is missing");
    }
    text.replace(position, old_value.size(), new_value);
}

void test_parse_canonical_round_trip() {
    const cf::RobustCapitalMobilizationFrontierConfig config =
        parse(valid_config());
    check(config.participation_fraction_grid.size() == 2U &&
            config.participation_fraction_grid[0] <
                config.participation_fraction_grid[1] &&
            config.catalytic_first_loss_million_grid.size() == 4U &&
            config.catalytic_first_loss_million_grid.front() < 9.0 &&
            config.catalytic_first_loss_million_grid.back() == 16.0,
        "parser canonicalizes both grids into ascending order");
    check(!config.constraints.maximum_market_wal_years.has_value() &&
            config.constraints.maximum_market_expected_loss_fraction ==
                0.05,
        "literal none is distinct from a declared numeric mandate");

    std::ostringstream first;
    cf::print_normalized_robust_capital_mobilization_frontier_config(
        first, config);
    const cf::RobustCapitalMobilizationFrontierConfig reparsed =
        parse(first.str());
    std::ostringstream second;
    cf::print_normalized_robust_capital_mobilization_frontier_config(
        second, reparsed);
    check(first.str() == second.str(),
        "normalized print-load-print is byte stable");
    check(first.str().find("mandate.maximum_market_wal_years=none\n") !=
            std::string::npos &&
            first.str().find(
                "participation_grid.1.fraction=0.8928571428571429\n") !=
                std::string::npos,
        "normalized text preserves absence and canonical grid order");

    const cf::RobustCapitalMobilizationFrontierConfig bom =
        parse(std::string("\xEF\xBB\xBF", 3U) + valid_config());
    check(bom.model_version == "0.1.0",
        "one leading UTF-8 BOM is accepted");
}

void test_closed_schema_and_numeric_guardrails() {
    expect_invalid_argument(
        [] {
            (void)parse(valid_config() + "frontier.unknown=true\n");
        },
        "unknown keys are rejected");

    std::string duplicate = valid_config();
    duplicate += "frontier.model_version=0.1.0\n";
    expect_invalid_argument([&] { (void)parse(duplicate); },
        "duplicate keys are rejected");

    std::string missing = valid_config();
    replace_once(missing,
        "mandate.maximum_market_wal_years=none\n", "");
    expect_invalid_argument([&] { (void)parse(missing); },
        "missing optional-valued keys are still rejected");

    std::string nonfinite = valid_config();
    replace_once(nonfinite,
        "mandate.maximum_market_expected_loss_fraction=0.05",
        "mandate.maximum_market_expected_loss_fraction=nan");
    expect_invalid_argument([&] { (void)parse(nonfinite); },
        "non-finite mandate values are rejected");

    std::string wrong_none = valid_config();
    replace_once(wrong_none,
        "mandate.maximum_market_wal_years=none",
        "mandate.maximum_market_wal_years=None");
    expect_invalid_argument([&] { (void)parse(wrong_none); },
        "only the exact literal none denotes an absent mandate");

    std::string duplicate_grid = valid_config();
    replace_once(duplicate_grid,
        "participation_grid.1.fraction=0.9678571428571429",
        "participation_grid.1.fraction=0.8928571428571429");
    expect_invalid_argument([&] { (void)parse(duplicate_grid); },
        "duplicate grid values are rejected after sorting");

    std::string microscopic = valid_config();
    replace_once(microscopic,
        "catalytic_first_loss_grid.2.amount_million=8.181818181818182",
        "catalytic_first_loss_grid.2.amount_million=1e-7");
    expect_invalid_argument([&] { (void)parse(microscopic); },
        "sub-unit generated claim amounts are rejected");

    std::string zero_count = valid_config();
    replace_once(zero_count, "participation_grid.count=2",
        "participation_grid.count=0");
    expect_invalid_argument([&] { (void)parse(zero_count); },
        "zero grid counts are rejected before indexed expansion");

    std::string huge_count = valid_config();
    replace_once(huge_count, "participation_grid.count=2",
        "participation_grid.count=1025");
    expect_invalid_argument([&] { (void)parse(huge_count); },
        "oversized grid counts are resource bounded");

    std::string cross_product = valid_config();
    replace_once(cross_product, "participation_grid.count=2",
        "participation_grid.count=33");
    replace_once(cross_product, "catalytic_first_loss_grid.count=4",
        "catalytic_first_loss_grid.count=32");
    expect_invalid_argument([&] { (void)parse(cross_product); },
        "candidate cross-products above 1,024 are rejected before expansion");
}

void test_text_bom_and_stream_guardrails() {
    std::string unsafe_id = valid_config();
    replace_once(unsafe_id,
        "frontier.market_claim_id=market-priority",
        "frontier.market_claim_id=../../market");
    expect_invalid_argument([&] { (void)parse(unsafe_id); },
        "unsafe claim identifiers are rejected");

    std::string padded_text = valid_config();
    replace_once(padded_text,
        "frontier.label=Finite two-claim frontier parser test",
        "frontier.label= Finite two-claim frontier parser test");
    expect_invalid_argument([&] { (void)parse(padded_text); },
        "text surrounding whitespace is rejected");

    std::string embedded_bom = valid_config();
    embedded_bom += std::string("# second ") +
        std::string("\xEF\xBB\xBF", 3U) + " marker\n";
    expect_invalid_argument([&] { (void)parse(embedded_bom); },
        "embedded BOMs are rejected even on comments");

    std::string long_line(4'097U, 'x');
    expect_invalid_argument([&] { (void)parse(long_line); },
        "individual lines are size bounded");

    std::istringstream failed_input(valid_config());
    failed_input.setstate(std::ios::badbit);
    expect_runtime_error(
        [&] {
            (void)cf::parse_robust_capital_mobilization_frontier_config(
                failed_input);
        },
        "non-EOF input stream failures are not mistaken for empty configs");

    cf::RobustCapitalMobilizationFrontierConfig injected =
        parse(valid_config());
    injected.source_note = "line one\nfrontier.synthetic_inputs=false";
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_robust_capital_mobilization_frontier_config(
                output, injected);
        },
        "normalized output rejects control-character schema injection");

    injected = parse(valid_config());
    injected.scenario_label = std::string("embedded ") +
        std::string("\xEF\xBB\xBF", 3U) + " BOM";
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_robust_capital_mobilization_frontier_config(
                output, injected);
        },
        "normalized output rejects embedded BOM text");

    const cf::RobustCapitalMobilizationFrontierConfig config =
        parse(valid_config());
    std::ostringstream failed_output;
    failed_output.setstate(std::ios::badbit);
    expect_runtime_error(
        [&] {
            cf::print_normalized_robust_capital_mobilization_frontier_config(
                failed_output, config);
        },
        "normalized writer reports output stream failures");
}

} // namespace

int main() {
    test_parse_canonical_round_trip();
    test_closed_schema_and_numeric_guardrails();
    test_text_bom_and_stream_guardrails();

    if (failures != 0) {
        std::cerr << failures
                  << " capital-mobilization-frontier config test(s) failed\n";
        return 1;
    }
    std::cout << "capital-mobilization-frontier config tests passed\n";
    return 0;
}
