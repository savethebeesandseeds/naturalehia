// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/calibration_binder.hpp>

#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::array<std::string_view, 18U> kBinderKeys{{
    "binder.version",
    "binder.id",
    "binder.project_id",
    "binder.dossier_id",
    "binder.as_of_date",
    "binder.source_note",
    "binder.candidate_only",
    "binder.probability_measure",
    "file.portfolio.path",
    "file.portfolio.sha256",
    "file.ambiguity.path",
    "file.ambiguity.sha256",
    "file.dossier.path",
    "file.dossier.sha256",
    "file.evidence_manifest.path",
    "file.evidence_manifest.sha256",
    "file.lineage.path",
    "file.lineage.sha256",
}};

constexpr std::string_view kLineageHeader =
    "input_id\ttarget_path\tinput_class\tinput_status\tmethod_id\t"
    "evidence_record_ids\trequirement_ids\tlimitations\tupdate_or_retire";

constexpr std::array<std::string_view, 8U> kMetadataKeys{{
    "portfolio.model_version",
    "portfolio.label",
    "portfolio.source_note",
    "portfolio.synthetic_inputs",
    "ambiguity.model_version",
    "ambiguity.label",
    "ambiguity.source_note",
    "ambiguity.synthetic_inputs",
}};

constexpr std::uintmax_t kMaximumPortfolioBytes = 16U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumAmbiguityBytes = 16U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumDossierBytes = 1024U * 1024U;
constexpr std::uintmax_t kMaximumManifestBytes = 32U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumLineageBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumLineageLineBytes = 8'192U;
constexpr std::size_t kMaximumLineageRows = 500'000U;

[[noreturn]] void invalid(std::string message) {
    throw std::invalid_argument(
        "calibration binder: " + std::move(message));
}

[[nodiscard]] std::string trim(std::string_view value) {
    std::size_t first = 0U;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1U])) != 0) {
        --last;
    }
    return std::string(value.substr(first, last - first));
}

[[nodiscard]] bool has_unsafe_text(std::string_view value) noexcept {
    constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";
    if (value.find(kUtf8Bom) != std::string_view::npos) {
        return true;
    }
    return std::any_of(
        value.begin(), value.end(), [](unsigned char character) {
            return character < 0x20U || character == 0x7fU;
        });
}

void require_safe_text(
    std::string_view value,
    std::string_view label,
    std::size_t maximum_size) {
    if (value.empty() || value.size() > maximum_size ||
        has_unsafe_text(value)) {
        invalid(std::string(label) + " is empty, oversized, or unsafe");
    }
}

void require_safe_identifier(
    std::string_view value,
    std::string_view label) {
    require_safe_text(value, label, 128U);
    if (!std::isalnum(static_cast<unsigned char>(value.front())) ||
        !std::all_of(
            value.begin(), value.end(), [](unsigned char character) {
                return std::isalnum(character) != 0 || character == '-' ||
                    character == '_' || character == '.';
            })) {
        invalid(std::string(label) + " is not a safe identifier");
    }
}

[[nodiscard]] bool is_leap_year(int year) noexcept {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

[[nodiscard]] bool is_iso_date(std::string_view value) noexcept {
    if (value.size() != 10U || value[4U] != '-' || value[7U] != '-') {
        return false;
    }
    for (std::size_t index = 0U; index < value.size(); ++index) {
        if (index == 4U || index == 7U) {
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(value[index])) == 0) {
            return false;
        }
    }
    const int year = std::stoi(std::string(value.substr(0U, 4U)));
    const int month = std::stoi(std::string(value.substr(5U, 2U)));
    const int day = std::stoi(std::string(value.substr(8U, 2U)));
    if (year == 0 || month < 1 || month > 12 || day < 1) {
        return false;
    }
    constexpr std::array<int, 12U> month_days{{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    }};
    const int maximum =
        month == 2 && is_leap_year(year)
        ? 29
        : month_days[static_cast<std::size_t>(month - 1)];
    return day <= maximum;
}

[[nodiscard]] bool is_lower_hex_sha256(std::string_view value) noexcept {
    return value.size() == 64U && std::all_of(
        value.begin(), value.end(), [](unsigned char character) {
            return std::isdigit(character) != 0 ||
                (character >= 'a' && character <= 'f');
        });
}

void validate_relative_path(
    const std::filesystem::path& path,
    std::string_view label) {
    const std::string value = path.generic_string();
    require_safe_text(value, label, 512U);
    const auto& native = path.native();
    if (!std::all_of(
            value.begin(), value.end(), [](unsigned char character) {
                return std::isalnum(character) != 0 || character == '-' ||
                    character == '_' || character == '.' || character == '/';
            }) ||
        native.find(
            static_cast<std::filesystem::path::value_type>('\\')) !=
            std::filesystem::path::string_type::npos ||
        path.is_absolute() || path.has_root_name() ||
        path.has_root_directory()) {
        invalid(std::string(label) + " must be a portable relative path");
    }
    for (const std::filesystem::path& part : path) {
        if (part.empty() || part == "." || part == "..") {
            invalid(std::string(label) + " contains an unsafe component");
        }
    }
}

