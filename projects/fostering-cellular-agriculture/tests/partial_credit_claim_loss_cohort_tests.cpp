// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/partial_credit_claim_loss_cohort.hpp>
#include <naturalehia/cellular_finance/evidence_gate.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] bool near(
    double first, double second, double tolerance = 1.0e-10) {
    return std::abs(first - second) <=
        tolerance * std::max({1.0, std::abs(first), std::abs(second)});
}

[[nodiscard]] double lower(const cf::ClaimLedgerValue& value) {
    if (!value.lower.has_value()) {
        throw std::runtime_error("test expected a lower amount endpoint");
    }
    return *value.lower;
}

[[nodiscard]] double upper(const cf::ClaimLedgerValue& value) {
    if (!value.upper.has_value()) {
        throw std::runtime_error("test expected an upper amount endpoint");
    }
    return *value.upper;
}

template <typename Function>
void expect_exception(Function&& operation, std::string_view diagnostic,
    std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::exception& error) {
        check(std::string_view(error.what()).find(diagnostic) !=
                std::string_view::npos,
            std::string(message) + " (stable diagnostic)");
    }
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not read temporary cohort fixture");
    }
    std::ostringstream output;
    output << input.rdbuf();
    if (input.bad()) {
        throw std::runtime_error(
            "could not read temporary cohort fixture completely");
    }
    return output.str();
}

void write_text(
    const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not write temporary cohort fixture");
    }
    output.write(contents.data(),
        static_cast<std::streamsize>(contents.size()));
    if (!output) {
        throw std::runtime_error(
            "could not write temporary cohort fixture completely");
    }
}

void replace_key(
    std::string& text, std::string_view key, std::string_view value) {
    const std::string prefix = std::string(key) + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos ||
        text.find(prefix, position + prefix.size()) != std::string::npos) {
        throw std::runtime_error(
            "temporary cohort fixture key must occur exactly once");
    }
    const std::size_t begin = position + prefix.size();
    const std::size_t end = text.find('\n', begin);
    text.replace(begin,
        end == std::string::npos ? std::string::npos : end - begin,
        value);
}

void replace_once(std::string& text, std::string_view before,
    std::string_view after) {
    const std::size_t position = text.find(before);
    if (position == std::string::npos ||
        text.find(before, position + before.size()) != std::string::npos) {
        throw std::runtime_error(
            "temporary cohort fixture target must occur exactly once");
    }
    text.replace(position, before.size(), after);
}

[[nodiscard]] std::string zero_entry_amount_columns(
    std::string_view text, std::size_t lower_column,
    std::size_t upper_column) {
    std::ostringstream output;
    std::size_t line_start = 0U;
    bool header = true;
    while (line_start < text.size()) {
        const std::size_t newline = text.find('\n', line_start);
        const std::size_t line_end = newline == std::string_view::npos
            ? text.size()
            : newline;
        std::string line(text.substr(line_start, line_end - line_start));
        if (header) {
            output << line;
            header = false;
        } else if (!line.empty()) {
            std::vector<std::string> fields;
            std::size_t field_start = 0U;
            while (true) {
                const std::size_t tab = line.find('\t', field_start);
                fields.push_back(line.substr(field_start,
                    tab == std::string::npos
                        ? std::string::npos
                        : tab - field_start));
                if (tab == std::string::npos) break;
                field_start = tab + 1U;
            }
            if (upper_column >= fields.size()) {
                throw std::runtime_error(
                    "temporary entry fixture has an unexpected row width");
            }
            fields[lower_column] = "0";
            fields[upper_column] = "0";
            for (std::size_t index = 0U; index < fields.size(); ++index) {
                if (index != 0U) output << '\t';
                output << fields[index];
            }
        }
        if (newline != std::string_view::npos) output << '\n';
        if (newline == std::string_view::npos) break;
        line_start = newline + 1U;
    }
    return output.str();
}

struct TemporaryClaimFixture {
    std::filesystem::path root{};

    explicit TemporaryClaimFixture(
        const std::filesystem::path& source) {
        const auto suffix = std::chrono::steady_clock::now()
                                .time_since_epoch()
                                .count();
        root = std::filesystem::temp_directory_path() /
            ("naturalehia-partial-credit-cohort-" +
                std::to_string(suffix));
        std::filesystem::copy(source, root,
            std::filesystem::copy_options::recursive);
    }

    TemporaryClaimFixture(const TemporaryClaimFixture&) = delete;
    TemporaryClaimFixture& operator=(const TemporaryClaimFixture&) = delete;

