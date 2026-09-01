// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/partial_credit_claim_loss_cohort_config.hpp>

#include "partial_credit_claim_loss_cohort_internal.hpp"

#include <naturalehia/cellular_finance/joint_cohort.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::string_view kBinderSchemaVersion =
    "partial-credit-claim-loss-cohort-binder-v0.1";
constexpr std::uintmax_t kMaximumConfigBytes = 1024U * 1024U;
constexpr std::uintmax_t kMaximumObservationBytes = 32U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumMethodBytes = 4U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumDossierBytes = 1024U * 1024U;
constexpr std::uintmax_t kMaximumManifestBytes = 32U * 1024U * 1024U;
constexpr std::size_t kMaximumConfigLineLength = 8192U;
constexpr std::size_t kMaximumTsvLineLength = 64U * 1024U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 4096U;
constexpr std::string_view kObservationHeader =
    "observation_id\teconomic_cluster_id\teligible_date\thorizon_end_date\t"
    "disposition\ttrigger_status\ttrigger_date\tclassification_date\t"
    "resolution_date\texclusion_rule_id\tclaim_cfg_path\t"
    "claim_config_sha256\trealized_scenario_id\tprovider_claim_id\t"
    "population_evidence_record_ids\tpopulation_requirement_ids\t"
    "classification_evidence_record_ids\tclassification_requirement_ids";

[[noreturn]] void invalid(std::string message) {
    throw std::invalid_argument(
        "partial-credit claim-loss cohort package: " + std::move(message));
}

[[nodiscard]] bool ascii_alphanumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool safe_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength ||
        !ascii_alphanumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return ascii_alphanumeric(character) || character == '-' ||
            character == '_' || character == '.';
    });
}

void require_safe_identifier(
    std::string_view value, std::string_view description) {
    if (!safe_identifier(value)) {
        invalid(std::string(description) + " must be a safe identifier");
    }
}

void require_safe_text(std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength) {
        invalid(std::string(description) + " must be non-empty and bounded");
    }
    for (const char raw : value) {
        const auto character = static_cast<unsigned char>(raw);
        if (character < 0x20U || character == 0x7fU) {
            invalid(std::string(description) + " contains a control character");
        }
    }
}

[[nodiscard]] bool lower_hex_sha256(std::string_view value) noexcept {
    return value.size() == 64U &&
        std::all_of(value.begin(), value.end(), [](char character) {
            return (character >= '0' && character <= '9') ||
                (character >= 'a' && character <= 'f');
        });
}

[[nodiscard]] bool valid_utf8(std::string_view value) noexcept {
    std::size_t index = 0U;
    while (index < value.size()) {
        const auto first = static_cast<unsigned char>(value[index]);
        if (first <= 0x7fU) {
            ++index;
            continue;
        }
        std::size_t count = 0U;
        std::uint32_t codepoint = 0U;
        if (first >= 0xc2U && first <= 0xdfU) {
            count = 2U;
            codepoint = first & 0x1fU;
        } else if (first >= 0xe0U && first <= 0xefU) {
            count = 3U;
            codepoint = first & 0x0fU;
        } else if (first >= 0xf0U && first <= 0xf4U) {
            count = 4U;
            codepoint = first & 0x07U;
        } else {
            return false;
        }
        if (count > value.size() - index) return false;
        for (std::size_t continuation = 1U;
             continuation < count; ++continuation) {
            const auto next = static_cast<unsigned char>(
                value[index + continuation]);
            if ((next & 0xc0U) != 0x80U) return false;
            codepoint = (codepoint << 6U) | (next & 0x3fU);
        }
        if ((count == 3U && codepoint < 0x800U) ||
            (count == 4U && codepoint < 0x10000U) ||
            (codepoint >= 0xd800U && codepoint <= 0xdfffU) ||
            codepoint > 0x10ffffU) {
            return false;
        }
        index += count;
    }
    return true;
}

void validate_text_snapshot(
    std::string_view bytes, std::string_view label) {
    if (bytes.starts_with("\xef\xbb\xbf")) {
        invalid(std::string(label) + " must not contain a UTF-8 BOM");
    }
    if (!valid_utf8(bytes)) {
        invalid(std::string(label) + " must contain valid UTF-8");
    }
    if (bytes.find('\r') != std::string_view::npos) {
        invalid(std::string(label) + " must use LF line endings");
    }
    if (bytes.find('\0') != std::string_view::npos) {
        invalid(std::string(label) + " must not contain NUL bytes");
    }
}

