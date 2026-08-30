// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/joint_cohort.hpp>
#include <naturalehia/cellular_finance/joint_cohort_config.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
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
    double first, double second, double tolerance = 1.0e-12) {
    return std::abs(first - second) <=
        tolerance * std::max({1.0, std::abs(first), std::abs(second)});
}

void expect_invalid(
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

[[nodiscard]] std::vector<cf::JointCohortObservation> repeat_matured(
    std::size_t count, std::string_view scenario_id) {
    std::vector<cf::JointCohortObservation> rows;
    rows.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const std::string suffix = std::to_string(index + 1U);
        rows.push_back(cf::JointCohortObservation{
            "generated-observation-" + suffix,
            "generated-cluster-" + suffix,
            "2025-02-01",
            "2026-06-30",
            cf::JointCohortObservationStatus::Matured,
            std::string(scenario_id),
            "2026-07-15",
            "NONE",
            {},
            {}});
    }
    return rows;
}

void test_fixture_candidate(const cf::JointCohortPackage& package) {
    std::vector<double> weights;
    for (const cf::JointScenario& scenario :
         package.portfolio.joint_scenarios) {
        weights.push_back(scenario.weight);
    }
    const cf::JointCohortResult result = cf::evaluate_joint_cohort(
        package.config.analysis, package.portfolio, package.observations);
    check(result.raw_observation_count == 22U &&
            result.included_observation_count == 20U &&
            result.matured_count == 18U && result.unknown_count == 2U &&
            result.excluded_count == 2U,
        "fixture counts every raw row without replacing the ledger by aggregates");
    const double expected_epsilon = std::sqrt(
        (std::log(2.0) + std::log(4.0) - std::log(0.05)) / 40.0);
    check(result.primary_outer_set_available &&
            near(result.hoeffding_bonferroni_epsilon, expected_epsilon) &&
            result.portfolio_reference_within_primary_bounds,
        "fixture emits the declared Hoeffding-Bonferroni outer set");
    check(result.scenario_envelopes.size() == 4U &&
            result.scenario_envelopes.front().compatible_maximum_count ==
                result.scenario_envelopes.front().matured_count + 2U,
        "every unknown included unit remains compatible with every scenario");
    check(std::all_of(result.scenario_envelopes.begin(),
                      result.scenario_envelopes.end(),
                      [](const cf::JointCohortScenarioEnvelope& row) {
                          return !row.descriptive_empirical_frequency.has_value();
                      }) &&
            !result.goodman_diagnostic_available,
        "incomplete cohorts do not publish a matured-only center or Goodman diagnostic");
    check(result.financial_ranges_available &&
            result.generated_probability_envelope.has_value() &&
            result.financial_ranges.has_value() &&
            result.project_impairment_projections_available &&
            result.project_impairment_probabilities.size() == 2U &&
            result.pair_impairment_projections_available &&
            result.pair_impairment_probabilities.size() == 1U &&
            result.any_project_impairment_probability.has_value() &&
            result.all_projects_impairment_probability.has_value(),
        "financial and project/joint linear projections are available when the reference is feasible");
    check(!result.calibrated_execution_authorized &&
            result.generated_envelope_synthetic,
        "candidate output never authorizes calibrated execution");
    if (result.financial_ranges.has_value()) {
        const cf::PortfolioAmbiguitySummary& financial =
            *result.financial_ranges;
        check(financial.projects.size() == 2U &&
                financial.principal_loss_tail_attribution_95.projects.size() ==
                    2U,
            "cohort export carries project-dollar ranges and common-witness tail attribution");
        for (const cf::JointCohortProjectImpairmentProjection& projected :
             result.project_impairment_probabilities) {
            const auto matching = std::find_if(financial.projects.begin(),
                financial.projects.end(), [&projected](const auto& project) {
                    return project.project_id == projected.project_id;
                });
            check(matching != financial.projects.end() &&
                    projected.impairment_probability.minimum.scenario_weights ==
                        matching->principal_impairment_probability.minimum
                            .scenario_weights &&
                    projected.impairment_probability.maximum.scenario_weights ==
                        matching->principal_impairment_probability.maximum
                            .scenario_weights &&
                    near(projected.impairment_probability.central,
                        matching->principal_impairment_probability.central),
                "joint-cohort project impairment reuses the generic project range and witnesses");
        }
    }
    for (std::size_t index = 0U; index < weights.size(); ++index) {
        check(package.portfolio.joint_scenarios[index].weight == weights[index],
            "evaluation never mutates caller portfolio weights");
    }
}

