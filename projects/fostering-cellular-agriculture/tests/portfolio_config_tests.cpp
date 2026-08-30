// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio_config.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

class NonCanonicalPunctuation final : public std::numpunct<char> {
protected:
    [[nodiscard]] char do_decimal_point() const override { return ','; }
    [[nodiscard]] std::string do_truename() const override { return "yes"; }
    [[nodiscard]] std::string do_falsename() const override { return "no"; }
};

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expect_invalid_argument(
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

[[nodiscard]] cf::ScenarioCashSource make_source(std::string id,
    cf::PortfolioCashSource kind, double amount) {
    cf::ScenarioCashSource source;
    source.id = std::move(id);
    source.kind = kind;
    if (amount > 0.0) {
        source.cash_available.push_back(cf::MonthlyAmount{12U, amount});
    }
    return source;
}

[[nodiscard]] cf::ProjectJointPath make_path(std::string project_id,
    cf::ProjectPathResolution resolution, double draw, double receipt,
    double principal, std::string source_id = "commercial-budget") {
    cf::ProjectJointPath path;
    path.project_id = std::move(project_id);
    path.resolution = resolution;
    if (draw > 0.0) {
        path.capital_draws.push_back(cf::MonthlyAmount{0U, draw});
    }
    if (receipt > 0.0) {
        path.investor_receipts.push_back(cf::InvestorReceipt{
            12U, std::move(source_id), receipt, principal});
    }
    return path;
}

[[nodiscard]] cf::PortfolioConfig full_config() {
    cf::PortfolioConfig config;
    config.scenario_label = "strict reloadable portfolio fixture";
    config.source_note = "synthetic parser roundtrip values only";
    config.currency_label = "TEST";
    config.monetary_basis = "constant test units at close";
    config.horizon_months = 12U;
    config.annual_physical_hurdle_rate = 1.0e-20;
    config.projects = {
        cf::PortfolioProject{"research", cf::ProjectStage::Research, 10.0},
        cf::PortfolioProject{"pilot", cf::ProjectStage::Pilot, 10.0},
        cf::PortfolioProject{
            "demonstration", cf::ProjectStage::Demonstration, 10.0},
        cf::PortfolioProject{
            "first-industrial", cf::ProjectStage::FirstIndustrial, 10.0},
        cf::PortfolioProject{
            "repeat-production", cf::ProjectStage::RepeatProduction, 10.0},
    };

    cf::JointScenario first;
    first.id = "mixed-performance";
    first.weight = 0.60;
    first.factor_tags = {"biology-stable", "demand-normal"};
    first.pool_costs = {
        cf::MonthlyAmount{0U, 0.125},
        cf::MonthlyAmount{1U, 1.0e-20},
    };
    first.cash_sources = {
        make_source("commercial-budget",
            cf::PortfolioCashSource::Commercial, 10.0),
        make_source("licensing-budget",
            cf::PortfolioCashSource::LicensingRoyalty, 9.0),
        make_source(
            "exit-budget", cf::PortfolioCashSource::ExitSale, 1.0),
        make_source(
            "recovery-budget", cf::PortfolioCashSource::Recovery, 2.0),
        make_source("refinancing-budget",
            cf::PortfolioCashSource::Refinancing, 5.0),
        make_source("support-budget",
            cf::PortfolioCashSource::ExplicitSupport, 0.0),
        make_source("fee-budget", cf::PortfolioCashSource::SponsorFee, 0.0),
        make_source("financing-fee-budget",
            cf::PortfolioCashSource::FinancingFee, 0.0),
    };
    // Deliberately not in configured-project order. The normalized printer
    // must index paths by their configured project identity.
    first.project_paths = {
        make_path("repeat-production", cf::ProjectPathResolution::Continuing,
            5.0, 5.0, 5.0, "refinancing-budget"),
        make_path("first-industrial", cf::ProjectPathResolution::Resolved,
            6.0, 2.0, 2.0, "recovery-budget"),
        make_path("demonstration", cf::ProjectPathResolution::Continuing,
            7.0, 1.0, 1.0, "exit-budget"),
        make_path("pilot", cf::ProjectPathResolution::Resolved,
            8.0, 9.0, 8.0, "licensing-budget"),
        make_path("research", cf::ProjectPathResolution::Resolved,
            10.0, 10.0, 10.0, "commercial-budget"),
    };

    cf::JointScenario second;
    second.id = "resolved-loss";
    second.weight = 0.40;
    second.factor_tags = {"shared-scale-shock"};
    for (const cf::PortfolioProject& project : config.projects) {
        second.project_paths.push_back(make_path(project.id,
            cf::ProjectPathResolution::Resolved, 1.0, 0.0, 0.0));
    }
    config.joint_scenarios = {first, second};
    config.loss_layers = {
        cf::LossLayer{"lower-loss", 0.0, 20.0},
        cf::LossLayer{"upper-loss", 20.0, 50.0},
    };
    return config;
}

[[nodiscard]] cf::PortfolioConfig explicit_principal_config() {
    cf::PortfolioConfig config;
    config.scenario_label = "explicit contractual principal roundtrip";
    config.source_note = "synthetic v0.2 parser fixture only";
    config.currency_label = "TEST";
    config.monetary_basis = "constant test units at close";
    config.horizon_months = 12U;
    config.annual_physical_hurdle_rate = 0.08;

    cf::PortfolioProject project;
    project.id = "claim-project";
    project.stage = cf::ProjectStage::FirstIndustrial;
    project.commitment_million = 10.0;
    project.principal_accounting_mode =
        cf::PrincipalAccountingMode::ExplicitContractualLedger;
    project.principal_limit_million = 12.0;
    project.opening_principal_million = 2.0;
    config.projects.push_back(project);

    cf::JointScenario scenario;
    scenario.id = "resolved-claim";
    scenario.weight = 1.0;
    scenario.cash_sources.push_back(make_source(
        "claim-receipt-budget", cf::PortfolioCashSource::Recovery, 6.0));

    cf::ProjectJointPath path;
    path.project_id = project.id;
    path.resolution = cf::ProjectPathResolution::Resolved;
    path.investor_outlays = {
        cf::InvestorOutlay{0U,
            cf::InvestorOutlayPurpose::PrimaryProjectFunding, 1.0},
        cf::InvestorOutlay{0U,
            cf::InvestorOutlayPurpose::ClaimPurchasePrice, 7.0},
        cf::InvestorOutlay{
            0U, cf::InvestorOutlayPurpose::BuyerDirectCost, 0.5},
    };
    path.investor_receipts.push_back(cf::InvestorReceipt{
        12U, "claim-receipt-budget", 6.0, 5.0});
    path.principal_movements = {
        cf::PrincipalMovement{0U,
            cf::PrincipalMovementKind::FundedPrincipalAddition, 8.0},
        cf::PrincipalMovement{0U,
            cf::PrincipalMovementKind::CapitalizedFeeAddition, 1.0},
        cf::PrincipalMovement{6U,
            cf::PrincipalMovementKind::CapitalizedInterestAddition, 1.0},
        cf::PrincipalMovement{12U,
            cf::PrincipalMovementKind::ConversionExtinguishment, 2.0},
        cf::PrincipalMovement{
            12U, cf::PrincipalMovementKind::Writeoff, 5.0},
    };
    scenario.project_paths.push_back(std::move(path));
    config.joint_scenarios.push_back(std::move(scenario));
    return config;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create portfolio parser fixture");
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        throw std::runtime_error("could not write portfolio parser fixture");
    }
}

