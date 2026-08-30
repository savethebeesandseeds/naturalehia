// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>
#include <naturalehia/cellular_finance/probability_polytope_config.hpp>
#include <naturalehia/cellular_finance/robust_issue_price_support.hpp>
#include <naturalehia/cellular_finance/robust_issue_price_support_config.hpp>
#include <naturalehia/cellular_finance/robust_market_priority_cap_config.hpp>
#include <naturalehia/cellular_finance/success_participation_config.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

void print_usage(std::string_view program) {
    std::cerr
        << "usage: " << program
        << " <portfolio.cfg> <event-polytope.cfg> "
           "<success-participation.cfg> <base-capital-stack.cfg> "
           "<market-priority-cap.cfg> <issue-price.cfg> "
           "[--print-normalized]\n"
        << "calibrated_execution_authorized=false\n";
}

[[nodiscard]] std::string_view bool_text(bool value) noexcept {
    return value ? "true" : "false";
}

[[nodiscard]] double display_value(double value) noexcept {
    return std::abs(value) < 0.5e-6 ? 0.0 : value;
}

void print_index_list(const std::vector<std::size_t>& indices) {
    if (indices.empty()) {
        std::cout << "none";
        return;
    }
    for (std::size_t position = 0U; position < indices.size(); ++position) {
        if (position != 0U) {
            std::cout << ',';
        }
        std::cout << indices[position];
    }
}

void print_optional_value(std::string_view label,
    const std::optional<double>& value, std::string_view unit) {
    std::cout << "  " << label << ": ";
    if (!value.has_value()) {
        std::cout << "not applicable\n";
        return;
    }
    std::cout << display_value(*value);
    if (!unit.empty()) {
        std::cout << ' ' << unit;
    }
    std::cout << '\n';
}

void print_range(std::string_view label,
    const cf::ProbabilityPolytopeMetricRange& range,
    std::string_view unit, double scale = 1.0) {
    std::cout << "  " << label << " | minimum="
              << display_value(range.minimum.value * scale)
              << " | central=" << display_value(range.central * scale)
              << " | maximum="
              << display_value(range.maximum.value * scale);
    if (!unit.empty()) {
        std::cout << ' ' << unit;
    }
    std::cout << '\n';
}

void print_optional_range(std::string_view label,
    const std::optional<cf::ProbabilityPolytopeMetricRange>& range,
    std::string_view unit, double scale = 1.0) {
    if (!range.has_value()) {
        std::cout << "  " << label << ": not applicable\n";
        return;
    }
    print_range(label, *range, unit, scale);
}

void print_tail_range(std::string_view label,
    const cf::ProbabilityPolytopeUpperExpectedShortfallProjection& tail,
    std::string_view unit) {
    std::cout << "  " << label << " | minimum="
              << display_value(tail.minimum.value)
              << " | central=" << display_value(tail.central)
              << " | maximum=" << display_value(tail.maximum.value);
    if (!unit.empty()) {
        std::cout << ' ' << unit;
    }
    std::cout << " | tail mass=" << tail.tail_probability << '\n';
}

void print_probability_vector(const std::vector<double>& weights,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    if (weights.size() != scenarios.size()) {
        throw std::logic_error(
            "issue-price-support endpoint witness has the wrong scenario dimension");
    }
    for (std::size_t index = 0U; index < weights.size(); ++index) {
        if (index != 0U) {
            std::cout << ';';
        }
        std::cout << scenarios[index].scenario_id << '=' << weights[index];
    }
}

void print_linear_witness(std::size_t case_index,
    std::string_view metric, std::string_view endpoint_name,
    const cf::ProbabilityPolytopeEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    std::cout << "  hurdle case " << case_index << " | " << metric
              << " | " << endpoint_name << " | value="
              << display_value(endpoint.value) << " | own physical p: ";
    print_probability_vector(endpoint.scenario_weights, scenarios);
    std::cout << '\n';
}

void print_range_witnesses(std::size_t case_index,
    std::string_view metric, const cf::ProbabilityPolytopeMetricRange& range,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    print_linear_witness(
        case_index, metric, "minimum", range.minimum, scenarios);
    print_linear_witness(
        case_index, metric, "maximum", range.maximum, scenarios);
}

void print_tail_witness(std::size_t case_index, std::string_view metric,
    std::string_view endpoint_name,
    const cf::ProbabilityPolytopeUpperExpectedShortfallEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios,
    std::string_view currency) {
    std::cout << "  hurdle case " << case_index << " | " << metric
              << " | " << endpoint_name << " | value="
              << display_value(endpoint.value) << ' ' << currency
              << " million | own physical p: ";
    print_probability_vector(endpoint.scenario_weights, scenarios);
    std::cout << " | own tail mass y: ";
    print_probability_vector(endpoint.tail_mass_weights, scenarios);
    std::cout << '\n';
}

