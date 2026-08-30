// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio_config.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumParsedProjects = 128U;
constexpr std::size_t kMaximumParsedScenarios = 10'000U;
constexpr std::size_t kMaximumParsedFactorTags = 64U;
constexpr std::size_t kMaximumParsedCashSources = 256U;
constexpr std::size_t kMaximumParsedLossLayers = 128U;
constexpr std::size_t kMaximumParsedCashRecords = 2'000'000U;
constexpr std::size_t kMaximumParsedProjectScenarioPairs = 500'000U;
constexpr std::uintmax_t kMaximumConfigBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumConfigLineBytes = 4'096U;
constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF", 3U};

struct RawValue {
    std::string value{};
    std::size_t line{0U};
};

using RawMap = std::unordered_map<std::string, RawValue>;

struct ProjectShape {
    std::size_t draw_count{0U};
    std::size_t receipt_count{0U};
    std::size_t investor_outlay_count{0U};
    std::size_t principal_movement_count{0U};
};

struct CashSourceShape {
    std::size_t cash_count{0U};
};

struct ScenarioShape {
    std::size_t factor_tag_count{0U};
    std::size_t pool_cost_count{0U};
    std::vector<CashSourceShape> cash_sources{};
    std::vector<ProjectShape> projects{};
};

class OutputStateGuard {
public:
    explicit OutputStateGuard(std::ostream& output)
        : output_(output), flags_(output.flags()),
          precision_(output.precision()), width_(output.width()),
          fill_(output.fill()), locale_(output.getloc()) {}

    OutputStateGuard(const OutputStateGuard&) = delete;
    OutputStateGuard& operator=(const OutputStateGuard&) = delete;

    ~OutputStateGuard() noexcept {
        try {
            output_.flags(flags_);
            output_.precision(precision_);
            output_.width(width_);
            output_.fill(fill_);
            output_.imbue(locale_);
        } catch (...) {
            // Formatting restoration must not throw during stack unwinding.
        }
    }

private:
    std::ostream& output_;
    std::ios_base::fmtflags flags_;
    std::streamsize precision_;
    std::streamsize width_;
    char fill_;
    std::locale locale_;
};

[[nodiscard]] std::string_view trim_view(std::string_view value) noexcept {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1U);
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1U);
    }
    return value;
}

[[noreturn]] void parse_error(
    std::size_t line, std::string_view message) {
    throw std::invalid_argument(
        "portfolio configuration line " + std::to_string(line) +
        ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "portfolio configuration is missing required key: " + key);
    }
    return iterator->second;
}

[[nodiscard]] double parse_double(const RawValue& raw) {
    double result{};
    const char* const begin = raw.value.data();
    const char* const end = begin + raw.value.size();
    const auto conversion = std::from_chars(begin, end, result);
    if (raw.value.empty() || conversion.ec != std::errc{} ||
        conversion.ptr != end) {
        parse_error(raw.line, "expected a decimal number");
    }
    return result;
}

[[nodiscard]] std::uint64_t parse_unsigned(const RawValue& raw) {
    std::uint64_t result{};
    const char* const begin = raw.value.data();
    const char* const end = begin + raw.value.size();
    const auto conversion = std::from_chars(begin, end, result);
    if (raw.value.empty() || conversion.ec != std::errc{} ||
        conversion.ptr != end) {
        parse_error(raw.line, "expected a non-negative integer");
    }
    return result;
}

[[nodiscard]] std::size_t parse_size(const RawValue& raw) {
    const std::uint64_t parsed = parse_unsigned(raw);
    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        if (parsed > static_cast<std::uint64_t>(
                         std::numeric_limits<std::size_t>::max())) {
            parse_error(raw.line, "integer is too large for this platform");
        }
    }
    return static_cast<std::size_t>(parsed);
}

[[nodiscard]] bool parse_bool(const RawValue& raw) {
    if (raw.value == "true") {
        return true;
    }
    if (raw.value == "false") {
        return false;
    }
    parse_error(raw.line, "expected true or false");
}

[[nodiscard]] ProjectStage parse_stage(const RawValue& raw) {
    if (raw.value == "research") {
        return ProjectStage::Research;
    }
    if (raw.value == "pilot") {
        return ProjectStage::Pilot;
    }
    if (raw.value == "demonstration") {
        return ProjectStage::Demonstration;
    }
    if (raw.value == "first-industrial") {
        return ProjectStage::FirstIndustrial;
    }
    if (raw.value == "repeat-production") {
        return ProjectStage::RepeatProduction;
    }
    parse_error(raw.line,
        "expected research, pilot, demonstration, first-industrial, or repeat-production");
}

[[nodiscard]] ProjectPathResolution parse_resolution(const RawValue& raw) {
    if (raw.value == "resolved") {
        return ProjectPathResolution::Resolved;
    }
    if (raw.value == "continuing") {
        return ProjectPathResolution::Continuing;
    }
    parse_error(raw.line, "expected resolved or continuing");
}

