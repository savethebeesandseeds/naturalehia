// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/success_participation.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kCashSourceCount = 8U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr std::size_t kBisectionIterations = 96U;

class CompensatedSum {
public:
    void add(long double value) noexcept {
        const long double next = sum_ + value;
        if (std::abs(sum_) >= std::abs(value)) {
            correction_ += (sum_ - next) + value;
        } else {
            correction_ += (value - next) + sum_;
        }
        sum_ = next;
    }

    [[nodiscard]] long double value() const noexcept {
        return sum_ + correction_;
    }

private:
    long double sum_{0.0L};
    long double correction_{0.0L};
};

struct ScenarioBasis {
    std::string scenario_id{};
    double q0_npv_million{0.0};
    double q1_npv_million{0.0};
    double full_nominal_million{0.0};
    double full_present_value_million{0.0};
    std::array<double, kCashSourceCount> source_nominal{};
    std::array<double, kCashSourceCount> source_present_value{};
    std::vector<SuccessParticipationSourceAmount> sources{};
};

[[nodiscard]] std::size_t source_index(PortfolioCashSource source) {
    const auto index = static_cast<std::size_t>(source);
    if (index >= kCashSourceCount) {
        throw std::invalid_argument(
            "success participation cash source is outside the taxonomy");
    }
    return index;
}

[[nodiscard]] bool permitted_scalable_source(
    PortfolioCashSource source) noexcept {
    return source == PortfolioCashSource::Commercial ||
        source == PortfolioCashSource::LicensingRoyalty ||
        source == PortfolioCashSource::ExitSale;
}

