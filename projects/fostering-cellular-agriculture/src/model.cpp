// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/model.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr double kNumericTolerance = 1.0e-10;
constexpr double kMaximumDispersion = 5.0;
constexpr double kMaximumScaleInput = 1.0e9;

struct AnnualState {
    double operating_fraction{0.0};
    double attempted_output_million_kg{0.0};
    double qualified_output_million_kg{0.0};
    double spot_price_per_kg{0.0};
    double variable_cost_per_kg{0.0};
    double fixed_opex_million{0.0};
};

struct ExogenousPath {
    double actual_capex_million{0.0};
    double construction_duration_years{0.0};
    std::vector<AnnualState> years{};
};

struct PathEvaluation {
    double project_npv_before_instruments_million{0.0};
    double project_npv_after_instruments_million{0.0};
    double sponsor_npv_after_financing_million{0.0};
    double total_qualified_output_million_kg{0.0};
    std::optional<double> minimum_dscr{};
    bool debt_defaulted{false};
    std::optional<double> debt_default_timing_years_after_close{};
    double debt_loss_at_default_million{0.0};
    double debt_loss_pv_million{0.0};
    double terminal_debt_balance_million{0.0};
    double instrument_net_receipts_pv_million{0.0};
    double instrument_net_receipts_after_default_pv_million{0.0};
    double offtake_repricing_pv_million{0.0};
    double price_support_net_settlement_pv_million{0.0};
    double completion_delay_cover_payout_pv_million{0.0};
    double upfront_fee_million{0.0};
    double positive_support_payout_pv_million{0.0};
};

class NormalGenerator {
public:
    explicit NormalGenerator(std::uint64_t seed) : engine_(seed) {}

    [[nodiscard]] double uniform_open_unit() {
        // Use 52 high-order bits so the half-step remains exactly representable
        // even where MSVC treats long double as double. The result is strictly
        // inside (0, 1), keeping Box-Muller away from log(0) without an
        // implementation-defined standard-library distribution.
        constexpr unsigned discarded_bits = 12U;
        constexpr int retained_bits = 52;
        const std::uint64_t bits = engine_() >> discarded_bits;
        return std::ldexp(static_cast<double>(bits) + 0.5, -retained_bits);
    }

    [[nodiscard]] double normal() {
        if (has_spare_) {
            has_spare_ = false;
            return spare_;
        }

        const double first = uniform_open_unit();
        const double second = uniform_open_unit();
        const double radius = std::sqrt(-2.0 * std::log(first));
        const double angle = 2.0 * std::numbers::pi * second;
        spare_ = radius * std::sin(angle);
        has_spare_ = true;
        return radius * std::cos(angle);
    }

private:
    std::mt19937_64 engine_;
    bool has_spare_{false};
    double spare_{0.0};
};

[[nodiscard]] bool finite(double value) noexcept {
    return std::isfinite(value);
}

