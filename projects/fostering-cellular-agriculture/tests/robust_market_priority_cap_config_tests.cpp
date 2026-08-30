// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_market_priority_cap_config.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
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
        "priority_cap.model_version=0.1.0\n"
        "priority_cap.label=Finite synthetic market priority-cap parser test\n"
        "priority_cap.source_note=Unvalidated synthetic priority-cap terms only\n"
        "priority_cap.synthetic_inputs=true\n"
        "priority_cap.junior_claim_id=catalytic-first-loss\n"
        "priority_cap.market_claim_id=market-priority\n"
        "priority_cap.contractual_ceiling_million=1\n"
        "priority_cap.junior_target_npv_million=0\n"
        "market_priority_cap_grid.count=5\n"
        "market_priority_cap_grid.1.amount_million=1\n"
        "market_priority_cap_grid.2.amount_million=0.5\n"
        "market_priority_cap_grid.3.amount_million=0\n"
        "market_priority_cap_grid.4.amount_million=0.5333333333333333\n"
        "market_priority_cap_grid.5.amount_million=0.08\n"
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

[[nodiscard]] cf::RobustMarketPriorityCapConfig parse(std::string text) {
    std::istringstream input(std::move(text));
    return cf::parse_robust_market_priority_cap_config(input);
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
    const cf::RobustMarketPriorityCapConfig config = parse(valid_config());
    check(config.market_priority_nonprincipal_cap_million_grid.size() == 5U &&
            config.market_priority_nonprincipal_cap_million_grid.front() ==
                0.0 &&
            config.market_priority_nonprincipal_cap_million_grid[1] == 0.08 &&
            config.market_priority_nonprincipal_cap_million_grid[2] == 0.5 &&
            config.market_priority_nonprincipal_cap_million_grid[3] > 0.53 &&
            config.market_priority_nonprincipal_cap_million_grid.back() ==
                config.contractual_ceiling_million,
        "parser canonicalizes the complete finite cap grid");
    check(!config.constraints.maximum_market_wal_years.has_value() &&
            config.constraints.minimum_market_robust_npv_margin_fraction ==
                0.0 &&
            config.junior_target_npv_million == 0.0,
        "literal none is distinct from explicit market and junior targets");

    std::ostringstream first;
    cf::print_normalized_robust_market_priority_cap_config(first, config);
    const cf::RobustMarketPriorityCapConfig reparsed = parse(first.str());
    std::ostringstream second;
    cf::print_normalized_robust_market_priority_cap_config(second, reparsed);
    check(first.str() == second.str(),
        "normalized priority-cap print-load-print is byte stable");
    check(first.str().find(
              "mandate.maximum_market_wal_years=none\n") !=
            std::string::npos &&
            first.str().find(
                "market_priority_cap_grid.1.amount_million=0\n") !=
                std::string::npos &&
            first.str().find(
                "market_priority_cap_grid.5.amount_million=1\n") !=
                std::string::npos &&
            first.str().find(
                "priority_cap.junior_target_npv_million=0\n") !=
                std::string::npos,
        "normalized text preserves absence, grid boundaries, and the explicit junior target");

    const cf::RobustMarketPriorityCapConfig bom =
        parse(std::string("\xEF\xBB\xBF", 3U) + valid_config());
    check(bom.model_version == "0.1.0",
        "one leading UTF-8 BOM is accepted");

    std::string crlf_text = valid_config();
    std::size_t newline = 0U;
    while ((newline = crlf_text.find('\n', newline)) != std::string::npos) {
        crlf_text.replace(newline, 1U, "\r\n");
        newline += 2U;
    }
    const cf::RobustMarketPriorityCapConfig crlf =
        parse(std::move(crlf_text));
    check(crlf.market_priority_nonprincipal_cap_million_grid ==
            config.market_priority_nonprincipal_cap_million_grid,
        "bounded incremental line reading preserves CRLF configuration semantics");
}