[[nodiscard]] PrincipalAccountingMode parse_principal_accounting_mode(
    const RawValue& raw) {
    if (raw.value == "draw-equals-principal-legacy") {
        return PrincipalAccountingMode::DrawEqualsPrincipalLegacy;
    }
    if (raw.value == "explicit-contractual-ledger") {
        return PrincipalAccountingMode::ExplicitContractualLedger;
    }
    parse_error(raw.line,
        "expected draw-equals-principal-legacy or explicit-contractual-ledger");
}

[[nodiscard]] PrincipalMovementKind parse_principal_movement_kind(
    const RawValue& raw) {
    if (raw.value == "funded-principal-addition") {
        return PrincipalMovementKind::FundedPrincipalAddition;
    }
    if (raw.value == "capitalized-fee-addition") {
        return PrincipalMovementKind::CapitalizedFeeAddition;
    }
    if (raw.value == "capitalized-interest-addition") {
        return PrincipalMovementKind::CapitalizedInterestAddition;
    }
    if (raw.value == "conversion-extinguishment") {
        return PrincipalMovementKind::ConversionExtinguishment;
    }
    if (raw.value == "writeoff") {
        return PrincipalMovementKind::Writeoff;
    }
    parse_error(raw.line,
        "expected a defined principal-movement kind");
}

[[nodiscard]] InvestorOutlayPurpose parse_investor_outlay_purpose(
    const RawValue& raw) {
    if (raw.value == "primary-project-funding") {
        return InvestorOutlayPurpose::PrimaryProjectFunding;
    }
    if (raw.value == "claim-purchase-price") {
        return InvestorOutlayPurpose::ClaimPurchasePrice;
    }
    if (raw.value == "buyer-direct-cost") {
        return InvestorOutlayPurpose::BuyerDirectCost;
    }
    parse_error(raw.line, "expected a defined investor-outlay purpose");
}

[[nodiscard]] PortfolioCashSource parse_cash_source_kind(
    const RawValue& raw) {
    if (raw.value == "commercial") {
        return PortfolioCashSource::Commercial;
    }
    if (raw.value == "licensing-royalty") {
        return PortfolioCashSource::LicensingRoyalty;
    }
    if (raw.value == "exit-sale") {
        return PortfolioCashSource::ExitSale;
    }
    if (raw.value == "recovery") {
        return PortfolioCashSource::Recovery;
    }
    if (raw.value == "refinancing") {
        return PortfolioCashSource::Refinancing;
    }
    if (raw.value == "explicit-support") {
        return PortfolioCashSource::ExplicitSupport;
    }
    if (raw.value == "sponsor-fee") {
        return PortfolioCashSource::SponsorFee;
    }
    if (raw.value == "financing-fee") {
        return PortfolioCashSource::FinancingFee;
    }
    parse_error(raw.line,
        "expected a defined portfolio cash-source kind");
}