[[nodiscard]] std::string normalized_config(
    const cf::PortfolioConfig& config) {
    std::ostringstream output;
    cf::print_normalized_portfolio_config(output, config);
    return output.str();
}

void set_value(
    std::string& text, const std::string& key, const std::string& value) {
    const std::string prefix = key + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos) {
        throw std::runtime_error("test fixture key not found: " + key);
    }
    const std::size_t value_begin = position + prefix.size();
    const std::size_t line_end = text.find('\n', value_begin);
    text.replace(value_begin,
        line_end == std::string::npos ? std::string::npos
                                     : line_end - value_begin,
        value);
}

void remove_key(std::string& text, const std::string& key) {
    const std::string prefix = key + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos) {
        throw std::runtime_error("test fixture key not found: " + key);
    }
    const std::size_t line_end = text.find('\n', position);
    text.erase(position, line_end == std::string::npos
            ? std::string::npos
            : line_end - position + 1U);
}

void test_full_roundtrip_and_output_state(
    const std::filesystem::path& path) {
    const cf::PortfolioConfig original = full_config();
    std::ostringstream output;
    const std::locale caller_locale(
        std::locale::classic(), new NonCanonicalPunctuation);
    output.imbue(caller_locale);
    output << std::fixed << std::hex << std::showbase << std::showpoint
           << std::showpos << std::uppercase << std::setprecision(6);
    output.fill('#');
    output.width(37);
    const std::ios_base::fmtflags caller_flags = output.flags();
    cf::print_normalized_portfolio_config(output, original);
    const std::string normalized = output.str();

    check(output.precision() == 6 && output.flags() == caller_flags &&
            output.width() == 37 && output.fill() == '#' &&
            output.getloc() == caller_locale,
        "normalized printer restores caller flags, precision, width, fill, and locale");
    check(normalized.find("project.1.stage=research\n") !=
                std::string::npos &&
            normalized.find("project.2.stage=pilot\n") !=
                std::string::npos &&
            normalized.find("project.3.stage=demonstration\n") !=
                std::string::npos &&
            normalized.find("project.4.stage=first-industrial\n") !=
                std::string::npos &&
            normalized.find("project.5.stage=repeat-production\n") !=
                std::string::npos,
        "printer uses every exact project-stage spelling");
    check(normalized.find("cash_source.1.kind=commercial\n") !=
                std::string::npos &&
            normalized.find("cash_source.2.kind=licensing-royalty\n") !=
                std::string::npos &&
            normalized.find("cash_source.3.kind=exit-sale\n") !=
                std::string::npos &&
            normalized.find("cash_source.4.kind=recovery\n") !=
                std::string::npos &&
            normalized.find("cash_source.5.kind=refinancing\n") !=
                std::string::npos &&
            normalized.find("cash_source.6.kind=explicit-support\n") !=
                std::string::npos &&
            normalized.find("cash_source.7.kind=sponsor-fee\n") !=
                std::string::npos &&
            normalized.find("cash_source.8.kind=financing-fee\n") !=
                std::string::npos,
        "printer uses every exact cash-source spelling");
    check(normalized.find(
              "scenario.1.project.1.project_id=research\n") !=
            std::string::npos,
        "printer canonicalizes scenario paths to configured project indexes");

    write_text(path, normalized);
    const cf::PortfolioConfig loaded = cf::load_portfolio_config(path);
    check(loaded.projects.size() == original.projects.size() &&
            loaded.joint_scenarios.size() ==
                original.joint_scenarios.size() &&
            loaded.loss_layers.size() == original.loss_layers.size() &&
            loaded.annual_physical_hurdle_rate ==
                original.annual_physical_hurdle_rate &&
            loaded.joint_scenarios[0].pool_costs[1].amount_million ==
                original.joint_scenarios[0].pool_costs[1].amount_million,
        "full schema and max_digits10 tiny doubles round-trip exactly");
    check(loaded.joint_scenarios[0].project_paths[2].resolution ==
                cf::ProjectPathResolution::Continuing &&
            loaded.joint_scenarios[0].cash_sources[6].kind ==
                cf::PortfolioCashSource::SponsorFee,
        "resolution and source enums round-trip through strict spellings");
    check(normalized_config(loaded) == normalized,
        "load-print normalization is idempotent");
}

