// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/joint_cohort_config.hpp>

#include <naturalehia/cellular_finance/evidence_gate.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <ostream>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::uintmax_t kMaximumConfigBytes = 1024U * 1024U;
constexpr std::uintmax_t kMaximumPortfolioBytes = 16U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumLedgerBytes = 32U * 1024U * 1024U;
constexpr std::size_t kMaximumConfigLineLength = 4096U;
constexpr std::size_t kMaximumLedgerLineLength = 8192U;
constexpr std::size_t kMaximumLedgerRows = 100000U;
constexpr std::size_t kMaximumRules = 1024U;
constexpr std::size_t kMaximumFieldLength = 2048U;
constexpr std::string_view kLedgerHeader =
    "observation_id\tcluster_id\teligible_date\thorizon_end_date\tstatus\t"
    "scenario_id\tclassification_date\texclusion_rule_id\t"
    "evidence_record_ids\trequirement_ids";

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
            // Stream-state restoration must not throw during unwinding.
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

[[noreturn]] void invalid(std::string message) {
    throw std::invalid_argument(
        "joint cohort package: " + std::move(message));
}

[[nodiscard]] bool is_lower_hex_sha256(std::string_view value) noexcept {
    return value.size() == 64U &&
        std::all_of(value.begin(), value.end(), [](char character) {
            return (character >= '0' && character <= '9') ||
                (character >= 'a' && character <= 'f');
        });
}

[[nodiscard]] bool is_safe_relative_path(std::string_view value) noexcept {
    if (value.empty() || value.size() > 512U || value.front() == '/' ||
        value.back() == '/' || value.find('\\') != std::string_view::npos ||
        value.find(':') != std::string_view::npos ||
        value.find("//") != std::string_view::npos) {
        return false;
    }
    if (!std::all_of(value.begin(), value.end(), [](char character) {
            return (character >= 'A' && character <= 'Z') ||
                (character >= 'a' && character <= 'z') ||
                (character >= '0' && character <= '9') ||
                character == '.' || character == '_' || character == '-' ||
                character == '/';
        })) {
        return false;
    }
    std::size_t start = 0U;
    while (start < value.size()) {
        const std::size_t end = value.find('/', start);
        const std::string_view part = value.substr(
            start, end == std::string_view::npos
                ? value.size() - start
                : end - start);
        if (part.empty() || part == "." || part == "..") {
            return false;
        }
        start = end == std::string_view::npos ? value.size() : end + 1U;
    }
    return true;
}

[[nodiscard]] bool parse_bool(std::string_view value, std::string_view key) {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    invalid(std::string(key) + " must be true or false");
}

[[nodiscard]] std::size_t parse_size(
    std::string_view value,
    std::string_view key,
    std::size_t maximum) {
    std::size_t result = 0U;
    const auto conversion = std::from_chars(
        value.data(), value.data() + value.size(), result);
    if (value.empty() || conversion.ec != std::errc{} ||
        conversion.ptr != value.data() + value.size() || result > maximum) {
        invalid(std::string(key) + " must be a bounded unsigned integer");
    }
    return result;
}

[[nodiscard]] double parse_double(
    std::string_view value,
    std::string_view key) {
    double result = 0.0;
    const auto conversion = std::from_chars(
        value.data(), value.data() + value.size(), result,
        std::chars_format::general);
    if (value.empty() || conversion.ec != std::errc{} ||
        conversion.ptr != value.data() + value.size() ||
        !std::isfinite(result)) {
        invalid(std::string(key) + " must be a finite decimal number");
    }
    return result;
}