void print_tail_witnesses(std::size_t case_index,
    std::string_view metric,
    const cf::ProbabilityPolytopeUpperExpectedShortfallProjection& tail,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios,
    std::string_view currency) {
    print_tail_witness(
        case_index, metric, "minimum", tail.minimum, scenarios, currency);
    print_tail_witness(
        case_index, metric, "maximum", tail.maximum, scenarios, currency);
}

void print_wal_witness(std::size_t case_index,
    std::string_view endpoint_name,
    const cf::CapitalStackProbabilityPolytopeWalEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    std::cout << "  hurdle case " << case_index
              << " | market principal-cash WAL | " << endpoint_name
              << " | value=" << endpoint.value_years
              << " | numerator=" << endpoint.numerator_million_years
              << " | denominator=" << endpoint.denominator_million
              << " | own common-measure physical p: ";
    print_probability_vector(endpoint.scenario_weights, scenarios);
    std::cout << '\n';
}

void print_input_evidence_labels(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& base_stack,
    const cf::RobustMarketPriorityCapConfig& priority_cap,
    const cf::RobustIssuePriceSupportConfig& issue_price) {
    const bool all_synthetic = portfolio.synthetic_inputs &&
        polytope.synthetic_inputs && participation.synthetic_inputs &&
        base_stack.synthetic_inputs && priority_cap.synthetic_inputs &&
        issue_price.synthetic_inputs;
    std::cout << "Input evidence labels\n"
              << "  portfolio.synthetic_inputs="
              << bool_text(portfolio.synthetic_inputs) << '\n'
              << "  event_polytope.synthetic_inputs="
              << bool_text(polytope.synthetic_inputs) << '\n'
              << "  success_participation.synthetic_inputs="
              << bool_text(participation.synthetic_inputs) << '\n'
              << "  base_capital_stack.synthetic_inputs="
              << bool_text(base_stack.synthetic_inputs) << '\n'
              << "  market_priority_cap.synthetic_inputs="
              << bool_text(priority_cap.synthetic_inputs) << '\n'
              << "  issue_price_support.synthetic_inputs="
              << bool_text(issue_price.synthetic_inputs) << '\n'
              << "  all_fixture_inputs_are_synthetic="
              << bool_text(all_synthetic)
              << "\n  A synthetic label is an evidence boundary, not a "
                 "calibration or transaction fact.\n\n";
}

void print_fixed_terms(const cf::PortfolioConfig& portfolio,
    const cf::RobustIssuePriceSupportConfig& terms,
    const cf::RobustIssuePriceSupportSummary& summary) {
    const std::string_view currency = portfolio.currency_label;
    std::cout
        << "Fixed claim, issue sources, and selected priority cap\n"
        << "  overall status: " << cf::to_string(summary.status) << '\n'
        << "  upstream priority-cap status: "
        << cf::to_string(summary.upstream_priority_cap_status) << '\n'
        << "  fixed underlying success participation q: "
        << summary.fixed_underlying_success_participation_fraction << '\n'
        << "  fixed junior first-loss capital A: "
        << summary.fixed_junior_first_loss_million << ' ' << currency
        << " million\n"
        << "  aggregate commitment and stack detachment K: "
        << summary.aggregate_commitment_and_stack_detachment_million << ' '
        << currency << " million\n"
        << "  fixed market claim principal M=K-A: "
        << summary.fixed_market_notional_million << ' ' << currency
        << " million\n"
        << "  selected market lifetime priority non-principal cap B: ";
    if (summary.selected_market_priority_nonprincipal_cap_million.has_value()) {
        std::cout << *summary.selected_market_priority_nonprincipal_cap_million
                  << ' ' << currency << " million | candidate "
                  << *summary.selected_priority_cap_candidate_index << '\n';
    } else {
        std::cout << "not available (upstream selection unavailable)\n";
    }
    std::cout
        << "  selected priority cap is balanced: "
        << bool_text(summary.selected_priority_cap_is_balanced) << '\n'
        << "  reference record id: " << terms.reference_price.record_id
        << '\n'
        << "  reference status: "
        << cf::to_string(summary.reference_price_status) << '\n'
        << "  normalized term/result id: "
        << terms.reference_price.normalized_term_result_id << '\n'
        << "  settled-secondary full-claim month-zero normalization "
           "asserted: "
        << bool_text(terms.reference_price
                         .secondary_price_normalized_to_full_month_zero_claim)
        << '\n'
        << "  gross buyer issue-price input P: "
        << summary.reference_gross_issue_price_million << ' ' << currency
        << " million\n"
        << "  issuer cost F: " << summary.issuer_cost_million << ' '
        << currency << " million\n"
        << "  buyer-direct cost C outside the reserve: "
        << summary.buyer_direct_cost_million << ' ' << currency
        << " million\n"
        << "  maximum non-repayable support capacity G: "
        << summary.maximum_issue_support_million << ' ' << currency
        << " million\n"
        << "  actual settled issue support S_obs: "
        << summary.settled_issue_support_million << ' ' << currency
        << " million\n"
        << "  issuer funding floor max(0,M+F-G): "
        << summary.issuer_funding_floor_million << ' ' << currency
        << " million\n"
        << "  support status: "
        << cf::to_string(summary.support_capacity_status) << '\n'
        << "  reference buyer cash payment evidenced: "
        << bool_text(terms.reference_price.buyer_cash_payment_evidenced)
        << '\n'
        << "  reference settlement evidenced: "
        << bool_text(terms.reference_price.settlement_evidenced) << '\n'
        << "  subscription-reserve deposit evidenced: "
        << bool_text(terms.reference_price
                         .subscription_reserve_deposit_evidenced)
        << '\n'
        << "  issuer-cost payment evidenced: "
        << bool_text(terms.reference_price.issuer_cost_payment_evidenced)
        << '\n'
        << "  issue-use evidence record id: "
        << terms.reference_price.issue_use_evidence_record_id << '\n'
        << "  support funding evidenced: "
        << bool_text(terms.support.funding_evidenced) << '\n'
        << "  support settlement to this issue evidenced: "
        << bool_text(terms.support.settlement_evidenced)
        << "\n  P, F, C, G, and S_obs are supplied month-zero facts or "
           "sensitivities. They do not alter future contractual claim cash "
           "or the physical probability set.\n\n";
}