void test_closed_schema_and_numeric_guardrails() {
    expect_invalid_argument(
        [] {
            (void)parse(valid_config() + "priority_cap.unknown=true\n");
        },
        "unknown keys are rejected");

    std::string duplicate = valid_config();
    duplicate += "priority_cap.model_version=0.1.0\n";
    expect_invalid_argument([&] { (void)parse(duplicate); },
        "duplicate keys are rejected");

    std::string missing_optional = valid_config();
    replace_once(missing_optional,
        "mandate.maximum_market_wal_years=none\n", "");
    expect_invalid_argument([&] { (void)parse(missing_optional); },
        "missing optional-valued keys are still rejected");

    std::string missing_junior_target = valid_config();
    replace_once(missing_junior_target,
        "priority_cap.junior_target_npv_million=0\n", "");
    expect_invalid_argument([&] { (void)parse(missing_junior_target); },
        "the cap term cannot silently reuse a legacy participation target");

    std::string nonfinite = valid_config();
    replace_once(nonfinite,
        "mandate.maximum_market_expected_loss_fraction=0.05",
        "mandate.maximum_market_expected_loss_fraction=nan");
    expect_invalid_argument([&] { (void)parse(nonfinite); },
        "non-finite mandate values are rejected");

    std::string nonfinite_target = valid_config();
    replace_once(nonfinite_target,
        "priority_cap.junior_target_npv_million=0",
        "priority_cap.junior_target_npv_million=inf");
    expect_invalid_argument([&] { (void)parse(nonfinite_target); },
        "non-finite junior targets are rejected");

    std::string wrong_none = valid_config();
    replace_once(wrong_none,
        "mandate.maximum_market_wal_years=none",
        "mandate.maximum_market_wal_years=None");
    expect_invalid_argument([&] { (void)parse(wrong_none); },
        "only the exact literal none denotes an absent mandate");

    std::string duplicate_grid = valid_config();
    replace_once(duplicate_grid,
        "market_priority_cap_grid.4.amount_million=0.5333333333333333",
        "market_priority_cap_grid.4.amount_million=0.5");
    expect_invalid_argument([&] { (void)parse(duplicate_grid); },
        "duplicate cap-grid values are rejected after sorting");

    std::string negative_cap = valid_config();
    replace_once(negative_cap,
        "market_priority_cap_grid.5.amount_million=0.08",
        "market_priority_cap_grid.5.amount_million=-0.08");
    expect_invalid_argument([&] { (void)parse(negative_cap); },
        "negative cap-grid values are rejected");

    std::string microscopic_cap = valid_config();
    replace_once(microscopic_cap,
        "market_priority_cap_grid.5.amount_million=0.08",
        "market_priority_cap_grid.5.amount_million=1e-7");
    expect_invalid_argument([&] { (void)parse(microscopic_cap); },
        "positive sub-unit cap-grid values are rejected");

    std::string negative_zero = valid_config();
    replace_once(negative_zero,
        "market_priority_cap_grid.3.amount_million=0",
        "market_priority_cap_grid.3.amount_million=-0");
    expect_invalid_argument([&] { (void)parse(negative_zero); },
        "negative zero is not accepted as the literal zero anchor");

    std::string missing_zero = valid_config();
    replace_once(missing_zero,
        "market_priority_cap_grid.3.amount_million=0",
        "market_priority_cap_grid.3.amount_million=0.01");
    expect_invalid_argument([&] { (void)parse(missing_zero); },
        "the declared grid must include a zero-cap baseline");

    std::string ceiling_mismatch = valid_config();
    replace_once(ceiling_mismatch,
        "priority_cap.contractual_ceiling_million=1",
        "priority_cap.contractual_ceiling_million=0.9");
    expect_invalid_argument([&] { (void)parse(ceiling_mismatch); },
        "the contractual ceiling must be the tested grid maximum");

    std::string invalid_probability = valid_config();
    replace_once(invalid_probability,
        "mandate.maximum_market_negative_npv_probability=0.1",
        "mandate.maximum_market_negative_npv_probability=1.1");
    expect_invalid_argument([&] { (void)parse(invalid_probability); },
        "probability mandates remain in the unit interval");

    std::string zero_count = valid_config();
    replace_once(zero_count, "market_priority_cap_grid.count=5",
        "market_priority_cap_grid.count=0");
    expect_invalid_argument([&] { (void)parse(zero_count); },
        "zero cap-grid counts are rejected before indexed expansion");

    std::string huge_count = valid_config();
    replace_once(huge_count, "market_priority_cap_grid.count=5",
        "market_priority_cap_grid.count=1025");
    expect_invalid_argument([&] { (void)parse(huge_count); },
        "cap-grid record counts above 1,024 are rejected");

    std::string no_cap_sensitive_mandate = valid_config();
    replace_once(no_cap_sensitive_mandate,
        "mandate.minimum_market_robust_npv_margin_fraction=0",
        "mandate.minimum_market_robust_npv_margin_fraction=none");
    replace_once(no_cap_sensitive_mandate,
        "mandate.maximum_market_negative_npv_probability=0.1",
        "mandate.maximum_market_negative_npv_probability=none");
    replace_once(no_cap_sensitive_mandate,
        "mandate.maximum_market_npv_shortfall_es95_fraction=0.51",
        "mandate.maximum_market_npv_shortfall_es95_fraction=none");
    replace_once(no_cap_sensitive_mandate,
        "mandate.maximum_market_npv_shortfall_es99_fraction=0.51",
        "mandate.maximum_market_npv_shortfall_es99_fraction=none");
    expect_invalid_argument([&] { (void)parse(no_cap_sensitive_mandate); },
        "at least one mandate must depend on the cap lever");

    std::string unsupported = valid_config();
    replace_once(unsupported,
        "priority_cap.model_version=0.1.0",
        "priority_cap.model_version=9.9.9");
    expect_invalid_argument([&] { (void)parse(unsupported); },
        "unsupported model versions fail closed");

    std::string non_synthetic = valid_config();
    replace_once(non_synthetic,
        "priority_cap.synthetic_inputs=true",
        "priority_cap.synthetic_inputs=false");
    expect_invalid_argument([&] { (void)parse(non_synthetic); },
        "v0.1 cannot be relabeled as calibrated input");
}