void test_explicit_principal_roundtrip() {
    const cf::PortfolioConfig original = explicit_principal_config();
    const std::string normalized = normalized_config(original);
    check(normalized.find(
              "project.1.principal_accounting_mode=explicit-contractual-ledger\n") !=
                std::string::npos &&
            normalized.find("project.1.principal_limit_million=12\n") !=
                std::string::npos &&
            normalized.find("project.1.opening_principal_million=2\n") !=
                std::string::npos,
        "v0.2 printer emits every explicit project-principal field");
    check(normalized.find(
              "scenario.1.project.1.investor_outlay.count=3\n") !=
                std::string::npos &&
            normalized.find(
                "scenario.1.project.1.investor_outlay.1.purpose=primary-project-funding\n") !=
                std::string::npos &&
            normalized.find(
                "scenario.1.project.1.investor_outlay.2.purpose=claim-purchase-price\n") !=
                std::string::npos &&
            normalized.find(
                "scenario.1.project.1.investor_outlay.3.purpose=buyer-direct-cost\n") !=
                std::string::npos,
        "v0.2 printer emits every investor-outlay purpose");
    check(normalized.find(
              "scenario.1.project.1.principal_movement.count=5\n") !=
                std::string::npos &&
            normalized.find("kind=funded-principal-addition\n") !=
                std::string::npos &&
            normalized.find("kind=capitalized-fee-addition\n") !=
                std::string::npos &&
            normalized.find("kind=capitalized-interest-addition\n") !=
                std::string::npos &&
            normalized.find("kind=conversion-extinguishment\n") !=
                std::string::npos &&
            normalized.find("kind=writeoff\n") != std::string::npos,
        "v0.2 printer emits every principal-movement kind");

    const cf::PortfolioConfig loaded =
        cf::load_portfolio_config_bytes(normalized);
    const cf::PortfolioProject& loaded_project = loaded.projects.at(0U);
    const cf::ProjectJointPath& loaded_path =
        loaded.joint_scenarios.at(0U).project_paths.at(0U);
    check(loaded_project.principal_accounting_mode ==
                cf::PrincipalAccountingMode::ExplicitContractualLedger &&
            loaded_project.principal_limit_million == 12.0 &&
            loaded_project.opening_principal_million == 2.0 &&
            loaded_path.capital_draws.empty() &&
            loaded_path.investor_outlays.size() == 3U &&
            loaded_path.principal_movements.size() == 5U,
        "v0.2 explicit principal structures round-trip exactly");
    check(normalized_config(loaded) == normalized,
        "v0.2 explicit load-print normalization is idempotent");
}