[[nodiscard]] std::string project_key(
    std::size_t project, std::string_view field) {
    return "project." + std::to_string(project + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string scenario_key(
    std::size_t scenario, std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string scenario_factor_tag_key(
    std::size_t scenario, std::size_t tag) {
    return "scenario." + std::to_string(scenario + 1U) +
        ".factor_tag." + std::to_string(tag + 1U);
}

[[nodiscard]] std::string scenario_pool_cost_key(std::size_t scenario,
    std::size_t cost, std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) +
        ".pool_cost." + std::to_string(cost + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string scenario_cash_source_key(std::size_t scenario,
    std::size_t source, std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) +
        ".cash_source." + std::to_string(source + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string scenario_cash_key(std::size_t scenario,
    std::size_t source, std::size_t cash, std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) +
        ".cash_source." + std::to_string(source + 1U) + ".cash." +
        std::to_string(cash + 1U) + "." + std::string(field);
}

[[nodiscard]] std::string scenario_project_key(std::size_t scenario,
    std::size_t project, std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) +
        ".project." + std::to_string(project + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string scenario_project_draw_key(std::size_t scenario,
    std::size_t project, std::size_t draw, std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) +
        ".project." + std::to_string(project + 1U) + ".draw." +
        std::to_string(draw + 1U) + "." + std::string(field);
}

[[nodiscard]] std::string scenario_project_investor_outlay_key(
    std::size_t scenario, std::size_t project, std::size_t outlay,
    std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) +
        ".project." + std::to_string(project + 1U) +
        ".investor_outlay." + std::to_string(outlay + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string scenario_project_principal_movement_key(
    std::size_t scenario, std::size_t project, std::size_t movement,
    std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) +
        ".project." + std::to_string(project + 1U) +
        ".principal_movement." + std::to_string(movement + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string scenario_project_receipt_key(
    std::size_t scenario, std::size_t project, std::size_t receipt,
    std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) +
        ".project." + std::to_string(project + 1U) + ".receipt." +
        std::to_string(receipt + 1U) + "." + std::string(field);
}

[[nodiscard]] std::string loss_layer_key(
    std::size_t layer, std::string_view field) {
    return "loss_layer." + std::to_string(layer + 1U) + "." +
        std::string(field);
}

void add_global_keys(std::unordered_set<std::string>& expected) {
    static constexpr std::string_view keys[] = {
        "portfolio.model_version",
        "portfolio.label",
        "portfolio.source_note",
        "portfolio.currency_label",
        "portfolio.monetary_basis",
        "portfolio.synthetic_inputs",
        "portfolio.horizon_months",
        "portfolio.annual_physical_hurdle_rate",
        "project.count",
        "scenario.count",
        "loss_layer.count",
    };
    for (const std::string_view key : keys) {
        expected.emplace(key);
    }
}

void require_expected_room(const std::unordered_set<std::string>& expected,
    std::size_t raw_size, std::size_t count, std::size_t fields_per_item) {
    if (expected.size() > raw_size ||
        count > (raw_size - expected.size()) / fields_per_item) {
        throw std::invalid_argument(
            "portfolio configuration declared counts require missing keys");
    }
}

void add_cash_record_count(
    std::size_t& total, std::size_t additional) {
    if (additional > kMaximumParsedCashRecords - total) {
        throw std::invalid_argument(
            "parsed aggregate cash record count exceeds 2000000");
    }
    total += additional;
}

[[nodiscard]] RawMap read_raw_stream(std::istream& input) {
    RawMap raw;
    std::string line_text;
    std::size_t line_number = 0U;
    std::size_t bytes_read = 0U;
    while (std::getline(input, line_text)) {
        ++line_number;
        if (line_text.size() > kMaximumConfigLineBytes) {
            parse_error(line_number,
                "configuration line exceeds the 4096-byte guardrail");
        }
        bytes_read += line_text.size() + 1U;
        if (bytes_read > kMaximumConfigBytes) {
            parse_error(line_number,
                "configuration exceeds the 16 MiB guardrail");
        }
        std::string_view line{line_text};
        if (line_number == 1U && line.starts_with(kUtf8Bom)) {
            line.remove_prefix(kUtf8Bom.size());
        }
        if (line.find(kUtf8Bom) != std::string_view::npos) {
            parse_error(line_number,
                "UTF-8 BOM is permitted only at the start of the file");
        }
        line = trim_view(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            parse_error(line_number, "expected key=value");
        }
        const std::string_view key = trim_view(line.substr(0U, equals));
        const std::string_view value = trim_view(line.substr(equals + 1U));
        if (key.empty() || value.empty()) {
            parse_error(line_number, "key and value must not be empty");
        }
        const auto [iterator, inserted] = raw.emplace(
            std::string(key), RawValue{std::string(value), line_number});
        if (!inserted) {
            parse_error(line_number, "duplicate key: " + iterator->first);
        }
    }
    if (!input.eof()) {
        throw std::runtime_error(
            "failed while reading portfolio configuration bytes");
    }
    return raw;
}

[[nodiscard]] RawMap read_raw(const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "portfolio configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open portfolio configuration file: " +
            path.string());
    }
    return read_raw_stream(input);
}

[[nodiscard]] RawMap read_raw_bytes(std::string_view bytes) {
    if (bytes.size() > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "portfolio configuration exceeds the 16 MiB guardrail");
    }
    std::istringstream input(
        std::string(bytes), std::ios::in | std::ios::binary);
    return read_raw_stream(input);
}

} // namespace

[[nodiscard]] PortfolioConfig parse_portfolio_config(const RawMap& raw) {
    const RawValue& model_version =
        required(raw, "portfolio.model_version");
    const bool legacy_schema =
        model_version.value == kPortfolioLegacyModelVersion;
    const bool current_schema = model_version.value == kPortfolioModelVersion;
    if (!legacy_schema && !current_schema) {
        parse_error(model_version.line,
            "expected portfolio model_version 0.1.0 or 0.2.0");
    }
    const std::size_t project_count =
        parse_size(required(raw, "project.count"));
    const std::size_t scenario_count =
        parse_size(required(raw, "scenario.count"));
    if (project_count == 0U || project_count > kMaximumParsedProjects) {
        throw std::invalid_argument(
            "parsed project count must be between one and 128");
    }
    if (scenario_count == 0U || scenario_count > kMaximumParsedScenarios) {
        throw std::invalid_argument(
            "parsed scenario count must be between one and 10000");
    }
    if (project_count >
        kMaximumParsedProjectScenarioPairs / scenario_count) {
        throw std::invalid_argument(
            "parsed project-scenario count exceeds 500000");
    }

    const std::size_t project_field_count = legacy_schema ? 3U : 6U;
    const std::size_t path_field_count = legacy_schema ? 4U : 6U;
    const std::size_t minimum_expected =
        11U + project_field_count * project_count +
        scenario_count * (5U + path_field_count * project_count);
    if (minimum_expected > raw.size()) {
        throw std::invalid_argument(
            "portfolio configuration declared counts require missing keys");
    }

    std::unordered_set<std::string> expected;
    expected.reserve(raw.size());
    add_global_keys(expected);
    for (std::size_t project = 0U; project < project_count; ++project) {
        expected.insert(project_key(project, "id"));
        expected.insert(project_key(project, "stage"));
        expected.insert(project_key(project, "commitment_million"));
        if (current_schema) {
            expected.insert(
                project_key(project, "principal_accounting_mode"));
            expected.insert(project_key(project, "principal_limit_million"));
            expected.insert(
                project_key(project, "opening_principal_million"));
        }
    }
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        expected.insert(scenario_key(scenario, "id"));
        expected.insert(scenario_key(scenario, "weight"));
        expected.insert(scenario_key(scenario, "factor_tag.count"));
        expected.insert(scenario_key(scenario, "pool_cost.count"));
        expected.insert(scenario_key(scenario, "cash_source.count"));
        for (std::size_t project = 0U; project < project_count; ++project) {
            expected.insert(
                scenario_project_key(scenario, project, "project_id"));
            expected.insert(
                scenario_project_key(scenario, project, "resolution"));
            expected.insert(
                scenario_project_key(scenario, project, "draw.count"));
            expected.insert(
                scenario_project_key(scenario, project, "receipt.count"));
            if (current_schema) {
                expected.insert(scenario_project_key(
                    scenario, project, "investor_outlay.count"));
                expected.insert(scenario_project_key(
                    scenario, project, "principal_movement.count"));
            }
        }
    }

    std::vector<ScenarioShape> shapes(scenario_count);
    std::size_t aggregate_cash_records = 0U;
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        ScenarioShape& shape = shapes[scenario];
        shape.factor_tag_count = parse_size(required(
            raw, scenario_key(scenario, "factor_tag.count")));
        if (shape.factor_tag_count > kMaximumParsedFactorTags) {
            throw std::invalid_argument(
                "parsed factor-tag count exceeds 64");
        }
        require_expected_room(
            expected, raw.size(), shape.factor_tag_count, 1U);
        for (std::size_t tag = 0U; tag < shape.factor_tag_count; ++tag) {
            expected.insert(scenario_factor_tag_key(scenario, tag));
        }

        shape.pool_cost_count = parse_size(required(
            raw, scenario_key(scenario, "pool_cost.count")));
        add_cash_record_count(aggregate_cash_records, shape.pool_cost_count);
        require_expected_room(
            expected, raw.size(), shape.pool_cost_count, 2U);
        for (std::size_t cost = 0U; cost < shape.pool_cost_count; ++cost) {
            expected.insert(
                scenario_pool_cost_key(scenario, cost, "month"));
            expected.insert(scenario_pool_cost_key(
                scenario, cost, "amount_million"));
        }

        const std::size_t cash_source_count = parse_size(required(
            raw, scenario_key(scenario, "cash_source.count")));
        if (cash_source_count > kMaximumParsedCashSources) {
            throw std::invalid_argument(
                "parsed scenario cash-source count exceeds 256");
        }
        require_expected_room(
            expected, raw.size(), cash_source_count, 3U);
        shape.cash_sources.resize(cash_source_count);
        for (std::size_t source = 0U; source < cash_source_count; ++source) {
            expected.insert(
                scenario_cash_source_key(scenario, source, "id"));
            expected.insert(
                scenario_cash_source_key(scenario, source, "kind"));
            expected.insert(
                scenario_cash_source_key(scenario, source, "cash.count"));
        }
        for (std::size_t source = 0U; source < cash_source_count; ++source) {
            CashSourceShape& source_shape = shape.cash_sources[source];
            source_shape.cash_count = parse_size(required(raw,
                scenario_cash_source_key(
                    scenario, source, "cash.count")));
            add_cash_record_count(
                aggregate_cash_records, source_shape.cash_count);
            require_expected_room(
                expected, raw.size(), source_shape.cash_count, 2U);
            for (std::size_t cash = 0U; cash < source_shape.cash_count;
                 ++cash) {
                expected.insert(
                    scenario_cash_key(scenario, source, cash, "month"));
                expected.insert(scenario_cash_key(
                    scenario, source, cash, "amount_million"));
            }
        }

        shape.projects.resize(project_count);
        for (std::size_t project = 0U; project < project_count; ++project) {
            ProjectShape& project_shape = shape.projects[project];
            project_shape.draw_count = parse_size(required(raw,
                scenario_project_key(scenario, project, "draw.count")));
            project_shape.receipt_count = parse_size(required(raw,
                scenario_project_key(
                    scenario, project, "receipt.count")));
            if (current_schema) {
                project_shape.investor_outlay_count = parse_size(required(raw,
                    scenario_project_key(
                        scenario, project, "investor_outlay.count")));
                project_shape.principal_movement_count = parse_size(required(
                    raw, scenario_project_key(
                             scenario, project,
                             "principal_movement.count")));
            }
            add_cash_record_count(
                aggregate_cash_records, project_shape.draw_count);
            add_cash_record_count(
                aggregate_cash_records, project_shape.receipt_count);
            add_cash_record_count(
                aggregate_cash_records, project_shape.investor_outlay_count);
            add_cash_record_count(aggregate_cash_records,
                project_shape.principal_movement_count);
            require_expected_room(
                expected, raw.size(), project_shape.draw_count, 2U);
            for (std::size_t draw = 0U; draw < project_shape.draw_count;
                 ++draw) {
                expected.insert(scenario_project_draw_key(
                    scenario, project, draw, "month"));
                expected.insert(scenario_project_draw_key(
                    scenario, project, draw, "amount_million"));
            }
            require_expected_room(expected, raw.size(),
                project_shape.investor_outlay_count, 3U);
            for (std::size_t outlay = 0U;
                 outlay < project_shape.investor_outlay_count; ++outlay) {
                expected.insert(scenario_project_investor_outlay_key(
                    scenario, project, outlay, "month"));
                expected.insert(scenario_project_investor_outlay_key(
                    scenario, project, outlay, "purpose"));
                expected.insert(scenario_project_investor_outlay_key(
                    scenario, project, outlay, "amount_million"));
            }
            require_expected_room(
                expected, raw.size(), project_shape.receipt_count, 4U);
            for (std::size_t receipt = 0U;
                 receipt < project_shape.receipt_count; ++receipt) {
                expected.insert(scenario_project_receipt_key(
                    scenario, project, receipt, "month"));
                expected.insert(scenario_project_receipt_key(
                    scenario, project, receipt, "cash_source_id"));
                expected.insert(scenario_project_receipt_key(
                    scenario, project, receipt, "amount_million"));
                expected.insert(scenario_project_receipt_key(scenario,
                    project, receipt, "principal_component_million"));
            }
            require_expected_room(expected, raw.size(),
                project_shape.principal_movement_count, 3U);
            for (std::size_t movement = 0U;
                 movement < project_shape.principal_movement_count;
                 ++movement) {
                expected.insert(scenario_project_principal_movement_key(
                    scenario, project, movement, "month"));
                expected.insert(scenario_project_principal_movement_key(
                    scenario, project, movement, "kind"));
                expected.insert(scenario_project_principal_movement_key(
                    scenario, project, movement, "amount_million"));
            }
        }
    }

    const std::size_t loss_layer_count =
        parse_size(required(raw, "loss_layer.count"));
    if (loss_layer_count > kMaximumParsedLossLayers) {
        throw std::invalid_argument(
            "parsed loss-layer count exceeds 128");
    }
    require_expected_room(expected, raw.size(), loss_layer_count, 3U);
    for (std::size_t layer = 0U; layer < loss_layer_count; ++layer) {
        expected.insert(loss_layer_key(layer, "id"));
        expected.insert(loss_layer_key(layer, "attachment_million"));
        expected.insert(loss_layer_key(layer, "detachment_million"));
    }

    for (const auto& [key, value] : raw) {
        if (!expected.contains(key)) {
            parse_error(value.line, "unknown key: " + key);
        }
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }

    PortfolioConfig config;
    config.model_version = model_version.value;
    config.scenario_label = required(raw, "portfolio.label").value;
    config.source_note = required(raw, "portfolio.source_note").value;
    config.currency_label =
        required(raw, "portfolio.currency_label").value;
    config.monetary_basis =
        required(raw, "portfolio.monetary_basis").value;
    config.synthetic_inputs =
        parse_bool(required(raw, "portfolio.synthetic_inputs"));
    config.horizon_months =
        parse_size(required(raw, "portfolio.horizon_months"));
    config.annual_physical_hurdle_rate = parse_double(
        required(raw, "portfolio.annual_physical_hurdle_rate"));

    config.projects.resize(project_count);
    for (std::size_t project = 0U; project < project_count; ++project) {
        PortfolioProject& parsed_project = config.projects[project];
        parsed_project.id = required(raw, project_key(project, "id")).value;
        parsed_project.stage =
            parse_stage(required(raw, project_key(project, "stage")));
        parsed_project.commitment_million = parse_double(required(
            raw, project_key(project, "commitment_million")));
        if (current_schema) {
            parsed_project.principal_accounting_mode =
                parse_principal_accounting_mode(required(raw,
                    project_key(project, "principal_accounting_mode")));
            parsed_project.principal_limit_million = parse_double(required(
                raw, project_key(project, "principal_limit_million")));
            parsed_project.opening_principal_million = parse_double(required(
                raw, project_key(project, "opening_principal_million")));
        }
    }

    config.joint_scenarios.resize(scenario_count);
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        JointScenario& parsed_scenario = config.joint_scenarios[scenario];
        const ScenarioShape& shape = shapes[scenario];
        parsed_scenario.id =
            required(raw, scenario_key(scenario, "id")).value;
        parsed_scenario.weight =
            parse_double(required(raw, scenario_key(scenario, "weight")));
        parsed_scenario.factor_tags.resize(shape.factor_tag_count);
        for (std::size_t tag = 0U; tag < shape.factor_tag_count; ++tag) {
            parsed_scenario.factor_tags[tag] =
                required(raw, scenario_factor_tag_key(scenario, tag)).value;
        }
        parsed_scenario.pool_costs.resize(shape.pool_cost_count);
        for (std::size_t cost = 0U; cost < shape.pool_cost_count; ++cost) {
            MonthlyAmount& parsed_cost = parsed_scenario.pool_costs[cost];
            parsed_cost.month = parse_size(required(
                raw, scenario_pool_cost_key(scenario, cost, "month")));
            parsed_cost.amount_million = parse_double(required(raw,
                scenario_pool_cost_key(
                    scenario, cost, "amount_million")));
        }
        parsed_scenario.cash_sources.resize(shape.cash_sources.size());
        for (std::size_t source = 0U; source < shape.cash_sources.size();
             ++source) {
            ScenarioCashSource& parsed_source =
                parsed_scenario.cash_sources[source];
            parsed_source.id = required(raw,
                scenario_cash_source_key(scenario, source, "id")).value;
            parsed_source.kind = parse_cash_source_kind(required(raw,
                scenario_cash_source_key(scenario, source, "kind")));
            parsed_source.cash_available.resize(
                shape.cash_sources[source].cash_count);
            for (std::size_t cash = 0U;
                 cash < shape.cash_sources[source].cash_count; ++cash) {
                MonthlyAmount& parsed_cash =
                    parsed_source.cash_available[cash];
                parsed_cash.month = parse_size(required(raw,
                    scenario_cash_key(
                        scenario, source, cash, "month")));
                parsed_cash.amount_million = parse_double(required(raw,
                    scenario_cash_key(
                        scenario, source, cash, "amount_million")));
            }
        }
        parsed_scenario.project_paths.resize(project_count);
        for (std::size_t project = 0U; project < project_count; ++project) {
            ProjectJointPath& parsed_path =
                parsed_scenario.project_paths[project];
            const ProjectShape& project_shape = shape.projects[project];
            parsed_path.project_id = required(raw,
                scenario_project_key(
                    scenario, project, "project_id")).value;
            parsed_path.resolution = parse_resolution(required(raw,
                scenario_project_key(
                    scenario, project, "resolution")));
            parsed_path.capital_draws.resize(project_shape.draw_count);
            for (std::size_t draw = 0U; draw < project_shape.draw_count;
                 ++draw) {
                MonthlyAmount& parsed_draw = parsed_path.capital_draws[draw];
                parsed_draw.month = parse_size(required(raw,
                    scenario_project_draw_key(
                        scenario, project, draw, "month")));
                parsed_draw.amount_million = parse_double(required(raw,
                    scenario_project_draw_key(
                        scenario, project, draw, "amount_million")));
            }
            parsed_path.investor_outlays.resize(
                project_shape.investor_outlay_count);
            for (std::size_t outlay = 0U;
                 outlay < project_shape.investor_outlay_count; ++outlay) {
                InvestorOutlay& parsed_outlay =
                    parsed_path.investor_outlays[outlay];
                parsed_outlay.month = parse_size(required(raw,
                    scenario_project_investor_outlay_key(
                        scenario, project, outlay, "month")));
                parsed_outlay.purpose = parse_investor_outlay_purpose(
                    required(raw, scenario_project_investor_outlay_key(
                                      scenario, project, outlay,
                                      "purpose")));
                parsed_outlay.amount_million = parse_double(required(raw,
                    scenario_project_investor_outlay_key(
                        scenario, project, outlay, "amount_million")));
            }
            parsed_path.investor_receipts.resize(
                project_shape.receipt_count);
            for (std::size_t receipt = 0U;
                 receipt < project_shape.receipt_count; ++receipt) {
                InvestorReceipt& parsed_receipt =
                    parsed_path.investor_receipts[receipt];
                parsed_receipt.month = parse_size(required(raw,
                    scenario_project_receipt_key(
                        scenario, project, receipt, "month")));
                parsed_receipt.cash_source_id = required(raw,
                    scenario_project_receipt_key(
                        scenario, project, receipt, "cash_source_id")).value;
                parsed_receipt.amount_million = parse_double(required(raw,
                    scenario_project_receipt_key(
                        scenario, project, receipt, "amount_million")));
                parsed_receipt.principal_component_million = parse_double(
                    required(raw, scenario_project_receipt_key(scenario,
                        project, receipt,
                        "principal_component_million")));
            }
            parsed_path.principal_movements.resize(
                project_shape.principal_movement_count);
            for (std::size_t movement = 0U;
                 movement < project_shape.principal_movement_count;
                 ++movement) {
                PrincipalMovement& parsed_movement =
                    parsed_path.principal_movements[movement];
                parsed_movement.month = parse_size(required(raw,
                    scenario_project_principal_movement_key(
                        scenario, project, movement, "month")));
                parsed_movement.kind = parse_principal_movement_kind(
                    required(raw, scenario_project_principal_movement_key(
                                      scenario, project, movement, "kind")));
                parsed_movement.amount_million = parse_double(required(raw,
                    scenario_project_principal_movement_key(
                        scenario, project, movement, "amount_million")));
            }
        }
    }

    config.loss_layers.resize(loss_layer_count);
    for (std::size_t layer = 0U; layer < loss_layer_count; ++layer) {
        LossLayer& parsed_layer = config.loss_layers[layer];
        parsed_layer.id = required(raw, loss_layer_key(layer, "id")).value;
        parsed_layer.attachment_million = parse_double(required(
            raw, loss_layer_key(layer, "attachment_million")));
        parsed_layer.detachment_million = parse_double(required(
            raw, loss_layer_key(layer, "detachment_million")));
    }

    validate_portfolio_config(config);
    return config;
}