void print_work_and_summary_invariants(
    const cf::RobustIssuePriceSupportSummary& summary) {
    std::cout
        << "Finite work and cross-case invariants\n"
        << "  raw portfolio cash records: "
        << summary.portfolio_cash_record_count << '\n'
        << "  raw portfolio auxiliary records: "
        << summary.portfolio_auxiliary_record_count << '\n'
        << "  raw portfolio records: " << summary.portfolio_record_count
        << '\n'
        << "  upstream priority-cap work units: "
        << summary.upstream_priority_cap_work_units << '\n'
        << "  hurdle-stack work units: "
        << summary.hurdle_stack_work_units << '\n'
        << "  reference-projection work units: "
        << summary.reference_projection_work_units << '\n'
        << "  scenario-month audit work units: "
        << summary.scenario_month_audit_work_units << '\n'
        << "  combined structural work: " << summary.structural_work_units
        << " / " << summary.structural_work_unit_limit << '\n'
        << "  base stack was not mutated: "
        << bool_text(summary.base_stack_was_not_mutated) << '\n'
        << "  only the market hurdle changed across cases: "
        << bool_text(summary.only_market_hurdle_changed_across_cases)
        << '\n'
        << "  every contractual-cash and principal-risk invariant holds: "
        << bool_text(
               summary.all_contractual_cash_and_principal_risk_invariants_hold)
        << "\n  modeled financeable hurdle-case indices: ";
    print_index_list(summary.financeable_hurdle_case_indices);
    std::cout << "\n  funded/escrowed-support-covered hurdle-case indices: ";
    print_index_list(summary.funded_support_covered_hurdle_case_indices);
    std::cout << "\n  no-nonnegative-price hurdle-case indices: ";
    print_index_list(summary.no_nonnegative_price_hurdle_case_indices);
    std::cout << "\n  literal-zero hurdle-case index: ";
    if (summary.literal_zero_hurdle_case_index.has_value()) {
        std::cout << *summary.literal_zero_hurdle_case_index;
    } else {
        std::cout << "none";
    }
    std::cout << "\n\n";
}