void test_v01_schema_compatibility() {
    cf::PortfolioConfig legacy = full_config();
    legacy.model_version = std::string(cf::kPortfolioLegacyModelVersion);
    const std::string normalized = normalized_config(legacy);
    check(normalized.find("portfolio.model_version=0.1.0\n") !=
                std::string::npos &&
            normalized.find("principal_accounting_mode") ==
                std::string::npos &&
            normalized.find("principal_limit_million") ==
                std::string::npos &&
            normalized.find("opening_principal_million") ==
                std::string::npos &&
            normalized.find("investor_outlay") == std::string::npos &&
            normalized.find("principal_movement") == std::string::npos,
        "v0.1 printer preserves the exact legacy key set");

    const cf::PortfolioConfig loaded =
        cf::load_portfolio_config_bytes(normalized);
    check(loaded.model_version == cf::kPortfolioLegacyModelVersion &&
            std::all_of(loaded.projects.begin(), loaded.projects.end(),
                [](const cf::PortfolioProject& project) {
                    return project.principal_accounting_mode ==
                            cf::PrincipalAccountingMode::DrawEqualsPrincipalLegacy &&
                        project.principal_limit_million == 0.0 &&
                        project.opening_principal_million == 0.0;
                }),
        "v0.1 parser initializes only the legacy principal model");
    check(normalized_config(loaded) == normalized,
        "v0.1 load-print normalization remains idempotent");

    const std::string v01_with_v02_key =
        normalized + "project.1.principal_limit_million=0\n";
    expect_invalid_argument(
        [&v01_with_v02_key] {
            (void)cf::load_portfolio_config_bytes(v01_with_v02_key);
        },
        "v0.1 parser rejects v0.2-only keys");

    cf::PortfolioConfig explicit_v01 = explicit_principal_config();
    explicit_v01.model_version =
        std::string(cf::kPortfolioLegacyModelVersion);
    expect_invalid_argument(
        [&explicit_v01] { (void)normalized_config(explicit_v01); },
        "v0.1 validation rejects explicit contractual principal mode");
}

void test_v02_enum_and_value_strictness() {
    const std::string normalized =
        normalized_config(explicit_principal_config());

    std::string malformed = normalized;
    set_value(malformed, "project.1.principal_accounting_mode",
        "principal-is-cash");
    expect_invalid_argument(
        [&malformed] { (void)cf::load_portfolio_config_bytes(malformed); },
        "unknown principal-accounting modes are rejected");

    malformed = normalized;
    set_value(malformed,
        "scenario.1.project.1.investor_outlay.1.purpose", "purchase");
    expect_invalid_argument(
        [&malformed] { (void)cf::load_portfolio_config_bytes(malformed); },
        "unknown investor-outlay purposes are rejected");

    malformed = normalized;
    set_value(malformed,
        "scenario.1.project.1.principal_movement.1.kind", "addition");
    expect_invalid_argument(
        [&malformed] { (void)cf::load_portfolio_config_bytes(malformed); },
        "unknown principal-movement kinds are rejected");

    malformed = normalized;
    set_value(malformed,
        "scenario.1.project.1.investor_outlay.count", "-1");
    expect_invalid_argument(
        [&malformed] { (void)cf::load_portfolio_config_bytes(malformed); },
        "malformed investor-outlay counts are rejected");

    malformed = normalized;
    set_value(malformed,
        "scenario.1.project.1.principal_movement.1.amount_million", "1x");
    expect_invalid_argument(
        [&malformed] { (void)cf::load_portfolio_config_bytes(malformed); },
        "malformed principal-movement amounts are rejected");
}