void validate_bound_file(
    const CalibrationBoundFile& file,
    std::string_view label) {
    validate_relative_path(file.relative_path, label);
    if (!is_lower_hex_sha256(file.sha256)) {
        invalid(std::string(label) + " SHA-256 must be 64 lowercase hex");
    }
}

void validate_binder_config(const CalibrationBinderConfig& config) {
    if (config.version != kCalibrationBinderVersion) {
        invalid("binder.version must be 0.1.0");
    }
    require_safe_identifier(config.id, "binder.id");
    require_safe_identifier(config.project_id, "binder.project_id");
    require_safe_identifier(config.dossier_id, "binder.dossier_id");
    if (!is_iso_date(config.as_of_date)) {
        invalid("binder.as_of_date must be a calendar date in YYYY-MM-DD");
    }
    require_safe_text(config.source_note, "binder.source_note", 512U);
    if (!config.candidate_only) {
        invalid("binder.candidate_only must be true");
    }
    if (config.probability_measure != "physical-P") {
        invalid("binder.probability_measure must be physical-P");
    }
    validate_bound_file(config.portfolio, "file.portfolio.path");
    validate_bound_file(config.ambiguity, "file.ambiguity.path");
    validate_bound_file(config.dossier, "file.dossier.path");
    validate_bound_file(
        config.evidence_manifest, "file.evidence_manifest.path");
    validate_bound_file(config.lineage, "file.lineage.path");

    const std::array<std::string, 5U> paths{{
        config.portfolio.relative_path.generic_string(),
        config.ambiguity.relative_path.generic_string(),
        config.dossier.relative_path.generic_string(),
        config.evidence_manifest.relative_path.generic_string(),
        config.lineage.relative_path.generic_string(),
    }};
    std::unordered_set<std::string> unique_paths;
    for (const std::string& path : paths) {
        if (!unique_paths.insert(path).second) {
            invalid("each bound file path must be distinct");
        }
    }
}

[[nodiscard]] std::unordered_map<std::string, std::string>
load_key_values(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > 1024U * 1024U) {
        invalid("binder config is missing or exceeds 1 MiB");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        invalid("could not open binder config: " + path.string());
    }
    std::unordered_map<std::string, std::string> values;
    std::string line;
    std::size_t line_number = 0U;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::string normalized = trim(line);
        if (normalized.empty() || normalized.front() == '#') {
            continue;
        }
        if (has_unsafe_text(normalized)) {
            invalid("unsafe text on binder line " +
                    std::to_string(line_number));
        }
        const std::size_t separator = normalized.find('=');
        if (separator == std::string::npos ||
            normalized.find('=', separator + 1U) != std::string::npos) {
            invalid("binder line " + std::to_string(line_number) +
                    " must contain exactly one '='");
        }
        const std::string key = trim(
            std::string_view(normalized).substr(0U, separator));
        const std::string value = trim(
            std::string_view(normalized).substr(separator + 1U));
        if (std::find(kBinderKeys.begin(), kBinderKeys.end(), key) ==
            kBinderKeys.end()) {
            invalid("unknown binder key: " + key);
        }
        if (value.empty()) {
            invalid("empty value for binder key: " + key);
        }
        if (!values.emplace(key, value).second) {
            invalid("duplicate binder key: " + key);
        }
    }
    if (!input.eof()) {
        invalid("could not read binder config completely");
    }
    for (const std::string_view key : kBinderKeys) {
        if (!values.contains(std::string(key))) {
            invalid("missing binder key: " + std::string(key));
        }
    }
    return values;
}

[[nodiscard]] const std::string& required(
    const std::unordered_map<std::string, std::string>& values,
    std::string_view key) {
    const auto found = values.find(std::string(key));
    if (found == values.end()) {
        invalid("internal missing binder key: " + std::string(key));
    }
    return found->second;
}

[[nodiscard]] CalibrationBoundFile parse_bound_file(
    const std::unordered_map<std::string, std::string>& values,
    std::string_view name) {
    const std::string prefix = "file." + std::string(name);
    CalibrationBoundFile result;
    result.relative_path = required(values, prefix + ".path");
    result.sha256 = required(values, prefix + ".sha256");
    return result;
}

[[nodiscard]] bool is_material_key(std::string_view key) {
    if (std::find(kMetadataKeys.begin(), kMetadataKeys.end(), key) !=
        kMetadataKeys.end()) {
        return false;
    }
    return !key.ends_with(".count");
}