[[nodiscard]] std::string read_bounded_file_bytes(
    const std::filesystem::path& path, std::uintmax_t maximum_bytes,
    std::string_view label) {
    std::error_code error;
    const bool regular = std::filesystem::is_regular_file(path, error);
    if (error || !regular) {
        invalid(std::string(label) + " is missing or is not a regular file");
    }
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size > maximum_bytes) {
        invalid(std::string(label) + " exceeds its byte limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) invalid("could not open " + std::string(label));
    std::string bytes;
    bytes.reserve(static_cast<std::size_t>(size));
    std::array<char, 8192U> buffer{};
    while (input) {
        input.read(buffer.data(),
                   static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            const std::size_t additional = static_cast<std::size_t>(count);
            if (additional > maximum_bytes - bytes.size()) {
                invalid(std::string(label) + " grew beyond its byte limit");
            }
            bytes.append(buffer.data(), additional);
        }
    }
    if (!input.eof()) invalid("could not read " + std::string(label));
    validate_text_snapshot(bytes, label);
    return bytes;
}

[[nodiscard]] bool safe_relative_path(std::string_view value) noexcept {
    if (value.empty() || value.size() > 512U || value.front() == '/' ||
        value.back() == '/' || value.find('\\') != std::string_view::npos ||
        value.find(':') != std::string_view::npos ||
        value.find("//") != std::string_view::npos) {
        return false;
    }
    std::size_t start = 0U;
    while (start < value.size()) {
        const std::size_t end = value.find('/', start);
        const std::string_view part = value.substr(start,
            end == std::string_view::npos ? value.size() - start : end - start);
        if (part.empty() || part == "." || part == ".." ||
            !std::all_of(part.begin(), part.end(), [](char character) {
                return ascii_alphanumeric(character) || character == '.' ||
                    character == '_' || character == '-';
            })) {
            return false;
        }
        start = end == std::string_view::npos ? value.size() : end + 1U;
    }
    return true;
}

[[nodiscard]] bool path_is_confined(
    const std::filesystem::path& path,
    const std::filesystem::path& directory) noexcept {
    const std::filesystem::path relative = path.lexically_relative(directory);
    if (relative.empty() || relative.is_absolute()) return false;
    return std::none_of(relative.begin(), relative.end(),
        [](const std::filesystem::path& component) {
            return component == std::filesystem::path("..");
        });
}

[[nodiscard]] std::filesystem::path canonical_directory(
    const std::filesystem::path& root) {
    std::error_code error;
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(root, error);
    if (error || !std::filesystem::is_directory(canonical, error) || error) {
        invalid("root must be an existing directory");
    }
    return canonical;
}

[[nodiscard]] std::filesystem::path resolve_confined_regular(
    const std::filesystem::path& directory, std::string_view relative,
    std::string_view label) {
    if (!safe_relative_path(relative)) {
        invalid(std::string(label) + " path must be safely relative");
    }
    std::error_code error;
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(
        directory / std::filesystem::path(relative), error);
    if (error || !path_is_confined(resolved, directory) ||
        !std::filesystem::is_regular_file(resolved, error) || error) {
        invalid(std::string(label) + " path is missing or escapes the package");
    }
    return resolved;
}

using KeyValues = std::unordered_map<std::string, std::string>;

[[nodiscard]] KeyValues parse_key_values(
    std::string_view bytes, std::string_view label) {
    KeyValues values;
    std::size_t start = 0U;
    std::size_t line_number = 0U;
    while (start <= bytes.size()) {
        const std::size_t end = bytes.find('\n', start);
        const std::string_view line = bytes.substr(start,
            end == std::string_view::npos ? bytes.size() - start : end - start);
        ++line_number;
        if (line.size() > kMaximumConfigLineLength) {
            invalid(std::string(label) + " line exceeds its length limit");
        }
        if (!line.empty() && line.front() != '#') {
            const std::size_t equals = line.find('=');
            if (equals == std::string_view::npos || equals == 0U ||
                equals + 1U == line.size() ||
                line.find('=', equals + 1U) != std::string_view::npos) {
                invalid(std::string(label) + " line " +
                    std::to_string(line_number) +
                    " must contain one nonempty key=value pair");
            }
            const std::string key(line.substr(0U, equals));
            const std::string value(line.substr(equals + 1U));
            if (key.front() == ' ' || key.back() == ' ' ||
                value.front() == ' ' || value.back() == ' ' ||
                !values.emplace(key, value).second) {
                invalid(std::string(label) +
                    " contains whitespace or a duplicate key: " + key);
            }
        }
        if (end == std::string_view::npos) break;
        start = end + 1U;
    }
    return values;
}

[[nodiscard]] const std::string& require_key(
    const KeyValues& values, std::string_view key) {
    const auto found = values.find(std::string(key));
    if (found == values.end()) {
        invalid("missing configuration key: " + std::string(key));
    }
    return found->second;
}

[[nodiscard]] bool parse_bool(
    std::string_view value, std::string_view key) {
    if (value == "true") return true;
    if (value == "false") return false;
    invalid(std::string(key) + " must be true or false");
}

[[nodiscard]] std::size_t parse_size(
    std::string_view value, std::string_view key, std::size_t maximum) {
    std::size_t parsed = 0U;
    const auto result = std::from_chars(
        value.data(), value.data() + value.size(), parsed);
    if (value.empty() || result.ec != std::errc{} ||
        result.ptr != value.data() + value.size() || parsed > maximum) {
        invalid(std::string(key) + " must be a bounded unsigned integer");
    }
    return parsed;
}

[[nodiscard]] std::vector<std::string> parse_ids(
    std::string_view value, std::string_view description) {
    if (value.empty() || value == "NONE") {
        invalid(std::string(description) + " must be a non-empty ID list");
    }
    std::vector<std::string> ids;
    std::size_t start = 0U;
    while (true) {
        const std::size_t separator = value.find(',', start);
        const std::string_view item = value.substr(start,
            separator == std::string_view::npos
                ? value.size() - start : separator - start);
        require_safe_identifier(item, description);
        ids.emplace_back(item);
        if (ids.size() > kMaximumPartialCreditCohortEvidenceIdsPerList) {
            invalid(std::string(description) + " exceeds its item limit");
        }
        if (separator == std::string_view::npos) break;
        start = separator + 1U;
    }
    if (!std::is_sorted(ids.begin(), ids.end()) ||
        std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
        invalid(std::string(description) + " must be sorted and unique");
    }
    return ids;
}

[[nodiscard]] PartialCreditClaimLossCohortBoundFile bound_file(
    const KeyValues& values, std::string_view path_key,
    std::string_view hash_key, std::string_view exact_path) {
    PartialCreditClaimLossCohortBoundFile file;
    file.relative_path = require_key(values, path_key);
    file.sha256 = require_key(values, hash_key);
    if (file.relative_path.generic_string() != exact_path) {
        invalid(std::string(path_key) + " must equal " +
            std::string(exact_path));
    }
    if (!lower_hex_sha256(file.sha256)) {
        invalid(std::string(hash_key) +
            " must be 64 lowercase hexadecimal characters");
    }
    return file;
}

[[nodiscard]] PartialCreditClaimLossCohortConfig parse_cohort_config(
    std::string_view bytes) {
    const KeyValues values = parse_key_values(bytes, "cohort.cfg");
    const std::set<std::string> scalar_keys{
        "schema_version", "cohort_id", "as_of_date", "frame_start_date",
        "frame_end_date", "population_definition", "source_note",
        "sampling_unit_definition", "economic_cluster_definition",
        "protection_term_stratum_definition", "outcome_horizon_definition",
        "loss_definition", "resolution_definition", "censoring_definition",
        "denominator_definition", "currency_label", "monetary_basis",
        "monetary_basis_definition", "population_frame_count",
        "candidate_only", "observations_path", "observations_sha256",
        "methods_path", "methods_sha256", "dossier_path", "dossier_sha256",
        "evidence_manifest_path", "evidence_manifest_sha256"};
    std::map<std::string, std::map<std::string, std::string>> exclusion_values;
    for (const auto& [key, value] : values) {
        if (scalar_keys.contains(key)) continue;
        constexpr std::string_view prefix = "exclusion_rule.";
        if (!std::string_view(key).starts_with(prefix)) {
            invalid("unknown configuration key: " + key);
        }
        const std::string_view remainder(key.data() + prefix.size(),
            key.size() - prefix.size());
        const std::size_t dot = remainder.rfind('.');
        if (dot == std::string_view::npos || dot == 0U ||
            dot + 1U == remainder.size()) {
            invalid("malformed exclusion rule key: " + key);
        }
        const std::string id(remainder.substr(0U, dot));
        const std::string field(remainder.substr(dot + 1U));
        require_safe_identifier(id, "exclusion rule ID");
        if (field != "frozen_date" && field != "outcome_blind_asserted" &&
            field != "statement" && field != "evidence_record_ids") {
            invalid("unknown exclusion rule field: " + field);
        }
        exclusion_values[id].emplace(field, value);
    }
    for (const std::string& key : scalar_keys) {
        static_cast<void>(require_key(values, key));
    }

    if (require_key(values, "schema_version") != kBinderSchemaVersion) {
        invalid("unsupported schema_version");
    }
    PartialCreditClaimLossCohortConfig config;
    config.version = std::string(kPartialCreditClaimLossCohortVersion);
    config.cohort_id = require_key(values, "cohort_id");
    config.as_of_date = require_key(values, "as_of_date");
    config.frame_start_date = require_key(values, "frame_start_date");
    config.frame_end_date = require_key(values, "frame_end_date");
    config.population_definition = require_key(values, "population_definition");
    config.source_note = require_key(values, "source_note");
    config.sampling_unit_definition = require_key(values, "sampling_unit_definition");
    config.economic_cluster_definition = require_key(values, "economic_cluster_definition");
    config.protection_term_stratum_definition = require_key(values, "protection_term_stratum_definition");
    config.outcome_horizon_definition = require_key(values, "outcome_horizon_definition");
    config.loss_definition = require_key(values, "loss_definition");
    config.resolution_definition = require_key(values, "resolution_definition");
    config.censoring_definition = require_key(values, "censoring_definition");
    config.denominator_definition = require_key(values, "denominator_definition");
    config.currency_label = require_key(values, "currency_label");
    config.monetary_basis = require_key(values, "monetary_basis");
    config.monetary_basis_definition = require_key(values, "monetary_basis_definition");
    config.population_frame_count = parse_size(
        require_key(values, "population_frame_count"),
        "population_frame_count", kMaximumPartialCreditCohortObservations);
    config.candidate_only = parse_bool(
        require_key(values, "candidate_only"), "candidate_only");
    config.observations_file = bound_file(values, "observations_path",
        "observations_sha256", "observations.tsv");
    config.methods_file = bound_file(values, "methods_path",
        "methods_sha256", "methods.cfg");
    config.dossier_file = bound_file(values, "dossier_path",
        "dossier_sha256", "dossier.cfg");
    config.evidence_manifest_file = bound_file(values,
        "evidence_manifest_path", "evidence_manifest_sha256",
        "evidence_manifest.tsv");
    for (const auto& [id, fields] : exclusion_values) {
        if (fields.size() != 4U) {
            invalid("exclusion rule " + id + " is incomplete");
        }
        const auto field = [&](std::string_view name) -> const std::string& {
            const auto found = fields.find(std::string(name));
            if (found == fields.end()) {
                invalid("exclusion rule " + id + " is missing " +
                    std::string(name));
            }
            return found->second;
        };
        PartialCreditClaimLossExclusionRule rule;
        rule.id = id;
        rule.frozen_date = field("frozen_date");
        rule.outcome_blind_asserted = parse_bool(
            field("outcome_blind_asserted"),
            "exclusion outcome_blind_asserted");
        rule.statement = field("statement");
        rule.evidence_record_ids = parse_ids(
            field("evidence_record_ids"), "exclusion evidence IDs");
        config.exclusion_rules.push_back(std::move(rule));
    }
    validate_partial_credit_claim_loss_cohort_config(config);
    require_safe_identifier(config.monetary_basis_definition,
        "monetary_basis_definition");
    return config;
}

struct BoundSnapshot {
    std::filesystem::path path{};
    std::string bytes{};
};

[[nodiscard]] BoundSnapshot load_bound_snapshot(
    const std::filesystem::path& directory,
    const PartialCreditClaimLossCohortBoundFile& file,
    std::uintmax_t maximum_bytes, std::string_view label) {
    const std::string relative = file.relative_path.generic_string();
    const std::filesystem::path path =
        resolve_confined_regular(directory, relative, label);
    BoundSnapshot snapshot{path,
        read_bounded_file_bytes(path, maximum_bytes, label)};
    if (sha256_bytes_lower_hex(snapshot.bytes) != file.sha256) {
        invalid(std::string(label) + " SHA-256 mismatch");
    }
    return snapshot;
}

void recheck_snapshot(
    const std::filesystem::path& directory,
    const PartialCreditClaimLossCohortBoundFile& file,
    const BoundSnapshot& original, std::uintmax_t maximum_bytes,
    std::string_view label) {
    const BoundSnapshot fresh = load_bound_snapshot(
        directory, file, maximum_bytes, label);
    if (fresh.path != original.path || fresh.bytes != original.bytes) {
        invalid(std::string(label) + " changed during package loading");
    }
}

struct RetainedEvidenceSnapshot {
    std::string record_id{};
    std::filesystem::path relative_path{};
    std::filesystem::path canonical_path{};
    std::uintmax_t byte_size{};
    std::string sha256{};

    [[nodiscard]] bool operator==(
        const RetainedEvidenceSnapshot&) const noexcept = default;
};

[[nodiscard]] RetainedEvidenceSnapshot snapshot_retained_evidence(
    const std::filesystem::path& directory,
    const EvidenceRecord& record) {
    const std::string relative = record.retained_copy.generic_string();
    const std::filesystem::path path = resolve_confined_regular(
        directory, relative, "retained evidence " + record.record_id);
    std::error_code error;
    const std::uintmax_t size_before =
        std::filesystem::file_size(path, error);
    if (error) {
        invalid("could not size retained evidence " + record.record_id);
    }
    const std::string actual_hash = sha256_file_lower_hex(path);
    const std::uintmax_t size_after =
        std::filesystem::file_size(path, error);
    if (error || size_before != size_after) {
        invalid("retained evidence changed while being snapshotted: " +
            record.record_id);
    }
    if (actual_hash != record.retained_sha256) {
        invalid("retained evidence SHA-256 mismatch: " + record.record_id);
    }
    return {record.record_id, record.retained_copy, path, size_after,
        actual_hash};
}

[[nodiscard]] std::vector<RetainedEvidenceSnapshot>
snapshot_retained_evidence_files(
    const std::filesystem::path& directory,
    const EvidenceDossier& dossier) {
    std::vector<RetainedEvidenceSnapshot> snapshots;
    snapshots.reserve(dossier.records.size());
    for (const EvidenceRecord& record : dossier.records) {
        if (!record.retained_copy.empty()) {
            snapshots.push_back(
                snapshot_retained_evidence(directory, record));
        }
    }
    return snapshots;
}

void recheck_retained_evidence_files(
    const std::filesystem::path& directory,
    const EvidenceDossier& dossier,
    const std::vector<RetainedEvidenceSnapshot>& original) {
    const std::vector<RetainedEvidenceSnapshot> fresh =
        snapshot_retained_evidence_files(directory, dossier);
    if (fresh != original) {
        invalid("retained evidence changed during package loading");
    }
}

[[nodiscard]] std::vector<std::string_view> split_tsv(
    std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0U;
    while (true) {
        const std::size_t tab = line.find('\t', start);
        fields.push_back(line.substr(start,
            tab == std::string_view::npos ? line.size() - start : tab - start));
        if (tab == std::string_view::npos) return fields;
        start = tab + 1U;
    }
}

[[nodiscard]] PartialCreditClaimLossDisposition parse_disposition(
    std::string_view value) {
    if (value == "resolved") return PartialCreditClaimLossDisposition::Resolved;
    if (value == "not-yet-matured") {
        return PartialCreditClaimLossDisposition::NotYetMatured;
    }
    if (value == "unresolved") return PartialCreditClaimLossDisposition::Unresolved;
    if (value == "excluded") return PartialCreditClaimLossDisposition::Excluded;
    invalid("observations.tsv contains an unknown disposition");
}

[[nodiscard]] PartialCreditClaimTriggerStatus parse_trigger_status(
    std::string_view value) {
    if (value == "triggered") return PartialCreditClaimTriggerStatus::Triggered;
    if (value == "not-triggered") {
        return PartialCreditClaimTriggerStatus::NotTriggered;
    }
    if (value == "unknown") return PartialCreditClaimTriggerStatus::Unknown;
    if (value == "not-applicable") {
        return PartialCreditClaimTriggerStatus::NotApplicable;
    }
    invalid("observations.tsv contains an unknown trigger_status");
}

[[nodiscard]] std::vector<PartialCreditClaimLossObservation>
parse_observations(std::string_view bytes) {
    std::vector<PartialCreditClaimLossObservation> observations;
    std::size_t start = 0U;
    std::size_t line_number = 0U;
    bool saw_header = false;
    while (start <= bytes.size()) {
        const std::size_t end = bytes.find('\n', start);
        const std::string_view line = bytes.substr(start,
            end == std::string_view::npos ? bytes.size() - start : end - start);
        ++line_number;
        if (line.size() > kMaximumTsvLineLength) {
            invalid("observations.tsv line exceeds its length limit");
        }
        if (!saw_header) {
            if (line != kObservationHeader) {
                invalid("observations.tsv header does not match the closed schema");
            }
            saw_header = true;
        } else if (!line.empty()) {
            const std::vector<std::string_view> fields = split_tsv(line);
            if (fields.size() != 18U ||
                std::any_of(fields.begin(), fields.end(),
                    [](std::string_view field) { return field.empty(); })) {
                invalid("observations.tsv row " + std::to_string(line_number) +
                    " must contain exactly 18 non-empty fields");
            }
            PartialCreditClaimLossObservation row;
            row.observation_id = fields[0];
            row.economic_cluster_id = fields[1];
            row.eligible_date = fields[2];
            row.horizon_end_date = fields[3];
            row.disposition = parse_disposition(fields[4]);
            row.trigger_status = parse_trigger_status(fields[5]);
            row.trigger_date = fields[6];
            row.classification_date = fields[7];
            row.resolution_date = fields[8];
            row.exclusion_rule_id = fields[9];
            if (fields[10] != "NONE") row.claim_cfg_path = fields[10];
            row.expected_claim_config_sha256 = fields[11];
            row.realized_scenario_id = fields[12];
            row.provider_claim_id = fields[13];
            row.population_evidence_record_ids = parse_ids(
                fields[14], "population evidence IDs");
            row.population_requirement_ids = parse_ids(
                fields[15], "population requirement IDs");
            row.classification_evidence_record_ids = parse_ids(
                fields[16], "classification evidence IDs");
            row.classification_requirement_ids = parse_ids(
                fields[17], "classification requirement IDs");
            observations.push_back(std::move(row));
            if (observations.size() > kMaximumPartialCreditCohortObservations) {
                invalid("observations.tsv exceeds its row limit");
            }
        }
        if (end == std::string_view::npos) break;
        start = end + 1U;
    }
    if (!saw_header) invalid("observations.tsv is empty");
    return observations;
}

[[nodiscard]] PartialCreditClaimLossMethodPurpose parse_method_purpose(
    std::string_view value) {
    if (value == "population") return PartialCreditClaimLossMethodPurpose::Population;
    if (value == "sampling-unit") return PartialCreditClaimLossMethodPurpose::SamplingUnit;
    if (value == "cluster") return PartialCreditClaimLossMethodPurpose::Cluster;
    if (value == "term-stratum") return PartialCreditClaimLossMethodPurpose::TermStratum;
    if (value == "horizon") return PartialCreditClaimLossMethodPurpose::Horizon;
    if (value == "loss") return PartialCreditClaimLossMethodPurpose::Loss;
    if (value == "resolution") return PartialCreditClaimLossMethodPurpose::Resolution;
    if (value == "censoring") return PartialCreditClaimLossMethodPurpose::Censoring;
    if (value == "denominator") return PartialCreditClaimLossMethodPurpose::Denominator;
    if (value == "monetary-basis") return PartialCreditClaimLossMethodPurpose::MonetaryBasis;
    if (value == "amount-bound") return PartialCreditClaimLossMethodPurpose::AmountBound;
    if (value == "metric") return PartialCreditClaimLossMethodPurpose::Metric;
    invalid("methods.cfg contains an unknown purpose");
}

[[nodiscard]] std::vector<PartialCreditClaimLossMethod> parse_methods(
    std::string_view bytes, std::string_view as_of_date) {
    const KeyValues values = parse_key_values(bytes, "methods.cfg");
    std::map<std::string, std::map<std::string, std::string>> records;
    for (const auto& [key, value] : values) {
        constexpr std::string_view prefix = "method.";
        if (!std::string_view(key).starts_with(prefix)) {
            invalid("unknown methods.cfg key: " + key);
        }
        const std::string_view remainder(key.data() + prefix.size(),
            key.size() - prefix.size());
        const std::size_t dot = remainder.rfind('.');
        if (dot == std::string_view::npos || dot == 0U ||
            dot + 1U == remainder.size()) {
            invalid("malformed method key: " + key);
        }
        const std::string id(remainder.substr(0U, dot));
        const std::string field(remainder.substr(dot + 1U));
        require_safe_identifier(id, "method ID");
        static const std::set<std::string> allowed{
            "purpose", "version", "implementation_id", "effective_date",
            "definition", "inputs", "output", "evidence_record_ids",
            "evidence_requirement_ids"};
        if (!allowed.contains(field)) {
            invalid("unknown method field: " + field);
        }
        records[id].emplace(field, value);
    }
    if (records.empty()) invalid("methods.cfg must contain method records");
    std::vector<PartialCreditClaimLossMethod> methods;
    methods.reserve(records.size());
    for (const auto& [id, fields] : records) {
        if (fields.size() != 9U) invalid("method " + id + " is incomplete");
        const auto field = [&](std::string_view name) -> const std::string& {
            const auto found = fields.find(std::string(name));
            if (found == fields.end()) {
                invalid("method " + id + " is missing " + std::string(name));
            }
            return found->second;
        };
        PartialCreditClaimLossMethod method;
        method.id = id;
        method.purpose = parse_method_purpose(field("purpose"));
        method.version = field("version");
        method.implementation_id = field("implementation_id");
        method.effective_date = field("effective_date");
        method.definition = field("definition");
        method.inputs = parse_ids(field("inputs"), "method inputs");
        method.output = field("output");
        method.evidence_record_ids = parse_ids(
            field("evidence_record_ids"), "method evidence IDs");
        method.evidence_requirement_ids = parse_ids(
            field("evidence_requirement_ids"), "method requirement IDs");
        require_safe_identifier(method.version, "method version");
        require_safe_identifier(method.implementation_id,
            "method implementation_id");
        if (method.implementation_id !=
                kPartialCreditClaimLossCohortMechanicalImplementationId) {
            invalid("method " + id + " uses an unsupported implementation_id");
        }
        if (!is_joint_cohort_iso_date(method.effective_date) ||
            method.effective_date > as_of_date) {
            invalid("method " + id + " has an invalid or future effective_date");
        }
        require_safe_text(method.definition, "method definition");
        require_safe_identifier(method.output, "method output");
        methods.push_back(std::move(method));
    }
    return methods;
}

void validate_method_bindings(
    const PartialCreditClaimLossCohortConfig& config,
    const std::vector<PartialCreditClaimLossMethod>& methods) {
    std::unordered_map<std::string, PartialCreditClaimLossMethodPurpose> index;
    std::set<PartialCreditClaimLossMethodPurpose> purposes;
    for (const PartialCreditClaimLossMethod& method : methods) {
        if (!index.emplace(method.id, method.purpose).second) {
            invalid("method IDs must be unique");
        }
        purposes.insert(method.purpose);
    }
    const auto require = [&](const std::string& id,
                             PartialCreditClaimLossMethodPurpose purpose,
                             std::string_view label) {
        const auto found = index.find(id);
        if (found == index.end() || found->second != purpose) {
            invalid(std::string(label) +
                " must reference a method with the matching purpose");
        }
    };
    require(config.population_definition,
        PartialCreditClaimLossMethodPurpose::Population, "population_definition");
    require(config.sampling_unit_definition,
        PartialCreditClaimLossMethodPurpose::SamplingUnit, "sampling_unit_definition");
    require(config.economic_cluster_definition,
        PartialCreditClaimLossMethodPurpose::Cluster, "economic_cluster_definition");
    require(config.protection_term_stratum_definition,
        PartialCreditClaimLossMethodPurpose::TermStratum, "protection_term_stratum_definition");
    require(config.outcome_horizon_definition,
        PartialCreditClaimLossMethodPurpose::Horizon, "outcome_horizon_definition");
    require(config.loss_definition,
        PartialCreditClaimLossMethodPurpose::Loss, "loss_definition");
    require(config.resolution_definition,
        PartialCreditClaimLossMethodPurpose::Resolution, "resolution_definition");
    require(config.censoring_definition,
        PartialCreditClaimLossMethodPurpose::Censoring, "censoring_definition");
    require(config.denominator_definition,
        PartialCreditClaimLossMethodPurpose::Denominator, "denominator_definition");
    require(config.monetary_basis_definition,
        PartialCreditClaimLossMethodPurpose::MonetaryBasis, "monetary_basis_definition");
    for (const auto purpose : {
             PartialCreditClaimLossMethodPurpose::Population,
             PartialCreditClaimLossMethodPurpose::SamplingUnit,
             PartialCreditClaimLossMethodPurpose::Cluster,
             PartialCreditClaimLossMethodPurpose::TermStratum,
             PartialCreditClaimLossMethodPurpose::Horizon,
             PartialCreditClaimLossMethodPurpose::Loss,
             PartialCreditClaimLossMethodPurpose::Resolution,
             PartialCreditClaimLossMethodPurpose::Censoring,
             PartialCreditClaimLossMethodPurpose::Denominator,
             PartialCreditClaimLossMethodPurpose::MonetaryBasis,
             PartialCreditClaimLossMethodPurpose::AmountBound,
             PartialCreditClaimLossMethodPurpose::Metric}) {
        if (!purposes.contains(purpose)) {
            invalid("methods.cfg is missing required purpose " +
                std::string(to_string(purpose)));
        }
    }
}

struct EvidenceIndex {
    std::unordered_map<std::string, const EvidenceRecord*> records{};
    std::unordered_map<std::string,
        const EvidenceRecordGateUseAssessment*> assessments{};
};

[[nodiscard]] EvidenceIndex make_evidence_index(
    const EvidenceDossier& dossier,
    const EvidenceGateUseBatchAssessment& assessment) {
    EvidenceIndex index;
    for (const EvidenceRecord& record : dossier.records) {
        index.records.emplace(record.record_id, &record);
    }
    for (const EvidenceRecordGateUseAssessment& record : assessment.records) {
        index.assessments.emplace(record.record_id, &record);
    }
    return index;
}

void assess_citations(
    const std::vector<std::string>& record_ids,
    const std::vector<std::string>& requirement_ids,
    const EvidenceIndex& index, std::string_view context,
    std::vector<std::string>& blockers) {
    std::unordered_set<std::string> cited_requirements(
        requirement_ids.begin(), requirement_ids.end());
    std::unordered_set<std::string> witnessed_requirements;
    for (const std::string& record_id : record_ids) {
        const auto record = index.records.find(record_id);
        const auto assessment = index.assessments.find(record_id);
        if (record == index.records.end() || assessment == index.assessments.end()) {
            invalid(std::string(context) + " cites unknown evidence record " + record_id);
        }
        if (!cited_requirements.contains(record->second->requirement_id)) {
            invalid(std::string(context) + " evidence/requirement IDs do not match");
        }
        witnessed_requirements.emplace(record->second->requirement_id);
        if (!assessment->second->record_qualifies ||
            !assessment->second->requirement_passed) {
            blockers.push_back(std::string(context) + ": evidence record " +
                record_id + " does not pass its compiled requirement");
        }
    }
    for (const std::string& requirement_id : requirement_ids) {
        if (!witnessed_requirements.contains(requirement_id)) {
            invalid(std::string(context) + " cites unwitnessed requirement " +
                requirement_id);
        }
    }
}

void assess_record_only_citations(
    const std::vector<std::string>& record_ids,
    const EvidenceIndex& index, std::string_view context,
    std::vector<std::string>& blockers) {
    for (const std::string& record_id : record_ids) {
        const auto record = index.records.find(record_id);
        const auto assessment = index.assessments.find(record_id);
        if (record == index.records.end() || assessment == index.assessments.end()) {
            invalid(std::string(context) + " cites unknown evidence record " + record_id);
        }
        if (!assessment->second->record_qualifies ||
            !assessment->second->requirement_passed) {
            blockers.push_back(std::string(context) + ": evidence record " +
                record_id + " does not pass its compiled requirement");
        }
    }
}

void bind_claim_packages(
    const std::filesystem::path& directory,
    std::vector<PartialCreditClaimLossObservation>& observations) {
    for (PartialCreditClaimLossObservation& observation : observations) {
        if (observation.disposition ==
                PartialCreditClaimLossDisposition::Excluded) {
            if (!observation.claim_cfg_path.empty() ||
                observation.expected_claim_config_sha256 != "NONE") {
                invalid("excluded observation must use NONE for claim binding fields");
            }
            continue;
        }
        const std::string relative = observation.claim_cfg_path.generic_string();
        if (!safe_relative_path(relative) ||
            observation.claim_cfg_path.filename() !=
                std::filesystem::path("claim.cfg") ||
            !lower_hex_sha256(observation.expected_claim_config_sha256)) {
            invalid("included observation has an unsafe claim.cfg binding");
        }
        const std::filesystem::path claim_path = resolve_confined_regular(
            directory, relative, "Claim Ledger claim.cfg");
        const std::string initial = read_bounded_file_bytes(
            claim_path, kMaximumConfigBytes, "Claim Ledger claim.cfg");
        if (sha256_bytes_lower_hex(initial) !=
                observation.expected_claim_config_sha256) {
            invalid("Claim Ledger claim.cfg SHA-256 mismatch for " +
                observation.observation_id);
        }
        ClaimLedgerPackage loaded;
        if (observation.disposition ==
                PartialCreditClaimLossDisposition::Resolved) {
            loaded = load_claim_ledger_package_with_full_path_evidence(
                claim_path, observation.realized_scenario_id).package;
        } else {
            loaded = load_claim_ledger_package(claim_path);
        }
        if (!loaded.package_integrity ||
            loaded.claim_config_filename != std::filesystem::path("claim.cfg") ||
            loaded.claim_config_sha256 != observation.expected_claim_config_sha256 ||
            loaded.directory != claim_path.parent_path()) {
            invalid("Claim Ledger package binding changed or failed integrity for " +
                observation.observation_id);
        }
        const std::string fresh = read_bounded_file_bytes(
            claim_path, kMaximumConfigBytes, "Claim Ledger claim.cfg");
        if (fresh != initial) {
            invalid("Claim Ledger claim.cfg changed during package loading for " +
                observation.observation_id);
        }
        observation.claim_package = std::move(loaded);
    }
}

} // namespace