void test_bom_rules(const std::filesystem::path& path,
    const std::string& normalized) {
    const std::string bom{"\xEF\xBB\xBF", 3U};
    write_text(path, bom + normalized);
    const cf::PortfolioConfig loaded = cf::load_portfolio_config(path);
    check(loaded.scenario_label == full_config().scenario_label,
        "UTF-8 BOM is accepted at byte zero");

    std::string misplaced = normalized;
    const std::size_t second_line = misplaced.find('\n');
    misplaced.insert(second_line + 1U, bom);
    write_text(path, misplaced);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "UTF-8 BOM is rejected anywhere except byte zero");
}

void test_strict_key_set(const std::filesystem::path& path,
    const std::string& normalized) {
    write_text(path, normalized + "unknown.field=1\n");
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "unknown keys are rejected");

    write_text(path, normalized + "project.count=5\n");
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "duplicate keys are rejected at read time");

    std::string missing = normalized;
    remove_key(missing, "scenario.1.project.3.resolution");
    write_text(path, missing);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "missing nested keys are rejected");

    write_text(path, "portfolio.model_version 0.1.0\n");
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "non-key-value lines are rejected");
}

void test_malformed_values(const std::filesystem::path& path,
    const std::string& normalized) {
    std::string malformed = normalized;
    set_value(malformed, "portfolio.annual_physical_hurdle_rate", "1.2x");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "malformed decimal numbers are rejected");

    malformed = normalized;
    set_value(malformed, "scenario.1.pool_cost.1.month", "-1");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "malformed unsigned integers are rejected");

    malformed = normalized;
    set_value(malformed, "project.1.stage", "laboratory");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "unknown project-stage spellings are rejected");

    malformed = normalized;
    set_value(malformed, "scenario.1.project.1.resolution", "performing");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "unknown path-resolution spellings are rejected");

    malformed = normalized;
    set_value(malformed, "scenario.1.cash_source.1.kind", "guarantee");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "unknown cash-source spellings are rejected");
}

void test_count_and_resource_guards(const std::filesystem::path& path,
    const std::string& normalized) {
    std::string oversized = normalized;
    set_value(oversized, "project.count", "129");
    write_text(path, oversized);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "project count cap is enforced before schema expansion");

    oversized = normalized;
    set_value(oversized, "scenario.1.cash_source.count", "257");
    write_text(path, oversized);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "nested cash-source count cap is enforced before allocation");

    std::string long_line = normalized;
    set_value(long_line, "portfolio.source_note", std::string(4'097U, 'x'));
    write_text(path, long_line);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "4096-byte line guard is enforced while reading");

    write_text(path, "x");
    std::filesystem::resize_file(path, 16U * 1024U * 1024U + 1U);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "16 MiB file guard is enforced before parsing");
}

void test_semantic_validation_propagates(
    const std::filesystem::path& path, const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid, "scenario.1.weight", "0.5");
    write_text(path, invalid);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "loader propagates portfolio scenario-weight validation");

    invalid = normalized;
    set_value(invalid,
        "scenario.1.project.1.draw.1.amount_million", "11");
    write_text(path, invalid);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_config(path); },
        "loader propagates project commitment validation");

    cf::PortfolioConfig invalid_config = full_config();
    invalid_config.synthetic_inputs = false;
    std::ostringstream output;
    expect_invalid_argument(
        [&output, &invalid_config] {
            cf::print_normalized_portfolio_config(output, invalid_config);
        },
        "printer validates before emitting an invalid portfolio");
    check(output.str().empty(),
        "printer emits no partial output when semantic validation fails");
}

} // namespace

int main() {
    const std::filesystem::path path =
        std::filesystem::current_path() / "portfolio-config-parser-test.cfg";
    try {
        const std::string normalized = normalized_config(full_config());
        test_full_roundtrip_and_output_state(path);
        test_explicit_principal_roundtrip();
        test_v01_schema_compatibility();
        test_v02_enum_and_value_strictness();
        test_bom_rules(path, normalized);
        test_strict_key_set(path, normalized);
        test_malformed_values(path, normalized);
        test_count_and_resource_guards(path, normalized);
        test_semantic_validation_propagates(path, normalized);
        (void)std::filesystem::remove(path);
    } catch (const std::exception& error) {
        (void)std::filesystem::remove(path);
        std::cerr << "FAIL: unexpected test exception: " << error.what()
                  << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << failures << " portfolio-config test(s) failed\n";
        return 1;
    }
    std::cout << "all portfolio-config tests passed\n";
    return 0;
}