    ~TemporaryClaimFixture() noexcept {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
};

void refresh_bound_hash(const std::filesystem::path& fixture,
    std::string_view key, std::string_view filename) {
    std::string config = read_text(fixture / "claim.cfg");
    replace_key(config, key,
        cf::sha256_file_lower_hex(fixture / filename));
    write_text(fixture / "claim.cfg", config);
}

void remove_failure_provider_cash(
    const std::filesystem::path& fixture) {
    std::string entries = read_text(fixture / "scenario_entries.tsv");
    replace_once(entries,
        "failure-with-provider\tfailure-guarantee-principal-cash\t"
        "failure-guarantee-principal-cash\tmaturity-failure\t"
        "guarantee-principal-cash\t12\t0\tknown\t4\t4\t",
        "failure-with-provider\tfailure-guarantee-principal-cash\t"
        "failure-guarantee-principal-cash\tmaturity-failure\t"
        "guarantee-principal-cash\t12\t0\tknown\t0\t0\t");
    replace_once(entries,
        "failure-with-provider\tfailure-principal-writeoff\t"
        "failure-principal-writeoff\tmaturity-failure\t"
        "principal-writeoff\t12\t0\tknown\t4\t4\t",
        "failure-with-provider\tfailure-principal-writeoff\t"
        "failure-principal-writeoff\tmaturity-failure\t"
        "principal-writeoff\t12\t0\tknown\t8\t8\t");
    write_text(fixture / "scenario_entries.tsv", entries);
    refresh_bound_hash(fixture, "file.scenario_entries.sha256",
        "scenario_entries.tsv");
}

void make_zero_draw_fixture(const std::filesystem::path& fixture) {
    std::string config = read_text(fixture / "claim.cfg");
    replace_key(config, "claim.contractual_face_amount_lower_million", "0");
    replace_key(config, "claim.contractual_face_amount_upper_million", "0");
    write_text(fixture / "claim.cfg", config);

    const std::string common = zero_entry_amount_columns(
        read_text(fixture / "common_entries.tsv"), 7U, 8U);
    write_text(fixture / "common_entries.tsv", common);
    refresh_bound_hash(fixture, "file.common_entries.sha256",
        "common_entries.tsv");

    const std::string scenarios = zero_entry_amount_columns(
        read_text(fixture / "scenario_entries.tsv"), 8U, 9U);
    write_text(fixture / "scenario_entries.tsv", scenarios);
    refresh_bound_hash(fixture, "file.scenario_entries.sha256",
        "scenario_entries.tsv");

    std::string terms = read_text(fixture / "terms.tsv");
    replace_once(terms,
        "contractual_face_amount_million\tknown\t10\t",
        "contractual_face_amount_million\tknown\t0\t");
    replace_once(terms, "buyer_price_million\tknown\t9\t",
        "buyer_price_million\tknown\t0\t");
    write_text(fixture / "terms.tsv", terms);
    refresh_bound_hash(fixture, "file.terms.sha256", "terms.tsv");
}

[[nodiscard]] cf::PartialCreditClaimLossCohortConfig base_config() {
    cf::PartialCreditClaimLossCohortConfig config;
    config.cohort_id = "synthetic-partial-credit-loss-book-v0.1";
    config.as_of_date = "2027-06-01";
    config.frame_start_date = "2026-01-01";
    config.frame_end_date = "2026-12-31";
    config.source_note =
        "Synthetic denominator and amount-range mechanics; not empirical calibration";
    config.population_definition = "synthetic-population-v0.1";
    config.sampling_unit_definition = "one-economic-claim-v0.1";
    config.economic_cluster_definition = "consolidated-cluster-v0.1";
    config.protection_term_stratum_definition =
        "principal-only-partial-credit-v0.1";
    config.outcome_horizon_definition = "declared-horizon-v0.1";
    config.loss_definition = "claim-ledger-principal-loss-v0.1";
    config.resolution_definition = "closed-principal-provider-path-v0.1";
    config.censoring_definition = "retain-open-members-v0.1";
    config.denominator_definition = "all-included-frame-members-v0.1";
    config.currency_label = "TEST";
    config.monetary_basis =
        "nominal synthetic millions at 2026-01-01";
    config.population_frame_count = 5U;
    config.exclusion_rules = {{"pre-existing-rule-v0.1", "2025-12-01",
        true, "Exclude a declared out-of-scope sampling unit before eligibility"}};
    return config;
}

[[nodiscard]] cf::PartialCreditClaimLossObservation included_observation(
    std::string id, const cf::ClaimLedgerPackage& package,
    cf::PartialCreditClaimLossDisposition disposition,
    cf::PartialCreditClaimTriggerStatus trigger_status,
    std::string horizon, std::string scenario,
    std::string provider_claim_id) {
    cf::PartialCreditClaimLossObservation observation;
    observation.observation_id = std::move(id);
    observation.economic_cluster_id = package.config.economic_cluster_id;
    observation.eligible_date = "2026-01-01";
    observation.horizon_end_date = std::move(horizon);
    observation.disposition = disposition;
    observation.trigger_status = trigger_status;
    observation.trigger_date =
        trigger_status == cf::PartialCreditClaimTriggerStatus::Triggered
        ? "2027-01-01" : "NONE";
    observation.classification_date =
        disposition == cf::PartialCreditClaimLossDisposition::NotYetMatured
        ? "NONE" : "2027-05-01";
    observation.resolution_date =
        disposition == cf::PartialCreditClaimLossDisposition::Resolved
        ? "2027-01-02" : "NONE";
    observation.claim_package = package;
    observation.expected_claim_config_sha256 =
        package.claim_config_sha256;
    observation.realized_scenario_id = std::move(scenario);
    observation.provider_claim_id = std::move(provider_claim_id);
    observation.population_evidence_record_ids = {"SYNTHETIC"};
    observation.population_requirement_ids = {"SYNTHETIC"};
    observation.classification_evidence_record_ids = {"SYNTHETIC"};
    observation.classification_requirement_ids = {"SYNTHETIC"};
    return observation;
}

[[nodiscard]] cf::PartialCreditClaimLossObservation excluded_observation() {
    cf::PartialCreditClaimLossObservation observation;
    observation.observation_id = "excluded-member";
    observation.economic_cluster_id = "excluded-synthetic-cluster";
    observation.eligible_date = "2026-01-01";
    observation.horizon_end_date = "2027-01-01";
    observation.disposition =
        cf::PartialCreditClaimLossDisposition::Excluded;
    observation.trigger_status =
        cf::PartialCreditClaimTriggerStatus::NotApplicable;
    observation.classification_date = "2026-01-01";
    observation.exclusion_rule_id = "pre-existing-rule-v0.1";
    observation.population_evidence_record_ids = {"SYNTHETIC"};
    observation.population_requirement_ids = {"SYNTHETIC"};
    observation.classification_evidence_record_ids = {"SYNTHETIC"};
    observation.classification_requirement_ids = {"SYNTHETIC"};
    return observation;
}

[[nodiscard]] cf::PartialCreditClaimLossCohortPackage base_package(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second,
    const cf::ClaimLedgerPackage& rare,
    const cf::ClaimLedgerPackage& shifted) {
    cf::PartialCreditClaimLossCohortPackage package;
    package.config = base_config();
    package.observations = {
        included_observation("resolved-performing", first,
            cf::PartialCreditClaimLossDisposition::Resolved,
            cf::PartialCreditClaimTriggerStatus::NotTriggered,
            "2027-01-01", "performing-maturity",
            "synthetic-provider-claim"),
        included_observation("resolved-loss", second,
            cf::PartialCreditClaimLossDisposition::Resolved,
            cf::PartialCreditClaimTriggerStatus::Triggered,
            "2027-01-01", "failure-with-provider",
            "synthetic-provider-claim"),
        included_observation("triggered-unresolved", rare,
            cf::PartialCreditClaimLossDisposition::Unresolved,
            cf::PartialCreditClaimTriggerStatus::Triggered,
            "2027-01-01", "NONE", "synthetic-provider-claim"),
        included_observation("not-yet-matured", shifted,
            cf::PartialCreditClaimLossDisposition::NotYetMatured,
            cf::PartialCreditClaimTriggerStatus::Unknown,
            "2028-01-01", "NONE", "synthetic-provider-claim"),
        excluded_observation(),
    };
    return package;
}

[[nodiscard]] const cf::PartialCreditClaimLossObservationResult& row(
    const cf::PartialCreditClaimLossCohortEvaluation& result,
    std::string_view id) {
    const auto found = std::find_if(result.observations.begin(),
        result.observations.end(), [id](const auto& candidate) {
            return candidate.observation_id == id;
        });
    if (found == result.observations.end()) {
        throw std::runtime_error("test observation result is missing");
    }
    return *found;
}

void test_complete_candidate_book(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second,
    const cf::ClaimLedgerPackage& rare,
    const cf::ClaimLedgerPackage& shifted) {
    const auto result = cf::evaluate_partial_credit_claim_loss_cohort(
        base_package(first, second, rare, shifted));
    check(result.candidate_only &&
            !result.calibrated_execution_authorized &&
            !result.portfolio_export_authorized &&
            !result.empirical_realized_cash_admissible &&
            result.synthetic_package_present &&
            result.claim_ledger_package_blockers_present,
        "cohort kernel cannot promote synthetic mechanics into calibration or pricing authority");
    check(result.frame_count == 5U && result.included_count == 4U &&
            result.resolved_count == 2U &&
            result.not_yet_matured_count == 1U &&
            result.unresolved_count == 1U &&
            result.excluded_count == 1U && result.censored_count == 2U,
        "frame, included, resolved, open, excluded and censored denominators reconcile");
    check(result.trigger_known_count == 3U &&
            result.triggered_count == 2U &&
            result.trigger_unknown_count == 1U &&
            result.resolved_provider_claim_generated_count == 1U &&
            result.resolved_provider_claim_paid_count == 1U &&
            result.provider_unpaid_claim_known_positive_count == 0U &&
            result.provider_unpaid_claim_possible_positive_count == 2U,
        "trigger and resolved/possible provider counts remain distinct disclosures");
    check(!result.mechanical_amount_ranges_available &&
            result.resolved_principal_conservation_reconciled &&
            near(lower(result.total_contractual_face_million), 40.0) &&
            near(upper(result.total_contractual_face_million), 40.0) &&
            near(lower(result.total_resolved_opening_principal_million), 0.0) &&
            near(lower(result.total_resolved_funded_principal_created_million), 18.0) &&
            near(lower(result.total_resolved_capitalized_principal_million), 2.0) &&
            near(lower(result.total_resolved_principal_rollforward_basis_million), 20.0) &&
            near(lower(result.total_resolved_borrower_principal_cash_million), 10.0) &&
            near(lower(result.total_resolved_recovery_principal_cash_million), 2.0) &&
            near(lower(result.total_resolved_provider_principal_cash_million), 4.0) &&
            near(lower(result.total_resolved_conversion_principal_million), 0.0) &&
            near(lower(result.total_resolved_final_principal_writeoff_million), 4.0) &&
            result.total_pre_support_principal_shortfall_million.status ==
                cf::ClaimLedgerValueStatus::Unknown &&
            near(lower(result.total_provider_claim_generated_million), 4.0) &&
            near(upper(result.total_provider_claim_generated_million), 12.0) &&
            near(lower(result.total_provider_claim_payable_million), 4.0) &&
            near(upper(result.total_provider_claim_payable_million), 12.0) &&
            near(lower(result.total_provider_principal_cash_million), 4.0) &&
            near(upper(result.total_provider_principal_cash_million), 12.0) &&
            near(lower(result.total_provider_unpaid_payable_claim_million), 0.0) &&
            near(upper(result.total_provider_unpaid_payable_claim_million), 8.0) &&
            near(lower(result.total_provider_claim_payable_after_horizon_million), 0.0) &&
            near(upper(result.total_provider_claim_payable_after_horizon_million), 8.0) &&
            result.total_final_principal_writeoff_million.status ==
                cf::ClaimLedgerValueStatus::Unknown,
        "resolved cash stays exact while provider caps remain finite and uncapped lifetime open losses stay unknown");
    check(near(lower(result.positive_pre_support_shortfall_frequency), 0.5) &&
            near(upper(result.positive_pre_support_shortfall_frequency), 0.75) &&
            near(lower(result.positive_provider_cash_frequency), 0.25) &&
            near(upper(result.positive_provider_cash_frequency), 0.75) &&
            near(lower(result.positive_final_writeoff_frequency), 0.25) &&
            near(upper(result.positive_final_writeoff_frequency), 0.75),
        "frequency identification uses the fixed included denominator and retains censoring");
    check(near(lower(result.provider_cash_to_contractual_face), 0.1) &&
            near(upper(result.provider_cash_to_contractual_face), 0.3) &&
            result.final_writeoff_to_contractual_face.status ==
                cf::ClaimLedgerValueStatus::Unknown,
        "only the finitely capped provider numerator forms a face ratio while open cumulative writeoff stays unknown");

    const auto& failed = row(result, "resolved-loss");
    check(failed.resolved_path_exact &&
            failed
                .selected_full_path_provenance_verified_during_evaluation &&
            !failed.empirical_realized_cash_admissible &&
            near(lower(failed.contractual_face_million), 10.0) &&
            near(lower(failed.funded_principal_created_million), 9.0) &&
            near(lower(failed.capitalized_principal_million), 1.0) &&
            near(lower(failed.principal_rollforward_basis_million), 10.0) &&
            near(lower(failed.borrower_principal_cash_million), 0.0) &&
            near(lower(failed.recovery_principal_cash_million), 2.0) &&
            near(lower(failed.pre_support_principal_shortfall_million), 8.0) &&
            near(lower(failed.provider_claim_generated_million), 4.0) &&
            near(lower(failed.provider_claim_payable_million), 4.0) &&
            near(lower(failed.provider_principal_cash_million), 4.0) &&
            near(lower(failed.final_principal_writeoff_million), 4.0),
        "resolved failure preserves shortfall, provider claim, payment and residual writeoff separately");
    check(std::is_sorted(result.observations.begin(), result.observations.end(),
            [](const auto& left, const auto& right) {
                return left.observation_id < right.observation_id;
            }),
        "cohort observation results are canonical by observation ID");
}

void test_reverification_ignores_mutable_caller_summary(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second,
    const cf::ClaimLedgerPackage& rare,
    const cf::ClaimLedgerPackage& shifted) {
    auto package = base_package(first, second, rare, shifted);
    package.observations.front().claim_package->config.claim_id =
        "caller-forged-claim";
    package.observations.front().claim_package->full_evaluation.reset();
    const auto result = cf::evaluate_partial_credit_claim_loss_cohort(package);
    check(row(result, "resolved-performing").claim_id ==
            first.config.claim_id,
        "cohort evaluation reloads the verified root instead of trusting mutable caller summaries");

    package = base_package(first, second, rare, shifted);
    package.observations.front().expected_claim_config_sha256 =
        std::string(64U, '0');
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "root changed", "claim root mismatch fails closed");
}