void print_reference_metrics(
    const cf::RobustIssuePriceSupportReferenceMetrics& reference,
    std::string_view currency) {
    const std::string money = std::string(currency) + " million";
    std::cout << "  Reference-price hypothetical-primary projection\n";
    print_range("expected investor contributions",
        reference.expected_investor_contributions_million, money);
    print_range("expected investor distributions",
        reference.expected_distributions_million, money);
    print_range("investor NPV at P and C",
        reference.investor_npv_million, money);
    print_optional_range("expected scenario NPV margin",
        reference.expected_scenario_npv_margin_fraction, "percent", 100.0);
    print_optional_range("expected scenario cash multiple",
        reference.expected_scenario_cash_multiple, "multiple");
    print_optional_range("expected scenario net return",
        reference.expected_scenario_net_return_fraction, "percent", 100.0);
    print_range("negative-NPV probability",
        reference.negative_npv_probability, "percent", 100.0);
    print_tail_range("NPV-shortfall ES95", reference.npv_shortfall_es95_million,
        money);
    print_tail_range("NPV-shortfall ES99", reference.npv_shortfall_es99_million,
        money);
    std::cout
        << "  robust investor NPV: "
        << display_value(reference.robust_investor_npv_million) << ' '
        << money << '\n'
        << "  investor term adequate at supplied independent hurdle: "
        << bool_text(reference.investor_term_adequate) << '\n'
        << "  modeled full-funding capacity adequate: "
        << bool_text(reference.modeled_full_funding_adequate) << '\n'
        << "  modeled joint investor/funding term adequate: "
        << bool_text(reference.modeled_joint_term_adequate) << '\n'
        << "  modeled required support S_req=M+F-P: "
        << reference.required_issue_support_million << ' ' << money << '\n'
        << "  actual observed settled support S_obs: "
        << reference.observed_settled_support_million << ' ' << money << '\n'
        << "  support capacity margin G-S_req: "
        << reference.support_capacity_margin_million << ' ' << money << '\n'
        << "  unused support capacity: "
        << reference.unused_support_capacity_million << ' ' << money << '\n'
        << "  support capacity shortfall: "
        << reference.support_capacity_shortfall_million << ' ' << money
        << '\n'
        << "  modeled issue sources P+S_req: "
        << reference.modeled_required_issue_sources_million << ' ' << money
        << '\n'
        << "  issue uses M+F: " << reference.issue_uses_million << ' '
        << money << '\n'
        << "  modeled amount entering subscription reserve: "
        << reference.modeled_amount_entering_subscription_reserve_million
        << ' ' << money << '\n'
        << "  modeled issuer cost paid: "
        << reference.modeled_issuer_cost_paid_million << ' ' << money << '\n'
        << "  buyer-direct cost outside reserve: "
        << reference.buyer_direct_cost_outside_reserve_million << ' '
        << money << '\n'
        << "  modeled funding identity error: "
        << reference.issue_funding_identity_error_million << ' ' << money
        << '\n'
        << "  reference is settled primary: "
        << bool_text(reference.reference_is_settled_primary) << '\n'
        << "  reference is settled secondary: "
        << bool_text(reference.reference_is_settled_secondary) << '\n'
        << "  observed primary buyer cash completed: "
        << bool_text(reference.observed_primary_price_cash_completed) << '\n'
        << "  observed support cash completed: "
        << bool_text(reference.observed_support_cash_completed) << '\n'
        << "  observed issue sources settled and modeled identity "
           "reconciled: "
        << bool_text(
               reference.observed_issue_sources_settled_and_reconciled)
        << '\n'
        << "  observed settled primary funding completed: "
        << bool_text(reference.observed_primary_funding_completed) << '\n'
        << "  observed primary buyer cash: "
        << reference.observed_primary_buyer_cash_million << ' ' << money
        << '\n'
        << "  observed issue-support cash: "
        << reference.observed_issue_support_cash_million << ' ' << money
        << '\n';
    print_optional_value("observed issue sources",
        reference.observed_issue_sources_million, money);
    print_optional_value("observed issue funding identity error",
        reference.observed_issue_funding_identity_error_million, money);
    print_optional_value("observed amount entering subscription reserve",
        reference.observed_amount_entering_subscription_reserve_million,
        money);
    print_optional_value("observed issuer cost paid",
        reference.observed_issuer_cost_paid_million, money);
}

void print_principal_risk(
    const cf::RobustIssuePriceSupportPrincipalRiskMetrics& risk,
    std::string_view currency) {
    const std::string money = std::string(currency) + " million";
    std::cout << "  Fixed future-cash physical principal risk\n"
              << "  contractual market principal notional M: "
              << risk.contractual_market_notional_million << ' ' << money
              << '\n';
    print_range("expected principal cash distribution",
        risk.expected_principal_cash_distribution_million, money);
    print_range("expected principal-loss fraction",
        risk.expected_principal_loss_fraction, "percent of M", 100.0);
    print_tail_range(
        "principal-loss ES95", risk.principal_loss_es95_million, money);
    print_tail_range(
        "principal-loss ES99", risk.principal_loss_es99_million, money);
    std::cout
        << "  worst principal-loss ES95 fraction: "
        << risk.worst_principal_loss_es95_fraction * 100.0
        << " percent of M\n"
        << "  worst principal-loss ES99 fraction: "
        << risk.worst_principal_loss_es99_fraction * 100.0
        << " percent of M\n";
    print_range("principal impairment probability",
        risk.principal_impairment_probability, "percent", 100.0);
    if (risk.principal_cash_wal_years.has_value()) {
        const auto& wal = *risk.principal_cash_wal_years;
        std::cout << "  principal-cash WAL | minimum="
                  << wal.minimum.value_years << " | central="
                  << wal.central_years << " | maximum="
                  << wal.maximum.value_years << " years\n";
    } else {
        std::cout << "  principal-cash WAL: not applicable\n";
    }
}