void test_text_bom_and_stream_guardrails() {
    std::string unsafe_id = valid_config();
    replace_once(unsafe_id,
        "priority_cap.market_claim_id=market-priority",
        "priority_cap.market_claim_id=../../market");
    expect_invalid_argument([&] { (void)parse(unsafe_id); },
        "unsafe claim identifiers are rejected");

    std::string duplicate_id = valid_config();
    replace_once(duplicate_id,
        "priority_cap.market_claim_id=market-priority",
        "priority_cap.market_claim_id=catalytic-first-loss");
    expect_invalid_argument([&] { (void)parse(duplicate_id); },
        "junior and market claim identifiers must remain different");

    std::string padded_text = valid_config();
    replace_once(padded_text,
        "priority_cap.label=Finite synthetic market priority-cap parser test",
        "priority_cap.label= Finite synthetic market priority-cap parser test");
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
            (void)cf::parse_robust_market_priority_cap_config(failed_input);
        },
        "non-EOF input stream failures are not mistaken for empty configs");

    cf::RobustMarketPriorityCapConfig injected = parse(valid_config());
    injected.source_note = "line one\npriority_cap.synthetic_inputs=false";
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_robust_market_priority_cap_config(
                output, injected);
        },
        "normalized output rejects control-character schema injection");

    injected = parse(valid_config());
    injected.scenario_label = std::string("embedded ") +
        std::string("\xEF\xBB\xBF", 3U) + " BOM";
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_robust_market_priority_cap_config(
                output, injected);
        },
        "normalized output rejects embedded BOM text");

    injected = parse(valid_config());
    injected.junior_target_npv_million =
        std::numeric_limits<double>::quiet_NaN();
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_robust_market_priority_cap_config(
                output, injected);
        },
        "normalized output validates in-memory numeric terms");

    const cf::RobustMarketPriorityCapConfig config = parse(valid_config());
    std::ostringstream failed_output;
    failed_output.setstate(std::ios::badbit);
    expect_runtime_error(
        [&] {
            cf::print_normalized_robust_market_priority_cap_config(
                failed_output, config);
        },
        "normalized writer reports output stream failures");
}