void require_finite(double value, std::string_view name) {
    if (!finite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

void require_non_negative(double value, std::string_view name) {
    require_finite(value, name);
    if (value < 0.0) {
        throw std::invalid_argument(
            std::string(name) + " must be non-negative");
    }
}

void require_bounded_dispersion(double value, std::string_view name) {
    require_non_negative(value, name);
    if (value > kMaximumDispersion) {
        throw std::invalid_argument(
            std::string(name) + " must not exceed 5");
    }
}

void require_positive(double value, std::string_view name) {
    require_finite(value, name);
    if (value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be positive");
    }
}

void require_bounded_non_negative(double value, std::string_view name) {
    require_non_negative(value, name);
    if (value > kMaximumScaleInput) {
        throw std::invalid_argument(
            std::string(name) + " must not exceed 1000000000");
    }
}

void require_bounded_positive(double value, std::string_view name) {
    require_positive(value, name);
    if (value > kMaximumScaleInput) {
        throw std::invalid_argument(
            std::string(name) + " must not exceed 1000000000");
    }
}

void require_closed_unit(double value, std::string_view name) {
    require_finite(value, name);
    if (value < 0.0 || value > 1.0) {
        throw std::invalid_argument(
            std::string(name) + " must be within [0, 1]");
    }
}

void require_open_unit(double value, std::string_view name) {
    require_finite(value, name);
    if (value <= 0.0 || value >= 1.0) {
        throw std::invalid_argument(
            std::string(name) + " must be strictly between 0 and 1");
    }
}

[[nodiscard]] double logistic(double value) noexcept {
    if (value >= 0.0) {
        const double exponential = std::exp(-value);
        return 1.0 / (1.0 + exponential);
    }
    const double exponential = std::exp(value);
    return exponential / (1.0 + exponential);
}

[[nodiscard]] double logit(double probability) noexcept {
    return std::log(probability / (1.0 - probability));
}

[[nodiscard]] bool has_unsafe_audit_text(std::string_view value) noexcept {
    if (value.empty()) {
        return true;
    }
    if (std::isspace(static_cast<unsigned char>(value.front())) != 0 ||
        std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        return true;
    }
    return std::any_of(value.begin(), value.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return byte < 0x20U || byte == 0x7FU;
    });
}

[[nodiscard]] double mean_one_lognormal(
    double log_sigma, double standard_normal) noexcept {
    return std::exp(
        log_sigma * standard_normal - 0.5 * log_sigma * log_sigma);
}

[[nodiscard]] double combined_normal(
    double loading, double persistent, double idiosyncratic) noexcept {
    const double residual = std::sqrt(std::max(0.0, 1.0 - loading * loading));
    return loading * persistent + residual * idiosyncratic;
}

[[nodiscard]] double discount_factor(double rate, double year) {
    return std::pow(1.0 + rate, year);
}

[[nodiscard]] double annuity_payment(
    double principal, double rate, std::size_t years) {
    if (principal <= 0.0) {
        return 0.0;
    }
    if (rate <= kNumericTolerance) {
        return principal / static_cast<double>(years);
    }
    const double periods = static_cast<double>(years);
    return principal * rate / (1.0 - std::pow(1.0 + rate, -periods));
}

[[nodiscard]] ExogenousPath generate_path(
    const SimulationConfig& config, NormalGenerator& random) {
    const double loading = config.risk.persistent_factor_loading;
    const double persistent_technical = random.normal();
    const double persistent_market = random.normal();

    const double capex_latent = combined_normal(
        loading, persistent_technical, random.normal());
    const double duration_latent = combined_normal(
        loading, persistent_technical, random.normal());

    ExogenousPath path;
    path.actual_capex_million = config.facility.base_capex_million *
        mean_one_lognormal(config.risk.capex_log_sigma, capex_latent);
    path.construction_duration_years =
        config.facility.planned_construction_years * mean_one_lognormal(
            config.risk.construction_duration_log_sigma, duration_latent);
    path.years.reserve(config.facility.analysis_years);

    for (std::size_t index = 0U;
         index < config.facility.analysis_years; ++index) {
        const double year_start = static_cast<double>(index);
        const double year_end = year_start + 1.0;
        const double operating_start =
            std::max(year_start, path.construction_duration_years);
        const double operating_fraction =
            std::clamp(year_end - operating_start, 0.0, 1.0);

        AnnualState annual;
        annual.operating_fraction = operating_fraction;
        annual.spot_price_per_kg = config.facility.base_spot_price_per_kg;
        annual.variable_cost_per_kg =
            config.facility.base_variable_cost_per_kg;

        if (operating_fraction <= 0.0) {
            path.years.push_back(annual);
            continue;
        }

        const double technical_latent = combined_normal(
            loading, persistent_technical, random.normal());
        const double market_latent = combined_normal(
            loading, persistent_market, random.normal());
        const double joint_cost_latent =
            (technical_latent + market_latent) / std::sqrt(2.0);

        const double utilization = logistic(
            logit(config.facility.steady_state_utilization) -
            config.risk.utilization_logit_sigma * technical_latent);
        const double yield_multiplier = mean_one_lognormal(
            config.risk.biological_yield_log_sigma, -technical_latent);

        const double operating_midpoint =
            operating_start + 0.5 * operating_fraction;
        const double operating_age = std::max(
            0.0, operating_midpoint - path.construction_duration_years);
        const double ramp = std::clamp(
            config.facility.ramp_at_commercial_operation +
                config.facility.annual_ramp_increment * operating_age,
            0.0, 1.0);

        annual.attempted_output_million_kg =
            config.facility.annual_nameplate_output_million_kg *
            operating_fraction * ramp * utilization * yield_multiplier;

        double full_year_contamination_probability =
            config.risk.annual_contamination_probability;
        if (full_year_contamination_probability > 0.0 &&
            full_year_contamination_probability < 1.0) {
            full_year_contamination_probability = logistic(
                logit(full_year_contamination_probability) +
                config.risk.contamination_logit_sigma * technical_latent);
        }
        const double contamination_probability = 1.0 - std::pow(
            1.0 - full_year_contamination_probability,
            operating_fraction);
        const bool contaminated =
            random.uniform_open_unit() < contamination_probability;
        const double contamination_multiplier = contaminated
            ? 1.0 - config.risk.contamination_output_loss_fraction
            : 1.0;
        annual.qualified_output_million_kg =
            annual.attempted_output_million_kg * contamination_multiplier;

        annual.spot_price_per_kg =
            config.facility.base_spot_price_per_kg * mean_one_lognormal(
                config.risk.output_price_log_sigma, -market_latent);
        annual.variable_cost_per_kg =
            config.facility.base_variable_cost_per_kg * mean_one_lognormal(
                config.risk.variable_cost_log_sigma, joint_cost_latent);
        annual.fixed_opex_million =
            config.facility.base_fixed_opex_million * operating_fraction *
            mean_one_lognormal(
                config.risk.fixed_opex_log_sigma, technical_latent);
        path.years.push_back(annual);
    }
    return path;
}

[[nodiscard]] double capex_draw_for_year(
    const ExogenousPath& path, std::size_t index) noexcept {
    const double start = static_cast<double>(index);
    const double end = start + 1.0;
    const double overlap = std::max(
        0.0, std::min(end, path.construction_duration_years) - start);
    return path.actual_capex_million * overlap /
        path.construction_duration_years;
}

[[nodiscard]] std::size_t completion_cover_payment_year(
    double settlement_time_years, std::size_t analysis_years) noexcept {
    const double rounded_up = std::ceil(settlement_time_years);
    const auto candidate = rounded_up <= 1.0
        ? std::size_t{0U}
        : static_cast<std::size_t>(rounded_up - 1.0);
    return std::min(candidate, analysis_years - 1U);
}

struct AnnualInstrumentResult {
    double net_transfer_million{0.0};
    double offtake_repricing_million{0.0};
    double price_support_net_settlement_million{0.0};
    double positive_support_payout_million{0.0};
};

[[nodiscard]] AnnualInstrumentResult annual_instrument_result(
    const AnnualState& annual,
    const InstrumentTerms& terms,
    double& remaining_price_support_capacity_million) noexcept {
    AnnualInstrumentResult result;

    if (terms.offtake_fraction > 0.0) {
        result.offtake_repricing_million =
            annual.qualified_output_million_kg *
            terms.offtake_fraction *
            (terms.offtake_price_per_kg - annual.spot_price_per_kg);
        result.net_transfer_million += result.offtake_repricing_million;
    }

    if (terms.price_support_kind == PriceSupportKind::None ||
        terms.price_support_fraction <= 0.0 ||
        remaining_price_support_capacity_million <= 0.0) {
        return result;
    }

    double raw_settlement = annual.qualified_output_million_kg *
        terms.price_support_fraction *
        (terms.price_support_strike_per_kg - annual.spot_price_per_kg);
    if (terms.price_support_kind == PriceSupportKind::OneWayFloor) {
        raw_settlement = std::max(0.0, raw_settlement);
    }

    const double absolute_cap = std::min(
        terms.price_support_annual_cap_million,
        remaining_price_support_capacity_million);
    const double settlement = std::clamp(
        raw_settlement, -absolute_cap, absolute_cap);
    remaining_price_support_capacity_million -= std::abs(settlement);
    result.net_transfer_million += settlement;
    result.price_support_net_settlement_million += settlement;
    result.positive_support_payout_million += std::max(0.0, settlement);
    return result;
}

[[nodiscard]] PathEvaluation evaluate_path(
    const SimulationConfig& config,
    const ExogenousPath& path,
    const InstrumentTerms& terms) {
    PathEvaluation result;
    const double rate = config.facility.project_discount_rate;

    result.project_npv_after_instruments_million = -terms.upfront_fee_million;
    result.sponsor_npv_after_financing_million = -terms.upfront_fee_million;
    result.instrument_net_receipts_pv_million = -terms.upfront_fee_million;
    result.upfront_fee_million = terms.upfront_fee_million;

    // If construction extends past the model horizon, settle only on elapsed
    // delay at that horizon. This avoids using an eventual completion date that
    // was not yet observable at the modeled payment time.
    const double completion_cover_settlement_time = std::min(
        path.construction_duration_years,
        static_cast<double>(config.facility.analysis_years));
    const double delay = std::max(
        0.0, completion_cover_settlement_time -
            terms.completion_delay_trigger_years);
    const double completion_payout = std::min(
        terms.completion_delay_cover_cap_million,
        delay * terms.completion_payout_per_delay_year_million);
    const std::size_t completion_year =
        completion_cover_payment_year(
            completion_cover_settlement_time,
            config.facility.analysis_years);

    double remaining_price_support_capacity =
        terms.price_support_lifetime_cap_million;
    const double debt_commitment =
        config.debt.debt_fraction_of_base_capex *
        config.facility.base_capex_million;
    double debt_drawn = 0.0;
    double debt_balance = 0.0;
    double annual_debt_service = 0.0;
    double capex_drawn = 0.0;
    bool sponsor_has_project = true;
    const std::size_t service_start_year = static_cast<std::size_t>(
        std::ceil(path.construction_duration_years));

    for (std::size_t index = 0U; index < path.years.size(); ++index) {
        const bool defaulted_before_period = result.debt_defaulted;
        const AnnualState& annual = path.years[index];
        const double year = static_cast<double>(index + 1U);
        const double factor = discount_factor(rate, year);
        const double capex_draw = capex_draw_for_year(path, index);
        capex_drawn += capex_draw;

        const double base_revenue = annual.qualified_output_million_kg *
            annual.spot_price_per_kg;
        const double variable_cost = annual.attempted_output_million_kg *
            annual.variable_cost_per_kg;
        const double base_cfads =
            base_revenue - variable_cost - annual.fixed_opex_million;

        AnnualInstrumentResult instrument = annual_instrument_result(
            annual, terms, remaining_price_support_capacity);
        if (index == completion_year && completion_payout > 0.0) {
            instrument.net_transfer_million += completion_payout;
            instrument.positive_support_payout_million += completion_payout;
        }

        result.project_npv_before_instruments_million +=
            (-capex_draw + base_cfads) / factor;
        result.project_npv_after_instruments_million +=
            (-capex_draw + base_cfads + instrument.net_transfer_million) /
            factor;
        result.instrument_net_receipts_pv_million +=
            instrument.net_transfer_million / factor;
        if (defaulted_before_period) {
            result.instrument_net_receipts_after_default_pv_million +=
                instrument.net_transfer_million / factor;
        }
        result.offtake_repricing_pv_million +=
            instrument.offtake_repricing_million / factor;
        result.price_support_net_settlement_pv_million +=
            instrument.price_support_net_settlement_million / factor;
        if (index == completion_year && completion_payout > 0.0) {
            result.completion_delay_cover_payout_pv_million +=
                completion_payout / factor;
        }
        result.positive_support_payout_pv_million +=
            instrument.positive_support_payout_million / factor;
        result.total_qualified_output_million_kg +=
            annual.qualified_output_million_kg;

        const double opening_debt_balance = debt_balance;
        const double remaining_commitment =
            std::max(0.0, debt_commitment - debt_drawn);
        const double requested_debt_draw =
            config.debt.debt_fraction_of_base_capex * capex_draw;
        const double debt_draw =
            std::min(remaining_commitment, requested_debt_draw);
        debt_drawn += debt_draw;
        debt_balance += debt_draw;

        double sponsor_cash_flow = -capex_draw + debt_draw;
        const double instrument_adjusted_cfads =
            base_cfads + instrument.net_transfer_million;

        if (!result.debt_defaulted && index < service_start_year) {
            const double construction_interest =
                (opening_debt_balance + 0.5 * debt_draw) *
                config.debt.annual_interest_rate;
            debt_balance += construction_interest;
            if (sponsor_has_project) {
                sponsor_cash_flow += instrument_adjusted_cfads;
            }
        } else if (!result.debt_defaulted &&
                   debt_balance > kNumericTolerance) {
            if (annual_debt_service <= 0.0) {
                annual_debt_service = annuity_payment(
                    debt_balance, config.debt.annual_interest_rate,
                    config.debt.tenor_years);
            }

            const double interest =
                debt_balance * config.debt.annual_interest_rate;
            const double scheduled_payment =
                std::min(annual_debt_service, debt_balance + interest);
            const double dscr = scheduled_payment > 0.0
                ? instrument_adjusted_cfads / scheduled_payment
                : std::numeric_limits<double>::infinity();
            result.minimum_dscr = result.minimum_dscr.has_value()
                ? std::min(*result.minimum_dscr, dscr)
                : dscr;

            const double cash_available =
                std::max(0.0, instrument_adjusted_cfads);
            const double payment = std::min(cash_available, scheduled_payment);
            if (payment + kNumericTolerance < scheduled_payment) {
                result.debt_defaulted = true;
                result.debt_default_timing_years_after_close = year;
                sponsor_has_project = false;
                const double exposure =
                    std::max(0.0, debt_balance + interest - payment);
                result.debt_loss_at_default_million = exposure *
                    (1.0 - config.debt.recovery_fraction_after_default);
                result.debt_loss_pv_million =
                    result.debt_loss_at_default_million / factor;
                debt_balance = 0.0;
            } else {
                const double principal_payment =
                    std::max(0.0, payment - interest);
                debt_balance =
                    std::max(0.0, debt_balance - principal_payment);
                if (sponsor_has_project) {
                    sponsor_cash_flow +=
                        instrument_adjusted_cfads - payment;
                }
            }
        } else if (sponsor_has_project) {
            sponsor_cash_flow += instrument_adjusted_cfads;
        }

        result.sponsor_npv_after_financing_million +=
            sponsor_cash_flow / factor;
    }

    const double terminal_factor = discount_factor(
        rate, static_cast<double>(config.facility.analysis_years));
    const double undrawn_capex =
        std::max(0.0, path.actual_capex_million - capex_drawn);
    if (undrawn_capex > 0.0) {
        result.project_npv_before_instruments_million -=
            undrawn_capex / terminal_factor;
        result.project_npv_after_instruments_million -=
            undrawn_capex / terminal_factor;
        if (sponsor_has_project) {
            result.sponsor_npv_after_financing_million -=
                undrawn_capex / terminal_factor;
        }
    }
    if (sponsor_has_project && debt_balance > kNumericTolerance) {
        result.terminal_debt_balance_million = debt_balance;
        result.sponsor_npv_after_financing_million -=
            debt_balance / terminal_factor;
    }
    return result;
}

void require_finite_evaluation(const PathEvaluation& evaluation) {
    const double values[]{
        evaluation.project_npv_before_instruments_million,
        evaluation.project_npv_after_instruments_million,
        evaluation.sponsor_npv_after_financing_million,
        evaluation.total_qualified_output_million_kg,
        evaluation.debt_loss_at_default_million,
        evaluation.debt_loss_pv_million,
        evaluation.terminal_debt_balance_million,
        evaluation.instrument_net_receipts_pv_million,
        evaluation.instrument_net_receipts_after_default_pv_million,
        evaluation.offtake_repricing_pv_million,
        evaluation.price_support_net_settlement_pv_million,
        evaluation.completion_delay_cover_payout_pv_million,
        evaluation.upfront_fee_million,
        evaluation.positive_support_payout_pv_million,
    };
    if (std::any_of(std::begin(values), std::end(values),
            [](double value) { return !finite(value); }) ||
        (evaluation.minimum_dscr.has_value() &&
         !finite(*evaluation.minimum_dscr)) ||
        (evaluation.debt_default_timing_years_after_close.has_value() &&
         !finite(*evaluation.debt_default_timing_years_after_close))) {
        throw std::overflow_error(
            "simulation produced a non-finite path result; review input scale");
    }
}

[[nodiscard]] double quantile_sorted(
    const std::vector<double>& sorted, double probability) {
    if (sorted.empty()) {
        throw std::invalid_argument("cannot summarize an empty sample");
    }
    if (sorted.size() == 1U) {
        return sorted.front();
    }
    const double position =
        probability * static_cast<double>(sorted.size() - 1U);
    const auto lower_index = static_cast<std::size_t>(std::floor(position));
    const auto upper_index = static_cast<std::size_t>(std::ceil(position));
    const double weight = position - static_cast<double>(lower_index);
    return sorted[lower_index] * (1.0 - weight) +
        sorted[upper_index] * weight;
}

[[nodiscard]] DistributionSummary summarize(
    const std::vector<double>& values) {
    if (values.empty()) {
        throw std::invalid_argument("cannot summarize an empty sample");
    }

    DistributionSummary summary;
    double sum = 0.0;
    double sum_compensation = 0.0;
    for (const double value : values) {
        if (!finite(value)) {
            throw std::overflow_error(
                "cannot summarize a non-finite simulation result");
        }
        const double corrected = value - sum_compensation;
        const double updated = sum + corrected;
        sum_compensation = (updated - sum) - corrected;
        sum = updated;
    }
    summary.mean = sum / static_cast<double>(values.size());

    double squared_deviation_sum = 0.0;
    double squared_deviation_compensation = 0.0;
    for (const double value : values) {
        const double deviation = value - summary.mean;
        const double square = deviation * deviation;
        const double corrected = square - squared_deviation_compensation;
        const double updated = squared_deviation_sum + corrected;
        squared_deviation_compensation =
            (updated - squared_deviation_sum) - corrected;
        squared_deviation_sum = updated;
    }
    summary.standard_deviation = std::sqrt(
        squared_deviation_sum / static_cast<double>(values.size()));

    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    summary.p05 = quantile_sorted(sorted, 0.05);
    summary.p50 = quantile_sorted(sorted, 0.50);
    summary.p95 = quantile_sorted(sorted, 0.95);

    std::vector<double> shortfalls;
    shortfalls.reserve(values.size());
    for (const double value : values) {
        shortfalls.push_back(std::max(0.0, -value));
    }
    std::sort(shortfalls.begin(), shortfalls.end());
    summary.shortfall_value_at_risk_95 =
        quantile_sorted(shortfalls, 0.95);
    const std::size_t tail_count = std::max<std::size_t>(
        1U, static_cast<std::size_t>(
                std::ceil(0.05 * static_cast<double>(shortfalls.size()))));
    double tail_sum = 0.0;
    double tail_compensation = 0.0;
    for (std::size_t offset = 0U; offset < tail_count; ++offset) {
        const double value = shortfalls[shortfalls.size() - 1U - offset];
        const double corrected = value - tail_compensation;
        const double updated = tail_sum + corrected;
        tail_compensation = (updated - tail_sum) - corrected;
        tail_sum = updated;
    }
    summary.shortfall_expected_shortfall_95 =
        tail_sum / static_cast<double>(tail_count);
    return summary;
}

[[nodiscard]] double probability_negative(
    const std::vector<double>& values) noexcept {
    const auto count = std::count_if(
        values.begin(), values.end(), [](double value) { return value < 0.0; });
    return static_cast<double>(count) / static_cast<double>(values.size());
}

[[nodiscard]] SimulationSummary summarize_evaluations(
    const std::vector<PathEvaluation>& evaluations,
    const std::vector<double>& capex,
    const std::vector<double>& operation_years) {
    SimulationSummary result;
    result.trials = evaluations.size();

    std::vector<double> project_before;
    std::vector<double> project_after;
    std::vector<double> sponsor;
    std::vector<double> output;
    std::vector<double> minimum_dscr;
    std::vector<double> default_timing;
    project_before.reserve(evaluations.size());
    project_after.reserve(evaluations.size());
    sponsor.reserve(evaluations.size());
    output.reserve(evaluations.size());
    minimum_dscr.reserve(evaluations.size());
    default_timing.reserve(evaluations.size());

    std::size_t defaults = 0U;
    std::size_t terminal_debt_paths = 0U;
    for (const PathEvaluation& evaluation : evaluations) {
        project_before.push_back(
            evaluation.project_npv_before_instruments_million);
        project_after.push_back(
            evaluation.project_npv_after_instruments_million);
        sponsor.push_back(evaluation.sponsor_npv_after_financing_million);
        output.push_back(evaluation.total_qualified_output_million_kg);
        if (evaluation.minimum_dscr.has_value()) {
            minimum_dscr.push_back(*evaluation.minimum_dscr);
        }
        defaults += evaluation.debt_defaulted ? 1U : 0U;
        if (evaluation.debt_defaulted) {
            if (!evaluation.debt_default_timing_years_after_close.has_value()) {
                throw std::logic_error(
                    "a defaulted path is missing its default timing");
            }
            default_timing.push_back(
                *evaluation.debt_default_timing_years_after_close);
        }
        terminal_debt_paths +=
            evaluation.terminal_debt_balance_million > kNumericTolerance
            ? 1U
            : 0U;
        result.unconditional_expected_debt_loss_at_default_date_million +=
            evaluation.debt_loss_at_default_million;
        result.unconditional_expected_debt_loss_pv_million +=
            evaluation.debt_loss_pv_million;
        result.expected_terminal_debt_balance_million +=
            evaluation.terminal_debt_balance_million;
        result.expected_instrument_net_receipts_pv_million +=
            evaluation.instrument_net_receipts_pv_million;
        result.expected_instrument_net_receipts_after_default_pv_million +=
            evaluation.instrument_net_receipts_after_default_pv_million;
        result.expected_offtake_repricing_pv_million +=
            evaluation.offtake_repricing_pv_million;
        result.expected_price_support_net_settlement_pv_million +=
            evaluation.price_support_net_settlement_pv_million;
        result.expected_completion_delay_cover_payout_pv_million +=
            evaluation.completion_delay_cover_payout_pv_million;
        result.expected_upfront_fee_million +=
            evaluation.upfront_fee_million;
        result.expected_positive_support_payout_pv_million +=
            evaluation.positive_support_payout_pv_million;
    }

    const double sample_count = static_cast<double>(evaluations.size());
    result.project_npv_before_instruments_million = summarize(project_before);
    result.project_npv_after_instruments_million = summarize(project_after);
    result.sponsor_npv_after_financing_million = summarize(sponsor);
    result.actual_capex_million = summarize(capex);
    result.commercial_operation_timing_years_after_close =
        summarize(operation_years);
    result.total_qualified_output_million_kg = summarize(output);
    if (!minimum_dscr.empty()) {
        result.minimum_dscr = summarize(minimum_dscr);
    }
    if (!default_timing.empty()) {
        result.debt_default_timing_years_after_close =
            summarize(default_timing);
    }
    result.probability_project_npv_negative = probability_negative(project_after);
    result.probability_sponsor_npv_negative = probability_negative(sponsor);
    result.debt_default_probability =
        static_cast<double>(defaults) / sample_count;
    result.fraction_paths_with_debt_service =
        static_cast<double>(minimum_dscr.size()) / sample_count;
    result.probability_terminal_debt_outstanding =
        static_cast<double>(terminal_debt_paths) / sample_count;
    if (defaults > 0U) {
        result.mean_debt_loss_given_default_at_default_date_million =
            result.unconditional_expected_debt_loss_at_default_date_million /
            static_cast<double>(defaults);
    }
    result.unconditional_expected_debt_loss_at_default_date_million /=
        sample_count;
    result.unconditional_expected_debt_loss_pv_million /= sample_count;
    result.expected_terminal_debt_balance_million /= sample_count;
    result.expected_instrument_net_receipts_pv_million /= sample_count;
    result.expected_instrument_net_receipts_after_default_pv_million /=
        sample_count;
    result.expected_offtake_repricing_pv_million /= sample_count;
    result.expected_price_support_net_settlement_pv_million /= sample_count;
    result.expected_completion_delay_cover_payout_pv_million /= sample_count;
    result.expected_upfront_fee_million /= sample_count;
    result.expected_positive_support_payout_pv_million /= sample_count;
    return result;
}

} // namespace

