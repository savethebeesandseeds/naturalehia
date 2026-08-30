// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/calibration_binder.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        path_ = std::filesystem::temp_directory_path() /
            ("fca-calibration-binder-config-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream output;
    output << input.rdbuf();
    if (!input || !output) {
        throw std::runtime_error("failed to read test fixture");
    }
    return output.str();
}

void write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary);
    output << text;
    if (!output) {
        throw std::runtime_error("failed to write test fixture");
    }
}

void replace_once(
    std::string& value,
    std::string_view from,
    std::string_view to) {
    const std::size_t position = value.find(from);
    if (position == std::string::npos) {
        throw std::runtime_error("test replacement source is absent");
    }
    value.replace(position, from.size(), to);
}

[[nodiscard]] bool parse_fails(
    const std::filesystem::path& path,
    std::string_view content) {
    write_text(path, content);
    try {
        static_cast<void>(cf::load_calibration_binder_config(path));
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

void run_tests(const std::filesystem::path& fixture_directory) {
    const std::filesystem::path source = fixture_directory / "binder.cfg";
    const cf::CalibrationBinderConfig config =
        cf::load_calibration_binder_config(source);
    check(config.version == cf::kCalibrationBinderVersion,
          "fixture version should parse");
    check(config.project_id == "synthetic-facility",
          "bound project ID should parse");
    check(config.dossier_id == "synthetic-binder-gap-dossier",
          "bound dossier ID should parse");
    check(config.candidate_only,
          "candidate-only boundary should parse as true");
    check(config.probability_measure == "physical-P",
          "probability measure should parse exactly");

    TemporaryDirectory temporary;
    const std::filesystem::path path = temporary.path() / "binder.cfg";
    std::ostringstream normalized;
    cf::print_normalized_calibration_binder_config(normalized, config);
    write_text(path, normalized.str());
    const cf::CalibrationBinderConfig round_trip =
        cf::load_calibration_binder_config(path);
    std::ostringstream normalized_again;
    cf::print_normalized_calibration_binder_config(
        normalized_again, round_trip);
    check(normalized.str() == normalized_again.str(),
          "normalized binder output should be deterministic and round-trip");

    const std::string base = read_text(source);
    std::string invalid = base + "binder.unknown=value\n";
    check(parse_fails(path, invalid), "unknown binder fields must fail");

    invalid = base;
    replace_once(invalid, "binder.candidate_only=true",
                 "binder.candidate_only=false");
    check(parse_fails(path, invalid), "candidate_only=false must fail");

    invalid = base;
    replace_once(invalid, "binder.probability_measure=physical-P",
                 "binder.probability_measure=risk-neutral-Q");
    check(parse_fails(path, invalid), "non-P probability measure must fail");

    invalid = base;
    replace_once(invalid, "file.portfolio.path=portfolio.cfg",
                 "file.portfolio.path=../portfolio.cfg");
    check(parse_fails(path, invalid), "parent-traversal paths must fail");

    invalid = base;
    replace_once(invalid, "file.portfolio.path=portfolio.cfg",
                 "file.portfolio.path=C:/portfolio.cfg");
    check(parse_fails(path, invalid), "absolute paths must fail");

    invalid = base;
    const std::size_t digest = invalid.find("file.portfolio.sha256=");
    if (digest == std::string::npos) {
        throw std::runtime_error("fixture portfolio hash is absent");
    }
    const std::size_t digest_value = digest +
        std::string_view("file.portfolio.sha256=").size();
    invalid[digest_value] = 'A';
    check(parse_fails(path, invalid),
          "uppercase SHA-256 characters must fail");

    invalid = base;
    replace_once(invalid, "binder.id=one-project-synthetic-candidate",
                 "binder.id=unsafe id");
    check(parse_fails(path, invalid), "unsafe binder identifiers must fail");

    invalid = base;
    replace_once(invalid, "binder.project_id=synthetic-facility\n", "");
    check(parse_fails(path, invalid), "missing project ID must fail");

    invalid = base;
    replace_once(
        invalid, "binder.dossier_id=synthetic-binder-gap-dossier",
        "binder.dossier_id=unsafe dossier id");
    check(parse_fails(path, invalid), "unsafe dossier IDs must fail");
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            throw std::invalid_argument("fixture directory argument required");
        }
        run_tests(std::filesystem::path(argv[1]));
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }
    if (failures != 0) {
        std::cerr << failures << " calibration binder config test(s) failed\n";
        return 1;
    }
    std::cout << "all calibration binder config tests passed\n";
    return 0;
}