[[nodiscard]] std::string read_bounded_file_bytes(
    const std::filesystem::path& path,
    std::uintmax_t maximum_bytes,
    std::string_view label) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size > maximum_bytes ||
        !std::filesystem::is_regular_file(path, error) || error) {
        invalid(std::string(label) + " is missing, non-regular, or exceeds its byte limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        invalid("could not open " + std::string(label));
    }
    std::string bytes;
    bytes.reserve(static_cast<std::size_t>(size));
    std::array<char, 8192U> buffer{};
    while (input) {
        input.read(buffer.data(),
                   static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            const std::size_t additional =
                static_cast<std::size_t>(count);
            if (additional > maximum_bytes - bytes.size()) {
                invalid(std::string(label) +
                        " grew beyond its byte limit while being read");
            }
            bytes.append(buffer.data(), additional);
        }
    }
    if (!input.eof()) {
        invalid("could not read " + std::string(label) + " completely");
    }
    return bytes;
}

[[nodiscard]] std::unordered_map<std::string, std::string>
load_key_values_bytes(std::string_view bytes) {
    std::istringstream input(
        std::string(bytes), std::ios::in | std::ios::binary);
    std::unordered_map<std::string, std::string> values;
    std::string line;
    std::size_t line_number = 0U;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.size() > kMaximumConfigLineLength) {
            invalid("configuration line exceeds 4096 bytes");
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos || equals == 0U ||
            equals + 1U == line.size() ||
            line.find('=', equals + 1U) != std::string::npos) {
            invalid("configuration line " + std::to_string(line_number) +
                    " must contain one nonempty key=value pair");
        }
        const std::string key = line.substr(0U, equals);
        const std::string value = line.substr(equals + 1U);
        if (key.front() == ' ' || key.back() == ' ' ||
            value.front() == ' ' || value.back() == ' ') {
            invalid("configuration fields must not have surrounding whitespace");
        }
        if (!values.emplace(key, value).second) {
            invalid("duplicate configuration key: " + key);
        }
    }
    if (!input.eof()) {
        invalid("could not read configuration completely");
    }
    return values;
}

[[nodiscard]] std::unordered_map<std::string, std::string> load_key_values(
    const std::filesystem::path& path) {
    return load_key_values_bytes(read_bounded_file_bytes(
        path, kMaximumConfigBytes, "configuration"));
}

[[nodiscard]] const std::string& require_key(
    const std::unordered_map<std::string, std::string>& values,
    std::string_view key) {
    const auto found = values.find(std::string(key));
    if (found == values.end()) {
        invalid("missing configuration key: " + std::string(key));
    }
    return found->second;
}

void validate_bound_file(
    const JointCohortBoundFile& file,
    std::string_view label) {
    if (!is_safe_relative_path(file.relative_path)) {
        invalid(std::string(label) + " path must be safely relative");
    }
    if (!is_lower_hex_sha256(file.sha256)) {
        invalid(std::string(label) +
                " SHA-256 must be 64 lowercase hexadecimal characters");
    }
}

struct BoundFileSnapshot {
    std::filesystem::path resolved_path{};
    std::string bytes{};
};

[[nodiscard]] BoundFileSnapshot resolve_bound_snapshot(
    const std::filesystem::path& directory,
    const JointCohortBoundFile& file,
    std::string_view label,
    std::uintmax_t maximum_bytes) {
    validate_bound_file(file, label);
    std::error_code error;
    const std::filesystem::path candidate =
        std::filesystem::weakly_canonical(
            directory / std::filesystem::path(file.relative_path), error);
    if (error) {
        invalid("could not resolve " + std::string(label));
    }
    const std::filesystem::path relative =
        candidate.lexically_relative(directory);
    if (relative.empty() || relative.is_absolute() ||
        std::any_of(relative.begin(), relative.end(),
            [](const std::filesystem::path& part) { return part == ".."; })) {
        invalid(std::string(label) + " escapes the configuration package");
    }
    if (!std::filesystem::is_regular_file(candidate, error) || error) {
        invalid(std::string(label) + " is not a regular file");
    }
    BoundFileSnapshot snapshot;
    snapshot.resolved_path = candidate;
    snapshot.bytes = read_bounded_file_bytes(
        candidate, maximum_bytes, label);
    if (sha256_bytes_lower_hex(snapshot.bytes) != file.sha256) {
        invalid(std::string(label) + " SHA-256 mismatch");
    }
    return snapshot;
}