std::string_view to_string(PriceSupportKind kind) noexcept {
    switch (kind) {
    case PriceSupportKind::None:
        return "none";
    case PriceSupportKind::OneWayFloor:
        return "one-way-floor";
    case PriceSupportKind::TwoWayDifference:
        return "two-way-difference";
    }
    return "unknown";
}

void validate_config(const SimulationConfig& config) {
    if (config.model_version != kModelVersion) {
        throw std::invalid_argument(
            "scenario model version does not match this engine (expected " +
            std::string(kModelVersion) + ")");
    }
    if (!config.synthetic_inputs) {
        throw std::invalid_argument(
            "version 0.1 accepts synthetic illustrations only; empirical "
            "validation governance has not been completed");
    }
    if (has_unsafe_audit_text(config.scenario_label) ||
        has_unsafe_audit_text(config.source_note) ||
        has_unsafe_audit_text(config.currency_label) ||
        has_unsafe_audit_text(config.monetary_basis)) {
        throw std::invalid_argument(
            "scenario text fields must be non-empty single-line text without "
            "surrounding or control whitespace");
    }
    if (config.currency_label.size() > 16U) {
        throw std::invalid_argument(
            "currency label must contain at most 16 characters");
    }
    if (config.monetary_basis != "real" &&
        config.monetary_basis != "nominal" &&
        config.monetary_basis != "unspecified-synthetic") {
        throw std::invalid_argument(
            "monetary basis must be real, nominal, or "
            "unspecified-synthetic");
    }
    if (config.trials == 0U || config.trials > 10'000'000U) {
        throw std::invalid_argument(
            "simulation trials must be within [1, 10000000]");
    }

    const FacilityAssumptions& facility = config.facility;
    if (facility.analysis_years < 3U || facility.analysis_years > 100U) {
        throw std::invalid_argument(
            "analysis years must be within [3, 100]");
    }
    require_positive(
        facility.planned_construction_years,
        "planned construction years");
    require_bounded_positive(facility.base_capex_million, "base capex");
    require_bounded_positive(
        facility.annual_nameplate_output_million_kg,
        "annual nameplate output");
    require_open_unit(
        facility.steady_state_utilization, "steady-state utilization");
    require_closed_unit(
        facility.ramp_at_commercial_operation,
        "ramp at commercial operation");
    require_closed_unit(
        facility.annual_ramp_increment, "annual ramp increment");
    require_bounded_non_negative(
        facility.base_spot_price_per_kg, "base spot price");
    require_bounded_non_negative(
        facility.base_variable_cost_per_kg, "base variable cost");
    require_bounded_non_negative(
        facility.base_fixed_opex_million, "base fixed opex");
    require_finite(facility.project_discount_rate, "project discount rate");
    if (facility.project_discount_rate < 0.0 ||
        facility.project_discount_rate > 2.0) {
        throw std::invalid_argument(
            "project discount rate must be within [0, 2]");
    }
    const double minimum_horizon =
        std::ceil(facility.planned_construction_years) +
        (config.debt.debt_fraction_of_base_capex > 0.0
            ? static_cast<double>(config.debt.tenor_years)
            : 0.0) +
        1.0;
    if (static_cast<double>(facility.analysis_years) < minimum_horizon) {
        throw std::invalid_argument(
            "analysis horizon must cover planned construction, debt tenor, "
            "and one additional year");
    }

    const RiskAssumptions& risk = config.risk;
    require_bounded_dispersion(risk.capex_log_sigma, "capex log sigma");
    require_bounded_dispersion(
        risk.construction_duration_log_sigma,
        "construction-duration log sigma");
    require_bounded_dispersion(
        risk.utilization_logit_sigma, "utilization logit sigma");
    require_bounded_dispersion(
        risk.biological_yield_log_sigma, "biological-yield log sigma");
    require_bounded_dispersion(
        risk.output_price_log_sigma, "output-price log sigma");
    require_bounded_dispersion(
        risk.variable_cost_log_sigma, "variable-cost log sigma");
    require_bounded_dispersion(
        risk.fixed_opex_log_sigma, "fixed-opex log sigma");
    require_closed_unit(
        risk.annual_contamination_probability,
        "annual contamination probability");
    require_bounded_dispersion(
        risk.contamination_logit_sigma, "contamination logit sigma");
    require_closed_unit(
        risk.contamination_output_loss_fraction,
        "contamination output loss fraction");
    require_closed_unit(
        risk.persistent_factor_loading, "persistent factor loading");
    if (risk.persistent_factor_loading >= 1.0) {
        throw std::invalid_argument(
            "persistent factor loading must be less than 1");
    }

    const DebtTerms& debt = config.debt;
    require_closed_unit(
        debt.debt_fraction_of_base_capex, "debt fraction");
    require_non_negative(debt.annual_interest_rate, "debt interest rate");
    if (debt.annual_interest_rate > 2.0) {
        throw std::invalid_argument("debt interest rate must not exceed 2");
    }
    if (debt.tenor_years == 0U || debt.tenor_years > 100U) {
        throw std::invalid_argument("debt tenor must be within [1, 100]");
    }
    require_closed_unit(
        debt.recovery_fraction_after_default, "debt recovery fraction");
    if (!debt.assume_terminal_balance_paid_by_sponsor) {
        throw std::invalid_argument(
            "version 0.1 requires the terminal sponsor-payment convention; "
            "a terminal default waterfall is not implemented");
    }

    const InstrumentTerms& instrument = config.instrument;
    switch (instrument.price_support_kind) {
    case PriceSupportKind::None:
    case PriceSupportKind::OneWayFloor:
    case PriceSupportKind::TwoWayDifference:
        break;
    default:
        throw std::invalid_argument("unknown price-support kind");
    }
    require_closed_unit(instrument.offtake_fraction, "offtake fraction");
    require_bounded_non_negative(
        instrument.offtake_price_per_kg, "offtake price");
    if (instrument.offtake_fraction > 0.0 &&
        instrument.offtake_price_per_kg <= 0.0) {
        throw std::invalid_argument(
            "a positive offtake fraction requires a positive price");
    }
    require_closed_unit(
        instrument.price_support_fraction, "price-support fraction");
    require_bounded_non_negative(
        instrument.price_support_strike_per_kg,
        "price-support strike");
    require_bounded_non_negative(
        instrument.price_support_annual_cap_million,
        "price-support annual cap");
    require_bounded_non_negative(
        instrument.price_support_lifetime_cap_million,
        "price-support lifetime cap");
    if (instrument.price_support_kind == PriceSupportKind::None) {
        if (instrument.price_support_fraction != 0.0 ||
            instrument.price_support_strike_per_kg != 0.0 ||
            instrument.price_support_annual_cap_million != 0.0 ||
            instrument.price_support_lifetime_cap_million != 0.0) {
            throw std::invalid_argument(
                "disabled price support requires zero support terms");
        }
    } else if (instrument.price_support_fraction <= 0.0 ||
               instrument.price_support_strike_per_kg <= 0.0 ||
               instrument.price_support_annual_cap_million <= 0.0 ||
               instrument.price_support_lifetime_cap_million <= 0.0) {
        throw std::invalid_argument(
            "enabled price support requires positive fraction, strike, "
            "annual cap, and lifetime cap");
    }
    if (instrument.offtake_fraction + instrument.price_support_fraction >
        1.0 + kNumericTolerance) {
        throw std::invalid_argument(
            "offtake and price-support fractions may not double-cover output");
    }
    require_bounded_non_negative(
        instrument.completion_delay_trigger_years,
        "completion-delay trigger");
    require_bounded_non_negative(
        instrument.completion_payout_per_delay_year_million,
        "completion payout rate");
    require_bounded_non_negative(
        instrument.completion_delay_cover_cap_million,
        "completion-delay cover cap");
    const bool any_completion_term =
        instrument.completion_payout_per_delay_year_million > 0.0 ||
        instrument.completion_delay_cover_cap_million > 0.0;
    const bool complete_completion_terms =
        instrument.completion_payout_per_delay_year_million > 0.0 &&
        instrument.completion_delay_cover_cap_million > 0.0;
    if (any_completion_term != complete_completion_terms) {
        throw std::invalid_argument(
            "completion protection requires both a payout rate and a cap");
    }
    require_bounded_non_negative(
        instrument.upfront_fee_million, "upfront fee");
}