void test_complete_and_blocked_paths(const cf::JointCohortPackage& package) {
    cf::JointCohortAnalysisConfig complete_config = package.config.analysis;
    std::vector<cf::JointCohortObservation> complete(
        package.observations.begin(), package.observations.begin() + 18);
    complete_config.population_frame_count = complete.size();
    const cf::JointCohortResult complete_result = cf::evaluate_joint_cohort(
        complete_config, package.portfolio, complete);
    check(complete_result.goodman_diagnostic_available &&
            complete_result.goodman_sparse_cell_warning &&
            std::all_of(complete_result.scenario_envelopes.begin(),
                        complete_result.scenario_envelopes.end(),
                        [](const cf::JointCohortScenarioEnvelope& row) {
                            return row.descriptive_empirical_frequency.has_value() &&
                                row.goodman_lower_weight.has_value() &&
                                row.goodman_upper_weight.has_value();
                        }),
        "complete multi-category cohorts publish empirical centers and sparse-cell-labeled Goodman diagnostics");
    check(complete_result.generated_probability_envelope.has_value() &&
            std::equal(
                complete_result.scenario_envelopes.begin(),
                complete_result.scenario_envelopes.end(),
                complete_result.generated_probability_envelope
                    ->scenario_probabilities.begin(),
                [](const cf::JointCohortScenarioEnvelope& primary,
                   const cf::ScenarioProbabilityBounds& exported) {
                    return primary.scenario_id == exported.scenario_id &&
                        primary.primary_lower_weight ==
                            exported.lower_weight &&
                        primary.primary_upper_weight ==
                            exported.upper_weight;
                }),
        "exported financial ambiguity uses primary bounds, never Goodman challengers");

    std::vector<cf::JointCohortObservation> repeated = package.observations;
    repeated[1U].cluster_id = repeated[0U].cluster_id;
    const cf::JointCohortResult blocked = cf::evaluate_joint_cohort(
        package.config.analysis, package.portfolio, repeated);
    check(!blocked.primary_outer_set_available &&
            !blocked.included_cluster_ids_unique &&
            blocked.scenario_envelopes.size() == 4U &&
            blocked.scenario_envelopes.front().compatible_maximum_count > 0U,
        "repeated included clusters block coverage but preserve descriptive compatible counts");

    cf::PortfolioConfig one_category = package.portfolio;
    one_category.joint_scenarios = {one_category.joint_scenarios.front()};
    one_category.joint_scenarios.front().weight = 1.0;
    std::vector<cf::JointCohortObservation> one_rows =
        repeat_matured(5U, one_category.joint_scenarios.front().id);
    cf::JointCohortAnalysisConfig one_config = package.config.analysis;
    one_config.population_frame_count = one_rows.size();
    one_config.exclusion_rules.clear();
    const cf::JointCohortResult one_result = cf::evaluate_joint_cohort(
        one_config, one_category, one_rows);
    check(one_result.primary_outer_set_available &&
            !one_result.goodman_diagnostic_available &&
            one_result.scenario_envelopes.front().descriptive_empirical_frequency ==
                1.0,
        "K=1 remains valid and deliberately omits Goodman");

    std::vector<cf::JointCohortObservation> one_unknown{
        cf::JointCohortObservation{
            "unknown-one", "unknown-cluster", "2025-02-01",
            "2027-02-01", cf::JointCohortObservationStatus::NotYetMatured,
            "NONE", "NONE", "NONE", {}, {}}};
    cf::JointCohortAnalysisConfig wide_config = package.config.analysis;
    wide_config.population_frame_count = 1U;
    wide_config.exclusion_rules.clear();
    const cf::JointCohortResult wide = cf::evaluate_joint_cohort(
        wide_config, package.portfolio, one_unknown);
    check(wide.hoeffding_bonferroni_epsilon >= 1.0 &&
            std::all_of(wide.scenario_envelopes.begin(),
                        wide.scenario_envelopes.end(),
                        [](const cf::JointCohortScenarioEnvelope& row) {
                            return row.primary_lower_weight == 0.0 &&
                                row.primary_upper_weight == 1.0;
                        }),
        "epsilon at least one honestly publishes the full unit interval");

    cf::JointCohortAnalysisConfig tiny_confidence = wide_config;
    tiny_confidence.confidence_level =
        std::numeric_limits<double>::denorm_min();
    const cf::JointCohortResult tiny_confidence_result =
        cf::evaluate_joint_cohort(
            tiny_confidence, package.portfolio, one_unknown);
    check(tiny_confidence_result.family_alpha < 1.0 &&
            tiny_confidence_result.family_alpha > 0.0 &&
            std::isfinite(
                tiny_confidence_result.hoeffding_bonferroni_epsilon),
        "subnormal positive confidence remains positive after directed conservative alpha arithmetic");

    cf::PortfolioConfig zero_atom = package.portfolio;
    for (cf::JointScenario& scenario : zero_atom.joint_scenarios) {
        if (scenario.id == "common-success") {
            scenario.weight = 0.60;
        } else if (scenario.id == "culture-loss-scaleup-success" ||
                   scenario.id == "culture-success-scaleup-loss") {
            scenario.weight = 0.20;
        } else {
            scenario.weight = 0.0;
        }
    }
    std::vector<cf::JointCohortObservation> zero_rows;
    const auto append = [&zero_rows](std::size_t count,
                                     std::string_view scenario_id) {
        std::vector<cf::JointCohortObservation> next =
            repeat_matured(count, scenario_id);
        const std::size_t offset = zero_rows.size();
        for (std::size_t index = 0U; index < next.size(); ++index) {
            next[index].observation_id = "zero-observation-" +
                std::to_string(offset + index + 1U);
            next[index].cluster_id = "zero-cluster-" +
                std::to_string(offset + index + 1U);
            zero_rows.push_back(std::move(next[index]));
        }
    };
    append(6U, "common-success");
    append(2U, "culture-loss-scaleup-success");
    append(2U, "culture-success-scaleup-loss");
    cf::JointCohortAnalysisConfig zero_config = package.config.analysis;
    zero_config.population_frame_count = zero_rows.size();
    zero_config.exclusion_rules.clear();
    const cf::JointCohortResult zero_result = cf::evaluate_joint_cohort(
        zero_config, zero_atom, zero_rows);
    const auto zero_cell = std::find_if(
        zero_result.scenario_envelopes.begin(),
        zero_result.scenario_envelopes.end(),
        [](const cf::JointCohortScenarioEnvelope& row) {
            return row.scenario_id == "common-loss";
        });
    const auto zero_export =
        zero_result.generated_probability_envelope.has_value()
        ? std::find_if(
              zero_result.generated_probability_envelope
                  ->scenario_probabilities.begin(),
              zero_result.generated_probability_envelope
                  ->scenario_probabilities.end(),
              [](const cf::ScenarioProbabilityBounds& row) {
                  return row.scenario_id == "common-loss";
              })
        : std::vector<cf::ScenarioProbabilityBounds>::const_iterator{};
    check(zero_cell != zero_result.scenario_envelopes.end() &&
            zero_cell->matured_count == 0U &&
            zero_cell->descriptive_empirical_frequency == 0.0 &&
            zero_cell->portfolio_reference_weight == 0.0 &&
            zero_cell->primary_lower_weight == 0.0 &&
            zero_cell->primary_upper_weight > 0.0 &&
            zero_result.generated_probability_envelope.has_value() &&
            zero_export != zero_result.generated_probability_envelope
                               ->scenario_probabilities.end() &&
            zero_export->central_weight == 0.0,
        "complete zero-count cells remain zero centrally without a pseudocount and retain sampling uncertainty");
}