[[nodiscard]] std::vector<std::string_view> split_tsv(
    std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0U;
    while (true) {
        const std::size_t tab = line.find('\t', start);
        fields.push_back(line.substr(start,
            tab == std::string_view::npos ? line.size() - start : tab - start));
        if (tab == std::string_view::npos) {
            return fields;
        }
        start = tab + 1U;
    }
}

[[nodiscard]] JointCohortObservationStatus parse_status(
    std::string_view value) {
    if (value == "matured") {
        return JointCohortObservationStatus::Matured;
    }
    if (value == "not-yet-matured") {
        return JointCohortObservationStatus::NotYetMatured;
    }
    if (value == "unresolved") {
        return JointCohortObservationStatus::Unresolved;
    }
    if (value == "excluded") {
        return JointCohortObservationStatus::Excluded;
    }
    invalid("ledger status is unknown");
}

[[nodiscard]] std::vector<std::string> parse_id_list(
    std::string_view value) {
    if (value == "NONE") {
        return {};
    }
    std::vector<std::string> result;
    std::size_t start = 0U;
    while (true) {
        const std::size_t separator = value.find(';', start);
        const std::string_view item = value.substr(start,
            separator == std::string_view::npos
                ? value.size() - start
                : separator - start);
        if (item.empty()) {
            invalid("ledger ID lists must not contain empty items");
        }
        result.emplace_back(item);
        if (separator == std::string_view::npos) {
            return result;
        }
        start = separator + 1U;
    }
}

[[nodiscard]] std::string join_ids(std::vector<std::string> values) {
    if (values.empty()) {
        return "NONE";
    }
    std::sort(values.begin(), values.end());
    std::string result;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (index != 0U) {
            result.push_back(';');
        }
        result += values[index];
    }
    return result;
}

} // namespace

