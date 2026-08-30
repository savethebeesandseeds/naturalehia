// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/config.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class TemporaryConfig {
public:
    TemporaryConfig() {
        const auto suffix = std::chrono::steady_clock::now()
            .time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("naturalehia-cellular-finance-" + std::to_string(suffix) +
             ".cfg");
    }

    ~TemporaryConfig() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    TemporaryConfig(const TemporaryConfig&) = delete;
    TemporaryConfig& operator=(const TemporaryConfig&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

    void write(std::string_view content) const {
        std::ofstream output(path_, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("could not create temporary config");
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!output) {
            throw std::runtime_error("could not write temporary config");
        }
    }

private:
    std::filesystem::path path_;
};

[[nodiscard]] std::string normalized(const cf::SimulationConfig& config) {
    std::ostringstream output;
    cf::print_normalized_config(output, config);
    return output.str();
}

template <typename Function>
void expect_failure(Function&& function, std::string_view message) {
    try {
        function();
        check(false, message);
    } catch (const std::invalid_argument&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

[[nodiscard]] cf::SimulationConfig fixture() {
    cf::SimulationConfig config;
    config.scenario_label = "round-trip fixture";
    config.source_note = "fully synthetic parser test";
    config.trials = 17U;
    config.seed = 42U;
    config.risk.annual_contamination_probability = 0.0;
    config.instrument.offtake_fraction = 0.2;
    config.instrument.offtake_price_per_kg = 15.5;
    config.instrument.price_support_kind = cf::PriceSupportKind::OneWayFloor;
    config.instrument.price_support_fraction = 0.3;
    config.instrument.price_support_strike_per_kg = 12.5;
    config.instrument.price_support_annual_cap_million = 9.0;
    config.instrument.price_support_lifetime_cap_million = 27.0;
    config.instrument.completion_delay_trigger_years = 2.5;
    config.instrument.completion_payout_per_delay_year_million = 4.0;
    config.instrument.completion_delay_cover_cap_million = 12.0;
    config.instrument.upfront_fee_million = 1.25;
    return config;
}

void replace_once(
    std::string& text, std::string_view from, std::string_view to) {
    const std::size_t position = text.find(from);
    if (position == std::string::npos) {
        throw std::runtime_error("test fixture token was not found");
    }
    text.replace(position, from.size(), to);
}

void test_round_trip_and_bom() {
    const cf::SimulationConfig original = fixture();
    const std::string canonical = normalized(original);

    TemporaryConfig plain;
    plain.write(canonical);
    const cf::SimulationConfig parsed = cf::load_config(plain.path());
    check(normalized(parsed) == canonical,
        "normalized configuration round-trips without hidden defaults");

    TemporaryConfig with_bom;
    const std::string bom = std::string("\xEF\xBB\xBF") + canonical;
    with_bom.write(bom);
    check(normalized(cf::load_config(with_bom.path())) == canonical,
        "a UTF-8 byte-order mark is accepted only at the file start");
}

void test_unknown_duplicate_missing_and_malformed_keys() {
    const std::string canonical = normalized(fixture());

    TemporaryConfig unknown;
    unknown.write(canonical + "unknown.field=1\n");
    expect_failure(
        [&unknown] { static_cast<void>(cf::load_config(unknown.path())); },
        "unknown keys are rejected");

    TemporaryConfig duplicate;
    duplicate.write(canonical + "simulation.seed=99\n");
    expect_failure(
        [&duplicate] { static_cast<void>(cf::load_config(duplicate.path())); },
        "duplicate keys are rejected");

    TemporaryConfig missing;
    std::string missing_text = canonical;
    replace_once(missing_text, "instrument.upfront_fee_million=1.25\n", "");
    missing.write(missing_text);
    expect_failure(
        [&missing] { static_cast<void>(cf::load_config(missing.path())); },
        "missing keys are rejected instead of receiving silent defaults");

    TemporaryConfig malformed;
    std::string malformed_text = canonical;
    replace_once(malformed_text,
        "scenario.synthetic_inputs=true",
        "scenario.synthetic_inputs=yes");
    malformed.write(malformed_text);
    expect_failure(
        [&malformed] { static_cast<void>(cf::load_config(malformed.path())); },
        "booleans use an unambiguous spelling");
}

void test_governance_gate_is_enforced_during_load() {
    std::string text = normalized(fixture());
    replace_once(text,
        "scenario.synthetic_inputs=true",
        "scenario.synthetic_inputs=false");
    TemporaryConfig config;
    config.write(text);
    expect_failure(
        [&config] { static_cast<void>(cf::load_config(config.path())); },
        "the parser cannot bypass the synthetic-input governance gate");
}

void test_monetary_basis_is_explicit() {
    std::string text = normalized(fixture());
    replace_once(text,
        "scenario.monetary_basis=unspecified-synthetic",
        "scenario.monetary_basis=maybe");
    TemporaryConfig config;
    config.write(text);
    expect_failure(
        [&config] { static_cast<void>(cf::load_config(config.path())); },
        "the real/nominal convention cannot be an arbitrary label");
}

} // namespace

int main() {
    try {
        test_round_trip_and_bom();
        test_unknown_duplicate_missing_and_malformed_keys();
        test_governance_gate_is_enforced_during_load();
        test_monetary_basis_is_explicit();
    } catch (const std::exception& error) {
        std::cerr << "unexpected test setup failure: " << error.what() << '\n';
        return 1;
    }

    if (failures != 0) {
        std::cerr << failures << " config test(s) failed\n";
        return 1;
    }
    std::cout << "all config tests passed\n";
    return 0;
}