void append_material_targets(
    std::vector<std::string>& targets,
    std::string_view source,
    std::string_view normalized) {
    std::size_t position = 0U;
    while (position < normalized.size()) {
        const std::size_t newline = normalized.find('\n', position);
        const std::size_t end = newline == std::string_view::npos
            ? normalized.size()
            : newline;
        const std::string_view line = normalized.substr(position, end - position);
        if (!line.empty()) {
            const std::size_t separator = line.find('=');
            if (separator == std::string_view::npos) {
                throw std::logic_error(
                    "normalized configuration emitted a line without '='");
            }
            const std::string_view key = line.substr(0U, separator);
            if (is_material_key(key)) {
                targets.push_back(
                    std::string(source) + "/" + std::string(key));
            }
        }
        if (newline == std::string_view::npos) {
            break;
        }
        position = newline + 1U;
    }
}

[[nodiscard]] std::filesystem::path resolve_bound_path(
    const std::filesystem::path& directory,
    const CalibrationBoundFile& bound,
    std::string_view label,
    std::uintmax_t maximum_bytes) {
    std::error_code error;
    const std::filesystem::path target = std::filesystem::canonical(
        directory / bound.relative_path, error);
    if (error || !std::filesystem::is_regular_file(target, error) || error) {
        invalid(std::string(label) +
                " is missing or is not a regular file");
    }
    const std::filesystem::path relative = target.lexically_relative(directory);
    if (relative.empty() || relative.is_absolute() ||
        std::any_of(
            relative.begin(), relative.end(),
            [](const std::filesystem::path& part) { return part == ".."; })) {
        invalid(std::string(label) + " resolves outside the binder directory");
    }
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(target, error);
    if (error || file_bytes > maximum_bytes) {
        invalid(std::string(label) + " exceeds its byte cap");
    }
    if (sha256_file_lower_hex(target) != bound.sha256) {
        invalid(std::string(label) + " SHA-256 mismatch");
    }
    return target;
}

[[nodiscard]] std::vector<std::string> split_tab_line(
    std::string_view line) {
    std::vector<std::string> fields;
    std::size_t position = 0U;
    for (;;) {
        const std::size_t tab = line.find('\t', position);
        const std::size_t end =
            tab == std::string_view::npos ? line.size() : tab;
        fields.emplace_back(line.substr(position, end - position));
        if (tab == std::string_view::npos) {
            break;
        }
        position = tab + 1U;
    }
    return fields;
}

[[nodiscard]] std::vector<std::string> parse_id_list(
    std::string_view value,
    std::string_view label) {
    if (value == "NONE") {
        return {};
    }
    std::vector<std::string> result;
    std::unordered_set<std::string> unique;
    std::size_t position = 0U;
    for (;;) {
        const std::size_t separator = value.find(';', position);
        const std::size_t end =
            separator == std::string_view::npos ? value.size() : separator;
        const std::string item(value.substr(position, end - position));
        require_safe_identifier(item, label);
        if (!unique.insert(item).second) {
            invalid(std::string(label) + " contains a duplicate ID");
        }
        result.push_back(item);
        if (separator == std::string_view::npos) {
            break;
        }
        position = separator + 1U;
    }
    std::sort(result.begin(), result.end());
    return result;
}

[[nodiscard]] CalibrationInputClass parse_input_class(
    std::string_view value) {
    if (value == "capital") {
        return CalibrationInputClass::Capital;
    }
    if (value == "transition") {
        return CalibrationInputClass::Transition;
    }
    if (value == "probability") {
        return CalibrationInputClass::Probability;
    }
    if (value == "recovery") {
        return CalibrationInputClass::Recovery;
    }
    if (value == "dependence") {
        return CalibrationInputClass::Dependence;
    }
    if (value == "qualified-output") {
        return CalibrationInputClass::QualifiedOutput;
    }
    if (value == "commercial-cash") {
        return CalibrationInputClass::CommercialCash;
    }
    if (value == "source-credit") {
        return CalibrationInputClass::SourceCredit;
    }
    if (value == "cost") {
        return CalibrationInputClass::Cost;
    }
    if (value == "policy-hurdle") {
        return CalibrationInputClass::PolicyHurdle;
    }
    if (value == "instrument-term") {
        return CalibrationInputClass::InstrumentTerm;
    }
    invalid("unknown lineage input_class: " + std::string(value));
}