void test_resolved_requires_explicit_complete_path_status(
    const cf::ClaimLedgerPackage& source) {
    TemporaryClaimFixture fixture(source.directory);
    std::string scenarios = read_text(fixture.root / "scenarios.tsv");
    replace_once(scenarios,
        "performing-maturity\tknown\t0.8\t0.8\t0\tstress\tSYNTHETIC\t"
        "complete-resolved\t0\tderived\tSYNTHETIC",
        "performing-maturity\tknown\t0.8\t0.8\t0\tstress\tSYNTHETIC\t"
        "incomplete\t0\tderived\tSYNTHETIC");
    write_text(fixture.root / "scenarios.tsv", scenarios);
    refresh_bound_hash(
        fixture.root, "file.scenarios.sha256", "scenarios.tsv");
    const cf::ClaimLedgerPackage incomplete =
        cf::load_claim_ledger_package(fixture.root / "claim.cfg");

    cf::PartialCreditClaimLossCohortPackage package;
    package.config = base_config();
    package.config.population_frame_count = 1U;
    package.config.exclusion_rules.clear();
    package.observations = {included_observation(
        "resolved-with-incomplete-attestation", incomplete,
        cf::PartialCreditClaimLossDisposition::Resolved,
        cf::PartialCreditClaimTriggerStatus::NotTriggered,
        "2027-01-01", "performing-maturity",
        "synthetic-provider-claim")};
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "explicit complete-resolved cash-path status",
        "exact mechanics cannot make an explicitly incomplete cash path a resolved cohort row");
}