void print_case_audit(const cf::RobustIssuePriceSupportCaseAudit& audit,
    std::string_view currency) {
    std::cout
        << "  Case invariants\n"
        << "  physical probability polytope unchanged: "
        << bool_text(audit.physical_probability_polytope_is_unchanged) << '\n'
        << "  market contractual cash unchanged: "
        << bool_text(audit.market_contractual_cash_is_unchanged) << '\n'
        << "  sparse market monthly ledger reconciles: "
        << bool_text(audit.sparse_market_monthly_ledger_reconciles) << '\n'
        << "  market principal risk unchanged: "
        << bool_text(audit.market_principal_risk_is_unchanged) << '\n'
        << "  market principal WAL unchanged: "
        << bool_text(audit.market_principal_wal_is_unchanged) << '\n'
        << "  junior cash and own-hurdle NPV unchanged: "
        << bool_text(audit.junior_cash_and_own_hurdle_npv_are_unchanged)
        << '\n'
        << "  raw price ceiling zero-NPV identity reconciles: "
        << bool_text(audit.raw_price_ceiling_zero_npv_reconciles) << '\n'
        << "  reference-price NPV shift reconciles: "
        << bool_text(audit.reference_price_npv_shift_reconciles) << '\n'
        << "  issue funding identity reconciles: "
        << bool_text(audit.issue_funding_identity_reconciles) << '\n'
        << std::scientific
        << "  maximum contractual cash change: "
        << audit.maximum_contractual_cash_change_million << ' ' << currency
        << " million\n"
        << "  maximum sparse monthly ledger error: "
        << audit.maximum_sparse_monthly_ledger_error_million << ' '
        << currency << " million\n"
        << "  maximum principal-risk change: "
        << audit.maximum_principal_risk_change << '\n'
        << "  maximum market WAL change: "
        << audit.maximum_market_wal_change_years << " years\n"
        << "  maximum junior change: "
        << audit.maximum_junior_change_million << ' ' << currency
        << " million\n"
        << "  raw price-ceiling zero-NPV error: "
        << audit.raw_price_ceiling_zero_npv_error_million << ' ' << currency
        << " million\n"
        << "  maximum reference-price NPV-shift error: "
        << audit.maximum_reference_price_npv_shift_error_million << ' '
        << currency << " million\n"
        << "  issue funding identity error: "
        << audit.issue_funding_identity_error_million << ' ' << currency
        << " million\n"
        << "  probability-constraint violation: "
        << audit.numerical_audit.maximum_probability_constraint_violation
        << '\n'
        << "  objective reconciliation error: "
        << audit.numerical_audit.maximum_objective_reconciliation_error
        << '\n'
        << "  reduced-cost optimality residual: "
        << audit.numerical_audit.maximum_reduced_cost_optimality_residual
        << '\n'
        << "  tail-mass violation: "
        << audit.numerical_audit.maximum_tail_mass_violation << '\n'
        << "  tail objective reconciliation error: "
        << audit.numerical_audit
               .maximum_tail_objective_reconciliation_error
        << '\n'
        << "  WAL ratio reconciliation error: "
        << audit.numerical_audit.maximum_wal_ratio_reconciliation_error_years
        << " years\n"
        << std::fixed;
}