JointCohortPackageConfig load_joint_cohort_config(
    const std::filesystem::path& path) {
    const std::unordered_map<std::string, std::string> values =
        load_key_values(path);

    const std::size_t rule_count = parse_size(
        require_key(values, "exclusion_rule.count"),
        "exclusion_rule.count", kMaximumRules);
    std::set<std::string> allowed{
        "joint_cohort.version",
        "joint_cohort.id",
        "joint_cohort.as_of_date",
        "joint_cohort.source_note",
        "joint_cohort.population_definition",
        "joint_cohort.sampling_unit_definition",
        "joint_cohort.outcome_mapping_definition",
        "joint_cohort.horizon_definition",
        "joint_cohort.scenario_taxonomy_frozen_date",
        "joint_cohort.population_frame_count",
        "joint_cohort.candidate_only",
        "joint_cohort.synthetic_inputs",
        "joint_cohort.probability_measure",
        "joint_cohort.sampling_assumption",
        "joint_cohort.interval_method",
        "joint_cohort.confidence_level",
        "file.portfolio.path",
        "file.portfolio.sha256",
        "file.ledger.path",
        "file.ledger.sha256",
        "exclusion_rule.count",
    };
    for (std::size_t index = 1U; index <= rule_count; ++index) {
        const std::string prefix =
            "exclusion_rule." + std::to_string(index) + '.';
        allowed.insert(prefix + "id");
        allowed.insert(prefix + "frozen_date");
        allowed.insert(prefix + "outcome_blind_asserted");
        allowed.insert(prefix + "statement");
    }
    for (const auto& [key, value] : values) {
        static_cast<void>(value);
        if (!allowed.contains(key)) {
            invalid("unknown configuration key: " + key);
        }
    }
    for (const std::string& key : allowed) {
        static_cast<void>(require_key(values, key));
    }

    JointCohortPackageConfig config;
    config.analysis.version = require_key(values, "joint_cohort.version");
    config.analysis.id = require_key(values, "joint_cohort.id");
    config.analysis.as_of_date =
        require_key(values, "joint_cohort.as_of_date");
    config.analysis.source_note =
        require_key(values, "joint_cohort.source_note");
    config.analysis.population_definition =
        require_key(values, "joint_cohort.population_definition");
    config.analysis.sampling_unit_definition =
        require_key(values, "joint_cohort.sampling_unit_definition");
    config.analysis.outcome_mapping_definition =
        require_key(values, "joint_cohort.outcome_mapping_definition");
    config.analysis.horizon_definition =
        require_key(values, "joint_cohort.horizon_definition");
    config.analysis.scenario_taxonomy_frozen_date = require_key(
        values, "joint_cohort.scenario_taxonomy_frozen_date");
    config.analysis.population_frame_count = parse_size(
        require_key(values, "joint_cohort.population_frame_count"),
        "joint_cohort.population_frame_count", kMaximumLedgerRows);
    config.analysis.candidate_only = parse_bool(
        require_key(values, "joint_cohort.candidate_only"),
        "joint_cohort.candidate_only");
    config.analysis.synthetic_inputs = parse_bool(
        require_key(values, "joint_cohort.synthetic_inputs"),
        "joint_cohort.synthetic_inputs");
    config.analysis.probability_measure =
        require_key(values, "joint_cohort.probability_measure");
    config.analysis.sampling_assumption =
        require_key(values, "joint_cohort.sampling_assumption");
    config.analysis.interval_method =
        require_key(values, "joint_cohort.interval_method");
    config.analysis.confidence_level = parse_double(
        require_key(values, "joint_cohort.confidence_level"),
        "joint_cohort.confidence_level");
    config.portfolio_file.relative_path =
        require_key(values, "file.portfolio.path");
    config.portfolio_file.sha256 =
        require_key(values, "file.portfolio.sha256");
    config.ledger_file.relative_path =
        require_key(values, "file.ledger.path");
    config.ledger_file.sha256 =
        require_key(values, "file.ledger.sha256");
    config.analysis.exclusion_rules.reserve(rule_count);
    for (std::size_t index = 1U; index <= rule_count; ++index) {
        const std::string prefix =
            "exclusion_rule." + std::to_string(index) + '.';
        JointCohortExclusionRule rule;
        rule.id = require_key(values, prefix + "id");
        rule.frozen_date = require_key(values, prefix + "frozen_date");
        rule.outcome_blind_asserted = parse_bool(
            require_key(values, prefix + "outcome_blind_asserted"),
            prefix + "outcome_blind_asserted");
        rule.statement = require_key(values, prefix + "statement");
        config.analysis.exclusion_rules.push_back(std::move(rule));
    }

    validate_joint_cohort_analysis_config(config.analysis);
    validate_bound_file(config.portfolio_file, "portfolio");
    validate_bound_file(config.ledger_file, "ledger");
    if (config.portfolio_file.relative_path ==
        config.ledger_file.relative_path) {
        invalid("portfolio and ledger paths must be distinct");
    }
    return config;
}