void test_fail_closed_population_controls(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second,
    const cf::ClaimLedgerPackage& rare,
    const cf::ClaimLedgerPackage& shifted) {
    auto package = base_package(first, second, rare, shifted);
    package.config.population_frame_count = 4U;
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "row count", "a declared frame-count and row-count mismatch is rejected");

    package = base_package(first, second, rare, shifted);
    package.observations[1U].economic_cluster_id =
        package.observations.front().economic_cluster_id;
    package.observations[1U].claim_package->config.economic_cluster_id =
        package.observations.front().economic_cluster_id;
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "unique economic cluster",
        "unconsolidated duplicate economic risk units are rejected across the frame");

    package = base_package(first, second, rare, shifted);
    package.config.exclusion_rules.front().frozen_date = "2026-02-01";
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "timely outcome-blind rule",
        "an exclusion frozen after eligibility is rejected");

    package = base_package(first, second, rare, shifted);
    package.observations.front().trigger_status =
        cf::PartialCreditClaimTriggerStatus::Triggered;
    package.observations.front().trigger_date = "2027-01-01";
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "trigger status contradicts",
        "resolved trigger classification must agree with verified claim cash");
}

void test_open_provider_identity_cannot_erase_or_invent_support(
    const cf::ClaimLedgerPackage& shifted) {
    cf::PartialCreditClaimLossCohortPackage package;
    package.config = base_config();
    package.config.population_frame_count = 1U;
    package.config.exclusion_rules.clear();
    package.observations = {included_observation("open-missing-provider",
        shifted, cf::PartialCreditClaimLossDisposition::Unresolved,
        cf::PartialCreditClaimTriggerStatus::Triggered,
        "2027-01-01", "NONE", "missing-provider-claim")};
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "does not match", "an open row cannot invent a provider identity");

    package.observations.front().provider_claim_id = "NONE";
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "omits an applicable", "an open row cannot erase verified protection");
}