void print_case_witnesses(std::size_t index,
    const cf::RobustIssuePriceSupportCaseResult& result,
    std::string_view currency) {
    const auto& scenarios =
        result.principal_risk.principal_loss_es95_million
            .scenario_probabilities;
    print_range_witnesses(index, "market par NPV",
        result.market_par_npv_million, scenarios);
    print_range_witnesses(index, "market expected principal cash",
        result.principal_risk.expected_principal_cash_distribution_million,
        scenarios);
    print_range_witnesses(index, "market expected principal-loss fraction",
        result.principal_risk.expected_principal_loss_fraction, scenarios);
    print_range_witnesses(index, "market principal impairment probability",
        result.principal_risk.principal_impairment_probability, scenarios);
    print_tail_witnesses(index, "market principal-loss ES95",
        result.principal_risk.principal_loss_es95_million, scenarios,
        currency);
    print_tail_witnesses(index, "market principal-loss ES99",
        result.principal_risk.principal_loss_es99_million, scenarios,
        currency);
    if (result.principal_risk.principal_cash_wal_years.has_value()) {
        print_wal_witness(index, "minimum",
            result.principal_risk.principal_cash_wal_years->minimum,
            scenarios);
        print_wal_witness(index, "maximum",
            result.principal_risk.principal_cash_wal_years->maximum,
            scenarios);
    }
    if (!result.reference_price.has_value()) {
        return;
    }
    const auto& reference = *result.reference_price;
    print_range_witnesses(index, "reference expected contributions",
        reference.expected_investor_contributions_million, scenarios);
    print_range_witnesses(index, "reference expected distributions",
        reference.expected_distributions_million, scenarios);
    print_range_witnesses(index, "reference investor NPV",
        reference.investor_npv_million, scenarios);
    print_range_witnesses(index, "reference negative-NPV probability",
        reference.negative_npv_probability, scenarios);
    print_tail_witnesses(index, "reference NPV-shortfall ES95",
        reference.npv_shortfall_es95_million, scenarios, currency);
    print_tail_witnesses(index, "reference NPV-shortfall ES99",
        reference.npv_shortfall_es99_million, scenarios, currency);
}

void print_hurdle_case(std::size_t index,
    const cf::RobustIssuePriceSupportCaseResult& result,
    std::string_view currency) {
    const std::string money = std::string(currency) + " million";
    std::cout
        << "Hurdle case " << index << " | id=" << result.case_id
        << " | annual effective hurdle="
        << result.annual_effective_hurdle_rate * 100.0
        << " percent | source=" << cf::to_string(result.hurdle_source_type)
        << " | relation="
        << cf::to_string(result.hurdle_reference_price_relation)
        << " | status=" << cf::to_string(result.status) << '\n'
        << "  hurdle as-of date: " << result.hurdle_as_of_date << '\n'
        << "  hurdle source reference: "
        << result.hurdle_source_reference << '\n'
        << "  hurdle evidence record id: "
        << result.hurdle_evidence_record_id << '\n'
        << "  hurdle source note: " << result.hurdle_source_note << '\n';
    print_range("market NPV at par M", result.market_par_npv_million, money);
    std::cout
        << "  raw robust investor price ceiling P*: "
        << display_value(result.raw_robust_investor_price_ceiling_million)
        << ' ' << money << '\n'
        << "  raw central investor price boundary: "
        << display_value(result.raw_central_investor_price_boundary_million)
        << ' ' << money << '\n'
        << "  raw maximum investor price boundary: "
        << display_value(result.raw_maximum_investor_price_boundary_million)
        << ' ' << money << '\n'
        << "  admissible nonnegative investor price ceiling: "
        << display_value(result.admissible_investor_price_ceiling_million)
        << ' ' << money << '\n'
        << "  issuer funding floor: "
        << display_value(result.issuer_funding_floor_million) << ' ' << money
        << '\n'
        << "  modeled conditional financeable window exists: "
        << bool_text(result.modeled_financeable_price_window_exists) << '\n';
    print_optional_value("modeled window lower bound",
        result.financeable_price_window_lower_million, money);
    print_optional_value("modeled window upper bound",
        result.financeable_price_window_upper_million, money);
    print_optional_value("minimum no-rights support for overlap",
        result.minimum_support_capacity_for_overlap_million, money);
    print_optional_value("support capacity shortfall",
        result.support_shortfall_million, money);
    std::cout
        << "  modeled overlap endpoint exists without support: "
        << bool_text(result.modeled_overlap_exists_without_support) << '\n'
        << "  documented support commitment covers overlap: "
        << bool_text(result.documented_support_commitment_covers_overlap)
        << '\n'
        << "  funded/escrowed support capacity covers overlap: "
        << bool_text(result.funded_support_capacity_covers_overlap) << '\n'
        << "  funded/escrowed-support-covered price window exists: "
        << bool_text(result.funded_support_covered_price_window_exists)
        << '\n'
        << "  reference price numerically eligible: "
        << bool_text(result.reference_price_numerically_eligible) << '\n'
        << "  reference numerical block reason: ";
    if (result.reference_price_numerical_block_reason.empty()) {
        std::cout << "none";
    } else {
        std::cout << result.reference_price_numerical_block_reason;
    }
    std::cout << '\n';
    if (result.reference_price.has_value()) {
        print_reference_metrics(*result.reference_price, currency);
    } else {
        std::cout
            << "  Reference-price hypothetical-primary projection: not "
               "applicable (the record is evidence-only for numerical "
               "purposes)\n";
    }
    print_principal_risk(result.principal_risk, currency);
    print_case_audit(result.audit, currency);
    std::cout
        << "  Endpoint witness ledger for this hurdle\n"
        << "  Each row has its own optimized physical probability measure; "
           "different rows are not one combined stress.\n";
    print_case_witnesses(index, result, currency);
    std::cout << '\n';
}