[[nodiscard]] CalibrationInputStatus parse_input_status(
    std::string_view value) {
    if (value == "observed") {
        return CalibrationInputStatus::Observed;
    }
    if (value == "contractual") {
        return CalibrationInputStatus::Contractual;
    }
    if (value == "derived") {
        return CalibrationInputStatus::Derived;
    }
    if (value == "estimated") {
        return CalibrationInputStatus::Estimated;
    }
    if (value == "transfer") {
        return CalibrationInputStatus::Transfer;
    }
    if (value == "synthetic") {
        return CalibrationInputStatus::Synthetic;
    }
    if (value == "hypothesis") {
        return CalibrationInputStatus::Hypothesis;
    }
    if (value == "stress") {
        return CalibrationInputStatus::Stress;
    }
    if (value == "policy") {
        return CalibrationInputStatus::Policy;
    }
    invalid("unknown lineage input_status: " + std::string(value));
}

[[nodiscard]] bool is_evidence_backed_status(
    CalibrationInputStatus value) noexcept {
    return value == CalibrationInputStatus::Observed ||
        value == CalibrationInputStatus::Contractual ||
        value == CalibrationInputStatus::Derived ||
        value == CalibrationInputStatus::Estimated ||
        value == CalibrationInputStatus::Transfer;
}

[[nodiscard]] bool is_ambiguous_cash_class(
    CalibrationInputClass value) noexcept {
    return value == CalibrationInputClass::CommercialCash ||
        value == CalibrationInputClass::SourceCredit ||
        value == CalibrationInputClass::Recovery ||
        value == CalibrationInputClass::QualifiedOutput;
}

[[nodiscard]] bool input_class_matches_target(
    std::string_view target,
    CalibrationInputClass input_class) noexcept {
    if (target.ends_with(".weight") || target.ends_with("_weight")) {
        return input_class == CalibrationInputClass::Probability;
    }
    if (target ==
        "portfolio/portfolio.annual_physical_hurdle_rate") {
        return input_class == CalibrationInputClass::PolicyHurdle;
    }
    if (target == "portfolio/portfolio.horizon_months" ||
        target == "portfolio/portfolio.currency_label" ||
        target == "portfolio/portfolio.monetary_basis") {
        return input_class == CalibrationInputClass::InstrumentTerm;
    }
    if (target.ends_with(".commitment_million") ||
        target.find(".draw.") != std::string_view::npos) {
        return input_class == CalibrationInputClass::Capital;
    }
    if (target.ends_with(".stage")) {
        return input_class == CalibrationInputClass::Transition;
    }
    if (target.find(".factor_tag.") != std::string_view::npos) {
        return input_class == CalibrationInputClass::Dependence;
    }
    if (target.find(".cash_source.") != std::string_view::npos ||
        target.find(".receipt.") != std::string_view::npos) {
        return is_ambiguous_cash_class(input_class);
    }
    if (target.ends_with(".resolution")) {
        return input_class == CalibrationInputClass::Recovery ||
            input_class == CalibrationInputClass::Transition;
    }
    if (target.ends_with(".project_id") ||
        (target.starts_with("portfolio/project.") &&
         target.ends_with(".id"))) {
        return input_class == CalibrationInputClass::Capital ||
            input_class == CalibrationInputClass::Transition;
    }
    if (target.find(".pool_cost.") != std::string_view::npos) {
        return input_class == CalibrationInputClass::Cost ||
            input_class == CalibrationInputClass::Capital;
    }
    if (target.starts_with("portfolio/loss_layer.")) {
        return input_class == CalibrationInputClass::Capital ||
            input_class == CalibrationInputClass::Recovery ||
            input_class == CalibrationInputClass::InstrumentTerm;
    }
    if ((target.starts_with("portfolio/scenario.") ||
         target.starts_with("ambiguity/scenario.")) &&
        target.ends_with(".id")) {
        return input_class == CalibrationInputClass::Dependence;
    }
    return false;
}

void validate_target_path_text(std::string_view value) {
    require_safe_text(value, "lineage target_path", 512U);
    if (!std::all_of(
            value.begin(), value.end(), [](unsigned char character) {
                return std::isalnum(character) != 0 || character == '-' ||
                    character == '_' || character == '.' || character == '/';
            }) || value.starts_with('/') || value.ends_with('/') ||
        value.find("../") != std::string_view::npos ||
        value.find("/..") != std::string_view::npos) {
        invalid("lineage target_path is unsafe");
    }
}