void test_bounded_adversarial_stream_consumption() {
    std::istringstream delimiter_free(
        std::string(2U * 1024U * 1024U, 'x'));
    bool long_line_rejected = false;
    try {
        (void)cf::parse_robust_market_priority_cap_config(delimiter_free);
    } catch (const std::invalid_argument& error) {
        long_line_rejected =
            std::string_view(error.what()).find("4096-byte") !=
            std::string_view::npos;
    } catch (...) {
        // Checked below.
    }
    const std::streampos long_line_position = delimiter_free.tellg();
    check(long_line_rejected && long_line_position >= std::streampos{0} &&
            long_line_position <= std::streampos{4'097},
        "delimiter-free input is rejected after bounded incremental consumption rather than an unbounded getline allocation");

    std::string unknown_flood;
    unknown_flood.reserve(512U * 1024U);
    for (std::size_t index = 0U; index < 20'000U; ++index) {
        unknown_flood += "unknown_key_" + std::to_string(index) + "=value\n";
    }
    const std::size_t first_line_bytes = unknown_flood.find('\n') + 1U;
    std::istringstream unknown_input(std::move(unknown_flood));
    bool unknown_rejected_at_first_line = false;
    try {
        (void)cf::parse_robust_market_priority_cap_config(unknown_input);
    } catch (const std::invalid_argument& error) {
        const std::string_view message{error.what()};
        unknown_rejected_at_first_line =
            message.find("configuration line 1: unknown key") !=
            std::string_view::npos;
    } catch (...) {
        // Checked below.
    }
    const std::streampos unknown_position = unknown_input.tellg();
    check(unknown_rejected_at_first_line &&
            unknown_position >= std::streampos{0} &&
            unknown_position <=
                static_cast<std::streamoff>(first_line_bytes),
        "an arbitrary unique unknown-key flood is rejected before later records are read or inserted");
}

void test_file_size_and_load_guardrails() {
    const std::filesystem::path oversized =
        std::filesystem::current_path() /
        "robust-market-priority-cap-config-oversized.tmp";
    {
        std::ofstream output(oversized, std::ios::binary | std::ios::trunc);
        check(static_cast<bool>(output),
            "oversized-loader fixture opens for writing");
    }
    std::filesystem::resize_file(oversized,
        static_cast<std::uintmax_t>(16U * 1024U * 1024U) + 1U);
    expect_invalid_argument(
        [&] {
            (void)cf::load_robust_market_priority_cap_config(oversized);
        },
        "loader rejects files above the 16 MiB guardrail before parsing");
    std::error_code remove_error;
    (void)std::filesystem::remove(oversized, remove_error);

    const std::filesystem::path missing =
        std::filesystem::current_path() /
        "robust-market-priority-cap-config-does-not-exist.tmp";
    expect_runtime_error(
        [&] {
            (void)cf::load_robust_market_priority_cap_config(missing);
        },
        "loader distinguishes missing files from configuration errors");
}

} // namespace

int main() {
    test_parse_canonical_round_trip();
    test_closed_schema_and_numeric_guardrails();
    test_text_bom_and_stream_guardrails();
    test_bounded_adversarial_stream_consumption();
    test_file_size_and_load_guardrails();

    if (failures != 0) {
        std::cerr << failures
                  << " market-priority-cap config test(s) failed\n";
        return 1;
    }
    std::cout << "market-priority-cap config tests passed\n";
    return 0;
}