void require_safe_text(std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength) {
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

[[nodiscard]] double to_double(long double value) {
    const double converted = static_cast<double>(value);
    if (!std::isfinite(converted)) {
        throw std::overflow_error(
            "success participation aggregation exceeded numeric range");
    }
    return converted;
}

[[nodiscard]] std::array<bool, kCashSourceCount> selected_sources(
    const SuccessParticipationConfig& participation) {
    std::array<bool, kCashSourceCount> selected{};
    if (participation.scalable_source_kinds.empty()) {
        throw std::invalid_argument(
            "success participation scalable_source_kinds must be explicit and non-empty");
    }
    for (const PortfolioCashSource source :
         participation.scalable_source_kinds) {
        const std::size_t index = source_index(source);
        if (!permitted_scalable_source(source)) {
            throw std::invalid_argument(
                "success participation may scale only commercial, licensing-royalty, or exit-sale cash");
        }
        if (selected[index]) {
            throw std::invalid_argument(
                "success participation scalable source kinds must be unique");
        }
        selected[index] = true;
    }
    return selected;
}

[[nodiscard]] std::unordered_map<std::string, PortfolioCashSource>
scenario_source_kinds(const JointScenario& scenario);

void validate_terms(const PortfolioConfig& portfolio,
    const SuccessParticipationConfig& participation) {
    validate_portfolio_config(portfolio);
    if (participation.model_version != kSuccessParticipationModelVersion) {
        throw std::invalid_argument(
            "success participation model_version does not match this engine");
    }
    if (!participation.synthetic_inputs) {
        throw std::invalid_argument(
            "success participation v0.1 accepts synthetic inputs only");
    }
    require_safe_text(participation.scenario_label,
        "success participation scenario_label");
    require_safe_text(
        participation.source_note, "success participation source_note");
    if (!participation
             .selected_nonprincipal_cash_is_contractually_scalable) {
        throw std::invalid_argument(
            "success participation requires an explicit contractual-scalability assertion");
    }
    if (!std::isfinite(
            participation.target_worst_expected_npv_million)) {
        throw std::invalid_argument(
            "success participation target robust NPV must be finite");
    }

    const std::array<bool, kCashSourceCount> selected =
        selected_sources(participation);
    std::array<bool, kCashSourceCount> source_kind_seen{};
    for (const JointScenario& scenario : portfolio.joint_scenarios) {
        for (const ScenarioCashSource& source : scenario.cash_sources) {
            const std::size_t index = source_index(source.kind);
            source_kind_seen[index] = true;
        }
        const auto source_kinds = scenario_source_kinds(scenario);
        for (const ProjectJointPath& path : scenario.project_paths) {
            for (const InvestorReceipt& receipt : path.investor_receipts) {
                const PortfolioCashSource source =
                    source_kinds.at(receipt.cash_source_id);
                if (selected[source_index(source)] &&
                    receipt.principal_component_million >
                        receipt.amount_million) {
                    throw std::invalid_argument(
                        "selected participation receipt has principal above cash amount");
                }
            }
        }
    }
    for (std::size_t index = 0U; index < selected.size(); ++index) {
        if (selected[index] && !source_kind_seen[index]) {
            throw std::invalid_argument(
                "a selected success-participation source kind is absent from every scenario");
        }
    }
}

[[nodiscard]] std::unordered_map<std::string, PortfolioCashSource>
scenario_source_kinds(const JointScenario& scenario) {
    std::unordered_map<std::string, PortfolioCashSource> result;
    result.reserve(scenario.cash_sources.size());
    for (const ScenarioCashSource& source : scenario.cash_sources) {
        result.emplace(source.id, source.kind);
    }
    return result;
}

[[nodiscard]] const JointScenario& find_scenario(
    const std::unordered_map<std::string, const JointScenario*>& scenarios,
    const std::string& scenario_id) {
    const auto matching = scenarios.find(scenario_id);
    if (matching == scenarios.end()) {
        throw std::logic_error(
            "success participation lost a validated portfolio scenario");
    }
    return *matching->second;
}

[[nodiscard]] std::vector<AmbiguityScenarioMetricValue> metric_values(
    const std::vector<ScenarioBasis>& basis,
    const std::vector<double>& values) {
    if (basis.size() != values.size()) {
        throw std::logic_error(
            "success participation metric has the wrong scenario count");
    }
    std::vector<AmbiguityScenarioMetricValue> result;
    result.reserve(basis.size());
    for (std::size_t index = 0U; index < basis.size(); ++index) {
        result.push_back(
            AmbiguityScenarioMetricValue{basis[index].scenario_id,
                values[index]});
    }
    return result;
}

[[nodiscard]] std::vector<double> participation_npvs(
    const std::vector<ScenarioBasis>& basis, double fraction) {
    std::vector<double> result;
    result.reserve(basis.size());
    for (const ScenarioBasis& scenario : basis) {
        result.push_back(std::fma(fraction,
            scenario.full_present_value_million,
            scenario.q0_npv_million));
    }
    return result;
}

[[nodiscard]] double witness_reconciliation_error(
    const AmbiguityMetricProjection& projection,
    const std::vector<AmbiguityScenarioMetricValue>& values) {
    if (projection.scenario_probability_bounds.size() != values.size()) {
        throw std::logic_error(
            "success participation projection has the wrong scenario count");
    }
    std::unordered_map<std::string, double> value_by_id;
    value_by_id.reserve(values.size());
    for (const AmbiguityScenarioMetricValue& value : values) {
        value_by_id.emplace(value.scenario_id, value.value);
    }

    double maximum_error = 0.0;
    const auto reconcile = [&](const AmbiguityEndpoint& endpoint) {
        if (endpoint.scenario_weights.size() != values.size()) {
            throw std::logic_error(
                "success participation witness has the wrong scenario count");
        }
        CompensatedSum objective;
        for (std::size_t index = 0U; index < values.size(); ++index) {
            const std::string& scenario_id =
                projection.scenario_probability_bounds[index].scenario_id;
            const auto matching = value_by_id.find(scenario_id);
            if (matching == value_by_id.end()) {
                throw std::logic_error(
                    "success participation witness lost a scenario value");
            }
            objective.add(static_cast<long double>(
                              endpoint.scenario_weights[index]) *
                static_cast<long double>(matching->second));
        }
        maximum_error = std::max(maximum_error,
            std::abs(to_double(objective.value()) - endpoint.value));
    };
    reconcile(projection.expectation.minimum);
    reconcile(projection.expectation.maximum);
    return maximum_error;
}

[[nodiscard]] SuccessParticipationRobustPoint project_point(
    const PortfolioAmbiguityProjector& projector,
    const std::vector<ScenarioBasis>& basis, double fraction,
    double& maximum_witness_error, double& maximum_probability_error) {
    const std::vector<double> npvs = participation_npvs(basis, fraction);
    const std::vector<AmbiguityScenarioMetricValue> values =
        metric_values(basis, npvs);
    const AmbiguityMetricProjection projection =
        projector.project_expectation(values);

    maximum_witness_error = std::max(maximum_witness_error,
        witness_reconciliation_error(projection, values));
    maximum_probability_error = std::max(maximum_probability_error,
        projection.maximum_endpoint_probability_error);

    SuccessParticipationRobustPoint point;
    point.participation_fraction = fraction;
    point.expected_npv_million = projection.expectation;
    point.maximum_endpoint_probability_error =
        projection.maximum_endpoint_probability_error;
    return point;
}

[[nodiscard]] AmbiguityMetricProjection project_scenario_values(
    const PortfolioAmbiguityProjector& projector,
    const std::vector<ScenarioBasis>& basis,
    const std::vector<double>& values, double& maximum_witness_error,
    double& maximum_probability_error) {
    const std::vector<AmbiguityScenarioMetricValue> keyed =
        metric_values(basis, values);
    const AmbiguityMetricProjection projection =
        projector.project_expectation(keyed);
    maximum_witness_error = std::max(maximum_witness_error,
        witness_reconciliation_error(projection, keyed));
    maximum_probability_error = std::max(maximum_probability_error,
        projection.maximum_endpoint_probability_error);
    return projection;
}

[[nodiscard]] double maximum_source_capacity_violation(
    const PortfolioConfig& portfolio) {
    struct CapacityEvent {
        std::size_t month{0U};
        long double available_million{0.0L};
        long double receipt_million{0.0L};
    };

    double maximum_violation = 0.0;
    for (const JointScenario& scenario : portfolio.joint_scenarios) {
        // Capacity can worsen only at a dated receipt. Sweep the sparse union
        // of cash and receipt records instead of materializing every month for
        // every source, which would exceed the portfolio engine's resource
        // model at its supported dimensional limits.
        std::unordered_map<std::string, std::vector<CapacityEvent>> events;
        events.reserve(scenario.cash_sources.size());
        for (const ScenarioCashSource& source : scenario.cash_sources) {
            std::vector<CapacityEvent>& source_events = events[source.id];
            source_events.reserve(source.cash_available.size());
            for (const MonthlyAmount& cash : source.cash_available) {
                source_events.push_back(CapacityEvent{cash.month,
                    static_cast<long double>(cash.amount_million), 0.0L});
            }
        }
        for (const ProjectJointPath& path : scenario.project_paths) {
            for (const InvestorReceipt& receipt : path.investor_receipts) {
                events.at(receipt.cash_source_id)
                    .push_back(CapacityEvent{receipt.month, 0.0L,
                        static_cast<long double>(receipt.amount_million)});
            }
        }
        for (const ScenarioCashSource& source : scenario.cash_sources) {
            std::vector<CapacityEvent>& source_events = events.at(source.id);
            std::sort(source_events.begin(), source_events.end(),
                [](const CapacityEvent& first, const CapacityEvent& second) {
                    return first.month < second.month;
                });
            CompensatedSum cumulative_available;
            CompensatedSum cumulative_receipts;
            std::size_t event = 0U;
            while (event < source_events.size()) {
                const std::size_t month = source_events[event].month;
                do {
                    cumulative_available.add(
                        source_events[event].available_million);
                    cumulative_receipts.add(
                        source_events[event].receipt_million);
                    ++event;
                } while (event < source_events.size() &&
                    source_events[event].month == month);
                maximum_violation = std::max(maximum_violation,
                    std::max(0.0, to_double(cumulative_receipts.value() -
                                      cumulative_available.value())));
            }
        }
    }
    return maximum_violation;
}

[[nodiscard]] const JointScenarioResult& scenario_result(
    const PortfolioSummary& summary, std::string_view scenario_id) {
    const auto matching = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [scenario_id](const JointScenarioResult& value) {
            return value.scenario_id == scenario_id;
        });
    if (matching == summary.scenarios.end()) {
        throw std::logic_error(
            "success participation evaluation lost a scenario result");
    }
    return *matching;
}

} // namespace