void validate_lineage_row_shape(const CalibrationLineageRow& row) {
    require_safe_identifier(row.input_id, "lineage input_id");
    validate_target_path_text(row.target_path);
    if (to_string(row.input_class) == "unknown") {
        invalid("lineage input_class enum value is invalid");
    }
    if (to_string(row.input_status) == "unknown") {
        invalid("lineage input_status enum value is invalid");
    }
    if (!input_class_matches_target(row.target_path, row.input_class)) {
        invalid("lineage input_class is incompatible with target_path: " +
                row.target_path);
    }
    if (row.input_class == CalibrationInputClass::Probability &&
        is_evidence_backed_status(row.input_status)) {
        invalid("binder v0.1 probability inputs require hypothesis, stress, synthetic, or policy status until an empirical population and method ledger is defined");
    }
    require_safe_identifier(row.method_id, "lineage method_id");
    require_safe_text(row.limitations, "lineage limitations", 1024U);
    require_safe_text(
        row.update_or_retire, "lineage update_or_retire", 1024U);
    std::unordered_set<std::string> evidence_ids;
    for (const std::string& id : row.evidence_record_ids) {
        require_safe_identifier(id, "evidence_record_ids");
        if (!evidence_ids.insert(id).second) {
            invalid("evidence_record_ids contains a duplicate ID");
        }
    }
    std::unordered_set<std::string> requirement_ids;
    for (const std::string& id : row.requirement_ids) {
        require_safe_identifier(id, "requirement_ids");
        if (!requirement_ids.insert(id).second) {
            invalid("requirement_ids contains a duplicate ID");
        }
    }
    if (is_evidence_backed_status(row.input_status) &&
        row.evidence_record_ids.empty()) {
        invalid("observed, contractual, derived, estimated, and transfer lineage rows require evidence citations");
    }
    if (row.evidence_record_ids.empty() != row.requirement_ids.empty()) {
        invalid("lineage evidence and requirement citations must both be NONE or both be present");
    }
}

[[nodiscard]] std::vector<CalibrationLineageRow> load_lineage(
    const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > kMaximumLineageBytes) {
        invalid("lineage file is missing or exceeds 16 MiB");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        invalid("could not open lineage file");
    }
    std::string line;
    if (!std::getline(input, line)) {
        invalid("lineage file is empty");
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (line != kLineageHeader) {
        invalid("lineage TSV header is not exact");
    }

    std::vector<CalibrationLineageRow> rows;
    std::size_t line_number = 1U;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            invalid("blank lineage row at line " +
                    std::to_string(line_number));
        }
        if (line.size() > kMaximumLineageLineBytes) {
            invalid("lineage line " + std::to_string(line_number) +
                    " exceeds 8192 bytes");
        }
        if (rows.size() >= kMaximumLineageRows) {
            invalid("lineage row count exceeds 500000");
        }
        const std::vector<std::string> fields = split_tab_line(line);
        if (fields.size() != 9U) {
            invalid("lineage line " + std::to_string(line_number) +
                    " must have exactly nine tab-separated fields");
        }
        CalibrationLineageRow row;
        row.input_id = fields[0U];
        row.target_path = fields[1U];
        row.input_class = parse_input_class(fields[2U]);
        row.input_status = parse_input_status(fields[3U]);
        row.method_id = fields[4U];
        row.evidence_record_ids =
            parse_id_list(fields[5U], "evidence_record_ids");
        row.requirement_ids =
            parse_id_list(fields[6U], "requirement_ids");
        row.limitations = fields[7U];
        row.update_or_retire = fields[8U];
        validate_lineage_row_shape(row);
        rows.push_back(std::move(row));
    }
    if (!input.eof()) {
        invalid("could not read lineage file completely");
    }
    if (rows.empty()) {
        invalid("lineage file contains no rows");
    }
    return rows;
}

void validate_lineage(
    const std::vector<CalibrationLineageRow>& rows,
    const std::vector<std::string>& targets,
    const EvidenceDossier& dossier,
    const std::vector<EvidenceRecordGateUseAssessment>&
        gate_use_assessments) {
    std::unordered_map<std::string, const EvidenceRecord*> evidence;
    evidence.reserve(dossier.records.size());
    for (const EvidenceRecord& record : dossier.records) {
        evidence.emplace(record.record_id, &record);
    }
    std::unordered_map<
        std::string, const EvidenceRecordGateUseAssessment*> qualifications;
    qualifications.reserve(gate_use_assessments.size());
    for (const EvidenceRecordGateUseAssessment& assessment :
         gate_use_assessments) {
        qualifications.emplace(assessment.record_id, &assessment);
    }
    const std::set<std::string> target_set(targets.begin(), targets.end());
    std::unordered_set<std::string> seen_inputs;
    std::set<std::string> seen_targets;
    for (const CalibrationLineageRow& row : rows) {
        validate_lineage_row_shape(row);
        if (!seen_inputs.insert(row.input_id).second) {
            invalid("duplicate lineage input_id: " + row.input_id);
        }
        if (!seen_targets.insert(row.target_path).second) {
            invalid("duplicate lineage target_path: " + row.target_path);
        }
        if (!target_set.contains(row.target_path)) {
            invalid("orphan lineage target_path: " + row.target_path);
        }

        std::set<std::string> cited_requirements;
        bool contractual_record_present = false;
        for (const std::string& record_id : row.evidence_record_ids) {
            const auto found = evidence.find(record_id);
            if (found == evidence.end()) {
                invalid("unknown evidence citation: " + record_id);
            }
            if (is_evidence_backed_status(row.input_status)) {
                const auto qualification = qualifications.find(record_id);
                if (qualification == qualifications.end()) {
                    invalid("evidence citation lacks a prepared gate-use assessment: " +
                            record_id);
                }
                if (!qualification->second->record_qualifies ||
                    !qualification->second->requirement_passed) {
                    invalid("evidence-backed lineage citation does not qualify for controlled gate use with a passing requirement: " +
                            record_id);
                }
                contractual_record_present =
                    contractual_record_present ||
                    found->second->source_class ==
                        SourceClass::ExecutedContract;
            }
            cited_requirements.insert(found->second->requirement_id);
        }
        if (row.input_status == CalibrationInputStatus::Contractual &&
            !contractual_record_present) {
            invalid("contractual lineage rows require an executed-contract citation");
        }
        const std::set<std::string> declared_requirements(
            row.requirement_ids.begin(), row.requirement_ids.end());
        if (cited_requirements != declared_requirements) {
            invalid("evidence requirement citations do not match declarations for " +
                    row.input_id);
        }
    }
    if (seen_targets != target_set) {
        const auto missing = std::find_if(
            target_set.begin(), target_set.end(),
            [&seen_targets](const std::string& target) {
                return !seen_targets.contains(target);
            });
        invalid("missing lineage target_path: " +
                (missing == target_set.end() ? std::string("unknown")
                                             : *missing));
    }
}