void test_contractual_face_is_not_principal_created(
    const cf::ClaimLedgerPackage& source) {
    TemporaryClaimFixture fixture(source.directory);
    std::string config = read_text(fixture.root / "claim.cfg");
    replace_key(config, "claim.contractual_face_amount_lower_million", "12");
    replace_key(config, "claim.contractual_face_amount_upper_million", "12");
    write_text(fixture.root / "claim.cfg", config);
    const cf::ClaimLedgerPackage underdrawn =
        cf::load_claim_ledger_package(fixture.root / "claim.cfg");

    cf::PartialCreditClaimLossCohortPackage package;
    package.config = base_config();
    package.config.population_frame_count = 1U;
    package.config.exclusion_rules.clear();
    package.observations = {included_observation("underdrawn-resolved-loss",
        underdrawn, cf::PartialCreditClaimLossDisposition::Resolved,
        cf::PartialCreditClaimTriggerStatus::Triggered,
        "2027-01-01", "failure-with-provider",
        "synthetic-provider-claim")};
    const auto result = cf::evaluate_partial_credit_claim_loss_cohort(package);
    const auto& resolved = row(result, "underdrawn-resolved-loss");
    check(near(lower(resolved.contractual_face_million), 12.0) &&
            near(lower(resolved.funded_principal_created_million), 9.0) &&
            near(lower(resolved.capitalized_principal_million), 1.0) &&
            near(lower(resolved.principal_rollforward_basis_million), 10.0) &&
            near(lower(resolved.recovery_principal_cash_million), 2.0) &&
            near(lower(resolved.provider_principal_cash_million), 4.0) &&
            near(lower(resolved.final_principal_writeoff_million), 4.0) &&
            near(lower(result.total_contractual_face_million), 12.0) &&
            near(lower(
                result.total_resolved_principal_rollforward_basis_million),
                10.0),
        "undrawn face capacity stays outside actual-principal conservation");
}

void test_unknown_open_provider_amount_widens_frequency(
    const cf::ClaimLedgerPackage& source) {
    TemporaryClaimFixture fixture(source.directory);
    remove_failure_provider_cash(fixture.root);
    std::string providers = read_text(fixture.root / "provider_claims.tsv");
    replace_once(providers, "\tknown\t4\t4\tknown\t0\t0\t",
        "\tunknown\tUNKNOWN\tUNKNOWN\tknown\t0\t0\t");
    write_text(fixture.root / "provider_claims.tsv", providers);
    refresh_bound_hash(fixture.root, "file.provider_claims.sha256",
        "provider_claims.tsv");
    const cf::ClaimLedgerPackage unknown_cap =
        cf::load_claim_ledger_package(fixture.root / "claim.cfg");

    cf::PartialCreditClaimLossCohortPackage package;
    package.config = base_config();
    package.config.population_frame_count = 1U;
    package.config.exclusion_rules.clear();
    package.observations = {included_observation("unknown-open-provider-cap",
        unknown_cap, cf::PartialCreditClaimLossDisposition::Unresolved,
        cf::PartialCreditClaimTriggerStatus::Triggered,
        "2027-01-01", "NONE", "synthetic-provider-claim")};
    const auto result = cf::evaluate_partial_credit_claim_loss_cohort(package);
    check(!result.mechanical_amount_ranges_available &&
            result.total_provider_principal_cash_million.status ==
                cf::ClaimLedgerValueStatus::Unknown &&
            near(lower(result.positive_pre_support_shortfall_frequency), 1.0) &&
            near(upper(result.positive_pre_support_shortfall_frequency), 1.0) &&
            near(lower(result.positive_provider_cash_frequency), 0.0) &&
            near(upper(result.positive_provider_cash_frequency), 1.0) &&
            result.provider_unpaid_claim_known_positive_count == 0U &&
            result.provider_unpaid_claim_possible_positive_count == 1U,
        "an unknown provider amount remains possible in binary bounds instead of acting as zero");
}