void test_goodman_published_numerical_regression() {
    constexpr std::size_t counts[10]{
        5U, 11U, 19U, 30U, 58U, 67U, 92U, 118U, 173U, 297U};
    constexpr double expected_lower[10]{
        0.0019221299557002, 0.005935610755607667,
        0.012237701616664277, 0.021745679831822663,
        0.047998919708708704, 0.05679860066682115,
        0.08181219266073275, 0.10847475704949135,
        0.16633127628436867, 0.30131047809670203};
    constexpr double expected_upper[10]{
        0.01705372494754372, 0.026728953148986337,
        0.03867847428972984, 0.05426646257704661,
        0.09189386470646096, 0.10362724725037363,
        0.1356499427620874, 0.16830511737917916,
        0.23592843065667765, 0.383849214872246};
    constexpr double published_critical = 6.6348966010212145;

    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "Goodman 1965 Table 2 numerical regression";
    portfolio.projects = {cf::PortfolioProject{
        "test-project", cf::ProjectStage::Research, 1.0}};
    std::vector<cf::JointCohortObservation> rows;
    rows.reserve(870U);
    for (std::size_t category = 0U; category < 10U; ++category) {
        cf::JointScenario scenario;
        scenario.id = category + 1U < 10U
            ? "state-0" + std::to_string(category + 1U)
            : "state-10";
        scenario.weight =
            static_cast<double>(counts[category]) / 870.0;
        scenario.project_paths = {
            cf::ProjectJointPath{"test-project", {}, {}, {}}};
        portfolio.joint_scenarios.push_back(std::move(scenario));
        for (std::size_t item = 0U; item < counts[category]; ++item) {
            const std::size_t number = rows.size() + 1U;
            rows.push_back(cf::JointCohortObservation{
                "goodman-observation-" + std::to_string(number),
                "goodman-cluster-" + std::to_string(number),
                "2025-02-01", "2026-06-30",
                cf::JointCohortObservationStatus::Matured,
                portfolio.joint_scenarios.back().id, "2026-07-15",
                "NONE", {}, {}});
        }
    }
    cf::JointCohortAnalysisConfig config;
    config.id = "goodman-table-2";
    config.as_of_date = "2026-08-30";
    config.source_note = "Published-count numerical challenger regression";
    config.population_definition = "Goodman published multinomial count table";
    config.sampling_unit_definition = "One independently sampled categorical unit";
    config.outcome_mapping_definition = "Each unit maps to one fixed state category";
    config.horizon_definition = "All outcomes are complete at the fixed horizon";
    config.scenario_taxonomy_frozen_date = "2025-01-01";
    config.population_frame_count = rows.size();
    config.confidence_level = 0.90;

    const cf::JointCohortResult result =
        cf::evaluate_joint_cohort(config, portfolio, rows);
    const double expected_epsilon = std::sqrt(
        (std::log(2.0) + std::log(10.0) - std::log(0.10)) /
        (2.0 * 870.0));
    check(result.goodman_diagnostic_available &&
            result.goodman_chi_square_critical_value.has_value() &&
            near(*result.goodman_chi_square_critical_value,
                published_critical, 1.0e-12) &&
            near(result.hoeffding_bonferroni_epsilon,
                expected_epsilon, 1.0e-13),
        "Goodman published critical value and independent Hoeffding radius match numerical goldens");
    for (std::size_t category = 0U; category < 10U; ++category) {
        const cf::JointCohortScenarioEnvelope& row =
            result.scenario_envelopes[category];
        const double empirical =
            static_cast<double>(counts[category]) / 870.0;
        const long double exact_alpha =
            1.0L - static_cast<long double>(config.confidence_level);
        const long double exact_epsilon = std::sqrt(
            (std::log(2.0L) + std::log(10.0L) - std::log(exact_alpha)) /
            (2.0L * 870.0L));
        const long double exact_lower = std::max(
            0.0L,
            static_cast<long double>(counts[category]) / 870.0L -
                exact_epsilon);
        const long double exact_upper = std::min(
            1.0L,
            static_cast<long double>(counts[category]) / 870.0L +
                exact_epsilon);
        check(row.goodman_lower_weight.has_value() &&
                row.goodman_upper_weight.has_value() &&
                near(*row.goodman_lower_weight,
                    expected_lower[category], 1.0e-12) &&
                near(*row.goodman_upper_weight,
                    expected_upper[category], 1.0e-12),
            "each Goodman Table 2 cell matches its fixed numerical interval");
        check(near(row.primary_lower_weight,
                       std::max(0.0, empirical - expected_epsilon), 1.0e-13) &&
                near(row.primary_upper_weight,
                       std::min(1.0, empirical + expected_epsilon), 1.0e-13),
            "each primary cell matches its independently computed Hoeffding bound");
        check(static_cast<long double>(row.primary_lower_weight) <=
                    exact_lower &&
                static_cast<long double>(row.primary_upper_weight) >=
                    exact_upper,
            "published primary doubles round outward and never shrink the exact long-double set");
        check(static_cast<long double>(result.family_alpha) <= exact_alpha,
            "reported family alpha is directed conservatively below the requested complement");
        check(result.generated_probability_envelope.has_value() &&
                result.generated_probability_envelope
                        ->scenario_probabilities[category].lower_weight ==
                    row.primary_lower_weight &&
                result.generated_probability_envelope
                        ->scenario_probabilities[category].upper_weight ==
                    row.primary_upper_weight,
            "Goodman never substitutes for exported primary probability bounds");
    }
}