[[nodiscard]] std::string join_ids(const std::vector<std::string>& values) {
    if (values.empty()) {
        return "NONE";
    }
    std::vector<std::string> normalized = values;
    std::sort(normalized.begin(), normalized.end());
    std::ostringstream output;
    for (std::size_t index = 0U; index < normalized.size(); ++index) {
        if (index != 0U) {
            output << ';';
        }
        output << normalized[index];
    }
    return output.str();
}

} // namespace

CalibrationBinderConfig load_calibration_binder_config(
    const std::filesystem::path& path) {
    const auto values = load_key_values(path);
    CalibrationBinderConfig config;
    config.version = required(values, "binder.version");
    config.id = required(values, "binder.id");
    config.project_id = required(values, "binder.project_id");
    config.dossier_id = required(values, "binder.dossier_id");
    config.as_of_date = required(values, "binder.as_of_date");
    config.source_note = required(values, "binder.source_note");
    const std::string& candidate_only =
        required(values, "binder.candidate_only");
    if (candidate_only != "true" && candidate_only != "false") {
        invalid("binder.candidate_only must be true or false");
    }
    config.candidate_only = candidate_only == "true";
    config.probability_measure =
        required(values, "binder.probability_measure");
    config.portfolio = parse_bound_file(values, "portfolio");
    config.ambiguity = parse_bound_file(values, "ambiguity");
    config.dossier = parse_bound_file(values, "dossier");
    config.evidence_manifest =
        parse_bound_file(values, "evidence_manifest");
    config.lineage = parse_bound_file(values, "lineage");
    validate_binder_config(config);
    return config;
}

void print_normalized_calibration_binder_config(
    std::ostream& output,
    const CalibrationBinderConfig& config) {
    validate_binder_config(config);
    output << "binder.version=" << config.version << '\n'
           << "binder.id=" << config.id << '\n'
           << "binder.project_id=" << config.project_id << '\n'
           << "binder.dossier_id=" << config.dossier_id << '\n'
           << "binder.as_of_date=" << config.as_of_date << '\n'
           << "binder.source_note=" << config.source_note << '\n'
           << "binder.candidate_only=true\n"
           << "binder.probability_measure=" << config.probability_measure
           << '\n';
    const auto print_file = [&output](
                                std::string_view name,
                                const CalibrationBoundFile& file) {
        output << "file." << name << ".path="
               << file.relative_path.generic_string() << '\n'
               << "file." << name << ".sha256=" << file.sha256 << '\n';
    };
    print_file("portfolio", config.portfolio);
    print_file("ambiguity", config.ambiguity);
    print_file("dossier", config.dossier);
    print_file("evidence_manifest", config.evidence_manifest);
    print_file("lineage", config.lineage);
}

std::vector<std::string> material_calibration_target_paths(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity) {
    std::ostringstream portfolio_output;
    std::ostringstream ambiguity_output;
    print_normalized_portfolio_config(portfolio_output, portfolio);
    print_normalized_portfolio_ambiguity_config(
        ambiguity_output, ambiguity);
    std::vector<std::string> targets;
    append_material_targets(
        targets, "portfolio", portfolio_output.str());
    append_material_targets(
        targets, "ambiguity", ambiguity_output.str());
    std::sort(targets.begin(), targets.end());
    if (std::adjacent_find(targets.begin(), targets.end()) != targets.end()) {
        throw std::logic_error(
            "material calibration target generation produced a duplicate");
    }
    return targets;
}