PortfolioConfig load_portfolio_config(const std::filesystem::path& path) {
    return parse_portfolio_config(read_raw(path));
}

PortfolioConfig load_portfolio_config_bytes(std::string_view bytes) {
    return parse_portfolio_config(read_raw_bytes(bytes));
}

void print_normalized_portfolio_config(
    std::ostream& output, const PortfolioConfig& config) {
    validate_portfolio_config(config);
    const bool current_schema = config.model_version == kPortfolioModelVersion;
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;
    output << "portfolio.model_version=" << config.model_version << '\n';
    output << "portfolio.label=" << config.scenario_label << '\n';
    output << "portfolio.source_note=" << config.source_note << '\n';
    output << "portfolio.currency_label=" << config.currency_label << '\n';
    output << "portfolio.monetary_basis=" << config.monetary_basis << '\n';
    output << "portfolio.synthetic_inputs=" << config.synthetic_inputs
           << '\n';
    output << "portfolio.horizon_months=" << config.horizon_months << '\n';
    output << "portfolio.annual_physical_hurdle_rate="
           << config.annual_physical_hurdle_rate << '\n';
    output << "project.count=" << config.projects.size() << '\n';
    for (std::size_t project = 0U; project < config.projects.size();
         ++project) {
        output << project_key(project, "id") << '='
               << config.projects[project].id << '\n';
        output << project_key(project, "stage") << '='
               << to_string(config.projects[project].stage) << '\n';
        output << project_key(project, "commitment_million") << '='
               << config.projects[project].commitment_million << '\n';
        if (current_schema) {
            output << project_key(project, "principal_accounting_mode")
                   << '='
                   << to_string(
                          config.projects[project].principal_accounting_mode)
                   << '\n';
            output << project_key(project, "principal_limit_million") << '='
                   << config.projects[project].principal_limit_million
                   << '\n';
            output << project_key(project, "opening_principal_million")
                   << '=' << config.projects[project].opening_principal_million
                   << '\n';
        }
    }
    output << "scenario.count=" << config.joint_scenarios.size() << '\n';
    for (std::size_t scenario = 0U;
         scenario < config.joint_scenarios.size(); ++scenario) {
        const JointScenario& scenario_value =
            config.joint_scenarios[scenario];
        output << scenario_key(scenario, "id") << '=' << scenario_value.id
               << '\n';
        output << scenario_key(scenario, "weight") << '='
               << scenario_value.weight << '\n';
        output << scenario_key(scenario, "factor_tag.count") << '='
               << scenario_value.factor_tags.size() << '\n';
        for (std::size_t tag = 0U; tag < scenario_value.factor_tags.size();
             ++tag) {
            output << scenario_factor_tag_key(scenario, tag) << '='
                   << scenario_value.factor_tags[tag] << '\n';
        }
        output << scenario_key(scenario, "pool_cost.count") << '='
               << scenario_value.pool_costs.size() << '\n';
        for (std::size_t cost = 0U; cost < scenario_value.pool_costs.size();
             ++cost) {
            output << scenario_pool_cost_key(scenario, cost, "month") << '='
                   << scenario_value.pool_costs[cost].month << '\n';
            output << scenario_pool_cost_key(
                          scenario, cost, "amount_million")
                   << '=' << scenario_value.pool_costs[cost].amount_million
                   << '\n';
        }
        output << scenario_key(scenario, "cash_source.count") << '='
               << scenario_value.cash_sources.size() << '\n';
        for (std::size_t source = 0U;
             source < scenario_value.cash_sources.size(); ++source) {
            const ScenarioCashSource& source_value =
                scenario_value.cash_sources[source];
            output << scenario_cash_source_key(scenario, source, "id")
                   << '=' << source_value.id << '\n';
            output << scenario_cash_source_key(scenario, source, "kind")
                   << '=' << to_string(source_value.kind) << '\n';
            output << scenario_cash_source_key(
                          scenario, source, "cash.count")
                   << '=' << source_value.cash_available.size() << '\n';
            for (std::size_t cash = 0U;
                 cash < source_value.cash_available.size(); ++cash) {
                output << scenario_cash_key(
                              scenario, source, cash, "month")
                       << '=' << source_value.cash_available[cash].month
                       << '\n';
                output << scenario_cash_key(
                              scenario, source, cash, "amount_million")
                       << '='
                       << source_value.cash_available[cash].amount_million
                       << '\n';
            }
        }
        for (std::size_t project = 0U; project < config.projects.size();
             ++project) {
            const auto path = std::find_if(
                scenario_value.project_paths.begin(),
                scenario_value.project_paths.end(),
                [&config, project](const ProjectJointPath& candidate) {
                    return candidate.project_id == config.projects[project].id;
                });
            if (path == scenario_value.project_paths.end()) {
                throw std::logic_error(
                    "validated portfolio scenario lost a project path");
            }
            output << scenario_project_key(
                          scenario, project, "project_id")
                   << '=' << path->project_id << '\n';
            output << scenario_project_key(
                          scenario, project, "resolution")
                   << '=' << to_string(path->resolution) << '\n';
            output << scenario_project_key(
                          scenario, project, "draw.count")
                   << '=' << path->capital_draws.size() << '\n';
            for (std::size_t draw = 0U; draw < path->capital_draws.size();
                 ++draw) {
                output << scenario_project_draw_key(
                              scenario, project, draw, "month")
                       << '=' << path->capital_draws[draw].month << '\n';
                output << scenario_project_draw_key(
                              scenario, project, draw, "amount_million")
                       << '=' << path->capital_draws[draw].amount_million
                       << '\n';
            }
            if (current_schema) {
                output << scenario_project_key(
                              scenario, project, "investor_outlay.count")
                       << '=' << path->investor_outlays.size() << '\n';
                for (std::size_t outlay = 0U;
                     outlay < path->investor_outlays.size(); ++outlay) {
                    const InvestorOutlay& outlay_value =
                        path->investor_outlays[outlay];
                    output << scenario_project_investor_outlay_key(
                                  scenario, project, outlay, "month")
                           << '=' << outlay_value.month << '\n';
                    output << scenario_project_investor_outlay_key(
                                  scenario, project, outlay, "purpose")
                           << '=' << to_string(outlay_value.purpose) << '\n';
                    output << scenario_project_investor_outlay_key(
                                  scenario, project, outlay,
                                  "amount_million")
                           << '=' << outlay_value.amount_million << '\n';
                }
            }
            output << scenario_project_key(
                          scenario, project, "receipt.count")
                   << '=' << path->investor_receipts.size() << '\n';
            for (std::size_t receipt = 0U;
                 receipt < path->investor_receipts.size(); ++receipt) {
                const InvestorReceipt& receipt_value =
                    path->investor_receipts[receipt];
                output << scenario_project_receipt_key(
                              scenario, project, receipt, "month")
                       << '=' << receipt_value.month << '\n';
                output << scenario_project_receipt_key(
                              scenario, project, receipt, "cash_source_id")
                       << '=' << receipt_value.cash_source_id << '\n';
                output << scenario_project_receipt_key(
                              scenario, project, receipt, "amount_million")
                       << '=' << receipt_value.amount_million << '\n';
                output << scenario_project_receipt_key(scenario, project,
                              receipt, "principal_component_million")
                       << '=' << receipt_value.principal_component_million
                       << '\n';
            }
            if (current_schema) {
                output << scenario_project_key(
                              scenario, project,
                              "principal_movement.count")
                       << '=' << path->principal_movements.size() << '\n';
                for (std::size_t movement = 0U;
                     movement < path->principal_movements.size();
                     ++movement) {
                    const PrincipalMovement& movement_value =
                        path->principal_movements[movement];
                    output << scenario_project_principal_movement_key(
                                  scenario, project, movement, "month")
                           << '=' << movement_value.month << '\n';
                    output << scenario_project_principal_movement_key(
                                  scenario, project, movement, "kind")
                           << '=' << to_string(movement_value.kind) << '\n';
                    output << scenario_project_principal_movement_key(
                                  scenario, project, movement,
                                  "amount_million")
                           << '=' << movement_value.amount_million << '\n';
                }
            }
        }
    }
    output << "loss_layer.count=" << config.loss_layers.size() << '\n';
    for (std::size_t layer = 0U; layer < config.loss_layers.size(); ++layer) {
        output << loss_layer_key(layer, "id") << '='
               << config.loss_layers[layer].id << '\n';
        output << loss_layer_key(layer, "attachment_million") << '='
               << config.loss_layers[layer].attachment_million << '\n';
        output << loss_layer_key(layer, "detachment_million") << '='
               << config.loss_layers[layer].detachment_million << '\n';
    }
}

} // namespace naturalehia::cellular_finance