void test_late_face_stays_unknown_and_provider_identity_still_fails_closed(
    const cf::ClaimLedgerPackage& source) {
    TemporaryClaimFixture fixture(source.directory);
    std::string config = read_text(fixture.root / "claim.cfg");
    replace_key(config,
        "claim.contractual_face_amount_known_at_period", "1");
    write_text(fixture.root / "claim.cfg", config);
    const cf::ClaimLedgerPackage late_face =
        cf::load_claim_ledger_package(fixture.root / "claim.cfg");

    cf::PartialCreditClaimLossCohortPackage package;
    package.config = base_config();
    package.config.population_frame_count = 1U;
    package.config.exclusion_rules.clear();
    package.observations = {included_observation("late-face-open",
        late_face, cf::PartialCreditClaimLossDisposition::Unresolved,
        cf::PartialCreditClaimTriggerStatus::Triggered,
        "2027-01-01", "NONE", "synthetic-provider-claim")};
    const auto result = cf::evaluate_partial_credit_claim_loss_cohort(package);
    check(!result.mechanical_amount_ranges_available &&
            result.total_contractual_face_million.status ==
                cf::ClaimLedgerValueStatus::Unknown &&
            near(lower(result.total_provider_principal_cash_million), 0.0) &&
            near(upper(result.total_provider_principal_cash_million), 4.0),
        "a face first known after decision stays unknown while an independent exact provider lifetime cap remains visible");

    package.observations.front().provider_claim_id = "NONE";
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "omits an applicable",
        "an unknown face cannot bypass omission of an applicable provider");

    package.observations.front().provider_claim_id = "invented-provider";
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "does not match",
        "an unknown face cannot bypass provider identity validation");
}

void test_zero_coverage_cannot_create_provider_cash(
    const cf::ClaimLedgerPackage& source) {
    TemporaryClaimFixture fixture(source.directory);
    remove_failure_provider_cash(fixture.root);
    std::string providers = read_text(fixture.root / "provider_claims.tsv");
    replace_once(providers, "\tknown\t0.5\t0.5\tknown\t0\t0\t",
        "\tknown\t0\t0\tknown\t0\t0\t");
    write_text(fixture.root / "provider_claims.tsv", providers);
    refresh_bound_hash(fixture.root, "file.provider_claims.sha256",
        "provider_claims.tsv");
    const cf::ClaimLedgerPackage zero_coverage =
        cf::load_claim_ledger_package(fixture.root / "claim.cfg");

    cf::PartialCreditClaimLossCohortPackage package;
    package.config = base_config();
    package.config.population_frame_count = 1U;
    package.config.exclusion_rules.clear();
    package.observations = {included_observation("zero-coverage-open",
        zero_coverage, cf::PartialCreditClaimLossDisposition::Unresolved,
        cf::PartialCreditClaimTriggerStatus::Triggered,
        "2027-01-01", "NONE", "synthetic-provider-claim")};
    const auto result = cf::evaluate_partial_credit_claim_loss_cohort(package);
    check(!result.mechanical_amount_ranges_available &&
            near(lower(result.total_provider_principal_cash_million), 0.0) &&
            near(upper(result.total_provider_principal_cash_million), 0.0) &&
            near(lower(result.positive_provider_cash_frequency), 0.0) &&
            near(upper(result.positive_provider_cash_frequency), 0.0),
        "the provider envelope applies coverage and cannot manufacture cash from a zero-coverage term");

    providers = read_text(fixture.root / "provider_claims.tsv");
    replace_once(providers, "\tknown\t4\t4\tknown\t0\t0\t",
        "\tunknown\tUNKNOWN\tUNKNOWN\tknown\t0\t0\t");
    write_text(fixture.root / "provider_claims.tsv", providers);
    refresh_bound_hash(fixture.root, "file.provider_claims.sha256",
        "provider_claims.tsv");
    const cf::ClaimLedgerPackage zero_coverage_unknown_cap =
        cf::load_claim_ledger_package(fixture.root / "claim.cfg");
    package.observations = {included_observation(
        "zero-coverage-unknown-cap", zero_coverage_unknown_cap,
        cf::PartialCreditClaimLossDisposition::Unresolved,
        cf::PartialCreditClaimTriggerStatus::Triggered,
        "2027-01-01", "NONE", "synthetic-provider-claim")};
    const auto unknown_cap_result =
        cf::evaluate_partial_credit_claim_loss_cohort(package);
    check(near(lower(
                   unknown_cap_result.total_provider_principal_cash_million),
              0.0) &&
            near(upper(
                   unknown_cap_result.total_provider_principal_cash_million),
              0.0) &&
            near(upper(unknown_cap_result.positive_provider_cash_frequency),
                0.0),
        "exact zero coverage proves zero payout even when an unrelated provider cap is unknown");
}