void print_interpretation_boundary(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& base_stack,
    const cf::RobustMarketPriorityCapConfig& priority_cap,
    const cf::RobustIssuePriceSupportConfig& terms,
    const cf::RobustIssuePriceSupportSummary& summary) {
    std::cout
        << "Interpretation boundary and full false-claim ledger\n"
        << "  A modeled conditional price window is arithmetic under fixed "
           "future cash paths and a supplied hurdle. It is not capital "
           "readiness, an executable price, or investor demand.\n"
        << "  A documented support commitment is distinct from funded or "
           "escrowed capacity; both are distinct from support actually "
           "settled to the identified issue.\n"
        << "  S_req=M+F-P is a modeled source identity. S_obs is separately "
           "evidenced settled cash. One never substitutes for the other.\n"
        << "  Settled source cash is not completed primary funding unless "
           "the subscription-reserve deposit and issuer-cost payment are "
           "also evidenced and the sources-to-uses identity reconciles.\n"
        << "  Settled-secondary buyer-to-seller cash never enters the "
           "project reserve. It is numerical only after explicit external "
           "normalization to the entire fixed month-zero claim.\n"
        << "  Hurdle provenance and relation to P are independent of "
           "transaction execution or settlement evidence. A model-implied "
           "or unresolved hurdle cannot establish a financeable window.\n"
        << "  Expected cash, principal loss, impairment probability, tails, "
           "and WAL are physical-measure analyses. They are not a price, "
           "discount curve, expected investor yield, IFRS 9 ECL, Basel EL, "
           "accounting impairment, rating, or legal default.\n"
        << "  Funding evidence does not calibrate underlying project cash or "
           "probabilities and does not prove financeability, additionality, "
           "deployment, displacement, or animal-welfare impact.\n"
        << "  Core model limitation: " << summary.model_limitation << '\n'
        << "  Portfolio source note: " << portfolio.source_note << '\n'
        << "  Event-polytope source note: " << polytope.source_note << '\n'
        << "  Participation source note: " << participation.source_note
        << '\n'
        << "  Base-stack source note: " << base_stack.source_note << '\n'
        << "  Priority-cap source note: " << priority_cap.source_note << '\n'
        << "  Issue-price/support source note: " << terms.source_note << '\n'
        << "market_hurdle_is_discovered_or_empirically_calibrated="
        << bool_text(
               summary.market_hurdle_is_discovered_or_empirically_calibrated)
        << '\n'
        << "fair_value_or_accounting_value_is_estimated="
        << bool_text(summary.fair_value_or_accounting_value_is_estimated)
        << '\n'
        << "market_consistent_discount_curve_or_pricing_measure_is_used="
        << bool_text(
               summary.market_consistent_discount_curve_or_pricing_measure_is_used)
        << '\n'
        << "bid_offer_executable_price_spread_or_rating_is_produced="
        << bool_text(
               summary.bid_offer_executable_price_spread_or_rating_is_produced)
        << '\n'
        << "investor_demand_suitability_or_placement_is_established="
        << bool_text(
               summary.investor_demand_suitability_or_placement_is_established)
        << '\n'
        << "support_provider_authority_or_budget_is_established="
        << bool_text(
               summary.support_provider_authority_or_budget_is_established)
        << '\n'
        << "support_counterparty_or_performance_risk_is_modeled="
        << bool_text(
               summary.support_counterparty_or_performance_risk_is_modeled)
        << '\n'
        << "legal_enforceability_tax_or_regulation_is_established="
        << bool_text(
               summary.legal_enforceability_tax_or_regulation_is_established)
        << '\n'
        << "capital_mobilization_or_financing_additionality_is_proven="
        << bool_text(
               summary.capital_mobilization_or_financing_additionality_is_proven)
        << '\n'
        << "animal_product_displacement_or_welfare_impact_is_proven="
        << bool_text(
               summary.animal_product_displacement_or_welfare_impact_is_proven)
        << '\n'
        << "calibrated_execution_authorized=false\n";
}