std::string_view to_string(
    SuccessParticipationSolveStatus status) noexcept {
    switch (status) {
    case SuccessParticipationSolveStatus::AlreadyMeetsTargetAtZero:
        return "already-meets-target-at-zero";
    case SuccessParticipationSolveStatus::CertifiedInteriorBracket:
        return "certified-interior-bracket";
    case SuccessParticipationSolveStatus::FullParticipationRequired:
        return "full-participation-required";
    case SuccessParticipationSolveStatus::NoSelectedParticipationCash:
        return "no-selected-participation-cash";
    case SuccessParticipationSolveStatus::UnattainableAtFullParticipation:
        return "unattainable-at-full-participation";
    }
    return "unknown";
}

void validate_success_participation_config(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation) {
    validate_terms(portfolio, participation);
    (void)PortfolioAmbiguityProjector(portfolio, ambiguity);
}

PortfolioConfig apply_success_participation_fraction(
    const PortfolioConfig& portfolio,
    const SuccessParticipationConfig& participation,
    double participation_fraction) {
    validate_terms(portfolio, participation);
    if (!std::isfinite(participation_fraction) ||
        participation_fraction < 0.0 || participation_fraction > 1.0) {
        throw std::invalid_argument(
            "success participation fraction must be finite and within [0, 1]");
    }
    if (participation_fraction == 1.0) {
        return portfolio;
    }

    const std::array<bool, kCashSourceCount> selected =
        selected_sources(participation);
    PortfolioConfig adjusted = portfolio;
    for (JointScenario& scenario : adjusted.joint_scenarios) {
        const auto source_kinds = scenario_source_kinds(scenario);
        for (ProjectJointPath& path : scenario.project_paths) {
            for (InvestorReceipt& receipt : path.investor_receipts) {
                const PortfolioCashSource source =
                    source_kinds.at(receipt.cash_source_id);
                if (!selected[source_index(source)]) {
                    continue;
                }
                if (receipt.principal_component_million >
                    receipt.amount_million) {
                    throw std::invalid_argument(
                        "selected participation receipt has principal above cash amount");
                }
                const double selected_payoff = receipt.amount_million -
                    receipt.principal_component_million;
                double amount = std::fma(participation_fraction,
                    selected_payoff, receipt.principal_component_million);
                amount = std::clamp(amount,
                    receipt.principal_component_million,
                    receipt.amount_million);
                receipt.amount_million = amount;
            }
        }
    }
    validate_portfolio_config(adjusted);
    return adjusted;
}