void test_fail_closed_semantics(const cf::JointCohortPackage& package) {
    cf::JointCohortAnalysisConfig config = package.config.analysis;
    config.population_frame_count = package.observations.size() - 1U;
    expect_invalid([&] {
        static_cast<void>(cf::evaluate_joint_cohort(
            config, package.portfolio, package.observations));
    }, "programmatic population frame mismatch is rejected");

    std::vector<cf::JointCohortObservation> rows = package.observations;
    rows.front().eligible_date = "2027-01-01";
    rows.front().horizon_end_date = "2027-02-01";
    rows.front().classification_date = "2027-02-01";
    expect_invalid([&] {
        static_cast<void>(cf::evaluate_joint_cohort(
            package.config.analysis, package.portfolio, rows));
    }, "eligibility after as-of is rejected");

    rows = package.observations;
    rows.front().classification_date = "2026-09-01";
    expect_invalid([&] {
        static_cast<void>(cf::evaluate_joint_cohort(
            package.config.analysis, package.portfolio, rows));
    }, "matured classifications after as-of are rejected");

    rows = package.observations;
    rows[18U].horizon_end_date = "2026-08-30";
    expect_invalid([&] {
        static_cast<void>(cf::evaluate_joint_cohort(
            package.config.analysis, package.portfolio, rows));
    }, "not-yet-matured rows with due horizons are rejected");

    rows = package.observations;
    rows[18U].status = cf::JointCohortObservationStatus::Unresolved;
    expect_invalid([&] {
        static_cast<void>(cf::evaluate_joint_cohort(
            package.config.analysis, package.portfolio, rows));
    }, "unresolved rows before their horizon are rejected");

    rows = package.observations;
    cf::JointCohortAnalysisConfig late_taxonomy = package.config.analysis;
    late_taxonomy.scenario_taxonomy_frozen_date = "2025-03-01";
    expect_invalid([&] {
        static_cast<void>(cf::evaluate_joint_cohort(
            late_taxonomy, package.portfolio, rows));
    }, "taxonomy frozen after included entry is rejected");

    cf::JointCohortAnalysisConfig late_rule = package.config.analysis;
    late_rule.exclusion_rules.front().frozen_date = "2025-03-01";
    expect_invalid([&] {
        static_cast<void>(cf::evaluate_joint_cohort(
            late_rule, package.portfolio, rows));
    }, "exclusion rules frozen after excluded entry are rejected");

    rows.front().status =
        static_cast<cf::JointCohortObservationStatus>(255U);
    expect_invalid([&] {
        cf::validate_joint_cohort_ledger_syntax(rows);
    }, "invalid programmatic status enum is rejected");

    rows = package.observations;
    rows.front().observation_id =
        std::string("non-ascii-") + static_cast<char>(0xe9U);
    expect_invalid([&] {
        cf::validate_joint_cohort_ledger_syntax(rows);
    }, "non-ASCII identifier bytes are rejected deterministically");

    std::vector<cf::JointCohortObservation> concentrated =
        repeat_matured(100U, "common-success");
    cf::JointCohortAnalysisConfig concentrated_config =
        package.config.analysis;
    concentrated_config.population_frame_count = concentrated.size();
    concentrated_config.exclusion_rules.clear();
    const cf::JointCohortResult outside = cf::evaluate_joint_cohort(
        concentrated_config, package.portfolio, concentrated);
    check(outside.primary_outer_set_available &&
            !outside.portfolio_reference_within_primary_bounds &&
            !outside.financial_ranges_available &&
            outside.block_reason.find("outside") != std::string::npos,
        "an outside declared center blocks cleanly before ambiguity validation");

    rows = package.observations;
    rows.front().scenario_id = "unknown-portfolio-state";
    expect_invalid([&] {
        static_cast<void>(cf::evaluate_joint_cohort(
            package.config.analysis, package.portfolio, rows));
    }, "matured rows must name an exact portfolio scenario");

    std::vector<cf::JointCohortObservation> all_excluded(
        package.observations.end() - 2, package.observations.end());
    cf::JointCohortAnalysisConfig excluded_config = package.config.analysis;
    excluded_config.population_frame_count = all_excluded.size();
    const cf::JointCohortResult empty_denominator =
        cf::evaluate_joint_cohort(
            excluded_config, package.portfolio, all_excluded);
    check(!empty_denominator.primary_outer_set_available &&
            empty_denominator.included_observation_count == 0U &&
            empty_denominator.excluded_count == 2U &&
            empty_denominator.scenario_envelopes.size() == 4U,
        "all-excluded frames disclose rows and block the empty denominator");
}