namespace {

[[nodiscard]] std::vector<JointCohortObservation>
parse_joint_cohort_ledger_bytes(std::string_view bytes) {
    std::istringstream input(
        std::string(bytes), std::ios::in | std::ios::binary);
    std::string line;
    if (!std::getline(input, line)) {
        invalid("ledger is empty");
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (line.size() > kMaximumLedgerLineLength) {
        invalid("ledger header exceeds 8192 bytes");
    }
    if (line != kLedgerHeader) {
        invalid("ledger header does not match the v0.1 schema");
    }

    std::vector<JointCohortObservation> observations;
    std::size_t line_number = 1U;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            invalid("ledger contains an empty row");
        }
        if (line.size() > kMaximumLedgerLineLength) {
            invalid("ledger line exceeds 8192 bytes");
        }
        if (observations.size() == kMaximumLedgerRows) {
            invalid("ledger exceeds 100000 rows");
        }
        const std::vector<std::string_view> fields = split_tsv(line);
        if (fields.size() != 10U) {
            invalid("ledger line " + std::to_string(line_number) +
                    " must contain exactly 10 columns");
        }
        for (const std::string_view field : fields) {
            if (field.empty() || field.size() > kMaximumFieldLength ||
                field.front() == ' ' || field.back() == ' ') {
                invalid("ledger fields must be bounded, nonempty, and unpadded");
            }
        }
        JointCohortObservation observation;
        observation.observation_id = fields[0U];
        observation.cluster_id = fields[1U];
        observation.eligible_date = fields[2U];
        observation.horizon_end_date = fields[3U];
        observation.status = parse_status(fields[4U]);
        observation.scenario_id = fields[5U];
        observation.classification_date = fields[6U];
        observation.exclusion_rule_id = fields[7U];
        observation.evidence_record_ids = parse_id_list(fields[8U]);
        observation.requirement_ids = parse_id_list(fields[9U]);
        observations.push_back(std::move(observation));
    }
    if (!input.eof()) {
        invalid("could not read ledger completely");
    }
    validate_joint_cohort_ledger_syntax(observations);
    return observations;
}

} // namespace

std::vector<JointCohortObservation> load_joint_cohort_ledger(
    const std::filesystem::path& path) {
    return parse_joint_cohort_ledger_bytes(read_bounded_file_bytes(
        path, kMaximumLedgerBytes, "ledger"));
}

JointCohortPackage load_joint_cohort_package(
    const std::filesystem::path& config_path) {
    std::error_code error;
    const std::filesystem::path absolute_config =
        std::filesystem::weakly_canonical(
            std::filesystem::absolute(config_path), error);
    if (error || !std::filesystem::is_regular_file(absolute_config, error) ||
        error) {
        invalid("configuration path is not a regular file");
    }
    const std::filesystem::path directory = absolute_config.parent_path();
    JointCohortPackage result;
    result.directory = directory;
    result.config = load_joint_cohort_config(absolute_config);
    const BoundFileSnapshot portfolio_snapshot = resolve_bound_snapshot(
        directory, result.config.portfolio_file, "portfolio",
        kMaximumPortfolioBytes);
    const BoundFileSnapshot ledger_snapshot = resolve_bound_snapshot(
        directory, result.config.ledger_file, "ledger",
        kMaximumLedgerBytes);
    if (portfolio_snapshot.resolved_path == ledger_snapshot.resolved_path) {
        invalid("portfolio and ledger resolve to the same file");
    }
    result.portfolio = load_portfolio_config_bytes(portfolio_snapshot.bytes);
    result.observations =
        parse_joint_cohort_ledger_bytes(ledger_snapshot.bytes);
    if (result.config.analysis.population_frame_count !=
        result.observations.size()) {
        invalid("population_frame_count does not match the raw ledger row count");
    }

    const BoundFileSnapshot portfolio_recheck = resolve_bound_snapshot(
        directory, result.config.portfolio_file, "portfolio",
        kMaximumPortfolioBytes);
    const BoundFileSnapshot ledger_recheck = resolve_bound_snapshot(
        directory, result.config.ledger_file, "ledger",
        kMaximumLedgerBytes);
    if (portfolio_recheck.resolved_path != portfolio_snapshot.resolved_path ||
        ledger_recheck.resolved_path != ledger_snapshot.resolved_path ||
        portfolio_recheck.bytes != portfolio_snapshot.bytes ||
        ledger_recheck.bytes != ledger_snapshot.bytes) {
        invalid("a bound artifact changed while the package was being parsed");
    }
    return result;
}