void test_triggered_zero_face_open_claim_is_rejected(
    const cf::ClaimLedgerPackage& source) {
    TemporaryClaimFixture fixture(source.directory);
    make_zero_draw_fixture(fixture.root);
    const cf::ClaimLedgerPackage zero_draw =
        cf::load_claim_ledger_package(fixture.root / "claim.cfg");

    cf::PartialCreditClaimLossCohortPackage package;
    package.config = base_config();
    package.config.population_frame_count = 1U;
    package.config.exclusion_rules.clear();
    package.observations = {included_observation("zero-face-triggered-open",
        zero_draw, cf::PartialCreditClaimLossDisposition::Unresolved,
        cf::PartialCreditClaimTriggerStatus::Triggered,
        "2027-01-01", "NONE", "synthetic-provider-claim")};
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "zero contractual face",
        "a positive-shortfall trigger cannot coexist with a zero mechanical loss cap");

    package.observations = {included_observation("zero-face-immature",
        zero_draw, cf::PartialCreditClaimLossDisposition::NotYetMatured,
        cf::PartialCreditClaimTriggerStatus::Unknown,
        "2028-01-01", "NONE", "synthetic-provider-claim")};
    const auto result = cf::evaluate_partial_credit_claim_loss_cohort(package);
    check(result.mechanical_amount_ranges_available &&
            near(lower(result.total_pre_support_principal_shortfall_million),
                0.0) &&
            near(upper(result.total_pre_support_principal_shortfall_million),
                0.0) &&
            near(lower(result.total_provider_principal_cash_million), 0.0) &&
            near(upper(result.total_provider_principal_cash_million), 0.0) &&
            near(lower(result.total_final_principal_writeoff_million), 0.0) &&
            near(upper(result.total_final_principal_writeoff_million), 0.0) &&
            near(lower(result.positive_pre_support_shortfall_frequency), 0.0) &&
            near(upper(result.positive_pre_support_shortfall_frequency), 0.0) &&
            near(lower(result.positive_provider_cash_frequency), 0.0) &&
            near(upper(result.positive_provider_cash_frequency), 0.0) &&
            near(lower(result.positive_final_writeoff_frequency), 0.0) &&
            near(upper(result.positive_final_writeoff_frequency), 0.0),
        "an untriggered zero-draw open claim remains a genuine zero rather than unidentified loss");

    std::string providers = read_text(fixture.root / "provider_claims.tsv");
    replace_once(providers, "\tknown\t0.5\t0.5\tknown\t0\t0\t",
        "\tunknown\tUNKNOWN\tUNKNOWN\tunknown\tUNKNOWN\tUNKNOWN\t");
    replace_once(providers, "\tknown\t4\t4\tknown\t0\t0\t",
        "\tunknown\tUNKNOWN\tUNKNOWN\tunknown\tUNKNOWN\tUNKNOWN\t");
    write_text(fixture.root / "provider_claims.tsv", providers);
    refresh_bound_hash(fixture.root, "file.provider_claims.sha256",
        "provider_claims.tsv");
    const cf::ClaimLedgerPackage zero_draw_unknown_terms =
        cf::load_claim_ledger_package(fixture.root / "claim.cfg");
    package.observations = {included_observation(
        "zero-face-unknown-provider-terms", zero_draw_unknown_terms,
        cf::PartialCreditClaimLossDisposition::NotYetMatured,
        cf::PartialCreditClaimTriggerStatus::Unknown,
        "2028-01-01", "NONE", "synthetic-provider-claim")};
    const auto unknown_terms_result =
        cf::evaluate_partial_credit_claim_loss_cohort(package);
    check(unknown_terms_result.mechanical_amount_ranges_available &&
            near(lower(
                unknown_terms_result.total_provider_principal_cash_million),
                0.0) &&
            near(upper(
                unknown_terms_result.total_provider_principal_cash_million),
                0.0) &&
            near(upper(unknown_terms_result
                    .positive_pre_support_shortfall_frequency),
                0.0) &&
            near(upper(unknown_terms_result.positive_provider_cash_frequency),
                0.0) &&
            near(upper(
                unknown_terms_result.positive_final_writeoff_frequency),
                0.0),
        "exact zero face proves zero loss and payout despite unrelated unknown provider numerics");
}

void test_exact_small_contractual_values_are_not_materiality_zeros(
    const cf::ClaimLedgerPackage& source) {
    TemporaryClaimFixture fixture(source.directory);
    make_zero_draw_fixture(fixture.root);
    std::string config = read_text(fixture.root / "claim.cfg");
    replace_key(config, "claim.contractual_face_amount_lower_million",
        "0.00000000001");
    replace_key(config, "claim.contractual_face_amount_upper_million",
        "0.00000000001");
    write_text(fixture.root / "claim.cfg", config);

    std::string providers = read_text(fixture.root / "provider_claims.tsv");
    replace_once(providers,
        "synthetic-catalytic-provider\t0\tknown\t1\t1\tknown\t0.5\t0.5\t",
        "synthetic-catalytic-provider\t0\tknown\t0.00000000001\t"
        "0.00000000001\tknown\t0.5\t0.5\t");
    replace_once(providers, "\tknown\t4\t4\tknown\t0\t0\t",
        "\tknown\t0.00000000001\t0.00000000001\tknown\t0\t0\t");
    write_text(fixture.root / "provider_claims.tsv", providers);
    refresh_bound_hash(fixture.root, "file.provider_claims.sha256",
        "provider_claims.tsv");
    const cf::ClaimLedgerPackage small_positive =
        cf::load_claim_ledger_package(fixture.root / "claim.cfg");

    cf::PartialCreditClaimLossCohortPackage package;
    package.config = base_config();
    package.config.population_frame_count = 1U;
    package.config.exclusion_rules.clear();
    package.observations = {included_observation("small-positive-open",
        small_positive,
        cf::PartialCreditClaimLossDisposition::NotYetMatured,
        cf::PartialCreditClaimTriggerStatus::Unknown,
        "2028-01-01", "NONE", "synthetic-provider-claim")};
    const auto result = cf::evaluate_partial_credit_claim_loss_cohort(package);
    check(!result.mechanical_amount_ranges_available &&
            near(lower(result.total_contractual_face_million), 1.0e-11,
                1.0e-14) &&
            result.total_pre_support_principal_shortfall_million.status ==
                cf::ClaimLedgerValueStatus::Unknown &&
            near(lower(result.total_provider_principal_cash_million), 0.0) &&
            near(upper(result.total_provider_principal_cash_million), 1.0e-11,
                1.0e-14) &&
            result.total_final_principal_writeoff_million.status ==
                cf::ClaimLedgerValueStatus::Unknown &&
            near(lower(result.positive_pre_support_shortfall_frequency), 0.0) &&
            near(upper(result.positive_pre_support_shortfall_frequency), 1.0) &&
            near(lower(result.positive_provider_cash_frequency), 0.0) &&
            near(upper(result.positive_provider_cash_frequency), 1.0),
        "exact positive contractual values are never collapsed to materiality zeros");
}