[[nodiscard]] cf::PortfolioConfig large_projection_portfolio() {
    constexpr std::size_t project_count = 128U;
    constexpr std::size_t scenario_count = 8U;
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "pair projection resource-bound test";
    portfolio.projects.reserve(project_count);
    for (std::size_t project = 0U; project < project_count; ++project) {
        portfolio.projects.push_back(cf::PortfolioProject{
            "project-" + std::to_string(project + 1U),
            cf::ProjectStage::Research, 1.0});
    }
    portfolio.joint_scenarios.reserve(scenario_count);
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        cf::JointScenario state;
        state.id = "state-" + std::to_string(scenario + 1U);
        state.weight = 1.0 / static_cast<double>(scenario_count);
        state.project_paths.reserve(project_count);
        for (const cf::PortfolioProject& project : portfolio.projects) {
            cf::ProjectJointPath path;
            path.project_id = project.id;
            state.project_paths.push_back(std::move(path));
        }
        portfolio.joint_scenarios.push_back(std::move(state));
    }
    return portfolio;
}

void test_pair_projection_cap(const cf::JointCohortPackage& package) {
    cf::PortfolioConfig portfolio = large_projection_portfolio();
    cf::JointCohortAnalysisConfig config = package.config.analysis;
    config.population_frame_count = 1U;
    config.exclusion_rules.clear();
    const std::vector<cf::JointCohortObservation> rows{
        cf::JointCohortObservation{
            "large-unknown", "large-cluster", "2025-02-01",
            "2027-02-01", cf::JointCohortObservationStatus::NotYetMatured,
            "NONE", "NONE", "NONE", {}, {}}};
    const cf::JointCohortResult result =
        cf::evaluate_joint_cohort(config, portfolio, rows);
    check(result.financial_ranges_available &&
            result.project_impairment_probabilities.size() == 128U &&
            !result.pair_impairment_projections_available &&
            result.pair_impairment_probabilities.empty() &&
            !result.pair_impairment_projection_block_reason.empty() &&
            result.any_project_impairment_probability.has_value() &&
            result.all_projects_impairment_probability.has_value(),
        "pair witness cap omits only quadratic pairs while preserving core and linear projections");
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: joint_cohort_tests <fixture-directory>\n";
        return 2;
    }
    try {
        const cf::JointCohortPackage package = cf::load_joint_cohort_package(
            std::filesystem::path(argv[1]) / "cohort.cfg");
        test_fixture_candidate(package);
        test_complete_and_blocked_paths(package);
        test_goodman_published_numerical_regression();
        test_fail_closed_semantics(package);
        test_pair_projection_cap(package);
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        ++failures;
    }
    if (failures != 0) {
        std::cerr << failures << " joint-cohort test(s) failed\n";
        return 1;
    }
    std::cout << "joint-cohort core tests passed\n";
    return 0;
}