PartialCreditClaimLossCohortConfig
load_partial_credit_claim_loss_cohort_config(
    const std::filesystem::path& config_path) {
    return parse_cohort_config(read_bounded_file_bytes(
        config_path, kMaximumConfigBytes, "cohort.cfg"));
}

namespace {

struct LoadedPartialCreditClaimLossCohortCandidate {
    PartialCreditClaimLossCohortPackage package{};
    bool population_frame_evidence_passed{};
    bool candidate_package_valid{};
};

[[nodiscard]] LoadedPartialCreditClaimLossCohortCandidate
load_partial_credit_claim_loss_cohort_package_impl(
    const std::filesystem::path& root,
    bool run_structural_postcondition) {
    const std::filesystem::path directory = canonical_directory(root);
    const std::filesystem::path config_path = resolve_confined_regular(
        directory, "cohort.cfg", "cohort.cfg");
    const std::string config_bytes = read_bounded_file_bytes(
        config_path, kMaximumConfigBytes, "cohort.cfg");
    PartialCreditClaimLossCohortPackage package;
    package.directory = directory;
    package.cohort_config_sha256 = sha256_bytes_lower_hex(config_bytes);
    package.config = parse_cohort_config(config_bytes);

    const BoundSnapshot observations = load_bound_snapshot(directory,
        package.config.observations_file, kMaximumObservationBytes,
        "observations.tsv");
    const BoundSnapshot methods = load_bound_snapshot(directory,
        package.config.methods_file, kMaximumMethodBytes, "methods.cfg");
    const BoundSnapshot dossier = load_bound_snapshot(directory,
        package.config.dossier_file, kMaximumDossierBytes, "dossier.cfg");
    const BoundSnapshot manifest = load_bound_snapshot(directory,
        package.config.evidence_manifest_file, kMaximumManifestBytes,
        "evidence_manifest.tsv");

    package.observations = parse_observations(observations.bytes);
    package.methods = parse_methods(methods.bytes, package.config.as_of_date);
    validate_method_bindings(package.config, package.methods);
    package.evidence_dossier = load_evidence_dossier_bytes(
        directory, dossier.bytes, manifest.bytes);
    if (package.evidence_dossier.metadata.subject_kind !=
            DossierSubjectKind::ClaimPopulation ||
        package.evidence_dossier.metadata.as_of_date != package.config.as_of_date ||
        package.evidence_dossier.metadata.population_program_or_book_id !=
            package.config.cohort_id ||
        package.evidence_dossier.metadata.population_reporting_currency !=
            package.config.currency_label) {
        invalid("claim-population dossier identity, date, book, or currency does not match cohort.cfg");
    }
    const std::vector<RetainedEvidenceSnapshot> retained_evidence =
        snapshot_retained_evidence_files(
            directory, package.evidence_dossier);

    bind_claim_packages(directory, package.observations);
    std::sort(package.observations.begin(), package.observations.end(),
        [](const auto& first, const auto& second) {
            return first.observation_id < second.observation_id;
        });

    // Reuse the sole Claim Ledger cash authority and all existing cohort
    // identity/date/conservation checks. The result is deliberately discarded;
    // successful evaluation is a structural loader postcondition, not an
    // empirical or Portfolio authorization.
    if (run_structural_postcondition) {
        static_cast<void>(
            evaluate_partial_credit_claim_loss_cohort(package));
    }

    // Evidence Gate authority is derived only after the potentially long
    // nested Claim Ledger reload/evaluation. Recheck every retained copy first,
    // then assess and recheck again so cached positive authority cannot survive
    // same-load retained-byte drift.
    recheck_retained_evidence_files(
        directory, package.evidence_dossier, retained_evidence);
    package.evidence_gate_assessment = assess_evidence_gate_use_batch(
        package.evidence_dossier, package.config.as_of_date);
    const bool population_frame_evidence_passed =
        claim_population_frame_passes(
            package.evidence_gate_assessment.dossier_assessment);
    if (!population_frame_evidence_passed) {
        package.admission_blockers.push_back(
            "the compiled FIN-CLAIM-POPULATION-FRAME conjunction did not pass");
    }

    const EvidenceIndex evidence = make_evidence_index(
        package.evidence_dossier, package.evidence_gate_assessment);
    for (const PartialCreditClaimLossExclusionRule& rule :
         package.config.exclusion_rules) {
        assess_record_only_citations(rule.evidence_record_ids, evidence,
            "exclusion rule " + rule.id, package.admission_blockers);
    }
    for (const PartialCreditClaimLossMethod& method : package.methods) {
        assess_citations(method.evidence_record_ids,
            method.evidence_requirement_ids, evidence,
            "method " + method.id, package.admission_blockers);
    }
    for (const PartialCreditClaimLossObservation& observation :
         package.observations) {
        assess_citations(observation.population_evidence_record_ids,
            observation.population_requirement_ids, evidence,
            "observation " + observation.observation_id + " population",
            package.admission_blockers);
        assess_citations(observation.classification_evidence_record_ids,
            observation.classification_requirement_ids, evidence,
            "observation " + observation.observation_id + " classification",
            package.admission_blockers);
    }
    const bool gate_bindings_passed = package.admission_blockers.empty();
    const bool candidate_package_valid =
        population_frame_evidence_passed && gate_bindings_passed;
    recheck_retained_evidence_files(
        directory, package.evidence_dossier, retained_evidence);

    recheck_snapshot(directory, package.config.observations_file,
        observations, kMaximumObservationBytes, "observations.tsv");
    recheck_snapshot(directory, package.config.methods_file,
        methods, kMaximumMethodBytes, "methods.cfg");
    recheck_snapshot(directory, package.config.dossier_file,
        dossier, kMaximumDossierBytes, "dossier.cfg");
    recheck_snapshot(directory, package.config.evidence_manifest_file,
        manifest, kMaximumManifestBytes, "evidence_manifest.tsv");
    const std::filesystem::path fresh_config_path = resolve_confined_regular(
        directory, "cohort.cfg", "cohort.cfg");
    const std::string fresh_config = read_bounded_file_bytes(
        fresh_config_path, kMaximumConfigBytes, "cohort.cfg");
    if (fresh_config_path != config_path || fresh_config != config_bytes) {
        invalid("cohort.cfg changed during package loading");
    }

    package.admission_blockers.push_back(
        "the compiled population-frame profile does not authenticate row classification, method validity, term comparability, exclusion timing, or realized-cash truth");
    package.admission_blockers.push_back(
        "empirical probabilities, calibration, pricing, ratings, expected returns, and Portfolio export remain prohibited");
    return {std::move(package), population_frame_evidence_passed,
        candidate_package_valid};
}

} // namespace