ComparisonSummary run_paired_simulation(const SimulationConfig& config) {
    validate_config(config);
    NormalGenerator random(config.seed);

    std::vector<PathEvaluation> baseline;
    std::vector<PathEvaluation> structured;
    std::vector<double> capex;
    std::vector<double> operation_years;
    std::vector<double> project_change;
    std::vector<double> sponsor_change;
    std::vector<double> instrument_transfer;
    std::vector<double> default_timing_change;
    baseline.reserve(config.trials);
    structured.reserve(config.trials);
    capex.reserve(config.trials);
    operation_years.reserve(config.trials);
    project_change.reserve(config.trials);
    sponsor_change.reserve(config.trials);
    instrument_transfer.reserve(config.trials);
    default_timing_change.reserve(config.trials);

    double default_change_sum = 0.0;
    std::size_t project_negative_to_nonnegative = 0U;
    std::size_t project_nonnegative_to_negative = 0U;
    std::size_t sponsor_negative_to_nonnegative = 0U;
    std::size_t sponsor_nonnegative_to_negative = 0U;
    std::size_t defaults_avoided_within_horizon = 0U;
    std::size_t defaults_introduced_within_horizon = 0U;
    std::size_t defaults_delayed = 0U;
    std::size_t defaults_accelerated = 0U;
    std::size_t defaults_same_timing = 0U;
    const InstrumentTerms empty_terms{};
    for (std::size_t trial = 0U; trial < config.trials; ++trial) {
        const ExogenousPath path = generate_path(config, random);
        const PathEvaluation no_instrument =
            evaluate_path(config, path, empty_terms);
        const PathEvaluation with_instrument =
            evaluate_path(config, path, config.instrument);
        require_finite_evaluation(no_instrument);
        require_finite_evaluation(with_instrument);

        baseline.push_back(no_instrument);
        structured.push_back(with_instrument);
        capex.push_back(path.actual_capex_million);
        operation_years.push_back(path.construction_duration_years);
        project_change.push_back(
            with_instrument.project_npv_after_instruments_million -
            no_instrument.project_npv_after_instruments_million);
        sponsor_change.push_back(
            with_instrument.sponsor_npv_after_financing_million -
            no_instrument.sponsor_npv_after_financing_million);
        instrument_transfer.push_back(
            with_instrument.instrument_net_receipts_pv_million);
        const bool project_was_negative =
            no_instrument.project_npv_after_instruments_million < 0.0;
        const bool project_is_negative =
            with_instrument.project_npv_after_instruments_million < 0.0;
        project_negative_to_nonnegative +=
            project_was_negative && !project_is_negative ? 1U : 0U;
        project_nonnegative_to_negative +=
            !project_was_negative && project_is_negative ? 1U : 0U;

        const bool sponsor_was_negative =
            no_instrument.sponsor_npv_after_financing_million < 0.0;
        const bool sponsor_is_negative =
            with_instrument.sponsor_npv_after_financing_million < 0.0;
        sponsor_negative_to_nonnegative +=
            sponsor_was_negative && !sponsor_is_negative ? 1U : 0U;
        sponsor_nonnegative_to_negative +=
            !sponsor_was_negative && sponsor_is_negative ? 1U : 0U;

        defaults_avoided_within_horizon +=
            no_instrument.debt_defaulted && !with_instrument.debt_defaulted
            ? 1U
            : 0U;
        defaults_introduced_within_horizon +=
            !no_instrument.debt_defaulted && with_instrument.debt_defaulted
            ? 1U
            : 0U;
        if (no_instrument.debt_defaulted && with_instrument.debt_defaulted) {
            if (!no_instrument.debt_default_timing_years_after_close.has_value() ||
                !with_instrument.debt_default_timing_years_after_close.has_value()) {
                throw std::logic_error(
                    "a paired default is missing its default timing");
            }
            const double timing_change =
                *with_instrument.debt_default_timing_years_after_close -
                *no_instrument.debt_default_timing_years_after_close;
            default_timing_change.push_back(timing_change);
            defaults_delayed += timing_change > kNumericTolerance ? 1U : 0U;
            defaults_accelerated +=
                timing_change < -kNumericTolerance ? 1U : 0U;
            defaults_same_timing +=
                std::abs(timing_change) <= kNumericTolerance ? 1U : 0U;
        }
        default_change_sum +=
            (with_instrument.debt_defaulted ? 1.0 : 0.0) -
            (no_instrument.debt_defaulted ? 1.0 : 0.0);
    }

    ComparisonSummary result;
    result.without_instrument =
        summarize_evaluations(baseline, capex, operation_years);
    result.with_instrument =
        summarize_evaluations(structured, capex, operation_years);
    result.paired_project_npv_change_million = summarize(project_change);
    result.paired_sponsor_npv_change_million = summarize(sponsor_change);
    result.paired_instrument_transfer_pv_million =
        summarize(instrument_transfer);
    result.paired_default_probability_change =
        default_change_sum / static_cast<double>(config.trials);
    if (!default_timing_change.empty()) {
        result.paired_default_timing_change_years =
            summarize(default_timing_change);
    }
    result.project_npv_negative_to_nonnegative_count =
        project_negative_to_nonnegative;
    result.project_npv_nonnegative_to_negative_count =
        project_nonnegative_to_negative;
    result.sponsor_npv_negative_to_nonnegative_count =
        sponsor_negative_to_nonnegative;
    result.sponsor_npv_nonnegative_to_negative_count =
        sponsor_nonnegative_to_negative;
    result.debt_default_avoided_within_horizon_count =
        defaults_avoided_within_horizon;
    result.debt_default_introduced_within_horizon_count =
        defaults_introduced_within_horizon;
    result.debt_default_delayed_count = defaults_delayed;
    result.debt_default_accelerated_count = defaults_accelerated;
    result.debt_default_same_timing_count = defaults_same_timing;
    return result;
}

} // namespace naturalehia::cellular_finance