void test_metadata_and_resource_guards(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second,
    const cf::ClaimLedgerPackage& rare,
    const cf::ClaimLedgerPackage& shifted) {
    auto package = base_package(first, second, rare, shifted);
    package.observations.front().disposition =
        static_cast<cf::PartialCreditClaimLossDisposition>(255U);
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "invalid disposition", "invalid disposition enums fail closed");

    package = base_package(first, second, rare, shifted);
    package.observations.front().trigger_status =
        cf::PartialCreditClaimTriggerStatus::NotApplicable;
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "not-applicable trigger", "included rows cannot escape the trigger denominator");

    package = base_package(first, second, rare, shifted);
    package.observations[2U].trigger_date = "2025-12-31";
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "chronologically ordered", "trigger dates cannot predate eligibility");

    package = base_package(first, second, rare, shifted);
    package.observations[3U].trigger_status =
        cf::PartialCreditClaimTriggerStatus::Triggered;
    package.observations[3U].trigger_date = "2027-01-01";
    package.observations[3U].classification_date = "2027-05-01";
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "must be unresolved", "triggered open rows cannot remain labeled immature");

    package = base_package(first, second, rare, shifted);
    package.observations.back().economic_cluster_id =
        package.observations.front().economic_cluster_id;
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "unique economic cluster", "excluded rows cannot duplicate a frame risk unit");

    package = base_package(first, second, rare, shifted);
    package.observations[1U].observation_id =
        package.observations.front().observation_id;
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "observation IDs", "duplicate observation IDs fail closed");

    package = base_package(first, second, rare, shifted);
    package.config.currency_label = "OTHER";
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "currency or monetary basis", "currency stratum mismatches fail closed");

    package = base_package(first, second, rare, shifted);
    auto& ids = package.observations.front().population_evidence_record_ids;
    ids.clear();
    for (std::size_t index = 0U;
         index <= cf::kMaximumPartialCreditCohortEvidenceIdsPerList;
         ++index) {
        ids.push_back("evidence-" + std::to_string(index));
    }
    std::sort(ids.begin(), ids.end());
    expect_exception([&] {
        (void)cf::evaluate_partial_credit_claim_loss_cohort(package);
    }, "per-list resource guardrail", "evidence lists are bounded before retention");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: partial-credit-claim-loss-cohort-tests "
                     "<first-claim.cfg> <second-claim.cfg> "
                     "<rare-claim.cfg> <shifted-claim.cfg>\n";
        return 2;
    }
    try {
        const cf::ClaimLedgerPackage first =
            cf::load_claim_ledger_package(std::filesystem::path(argv[1]));
        const cf::ClaimLedgerPackage second =
            cf::load_claim_ledger_package(std::filesystem::path(argv[2]));
        const cf::ClaimLedgerPackage rare =
            cf::load_claim_ledger_package(std::filesystem::path(argv[3]));
        const cf::ClaimLedgerPackage shifted =
            cf::load_claim_ledger_package(std::filesystem::path(argv[4]));
        test_complete_candidate_book(first, second, rare, shifted);
        test_reverification_ignores_mutable_caller_summary(
            first, second, rare, shifted);
        test_resolved_requires_explicit_complete_path_status(first);
        test_fail_closed_population_controls(
            first, second, rare, shifted);
        test_open_provider_identity_cannot_erase_or_invent_support(shifted);
        test_contractual_face_is_not_principal_created(first);
        test_unknown_open_provider_amount_widens_frequency(shifted);
        test_late_face_stays_unknown_and_provider_identity_still_fails_closed(
            shifted);
        test_zero_coverage_cannot_create_provider_cash(shifted);
        test_triggered_zero_face_open_claim_is_rejected(shifted);
        test_exact_small_contractual_values_are_not_materiality_zeros(
            shifted);
        test_metadata_and_resource_guards(first, second, rare, shifted);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << failures
                  << " partial-credit cohort test(s) failed\n";
        return 1;
    }
    std::cout << "all partial-credit cohort tests passed\n";
    return 0;
}