SuccessParticipationSummary solve_success_participation(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation) {
    validate_terms(portfolio, participation);
    const std::array<bool, kCashSourceCount> selected =
        selected_sources(participation);
    const PortfolioAmbiguityProjector projector(portfolio, ambiguity);
    const PortfolioSummary q1_portfolio = evaluate_portfolio(portfolio);
    const PortfolioConfig q0_config = apply_success_participation_fraction(
        portfolio, participation, 0.0);
    const PortfolioSummary q0_portfolio = evaluate_portfolio(q0_config);

    std::unordered_map<std::string, const JointScenario*> config_scenarios;
    config_scenarios.reserve(portfolio.joint_scenarios.size());
    for (const JointScenario& scenario : portfolio.joint_scenarios) {
        config_scenarios.emplace(scenario.id, &scenario);
    }

    const double monthly_rate_base =
        1.0 + portfolio.annual_physical_hurdle_rate;
    std::vector<ScenarioBasis> basis;
    basis.reserve(q1_portfolio.scenarios.size());
    double maximum_q1_error = 0.0;
    for (const JointScenarioResult& q1_scenario : q1_portfolio.scenarios) {
        const JointScenario& configured =
            find_scenario(config_scenarios, q1_scenario.scenario_id);
        const auto source_kinds = scenario_source_kinds(configured);
        std::unordered_map<std::string, SuccessParticipationSourceAmount>
            source_amounts;

        ScenarioBasis scenario;
        scenario.scenario_id = q1_scenario.scenario_id;
        scenario.q1_npv_million = q1_scenario.npv_million;
        scenario.q0_npv_million =
            scenario_result(q0_portfolio, scenario.scenario_id).npv_million;

        CompensatedSum nominal;
        CompensatedSum present_value;
        std::array<CompensatedSum, kCashSourceCount> source_nominal{};
        std::array<CompensatedSum, kCashSourceCount> source_pv{};
        for (const ProjectJointPath& path : configured.project_paths) {
            for (const InvestorReceipt& receipt : path.investor_receipts) {
                const PortfolioCashSource source =
                    source_kinds.at(receipt.cash_source_id);
                if (!selected[source_index(source)]) {
                    continue;
                }
                if (receipt.principal_component_million >
                    receipt.amount_million) {
                    throw std::invalid_argument(
                        "selected participation receipt has principal above cash amount");
                }
                const long double payoff = static_cast<long double>(
                    receipt.amount_million -
                    receipt.principal_component_million);
                const long double discount = static_cast<long double>(
                    std::pow(monthly_rate_base,
                        static_cast<double>(receipt.month) / 12.0));
                const long double payoff_pv = payoff / discount;
                nominal.add(payoff);
                present_value.add(payoff_pv);
                source_nominal[source_index(source)].add(payoff);
                source_pv[source_index(source)].add(payoff_pv);

                auto [position, inserted] = source_amounts.try_emplace(
                    receipt.cash_source_id);
                if (inserted) {
                    position->second.cash_source_id = receipt.cash_source_id;
                    position->second.source = source;
                }
                position->second.full_participation_nominal_million +=
                    to_double(payoff);
                position->second.full_participation_present_value_million +=
                    to_double(payoff_pv);
            }
        }
        scenario.full_nominal_million = to_double(nominal.value());
        scenario.full_present_value_million =
            to_double(present_value.value());
        for (std::size_t source = 0U; source < kCashSourceCount; ++source) {
            scenario.source_nominal[source] =
                to_double(source_nominal[source].value());
            scenario.source_present_value[source] =
                to_double(source_pv[source].value());
        }
        scenario.sources.reserve(source_amounts.size());
        for (auto& [unused_id, amount] : source_amounts) {
            (void)unused_id;
            scenario.sources.push_back(std::move(amount));
        }
        std::sort(scenario.sources.begin(), scenario.sources.end(),
            [](const SuccessParticipationSourceAmount& first,
                const SuccessParticipationSourceAmount& second) {
                return first.cash_source_id < second.cash_source_id;
            });
        maximum_q1_error = std::max(maximum_q1_error,
            std::abs(std::fma(1.0, scenario.full_present_value_million,
                         scenario.q0_npv_million) -
                scenario.q1_npv_million));
        basis.push_back(std::move(scenario));
    }

    SuccessParticipationSummary result;
    result.target_worst_expected_npv_million =
        participation.target_worst_expected_npv_million;
    result.maximum_q1_cash_reconstruction_error_million = maximum_q1_error;

    std::vector<double> full_nominal;
    std::vector<double> full_present_value;
    full_nominal.reserve(basis.size());
    full_present_value.reserve(basis.size());
    for (const ScenarioBasis& scenario : basis) {
        full_nominal.push_back(scenario.full_nominal_million);
        full_present_value.push_back(scenario.full_present_value_million);
    }
    const AmbiguityMetricProjection nominal_projection =
        project_scenario_values(projector, basis, full_nominal,
            result.maximum_witness_reconciliation_error_million,
            result.maximum_endpoint_probability_error);
    const AmbiguityMetricProjection present_value_projection =
        project_scenario_values(projector, basis, full_present_value,
            result.maximum_witness_reconciliation_error_million,
            result.maximum_endpoint_probability_error);
    result.full_participation_nominal_million =
        nominal_projection.expectation;
    result.full_participation_present_value_million =
        present_value_projection.expectation;

    result.source_ranges.reserve(participation.scalable_source_kinds.size());
    for (std::size_t source = 0U; source < selected.size(); ++source) {
        if (!selected[source]) {
            continue;
        }
        std::vector<double> nominal_by_source;
        std::vector<double> pv_by_source;
        nominal_by_source.reserve(basis.size());
        pv_by_source.reserve(basis.size());
        for (const ScenarioBasis& scenario : basis) {
            nominal_by_source.push_back(scenario.source_nominal[source]);
            pv_by_source.push_back(scenario.source_present_value[source]);
        }
        const AmbiguityMetricProjection source_nominal_projection =
            project_scenario_values(projector, basis, nominal_by_source,
                result.maximum_witness_reconciliation_error_million,
                result.maximum_endpoint_probability_error);
        const AmbiguityMetricProjection source_pv_projection =
            project_scenario_values(projector, basis, pv_by_source,
                result.maximum_witness_reconciliation_error_million,
                result.maximum_endpoint_probability_error);
        SuccessParticipationSourceRange range;
        range.source = static_cast<PortfolioCashSource>(source);
        range.full_participation_nominal_million =
            source_nominal_projection.expectation;
        range.full_participation_present_value_million =
            source_pv_projection.expectation;
        result.source_ranges.push_back(std::move(range));
    }

    result.q0 = project_point(projector, basis, 0.0,
        result.maximum_witness_reconciliation_error_million,
        result.maximum_endpoint_probability_error);
    result.q1 = project_point(projector, basis, 1.0,
        result.maximum_witness_reconciliation_error_million,
        result.maximum_endpoint_probability_error);
    result.scenario_probability_bounds =
        nominal_projection.scenario_probability_bounds;

    const double target = participation.target_worst_expected_npv_million;
    const double worst_q0 = result.q0.expected_npv_million.minimum.value;
    const double worst_q1 = result.q1.expected_npv_million.minimum.value;
    const bool any_participation = std::any_of(basis.begin(), basis.end(),
        [](const ScenarioBasis& scenario) {
            return scenario.full_present_value_million > 0.0;
        });

    if (worst_q0 >= target) {
        result.status =
            SuccessParticipationSolveStatus::AlreadyMeetsTargetAtZero;
        result.exact_minimum_fraction = 0.0;
        result.feasible_fraction_upper_bound = 0.0;
        result.reported_fraction = 0.0;
        result.reported = result.q0;
    } else if (!any_participation) {
        result.status =
            SuccessParticipationSolveStatus::NoSelectedParticipationCash;
        result.failing_fraction_lower_bound = 1.0;
        result.reported_fraction = 0.0;
        result.reported = result.q0;
    } else if (worst_q1 < target) {
        result.status = SuccessParticipationSolveStatus::
            UnattainableAtFullParticipation;
        result.failing_fraction_lower_bound = 1.0;
        result.reported_fraction = 1.0;
        result.reported = result.q1;
    } else {
        double lower = 0.0;
        double upper = 1.0;
        SuccessParticipationRobustPoint upper_point = result.q1;
        for (std::size_t iteration = 0U;
             iteration < kBisectionIterations; ++iteration) {
            const double midpoint = std::midpoint(lower, upper);
            if (midpoint == lower || midpoint == upper) {
                break;
            }
            SuccessParticipationRobustPoint midpoint_point = project_point(
                projector, basis, midpoint,
                result.maximum_witness_reconciliation_error_million,
                result.maximum_endpoint_probability_error);
            if (midpoint_point.expected_npv_million.minimum.value >= target) {
                upper = midpoint;
                upper_point = std::move(midpoint_point);
            } else {
                lower = midpoint;
            }
        }
        result.failing_fraction_lower_bound = lower;
        result.feasible_fraction_upper_bound = upper;
        result.reported_fraction = upper;
        result.reported = std::move(upper_point);
        if (upper == 1.0 && worst_q1 == target) {
            result.status =
                SuccessParticipationSolveStatus::FullParticipationRequired;
            result.exact_minimum_fraction = 1.0;
        } else {
            result.status = SuccessParticipationSolveStatus::
                CertifiedInteriorBracket;
        }
    }
    result.target_gap_at_full_participation_million =
        std::max(0.0, target - worst_q1);

    const PortfolioConfig reported_config =
        apply_success_participation_fraction(portfolio, participation,
            result.reported_fraction);
    const PortfolioSummary reported_portfolio =
        evaluate_portfolio(reported_config);

    result.maximum_source_capacity_violation_million = std::max({
        maximum_source_capacity_violation(q0_config),
        maximum_source_capacity_violation(portfolio),
        maximum_source_capacity_violation(reported_config)});

    result.scenarios.reserve(basis.size());
    const std::vector<double> reported_npvs =
        participation_npvs(basis, result.reported_fraction);
    for (std::size_t index = 0U; index < basis.size(); ++index) {
        const ScenarioBasis& scenario = basis[index];
        const JointScenarioResult& q1_scenario =
            scenario_result(q1_portfolio, scenario.scenario_id);
        const JointScenarioResult& q0_scenario =
            scenario_result(q0_portfolio, scenario.scenario_id);
        const JointScenarioResult& reported_scenario =
            scenario_result(reported_portfolio, scenario.scenario_id);
        result.maximum_principal_loss_reconciliation_error_million =
            std::max({result.maximum_principal_loss_reconciliation_error_million,
                std::abs(q0_scenario.principal_loss_million -
                    q1_scenario.principal_loss_million),
                std::abs(reported_scenario.principal_loss_million -
                    q1_scenario.principal_loss_million)});
        result.maximum_q1_cash_reconstruction_error_million = std::max(
            result.maximum_q1_cash_reconstruction_error_million,
            std::abs(q1_scenario.npv_million -
                std::fma(1.0, scenario.full_present_value_million,
                    scenario.q0_npv_million)));

        SuccessParticipationScenarioResult published;
        published.scenario_id = scenario.scenario_id;
        published.selected_participation_off_npv_million =
            scenario.q0_npv_million;
        published.configured_q1_npv_million = scenario.q1_npv_million;
        published.full_participation_nominal_million =
            scenario.full_nominal_million;
        published.full_participation_present_value_million =
            scenario.full_present_value_million;
        published.npv_at_reported_fraction_million = reported_npvs[index];
        published.sources = scenario.sources;
        result.scenarios.push_back(std::move(published));
    }

    return result;
}

} // namespace naturalehia::cellular_finance