void print_normalized_joint_cohort_config(
    std::ostream& output,
    const JointCohortPackageConfig& config) {
    validate_joint_cohort_analysis_config(config.analysis);
    validate_bound_file(config.portfolio_file, "portfolio");
    validate_bound_file(config.ledger_file, "ledger");
    if (config.portfolio_file.relative_path ==
        config.ledger_file.relative_path) {
        invalid("portfolio and ledger paths must be distinct");
    }
    std::vector<JointCohortExclusionRule> rules =
        config.analysis.exclusion_rules;
    std::sort(rules.begin(), rules.end(),
        [](const JointCohortExclusionRule& left,
           const JointCohortExclusionRule& right) {
            return left.id < right.id;
        });
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha
           << "joint_cohort.version=" << config.analysis.version << '\n'
           << "joint_cohort.id=" << config.analysis.id << '\n'
           << "joint_cohort.as_of_date=" << config.analysis.as_of_date << '\n'
           << "joint_cohort.source_note=" << config.analysis.source_note << '\n'
           << "joint_cohort.population_definition="
           << config.analysis.population_definition << '\n'
           << "joint_cohort.sampling_unit_definition="
           << config.analysis.sampling_unit_definition << '\n'
           << "joint_cohort.outcome_mapping_definition="
           << config.analysis.outcome_mapping_definition << '\n'
           << "joint_cohort.horizon_definition="
           << config.analysis.horizon_definition << '\n'
           << "joint_cohort.scenario_taxonomy_frozen_date="
           << config.analysis.scenario_taxonomy_frozen_date << '\n'
           << "joint_cohort.population_frame_count="
           << config.analysis.population_frame_count << '\n'
           << "joint_cohort.candidate_only="
           << (config.analysis.candidate_only ? "true" : "false") << '\n'
           << "joint_cohort.synthetic_inputs="
           << (config.analysis.synthetic_inputs ? "true" : "false") << '\n'
           << "joint_cohort.probability_measure="
           << config.analysis.probability_measure << '\n'
           << "joint_cohort.sampling_assumption="
           << config.analysis.sampling_assumption << '\n'
           << "joint_cohort.interval_method="
           << config.analysis.interval_method << '\n'
           << "joint_cohort.confidence_level="
           << config.analysis.confidence_level << '\n'
           << "file.portfolio.path="
           << config.portfolio_file.relative_path << '\n'
           << "file.portfolio.sha256="
           << config.portfolio_file.sha256 << '\n'
           << "file.ledger.path=" << config.ledger_file.relative_path << '\n'
           << "file.ledger.sha256=" << config.ledger_file.sha256 << '\n'
           << "exclusion_rule.count=" << rules.size() << '\n';
    for (std::size_t index = 0U; index < rules.size(); ++index) {
        const std::string prefix =
            "exclusion_rule." + std::to_string(index + 1U) + '.';
        output << prefix << "id=" << rules[index].id << '\n'
               << prefix << "frozen_date=" << rules[index].frozen_date << '\n'
               << prefix << "outcome_blind_asserted="
               << (rules[index].outcome_blind_asserted ? "true" : "false")
               << '\n'
               << prefix << "statement=" << rules[index].statement << '\n';
    }
}

void print_normalized_joint_cohort_ledger(
    std::ostream& output,
    const std::vector<JointCohortObservation>& observations) {
    validate_joint_cohort_ledger_syntax(observations);
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase;
    std::vector<JointCohortObservation> ordered = observations;
    std::sort(ordered.begin(), ordered.end(),
        [](const JointCohortObservation& left,
           const JointCohortObservation& right) {
            return left.observation_id < right.observation_id;
        });
    output << kLedgerHeader << '\n';
    for (const JointCohortObservation& observation : ordered) {
        output << observation.observation_id << '\t'
               << observation.cluster_id << '\t'
               << observation.eligible_date << '\t'
               << observation.horizon_end_date << '\t'
               << to_string(observation.status) << '\t'
               << observation.scenario_id << '\t'
               << observation.classification_date << '\t'
               << observation.exclusion_rule_id << '\t'
               << join_ids(observation.evidence_record_ids) << '\t'
               << join_ids(observation.requirement_ids) << '\n';
    }
}

} // namespace naturalehia::cellular_finance