void print_report(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& base_stack,
    const cf::RobustMarketPriorityCapConfig& priority_cap,
    const cf::RobustIssuePriceSupportConfig& issue_price,
    const cf::RobustIssuePriceSupportSummary& summary) {
    const bool all_synthetic = portfolio.synthetic_inputs &&
        polytope.synthetic_inputs && participation.synthetic_inputs &&
        base_stack.synthetic_inputs && priority_cap.synthetic_inputs &&
        issue_price.synthetic_inputs;
    std::cout << std::fixed << std::setprecision(6)
              << (all_synthetic ? "SYNTHETIC " : "")
              << "ROBUST ISSUE-PRICE SUPPORT TERM\n"
              << "Finite physical-probability funded-cash-claim sensitivity "
                 "only; not fair value, an offer, rating, financing proof, "
                 "or recommendation.\n\n";
    print_input_evidence_labels(portfolio, polytope, participation,
        base_stack, priority_cap, issue_price);
    print_fixed_terms(portfolio, issue_price, summary);
    print_work_and_summary_invariants(summary);
    std::cout << "Every supplied hurdle case\n";
    if (summary.hurdle_cases.empty()) {
        std::cout
            << "  not applicable: upstream priority-cap selection is "
               "unavailable, so no hurdle case was projected\n\n";
    }
    for (std::size_t index = 0U; index < summary.hurdle_cases.size();
         ++index) {
        print_hurdle_case(
            index, summary.hurdle_cases[index], portfolio.currency_label);
    }
    print_interpretation_boundary(portfolio, polytope, participation,
        base_stack, priority_cap, issue_price, summary);
}

void print_normalized_inputs(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& base_stack,
    const cf::RobustMarketPriorityCapConfig& priority_cap,
    const cf::RobustIssuePriceSupportConfig& issue_price) {
    std::cout << "\nNormalized portfolio configuration\n";
    cf::print_normalized_portfolio_config(std::cout, portfolio);
    std::cout << "\nNormalized event-probability-polytope configuration\n";
    cf::print_normalized_probability_polytope_config(std::cout, polytope);
    std::cout << "\nNormalized success-participation configuration\n";
    cf::print_normalized_success_participation_config(
        std::cout, participation);
    std::cout << "\nNormalized base-capital-stack configuration\n";
    cf::print_normalized_capital_stack_config(std::cout, base_stack);
    std::cout << "\nNormalized robust-market-priority-cap configuration\n";
    cf::print_normalized_robust_market_priority_cap_config(
        std::cout, priority_cap);
    std::cout << "\nNormalized robust-issue-price-support configuration\n";
    cf::print_normalized_robust_issue_price_support_config(
        std::cout, issue_price);
}

} // namespace

int main(int argc, char** argv) {
    if ((argc != 7 && argc != 8) ||
        (argc == 8 &&
            std::string_view(argv[7]) != "--print-normalized")) {
        print_usage(argc > 0 ? std::string_view(argv[0])
                             : std::string_view("issue-price-support"));
        return 1;
    }
    const bool print_normalized = argc == 8;

    cf::PortfolioConfig portfolio;
    cf::ProbabilityPolytopeConfig polytope;
    cf::SuccessParticipationConfig participation;
    cf::CapitalStackConfig base_stack;
    cf::RobustMarketPriorityCapConfig priority_cap;
    cf::RobustIssuePriceSupportConfig issue_price;
    try {
        portfolio = cf::load_portfolio_config(std::filesystem::path(argv[1]));
        polytope = cf::load_probability_polytope_config(
            std::filesystem::path(argv[2]));
        participation = cf::load_success_participation_config(
            std::filesystem::path(argv[3]));
        base_stack = cf::load_capital_stack_config(
            std::filesystem::path(argv[4]));
        priority_cap = cf::load_robust_market_priority_cap_config(
            std::filesystem::path(argv[5]));
        issue_price = cf::load_robust_issue_price_support_config(
            std::filesystem::path(argv[6]));
    } catch (const std::exception& error) {
        std::cerr << "issue-price-support input/configuration failed: "
                  << error.what()
                  << "\ncalibrated_execution_authorized=false\n";
        return 2;
    }

    try {
        const cf::RobustIssuePriceSupportSummary summary =
            cf::evaluate_robust_issue_price_support(portfolio, polytope,
                participation, base_stack, priority_cap, issue_price);
        print_report(portfolio, polytope, participation, base_stack,
            priority_cap, issue_price, summary);
        if (print_normalized) {
            print_normalized_inputs(portfolio, polytope, participation,
                base_stack, priority_cap, issue_price);
        }
        std::cout.flush();
        if (!std::cout) {
            throw std::runtime_error(
                "failed while writing issue-price-support report");
        }
    } catch (const std::exception& error) {
        std::cerr << "issue-price-support analysis failed: " << error.what()
                  << "\ncalibrated_execution_authorized=false\n";
        return 3;
    }
    return 0;
}