CalibrationBinder load_calibration_binder(
    const std::filesystem::path& binder_path,
    std::string_view evaluation_date) {
    CalibrationBinder result;
    std::error_code error;
    result.directory = std::filesystem::canonical(
        std::filesystem::absolute(binder_path).parent_path(), error);
    if (error) {
        invalid("could not resolve binder directory");
    }
    result.config = load_calibration_binder_config(binder_path);

    const std::filesystem::path portfolio_path = resolve_bound_path(
        result.directory, result.config.portfolio, "portfolio file",
        kMaximumPortfolioBytes);
    const std::filesystem::path ambiguity_path = resolve_bound_path(
        result.directory, result.config.ambiguity, "ambiguity file",
        kMaximumAmbiguityBytes);
    const std::filesystem::path dossier_path = resolve_bound_path(
        result.directory, result.config.dossier, "dossier file",
        kMaximumDossierBytes);
    const std::filesystem::path manifest_path = resolve_bound_path(
        result.directory, result.config.evidence_manifest,
        "evidence manifest file", kMaximumManifestBytes);
    const std::filesystem::path lineage_path = resolve_bound_path(
        result.directory, result.config.lineage, "lineage file",
        kMaximumLineageBytes);

    result.portfolio = load_portfolio_config(portfolio_path);
    result.ambiguity = load_portfolio_ambiguity_config(ambiguity_path);
    if (!result.portfolio.synthetic_inputs ||
        !result.ambiguity.synthetic_inputs) {
        invalid("portfolio and ambiguity synthetic_inputs must both remain true");
    }
    if (result.portfolio.projects.size() != 1U) {
        invalid("binder v0.1 requires exactly one portfolio project");
    }
    if (result.config.project_id != result.portfolio.projects.front().id) {
        invalid("binder.project_id does not match the portfolio project ID");
    }
    static_cast<void>(
        evaluate_portfolio_ambiguity(result.portfolio, result.ambiguity));
    result.material_target_paths = material_calibration_target_paths(
        result.portfolio, result.ambiguity);

    result.dossier = load_evidence_dossier(dossier_path, manifest_path);
    if (result.config.dossier_id != result.dossier.metadata.id) {
        invalid("binder.dossier_id does not match the loaded dossier ID");
    }
    if (result.dossier.metadata.as_of_date > result.config.as_of_date) {
        invalid("dossier as-of date follows binder as-of date");
    }
    result.evaluation_date = evaluation_date.empty()
        ? result.config.as_of_date
        : std::string(evaluation_date);
    if (!is_iso_date(result.evaluation_date)) {
        invalid("evaluation date must be a calendar date in YYYY-MM-DD");
    }
    if (result.evaluation_date < result.config.as_of_date) {
        invalid("evaluation date must not precede binder as-of date");
    }
    EvidenceGateUseBatchAssessment gate_use_batch =
        assess_evidence_gate_use_batch(
            result.dossier, result.evaluation_date);
    result.evidence_assessment =
        std::move(gate_use_batch.dossier_assessment);

    result.lineage = load_lineage(lineage_path);
    validate_lineage(
        result.lineage, result.material_target_paths, result.dossier,
        gate_use_batch.records);

    const auto recheck = [&result](
                             const CalibrationBoundFile& bound,
                             std::string_view label,
                             std::uintmax_t maximum_bytes,
                             const std::filesystem::path& expected_path) {
        const std::filesystem::path current = resolve_bound_path(
            result.directory, bound, label, maximum_bytes);
        if (current != expected_path) {
            invalid(std::string(label) +
                    " resolved path changed during binder review");
        }
    };
    recheck(result.config.portfolio, "portfolio file",
            kMaximumPortfolioBytes, portfolio_path);
    recheck(result.config.ambiguity, "ambiguity file",
            kMaximumAmbiguityBytes, ambiguity_path);
    recheck(result.config.dossier, "dossier file",
            kMaximumDossierBytes, dossier_path);
    recheck(result.config.evidence_manifest, "evidence manifest file",
            kMaximumManifestBytes, manifest_path);
    recheck(result.config.lineage, "lineage file",
            kMaximumLineageBytes, lineage_path);
    result.candidate_package_valid = true;
    result.calibrated_execution_authorized = false;
    return result;
}

