// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/evidence_gate.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>
#include <naturalehia/cellular_finance/portfolio.hpp>

#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kCalibrationBinderVersion = "0.1.0";

enum class CalibrationInputClass {
    Capital,
    Transition,
    Probability,
    Recovery,
    Dependence,
    QualifiedOutput,
    CommercialCash,
    SourceCredit,
    Cost,
    PolicyHurdle,
    InstrumentTerm,
};

enum class CalibrationInputStatus {
    Observed,
    Contractual,
    Derived,
    Estimated,
    Transfer,
    Hypothesis,
    Stress,
    Synthetic,
    Policy,
};

struct CalibrationBoundFile {
    std::filesystem::path relative_path;
    std::string sha256;
};

struct CalibrationBinderConfig {
    std::string version;
    std::string id;
    std::string project_id;
    std::string dossier_id;
    std::string as_of_date;
    std::string source_note;
    bool candidate_only{true};
    std::string probability_measure;
    CalibrationBoundFile portfolio;
    CalibrationBoundFile ambiguity;
    CalibrationBoundFile dossier;
    CalibrationBoundFile evidence_manifest;
    CalibrationBoundFile lineage;
};

struct CalibrationLineageRow {
    std::string input_id;
    std::string target_path;
    CalibrationInputClass input_class{CalibrationInputClass::Capital};
    CalibrationInputStatus input_status{CalibrationInputStatus::Synthetic};
    std::string method_id;
    std::vector<std::string> evidence_record_ids;
    std::vector<std::string> requirement_ids;
    std::string limitations;
    std::string update_or_retire;
};

struct CalibrationBinder {
    std::filesystem::path directory;
    CalibrationBinderConfig config;
    PortfolioConfig portfolio;
    PortfolioAmbiguityConfig ambiguity;
    EvidenceDossier dossier;
    EvidenceAssessment evidence_assessment;
    std::vector<std::string> material_target_paths;
    std::vector<CalibrationLineageRow> lineage;
    std::string evaluation_date;
    bool candidate_package_valid{};
    bool calibrated_execution_authorized{};
};

// Strictly parses the binder metadata. Paths must be confined relative paths,
// and every bound hash must be 64 lowercase hexadecimal characters.
[[nodiscard]] CalibrationBinderConfig load_calibration_binder_config(
    const std::filesystem::path& path);

void print_normalized_calibration_binder_config(
    std::ostream& output,
    const CalibrationBinderConfig& config);

// The target namespace is derived from normalized configs. Descriptive
// metadata and all *.count keys are intentionally excluded; all other keys are
// material and require exactly one lineage row.
[[nodiscard]] std::vector<std::string>
material_calibration_target_paths(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity);

// Loads and verifies every hash-bound file, validates exact lineage coverage,
// checks the dossier as at evaluation_date (or the binder as-of date when the
// argument is empty), and preserves the synthetic-only execution boundary.
[[nodiscard]] CalibrationBinder load_calibration_binder(
    const std::filesystem::path& binder_path,
    std::string_view evaluation_date = {});

void print_normalized_calibration_lineage(
    std::ostream& output,
    const std::vector<CalibrationLineageRow>& lineage);

void print_calibration_binder_report(
    std::ostream& output,
    const CalibrationBinder& binder);

[[nodiscard]] std::string_view to_string(
    CalibrationInputClass value) noexcept;
[[nodiscard]] std::string_view to_string(
    CalibrationInputStatus value) noexcept;

} // namespace naturalehia::cellular_finance