PartialCreditClaimLossCohortPackage
load_partial_credit_claim_loss_cohort_package(
    const std::filesystem::path& root) {
    LoadedPartialCreditClaimLossCohortCandidate loaded =
        load_partial_credit_claim_loss_cohort_package_impl(root, true);
    loaded.package.load_seal_ = std::shared_ptr<
        const PartialCreditClaimLossCohortPackage::LoadSeal>(
            new PartialCreditClaimLossCohortPackage::LoadSeal{
                loaded.package.directory,
                loaded.package.cohort_config_sha256,
                loaded.population_frame_evidence_passed,
                loaded.candidate_package_valid});
    return std::move(loaded.package);
}

PartialCreditClaimLossCohortPackage
detail::PartialCreditClaimLossCohortLoadAccess::reload_for_evaluation(
    const std::filesystem::path& canonical_directory,
    std::string_view expected_cohort_config_sha256) {
    LoadedPartialCreditClaimLossCohortCandidate loaded =
        load_partial_credit_claim_loss_cohort_package_impl(
            canonical_directory, false);
    if (loaded.package.directory != canonical_directory ||
        loaded.package.cohort_config_sha256 !=
            expected_cohort_config_sha256) {
        throw std::logic_error(
            "partial-credit cohort sealed binder changed after loading");
    }
    loaded.package.load_seal_ = std::shared_ptr<
        const PartialCreditClaimLossCohortPackage::LoadSeal>(
            new PartialCreditClaimLossCohortPackage::LoadSeal{
                loaded.package.directory,
                loaded.package.cohort_config_sha256,
                loaded.population_frame_evidence_passed,
                loaded.candidate_package_valid});
    return std::move(loaded.package);
}

} // namespace naturalehia::cellular_finance