void print_normalized_calibration_lineage(
    std::ostream& output,
    const std::vector<CalibrationLineageRow>& lineage) {
    std::vector<CalibrationLineageRow> normalized = lineage;
    std::sort(
        normalized.begin(), normalized.end(),
        [](const CalibrationLineageRow& left,
           const CalibrationLineageRow& right) {
            return left.target_path < right.target_path;
        });
    std::unordered_set<std::string> inputs;
    std::unordered_set<std::string> targets;
    output << kLineageHeader << '\n';
    for (const CalibrationLineageRow& row : normalized) {
        validate_lineage_row_shape(row);
        if (!inputs.insert(row.input_id).second ||
            !targets.insert(row.target_path).second) {
            invalid("normalized lineage contains duplicate IDs or targets");
        }
        output << row.input_id << '\t' << row.target_path << '\t'
               << to_string(row.input_class) << '\t'
               << to_string(row.input_status) << '\t' << row.method_id
               << '\t' << join_ids(row.evidence_record_ids) << '\t'
               << join_ids(row.requirement_ids) << '\t' << row.limitations
               << '\t' << row.update_or_retire << '\n';
    }
}

void print_calibration_binder_report(
    std::ostream& output,
    const CalibrationBinder& binder) {
    if (!binder.candidate_package_valid ||
        binder.calibrated_execution_authorized) {
        throw std::logic_error(
            "calibration binder report received an invalid authorization state");
    }
    output
        << "CALIBRATION BINDER CANDIDATE REVIEW - NO CALIBRATED EXECUTION\n"
        << "binder_id=" << binder.config.id << '\n'
        << "project_id=" << binder.config.project_id << '\n'
        << "dossier_id=" << binder.config.dossier_id << '\n'
        << "binder_as_of_date=" << binder.config.as_of_date << '\n'
        << "evaluation_date=" << binder.evaluation_date << '\n'
        << "probability_measure=" << binder.config.probability_measure << '\n'
        << "candidate_only=true\n"
        << "candidate_status=structurally-checked-synthetic-candidate\n"
        << "calibrated_execution_authorized=false\n"
        << "dossier_status=" << to_string(binder.dossier.metadata.status)
        << '\n'
        << "dossier_highest_allowed_use="
        << to_string(binder.evidence_assessment.highest_allowed_use) << '\n'
        << "material_target_count=" << binder.material_target_paths.size()
        << '\n'
        << "lineage_row_count=" << binder.lineage.size() << "\n\n"
        << "Hash-bound files\n"
        << "  portfolio " << binder.config.portfolio.sha256 << '\n'
        << "  ambiguity " << binder.config.ambiguity.sha256 << '\n'
        << "  dossier " << binder.config.dossier.sha256 << '\n'
        << "  evidence_manifest "
        << binder.config.evidence_manifest.sha256 << '\n'
        << "  lineage " << binder.config.lineage.sha256 << "\n\n"
        << "Dossier gates\n";
    for (const GateAssessment& gate : binder.evidence_assessment.gates) {
        output << "  " << (gate.passed ? "PASS" : "FAIL") << ' '
               << to_string(gate.gate) << ' ' << gate.requirements_met << '/'
               << gate.requirements_total << '\n';
    }
    output
        << "\nInterpretation boundary\n"
        << "  This report verifies package confinement, raw-file SHA-256,\n"
        << "  exact material-target lineage coverage, declared evidence links,\n"
        << "  physical-P labeling, synthetic model compatibility, and dossier\n"
        << "  gate status. It never authorizes calibrated execution, valuation,\n"
        << "  pricing, a rating, an offering, or an investment recommendation.\n";
}

std::string_view to_string(CalibrationInputClass value) noexcept {
    switch (value) {
    case CalibrationInputClass::Capital:
        return "capital";
    case CalibrationInputClass::Transition:
        return "transition";
    case CalibrationInputClass::Probability:
        return "probability";
    case CalibrationInputClass::Recovery:
        return "recovery";
    case CalibrationInputClass::Dependence:
        return "dependence";
    case CalibrationInputClass::QualifiedOutput:
        return "qualified-output";
    case CalibrationInputClass::CommercialCash:
        return "commercial-cash";
    case CalibrationInputClass::SourceCredit:
        return "source-credit";
    case CalibrationInputClass::Cost:
        return "cost";
    case CalibrationInputClass::PolicyHurdle:
        return "policy-hurdle";
    case CalibrationInputClass::InstrumentTerm:
        return "instrument-term";
    }
    return "unknown";
}

std::string_view to_string(CalibrationInputStatus value) noexcept {
    switch (value) {
    case CalibrationInputStatus::Observed:
        return "observed";
    case CalibrationInputStatus::Contractual:
        return "contractual";
    case CalibrationInputStatus::Derived:
        return "derived";
    case CalibrationInputStatus::Estimated:
        return "estimated";
    case CalibrationInputStatus::Transfer:
        return "transfer";
    case CalibrationInputStatus::Hypothesis:
        return "hypothesis";
    case CalibrationInputStatus::Stress:
        return "stress";
    case CalibrationInputStatus::Synthetic:
        return "synthetic";
    case CalibrationInputStatus::Policy:
        return "policy";
    }
    return "unknown";
}

} // namespace naturalehia::cellular_finance
