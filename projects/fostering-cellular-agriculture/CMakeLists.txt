# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.24)

project(
    NaturalehiaCellularFinance
    VERSION 0.1.0
    DESCRIPTION "Transparent risk models for financing cellular-agriculture facilities"
    LANGUAGES CXX
)

set(cellular_finance_program_emulator)
if(EMSCRIPTEN)
    if(NOT CMAKE_CROSSCOMPILING_EMULATOR)
        find_program(
            NATURALEHIA_CELLULAR_FINANCE_NODE_EXECUTABLE
            NAMES node nodejs
            REQUIRED
        )
        set(
            CMAKE_CROSSCOMPILING_EMULATOR
            "${NATURALEHIA_CELLULAR_FINANCE_NODE_EXECUTABLE}"
        )
    endif()
    set(
        cellular_finance_program_emulator
        "${CMAKE_CROSSCOMPILING_EMULATOR}"
    )
endif()

option(
    NATURALEHIA_CELLULAR_FINANCE_BUILD_CLI
    "Build the cellular-agriculture risk-model command line application"
    ON
)
option(
    NATURALEHIA_CELLULAR_FINANCE_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors"
    OFF
)

function(cellular_finance_set_compile_policy target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
        if(NATURALEHIA_CELLULAR_FINANCE_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(
            ${target_name}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
                -Werror=return-type
        )
        if(NATURALEHIA_CELLULAR_FINANCE_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(
    naturalehia_cellular_finance
    STATIC
        src/capital_stack.cpp
        src/capital_stack_probability_polytope.cpp
        src/capital_stack_config.cpp
        src/calibration_binder.cpp
        src/claim_ledger.cpp
        src/claim_ledger_joint_portfolio.cpp
        src/claim_ledger_package.cpp
        src/claim_ledger_portfolio_adapter.cpp
        src/config.cpp
        src/evidence_gate.cpp
        src/joint_cohort.cpp
        src/joint_cohort_capital_stack.cpp
        src/joint_cohort_config.cpp
        src/model.cpp
        src/portfolio.cpp
        src/portfolio_adapters.cpp
        src/portfolio_ambiguity.cpp
        src/portfolio_ambiguity_config.cpp
        src/portfolio_config.cpp
        src/partial_credit_claim_loss_cohort.cpp
        src/partial_credit_claim_loss_cohort_config.cpp
        src/probability_polytope.cpp
        src/probability_polytope_config.cpp
        src/probability_polytope_tail_attribution.cpp
        src/pooled_loss_protection.cpp
        src/pooled_loss_protection_config.cpp
        src/provider_credit_stress.cpp
        src/provider_credit_stress_config.cpp
        src/provider_price_ladder.cpp
        src/provider_price_ladder_config.cpp
        src/robust_capital_mobilization_frontier.cpp
        src/robust_capital_mobilization_frontier_config.cpp
        src/robust_market_priority_cap.cpp
        src/robust_market_priority_cap_config.cpp
        src/robust_issue_price_support.cpp
        src/robust_issue_price_support_config.cpp
        src/robust_hurdle_envelope.cpp
        src/robust_hurdle_envelope_config.cpp
        src/staged_capital.cpp
        src/staged_capital_config.cpp
        src/success_participation.cpp
        src/success_participation_config.cpp
)
add_library(
    Naturalehia::CellularFinance
    ALIAS naturalehia_cellular_finance
)

target_compile_features(naturalehia_cellular_finance PUBLIC cxx_std_20)
target_include_directories(
    naturalehia_cellular_finance
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
set_target_properties(
    naturalehia_cellular_finance
    PROPERTIES
        CXX_EXTENSIONS OFF
        EXPORT_NAME CellularFinance
        VERSION ${PROJECT_VERSION}
)
cellular_finance_set_compile_policy(naturalehia_cellular_finance)

if(NATURALEHIA_CELLULAR_FINANCE_BUILD_CLI)
    add_executable(
        naturalehia_cellular_finance_cli
        apps/risk_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_cellular_finance_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_cellular_finance_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_cellular_finance_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-cellular-finance"
    )
    cellular_finance_set_compile_policy(
        naturalehia_cellular_finance_cli
    )

    add_executable(
        naturalehia_evidence_gate_cli
        apps/evidence_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_evidence_gate_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_evidence_gate_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_evidence_gate_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-evidence-gate"
    )
    cellular_finance_set_compile_policy(
        naturalehia_evidence_gate_cli
    )

    add_executable(
        naturalehia_staged_capital_cli
        apps/staged_capital_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_staged_capital_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_staged_capital_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_staged_capital_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-staged-capital"
    )
    cellular_finance_set_compile_policy(
        naturalehia_staged_capital_cli
    )

    add_executable(
        naturalehia_portfolio_cli
        apps/portfolio_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_portfolio_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_portfolio_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_portfolio_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-participation-pool"
    )
    cellular_finance_set_compile_policy(
        naturalehia_portfolio_cli
    )

    add_executable(
        naturalehia_portfolio_ambiguity_cli
        apps/ambiguity_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_portfolio_ambiguity_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_portfolio_ambiguity_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_portfolio_ambiguity_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-probability-envelope"
    )
    cellular_finance_set_compile_policy(
        naturalehia_portfolio_ambiguity_cli
    )

    add_executable(
        naturalehia_success_participation_cli
        apps/success_participation_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_success_participation_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_success_participation_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_success_participation_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-success-participation"
    )
    cellular_finance_set_compile_policy(
        naturalehia_success_participation_cli
    )

    add_executable(
        naturalehia_pooled_loss_protection_cli
        apps/pooled_loss_protection_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_pooled_loss_protection_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_pooled_loss_protection_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_pooled_loss_protection_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-pooled-loss-protection"
    )
    cellular_finance_set_compile_policy(
        naturalehia_pooled_loss_protection_cli
    )

    add_executable(
        naturalehia_provider_price_ladder_cli
        apps/provider_price_ladder_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_provider_price_ladder_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_provider_price_ladder_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_provider_price_ladder_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-provider-price-ladder"
    )
    cellular_finance_set_compile_policy(
        naturalehia_provider_price_ladder_cli
    )

    add_executable(
        naturalehia_provider_credit_stress_cli
        apps/provider_credit_stress_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_provider_credit_stress_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_provider_credit_stress_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_provider_credit_stress_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-provider-credit-stress"
    )
    cellular_finance_set_compile_policy(
        naturalehia_provider_credit_stress_cli
    )

    add_executable(
        naturalehia_capital_stack_cli
        apps/capital_stack_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_capital_stack_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_capital_stack_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_capital_stack_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-capital-stack"
    )
    cellular_finance_set_compile_policy(
        naturalehia_capital_stack_cli
    )

    add_executable(
        cf_frontier_cli
        apps/capital_mobilization_frontier_cli/main.cpp
    )
    target_link_libraries(
        cf_frontier_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_frontier_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_frontier_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-capital-mobilization-frontier"
    )
    cellular_finance_set_compile_policy(
        cf_frontier_cli
    )

    add_executable(
        cf_priority_cap_cli
        apps/market_priority_cap_cli/main.cpp
    )
    target_link_libraries(
        cf_priority_cap_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_priority_cap_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_priority_cap_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-market-priority-cap"
    )
    cellular_finance_set_compile_policy(
        cf_priority_cap_cli
    )

    add_executable(
        cf_issue_price_support_cli
        apps/issue_price_support_cli/main.cpp
    )
    target_link_libraries(
        cf_issue_price_support_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_issue_price_support_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_issue_price_support_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-issue-price-support"
    )
    cellular_finance_set_compile_policy(
        cf_issue_price_support_cli
    )

    if(EMSCRIPTEN)
        set(
            cellular_finance_browser_input_dir
            "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/ten-claim-instrument-v1-synthetic"
        )
        set(
            cellular_finance_browser_inputs
            "${cellular_finance_browser_input_dir}/portfolio.cfg"
            "${cellular_finance_browser_input_dir}/event-polytope-v0.2.cfg"
            "${cellular_finance_browser_input_dir}/success-participation.cfg"
            "${cellular_finance_browser_input_dir}/capital-stack-v0.2.cfg"
            "${cellular_finance_browser_input_dir}/market-priority-cap-v0.2.cfg"
            "${cellular_finance_browser_input_dir}/issue-price-support-v0.2.cfg"
        )
        set(
            cellular_finance_browser_output_dir
            "${CMAKE_CURRENT_BINARY_DIR}/browser-assets"
        )

        add_executable(
            cf_issue_price_support_browser
            apps/issue_price_support_cli/main.cpp
        )
        target_link_libraries(
            cf_issue_price_support_browser
            PRIVATE Naturalehia::CellularFinance
        )
        target_compile_features(
            cf_issue_price_support_browser
            PRIVATE cxx_std_20
        )
        set_target_properties(
            cf_issue_price_support_browser
            PROPERTIES
                CXX_EXTENSIONS OFF
                OUTPUT_NAME "naturalehia-issue-price-support-browser"
                RUNTIME_OUTPUT_DIRECTORY "${cellular_finance_browser_output_dir}"
                SUFFIX ".mjs"
        )
        cellular_finance_set_compile_policy(
            cf_issue_price_support_browser
        )
        target_link_options(
            cf_issue_price_support_browser
            PRIVATE
                # The Release/Node verification build may set NODERAWFS
                # globally. This browser target needs Emscripten's virtual
                # filesystem for its embedded inputs, so override that
                # setting locally without changing any Node CLI target.
                "SHELL:-sNODERAWFS=0"
                "SHELL:-sMODULARIZE=1"
                "SHELL:-sEXPORT_ES6=1"
                "SHELL:-sEXPORT_NAME=createNaturalehiaIssuePriceSupportModule"
                "SHELL:-sENVIRONMENT=web"
                "SHELL:-sEXIT_RUNTIME=1"
                "SHELL:-sALLOW_MEMORY_GROWTH=1"
                "SHELL:-sSTACK_SIZE=1048576"
                "SHELL:-sFILESYSTEM=1"
                "SHELL:--embed-file \"${cellular_finance_browser_input_dir}/portfolio.cfg@/financeability-inputs/portfolio.cfg\""
                "SHELL:--embed-file \"${cellular_finance_browser_input_dir}/event-polytope-v0.2.cfg@/financeability-inputs/event-polytope-v0.2.cfg\""
                "SHELL:--embed-file \"${cellular_finance_browser_input_dir}/success-participation.cfg@/financeability-inputs/success-participation.cfg\""
                "SHELL:--embed-file \"${cellular_finance_browser_input_dir}/capital-stack-v0.2.cfg@/financeability-inputs/capital-stack-v0.2.cfg\""
                "SHELL:--embed-file \"${cellular_finance_browser_input_dir}/market-priority-cap-v0.2.cfg@/financeability-inputs/market-priority-cap-v0.2.cfg\""
                "SHELL:--embed-file \"${cellular_finance_browser_input_dir}/issue-price-support-v0.2.cfg@/financeability-inputs/issue-price-support-v0.2.cfg\""
        )
        set_property(
            TARGET cf_issue_price_support_browser
            APPEND PROPERTY LINK_DEPENDS ${cellular_finance_browser_inputs}
        )

        add_custom_target(
            cf_issue_price_support_browser_assets
            COMMAND
                ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/wasm"
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:cf_issue_price_support_browser>"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/wasm/naturalehia-issue-price-support-browser.mjs"
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE_DIR:cf_issue_price_support_browser>/naturalehia-issue-price-support-browser.wasm"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/wasm/naturalehia-issue-price-support-browser.wasm"
            DEPENDS cf_issue_price_support_browser
            COMMENT "Publishing the browser financeability module to the static site"
            VERBATIM
        )
    endif()

    add_executable(
        cf_hurdle_evidence_cli
        apps/hurdle_evidence_cli/main.cpp
    )
    target_link_libraries(
        cf_hurdle_evidence_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_hurdle_evidence_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_hurdle_evidence_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-hurdle-evidence"
    )
    cellular_finance_set_compile_policy(
        cf_hurdle_evidence_cli
    )

    add_executable(
        naturalehia_calibration_binder_cli
        apps/calibration_binder_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_calibration_binder_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_calibration_binder_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_calibration_binder_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-calibration-binder"
    )
    cellular_finance_set_compile_policy(
        naturalehia_calibration_binder_cli
    )

    add_executable(
        naturalehia_claim_ledger_cli
        apps/claim_ledger_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_claim_ledger_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_claim_ledger_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_claim_ledger_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-claim-ledger"
    )
    cellular_finance_set_compile_policy(
        naturalehia_claim_ledger_cli
    )

    add_executable(
        naturalehia_joint_cohort_cli
        apps/joint_cohort_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_joint_cohort_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_joint_cohort_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_joint_cohort_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-joint-cohort-envelope"
    )
    cellular_finance_set_compile_policy(
        naturalehia_joint_cohort_cli
    )

    add_executable(
        naturalehia_instrument_family_cli
        apps/instrument_family_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_instrument_family_cli
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        naturalehia_instrument_family_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_instrument_family_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-instrument-family"
    )
    cellular_finance_set_compile_policy(
        naturalehia_instrument_family_cli
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(
        cellular_finance_model_tests
        tests/model_tests.cpp
    )
    target_link_libraries(
        cellular_finance_model_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_model_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_model_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(cellular_finance_model_tests)
    add_test(
        NAME cellular_finance_model_tests
        COMMAND cellular_finance_model_tests
    )

    add_executable(
        cellular_finance_claim_ledger_tests
        tests/claim_ledger_tests.cpp
    )
    target_link_libraries(
        cellular_finance_claim_ledger_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_claim_ledger_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_claim_ledger_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_claim_ledger_tests
    )
    add_test(
        NAME cellular_finance_claim_ledger_tests
        COMMAND cellular_finance_claim_ledger_tests
    )

    add_executable(
        cellular_finance_claim_ledger_package_tests
        tests/claim_ledger_package_tests.cpp
    )
    target_link_libraries(
        cellular_finance_claim_ledger_package_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_claim_ledger_package_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_claim_ledger_package_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_claim_ledger_package_tests
    )
    if(EMSCRIPTEN)
        # Deep package-adversary construction exceeds Emscripten's default
        # stack in an optimized build and can corrupt a later fixture read.
        target_link_options(
            cellular_finance_claim_ledger_package_tests
            PRIVATE "SHELL:-sSTACK_SIZE=1048576"
        )
    endif()
    add_test(
        NAME cellular_finance_claim_ledger_package_tests
        COMMAND cellular_finance_claim_ledger_package_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic/claim.cfg
            ${CMAKE_CURRENT_SOURCE_DIR}/reference_transactions/liberation-labs-facility-financing-2024-2025/claim_ledger_v0_1/claim.cfg
            ${CMAKE_CURRENT_SOURCE_DIR}/reference_transactions/solar-foods-factory-01-facility-2022-2025/claim_ledger_v0_1/claim.cfg
    )

    add_executable(
        cellular_finance_claim_ledger_portfolio_adapter_tests
        tests/claim_ledger_portfolio_adapter_tests.cpp
    )
    target_link_libraries(
        cellular_finance_claim_ledger_portfolio_adapter_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_claim_ledger_portfolio_adapter_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_claim_ledger_portfolio_adapter_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_claim_ledger_portfolio_adapter_tests
    )
    add_test(
        NAME cellular_finance_claim_ledger_portfolio_adapter_tests
        COMMAND cellular_finance_claim_ledger_portfolio_adapter_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic/claim.cfg
    )

    add_executable(
        cellular_finance_claim_ledger_joint_portfolio_tests
        tests/claim_ledger_joint_portfolio_tests.cpp
    )
    target_link_libraries(
        cellular_finance_claim_ledger_joint_portfolio_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_claim_ledger_joint_portfolio_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_claim_ledger_joint_portfolio_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_claim_ledger_joint_portfolio_tests
    )
    if(EMSCRIPTEN)
        # The fail-closed joint-assembler cases unwind several deeply nested
        # package/portfolio values. Emscripten's default stack is too small
        # for that valid Release path and otherwise traps before C++ can
        # deliver the expected invalid_argument.
        target_link_options(
            cellular_finance_claim_ledger_joint_portfolio_tests
            PRIVATE "SHELL:-sSTACK_SIZE=1048576"
        )
    endif()
    add_test(
        NAME cellular_finance_claim_ledger_joint_portfolio_tests
        COMMAND cellular_finance_claim_ledger_joint_portfolio_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic/claim.cfg
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic-peer/claim.cfg
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic-rare/claim.cfg
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic-shifted/claim.cfg
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic-rounding/claim.cfg
    )

    add_executable(
        cellular_finance_partial_credit_claim_loss_cohort_tests
        tests/partial_credit_claim_loss_cohort_tests.cpp
    )
    target_link_libraries(
        cellular_finance_partial_credit_claim_loss_cohort_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_partial_credit_claim_loss_cohort_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_partial_credit_claim_loss_cohort_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_partial_credit_claim_loss_cohort_tests
    )
    add_test(
        NAME cellular_finance_partial_credit_claim_loss_cohort_tests
        COMMAND cellular_finance_partial_credit_claim_loss_cohort_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic/claim.cfg
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic-peer/claim.cfg
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic-rare/claim.cfg
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic-shifted/claim.cfg
    )

    add_executable(
        cellular_finance_partial_credit_claim_loss_cohort_config_tests
        tests/partial_credit_claim_loss_cohort_config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_partial_credit_claim_loss_cohort_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_partial_credit_claim_loss_cohort_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_partial_credit_claim_loss_cohort_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_partial_credit_claim_loss_cohort_config_tests
    )
    if(EMSCRIPTEN)
        target_link_options(
            cellular_finance_partial_credit_claim_loss_cohort_config_tests
            PRIVATE "SHELL:-sSTACK_SIZE=1048576"
        )
    endif()
    add_test(
        NAME cellular_finance_partial_credit_claim_loss_cohort_config_tests
        COMMAND cellular_finance_partial_credit_claim_loss_cohort_config_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic/claim.cfg
    )

    add_executable(
        cellular_finance_config_tests
        tests/config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(cellular_finance_config_tests)
    add_test(
        NAME cellular_finance_config_tests
        COMMAND cellular_finance_config_tests
    )

    add_executable(
        cellular_finance_evidence_gate_tests
        tests/evidence_gate_tests.cpp
    )
    target_link_libraries(
        cellular_finance_evidence_gate_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_evidence_gate_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_evidence_gate_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_evidence_gate_tests
    )
    add_test(
        NAME cellular_finance_evidence_gate_tests
        COMMAND cellular_finance_evidence_gate_tests
    )

    add_executable(
        cellular_finance_staged_capital_tests
        tests/staged_capital_tests.cpp
    )
    target_link_libraries(
        cellular_finance_staged_capital_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_staged_capital_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_staged_capital_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_staged_capital_tests
    )
    add_test(
        NAME cellular_finance_staged_capital_tests
        COMMAND cellular_finance_staged_capital_tests
    )

    add_executable(
        cellular_finance_portfolio_tests
        tests/portfolio_tests.cpp
    )
    target_link_libraries(
        cellular_finance_portfolio_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_portfolio_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_portfolio_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_portfolio_tests
    )
    add_test(
        NAME cellular_finance_portfolio_tests
        COMMAND cellular_finance_portfolio_tests
    )

    add_executable(
        cellular_finance_portfolio_config_tests
        tests/portfolio_config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_portfolio_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_portfolio_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_portfolio_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_portfolio_config_tests
    )
    add_test(
        NAME cellular_finance_portfolio_config_tests
        COMMAND cellular_finance_portfolio_config_tests
    )

    add_executable(
        cellular_finance_portfolio_adapters_tests
        tests/portfolio_adapters_tests.cpp
    )
    target_link_libraries(
        cellular_finance_portfolio_adapters_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_portfolio_adapters_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_portfolio_adapters_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_portfolio_adapters_tests
    )
    add_test(
        NAME cellular_finance_portfolio_adapters_tests
        COMMAND cellular_finance_portfolio_adapters_tests
    )

    add_executable(
        cellular_finance_portfolio_ambiguity_tests
        tests/portfolio_ambiguity_tests.cpp
    )
    target_link_libraries(
        cellular_finance_portfolio_ambiguity_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_portfolio_ambiguity_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_portfolio_ambiguity_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_portfolio_ambiguity_tests
    )
    add_test(
        NAME cellular_finance_portfolio_ambiguity_tests
        COMMAND cellular_finance_portfolio_ambiguity_tests
    )

    add_executable(
        cellular_finance_portfolio_ambiguity_config_tests
        tests/portfolio_ambiguity_config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_portfolio_ambiguity_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_portfolio_ambiguity_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_portfolio_ambiguity_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_portfolio_ambiguity_config_tests
    )
    add_test(
        NAME cellular_finance_portfolio_ambiguity_config_tests
        COMMAND cellular_finance_portfolio_ambiguity_config_tests
    )

    add_executable(
        cellular_finance_probability_polytope_tests
        tests/probability_polytope_tests.cpp
    )
    target_link_libraries(
        cellular_finance_probability_polytope_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_probability_polytope_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_probability_polytope_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_probability_polytope_tests
    )
    add_test(
        NAME cellular_finance_probability_polytope_tests
        COMMAND cellular_finance_probability_polytope_tests
    )

    add_executable(
        cellular_finance_probability_polytope_config_tests
        tests/probability_polytope_config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_probability_polytope_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_probability_polytope_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_probability_polytope_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_probability_polytope_config_tests
    )
    add_test(
        NAME cellular_finance_probability_polytope_config_tests
        COMMAND cellular_finance_probability_polytope_config_tests
    )

    add_executable(
        cf_polytope_attribution_tests
        tests/probability_polytope_tail_attribution_tests.cpp
    )
    target_link_libraries(
        cf_polytope_attribution_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_polytope_attribution_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_polytope_attribution_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cf_polytope_attribution_tests
    )
    add_test(
        NAME cellular_finance_probability_polytope_tail_attribution_tests
        COMMAND cf_polytope_attribution_tests
    )

    add_executable(
        cellular_finance_success_participation_tests
        tests/success_participation_tests.cpp
    )
    target_link_libraries(
        cellular_finance_success_participation_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_success_participation_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_success_participation_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_success_participation_tests
    )
    add_test(
        NAME cellular_finance_success_participation_tests
        COMMAND cellular_finance_success_participation_tests
    )

    add_executable(
        cellular_finance_success_participation_config_tests
        tests/success_participation_config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_success_participation_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_success_participation_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_success_participation_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_success_participation_config_tests
    )
    add_test(
        NAME cellular_finance_success_participation_config_tests
        COMMAND cellular_finance_success_participation_config_tests
    )

    add_executable(
        cellular_finance_pooled_loss_protection_tests
        tests/pooled_loss_protection_tests.cpp
    )
    target_link_libraries(
        cellular_finance_pooled_loss_protection_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_pooled_loss_protection_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_pooled_loss_protection_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_pooled_loss_protection_tests
    )
    add_test(
        NAME cellular_finance_pooled_loss_protection_tests
        COMMAND cellular_finance_pooled_loss_protection_tests
    )

    add_executable(
        cellular_finance_pooled_loss_protection_config_tests
        tests/pooled_loss_protection_config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_pooled_loss_protection_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_pooled_loss_protection_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_pooled_loss_protection_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_pooled_loss_protection_config_tests
    )
    add_test(
        NAME cellular_finance_pooled_loss_protection_config_tests
        COMMAND cellular_finance_pooled_loss_protection_config_tests
    )

    add_executable(
        cellular_finance_provider_price_ladder_tests
        tests/provider_price_ladder_tests.cpp
    )
    target_link_libraries(
        cellular_finance_provider_price_ladder_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_provider_price_ladder_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_provider_price_ladder_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_provider_price_ladder_tests
    )
    add_test(
        NAME cellular_finance_provider_price_ladder_tests
        COMMAND cellular_finance_provider_price_ladder_tests
    )

    add_executable(
        cellular_finance_provider_price_ladder_config_tests
        tests/provider_price_ladder_config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_provider_price_ladder_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_provider_price_ladder_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_provider_price_ladder_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_provider_price_ladder_config_tests
    )
    add_test(
        NAME cellular_finance_provider_price_ladder_config_tests
        COMMAND cellular_finance_provider_price_ladder_config_tests
    )

    add_executable(
        cellular_finance_provider_credit_stress_tests
        tests/provider_credit_stress_tests.cpp
    )
    target_link_libraries(
        cellular_finance_provider_credit_stress_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_provider_credit_stress_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_provider_credit_stress_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_provider_credit_stress_tests
    )
    add_test(
        NAME cellular_finance_provider_credit_stress_tests
        COMMAND cellular_finance_provider_credit_stress_tests
    )

    add_executable(
        cellular_finance_provider_credit_stress_config_tests
        tests/provider_credit_stress_config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_provider_credit_stress_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_provider_credit_stress_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_provider_credit_stress_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_provider_credit_stress_config_tests
    )
    add_test(
        NAME cellular_finance_provider_credit_stress_config_tests
        COMMAND cellular_finance_provider_credit_stress_config_tests
    )

    add_executable(
        cellular_finance_capital_stack_tests
        tests/capital_stack_tests.cpp
    )
    target_link_libraries(
        cellular_finance_capital_stack_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_capital_stack_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_capital_stack_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_capital_stack_tests
    )
    add_test(
        NAME cellular_finance_capital_stack_tests
        COMMAND cellular_finance_capital_stack_tests
    )

    add_executable(
        cellular_finance_capital_stack_v02_tests
        tests/capital_stack_v02_tests.cpp
    )
    target_link_libraries(
        cellular_finance_capital_stack_v02_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_capital_stack_v02_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_capital_stack_v02_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_capital_stack_v02_tests
    )
    add_test(
        NAME cellular_finance_capital_stack_v02_tests
        COMMAND cellular_finance_capital_stack_v02_tests
    )

    add_executable(
        cellular_finance_claim_ledger_capital_stack_v02_tests
        tests/claim_ledger_capital_stack_v02_tests.cpp
    )
    target_link_libraries(
        cellular_finance_claim_ledger_capital_stack_v02_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_claim_ledger_capital_stack_v02_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_claim_ledger_capital_stack_v02_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_claim_ledger_capital_stack_v02_tests
    )
    add_test(
        NAME cellular_finance_claim_ledger_capital_stack_v02_tests
        COMMAND cellular_finance_claim_ledger_capital_stack_v02_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic/claim.cfg
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic-peer/claim.cfg
    )

    add_executable(
        cf_stack_polytope_tests
        tests/capital_stack_probability_polytope_tests.cpp
    )
    target_link_libraries(
        cf_stack_polytope_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_stack_polytope_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_stack_polytope_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cf_stack_polytope_tests
    )
    add_test(
        NAME cellular_finance_capital_stack_probability_polytope_tests
        COMMAND cf_stack_polytope_tests
    )

    add_executable(
        cf_frontier_tests
        tests/robust_capital_mobilization_frontier_tests.cpp
    )
    target_link_libraries(
        cf_frontier_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_frontier_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_frontier_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cf_frontier_tests
    )
    add_test(
        NAME cellular_finance_robust_capital_mobilization_frontier_tests
        COMMAND cf_frontier_tests
    )

    add_executable(
        cf_frontier_config_tests
        tests/robust_capital_mobilization_frontier_config_tests.cpp
    )
    target_link_libraries(
        cf_frontier_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_frontier_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_frontier_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cf_frontier_config_tests
    )
    add_test(
        NAME cellular_finance_robust_capital_mobilization_frontier_config_tests
        COMMAND cf_frontier_config_tests
    )

    add_executable(
        cf_priority_cap_tests
        tests/robust_market_priority_cap_tests.cpp
    )
    target_link_libraries(
        cf_priority_cap_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_priority_cap_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_priority_cap_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cf_priority_cap_tests
    )
    add_test(
        NAME cellular_finance_robust_market_priority_cap_tests
        COMMAND cf_priority_cap_tests
    )

    add_executable(
        cf_priority_cap_config_tests
        tests/robust_market_priority_cap_config_tests.cpp
    )
    target_link_libraries(
        cf_priority_cap_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_priority_cap_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_priority_cap_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cf_priority_cap_config_tests
    )
    add_test(
        NAME cellular_finance_robust_market_priority_cap_config_tests
        COMMAND cf_priority_cap_config_tests
    )

    add_executable(
        cf_issue_price_support_config_tests
        tests/robust_issue_price_support_config_tests.cpp
    )
    target_link_libraries(
        cf_issue_price_support_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_issue_price_support_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_issue_price_support_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cf_issue_price_support_config_tests
    )
    add_test(
        NAME cellular_finance_robust_issue_price_support_config_tests
        COMMAND cf_issue_price_support_config_tests
    )

    add_executable(
        cf_issue_price_support_tests
        tests/robust_issue_price_support_tests.cpp
    )
    target_link_libraries(
        cf_issue_price_support_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_issue_price_support_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_issue_price_support_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cf_issue_price_support_tests
    )
    add_test(
        NAME cellular_finance_robust_issue_price_support_tests
        COMMAND cf_issue_price_support_tests
    )

    add_executable(
        cf_hurdle_envelope_tests
        tests/robust_hurdle_envelope_tests.cpp
    )
    target_link_libraries(
        cf_hurdle_envelope_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_hurdle_envelope_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_hurdle_envelope_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cf_hurdle_envelope_tests
    )
    add_test(
        NAME cellular_finance_robust_hurdle_envelope_tests
        COMMAND cf_hurdle_envelope_tests
    )

    add_executable(
        cf_hurdle_envelope_config_tests
        tests/robust_hurdle_envelope_config_tests.cpp
    )
    target_link_libraries(
        cf_hurdle_envelope_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cf_hurdle_envelope_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cf_hurdle_envelope_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cf_hurdle_envelope_config_tests
    )
    add_test(
        NAME cellular_finance_robust_hurdle_envelope_config_tests
        COMMAND cf_hurdle_envelope_config_tests
    )
    set_tests_properties(
        cellular_finance_robust_hurdle_envelope_config_tests
        PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_executable(
        cellular_finance_capital_stack_config_tests
        tests/capital_stack_config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_capital_stack_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_capital_stack_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_capital_stack_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_capital_stack_config_tests
    )
    add_test(
        NAME cellular_finance_capital_stack_config_tests
        COMMAND cellular_finance_capital_stack_config_tests
    )

    add_executable(
        cellular_finance_calibration_binder_tests
        tests/calibration_binder_tests.cpp
    )
    target_link_libraries(
        cellular_finance_calibration_binder_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_calibration_binder_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_calibration_binder_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_calibration_binder_tests
    )
    add_test(
        NAME cellular_finance_calibration_binder_tests
        COMMAND cellular_finance_calibration_binder_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/calibration-binder-v0.1-synthetic
    )

    add_executable(
        cellular_finance_calibration_binder_config_tests
        tests/calibration_binder_config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_calibration_binder_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_calibration_binder_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_calibration_binder_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_calibration_binder_config_tests
    )
    add_test(
        NAME cellular_finance_calibration_binder_config_tests
        COMMAND cellular_finance_calibration_binder_config_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/calibration-binder-v0.1-synthetic
    )

    add_executable(
        cellular_finance_joint_cohort_tests
        tests/joint_cohort_tests.cpp
    )
    target_link_libraries(
        cellular_finance_joint_cohort_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_joint_cohort_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_joint_cohort_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_joint_cohort_tests
    )
    add_test(
        NAME cellular_finance_joint_cohort_tests
        COMMAND cellular_finance_joint_cohort_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic
    )

    add_executable(
        cellular_finance_joint_cohort_capital_stack_tests
        tests/joint_cohort_capital_stack_tests.cpp
    )
    target_link_libraries(
        cellular_finance_joint_cohort_capital_stack_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_joint_cohort_capital_stack_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_joint_cohort_capital_stack_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_joint_cohort_capital_stack_tests
    )
    add_test(
        NAME cellular_finance_joint_cohort_capital_stack_tests
        COMMAND cellular_finance_joint_cohort_capital_stack_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic/success-participation.cfg
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic/capital-stack.cfg
    )

    add_executable(
        cellular_finance_joint_cohort_config_tests
        tests/joint_cohort_config_tests.cpp
    )
    target_link_libraries(
        cellular_finance_joint_cohort_config_tests
        PRIVATE Naturalehia::CellularFinance
    )
    target_compile_features(
        cellular_finance_joint_cohort_config_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        cellular_finance_joint_cohort_config_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    cellular_finance_set_compile_policy(
        cellular_finance_joint_cohort_config_tests
    )
    add_test(
        NAME cellular_finance_joint_cohort_config_tests
        COMMAND cellular_finance_joint_cohort_config_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic
    )

    if(NATURALEHIA_CELLULAR_FINANCE_BUILD_CLI)
        add_test(
            NAME cellular_finance_cli_illustrative
            COMMAND naturalehia_cellular_finance_cli
                ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/illustrative.cfg
                --trials 256
        )
        set_tests_properties(
            cellular_finance_cli_illustrative
            PROPERTIES
                PASS_REGULAR_EXPRESSION "SYNTHETIC ILLUSTRATION"
        )

        add_test(
            NAME cellular_finance_cli_adverse
            COMMAND naturalehia_cellular_finance_cli
                ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/illustrative-adverse.cfg
                --trials 256
        )
        set_tests_properties(
            cellular_finance_cli_adverse
            PROPERTIES
                PASS_REGULAR_EXPRESSION "Illustrative adverse facility stress"
        )

        add_test(
            NAME cellular_finance_staged_capital_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_staged_capital_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DSCENARIO=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/milestone-gated-capital-synthetic.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_staged_capital_cli.cmake
        )

        add_test(
            NAME cellular_finance_portfolio_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_portfolio_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DSCENARIO=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-participation-pool-synthetic.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_portfolio_cli.cmake
        )

        add_test(
            NAME cellular_finance_portfolio_ambiguity_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_portfolio_ambiguity_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DPORTFOLIO=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-participation-pool-synthetic.cfg
                -DAMBIGUITY=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-probability-envelope-synthetic.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_ambiguity_cli.cmake
        )

        add_test(
            NAME cellular_finance_probability_polytope_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_portfolio_ambiguity_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DPORTFOLIO=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-participation-pool-synthetic.cfg
                -DPOLYTOPE=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-event-polytope-synthetic.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_probability_polytope_cli.cmake
        )

        add_test(
            NAME cellular_finance_success_participation_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_success_participation_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DPORTFOLIO=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-participation-pool-synthetic.cfg
                -DAMBIGUITY=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-probability-envelope-synthetic.cfg
                -DPARTICIPATION=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-success-participation-synthetic.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_success_participation_cli.cmake
        )

        add_test(
            NAME cellular_finance_pooled_loss_protection_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_pooled_loss_protection_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DPORTFOLIO=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-participation-pool-synthetic.cfg
                -DAMBIGUITY=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-probability-envelope-synthetic.cfg
                -DPARTICIPATION=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-success-participation-synthetic.cfg
                -DPROTECTION=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-pooled-loss-protection-synthetic.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_pooled_loss_protection_cli.cmake
        )

        add_test(
            NAME cellular_finance_provider_price_ladder_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_provider_price_ladder_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DPORTFOLIO=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-participation-pool-synthetic.cfg
                -DAMBIGUITY=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-probability-envelope-synthetic.cfg
                -DPARTICIPATION=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-success-participation-synthetic.cfg
                -DPROTECTION=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-pooled-loss-protection-synthetic.cfg
                -DPROVIDER_PRICE=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-provider-price-ladder-synthetic.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_provider_price_ladder_cli.cmake
        )

        add_test(
            NAME cellular_finance_provider_credit_stress_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_provider_credit_stress_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DPORTFOLIO=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-participation-pool-synthetic.cfg
                -DAMBIGUITY=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-probability-envelope-synthetic.cfg
                -DPARTICIPATION=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-success-participation-synthetic.cfg
                -DPROTECTION=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-pooled-loss-protection-synthetic.cfg
                -DPROVIDER_PRICE=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-provider-price-ladder-synthetic.cfg
                -DPROVIDER_CREDIT=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-provider-credit-stress-synthetic.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_provider_credit_stress_cli.cmake
        )

        add_test(
            NAME cellular_finance_capital_stack_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_capital_stack_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DPORTFOLIO=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-participation-pool-synthetic.cfg
                -DAMBIGUITY=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-probability-envelope-synthetic.cfg
                -DPARTICIPATION=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-success-participation-synthetic.cfg
                -DSTACK=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-capital-stack-synthetic.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_capital_stack_cli.cmake
        )

        add_test(
            NAME cf_stack_poly_cli
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_capital_stack_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DPORTFOLIO=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-participation-pool-synthetic.cfg
                -DPOLYTOPE=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-event-polytope-synthetic.cfg
                -DAMBIGUITY=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-probability-envelope-synthetic.cfg
                -DPARTICIPATION=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-success-participation-synthetic.cfg
                -DSTACK=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/two-project-capital-stack-synthetic.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_stack_poly_cli.cmake
        )

        add_test(
            NAME cf_frontier_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:cf_frontier_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/capital-mobilization-frontier-v0.1-synthetic
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_capital_mobilization_frontier_cli.cmake
        )

        add_test(
            NAME cf_frontier_v02_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:cf_frontier_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/ten-claim-instrument-v1-synthetic
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_capital_mobilization_frontier_v02_cli.cmake
        )

        add_test(
            NAME cf_priority_cap_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:cf_priority_cap_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/market-priority-cap-v0.1-synthetic
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_market_priority_cap_cli.cmake
        )

        add_test(
            NAME cf_issue_price_support_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:cf_issue_price_support_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/market-priority-cap-v0.1-synthetic
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_issue_price_support_cli.cmake
        )

        if(EMSCRIPTEN)
            add_test(
                NAME cf_issue_price_support_browser_assets_match_published
                COMMAND
                    ${CMAKE_COMMAND}
                    "-DBUILT_MJS=$<TARGET_FILE:cf_issue_price_support_browser>"
                    "-DBUILT_WASM=$<TARGET_FILE_DIR:cf_issue_price_support_browser>/naturalehia-issue-price-support-browser.wasm"
                    -DPUBLISHED_MJS=${CMAKE_CURRENT_SOURCE_DIR}/assets/wasm/naturalehia-issue-price-support-browser.mjs
                    -DPUBLISHED_WASM=${CMAKE_CURRENT_SOURCE_DIR}/assets/wasm/naturalehia-issue-price-support-browser.wasm
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_issue_price_support_browser_assets.cmake
            )
            set_tests_properties(
                cf_issue_price_support_browser_assets_match_published
                PROPERTIES LABELS "browser;wasm;published-provenance"
            )
        endif()

        add_test(
            NAME cf_hurdle_evidence_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:cf_hurdle_evidence_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/market-priority-cap-v0.1-synthetic
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_hurdle_evidence_cli.cmake
        )

        add_test(
            NAME cellular_finance_joint_cohort_capital_stack_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_capital_stack_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DCOHORT=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic/cohort.cfg
                -DBLOCKED=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic/blocked.cfg
                -DPARTICIPATION=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic/success-participation.cfg
                -DSTACK=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic/capital-stack.cfg
                -DSTACK_Q037=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic/capital-stack-q-0.37.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_joint_cohort_capital_stack_cli.cmake
        )

        add_test(
            NAME cellular_finance_calibration_binder_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_calibration_binder_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DBINDER=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/calibration-binder-v0.1-synthetic/binder.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_calibration_binder_cli.cmake
        )

        add_test(
            NAME cellular_finance_claim_ledger_cli
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_claim_ledger_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DSYNTHETIC=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/claim-ledger-v0.1-synthetic/claim.cfg
                -DLIBERATION=${CMAKE_CURRENT_SOURCE_DIR}/reference_transactions/liberation-labs-facility-financing-2024-2025/claim_ledger_v0_1/claim.cfg
                -DSOLAR=${CMAKE_CURRENT_SOURCE_DIR}/reference_transactions/solar-foods-factory-01-facility-2022-2025/claim_ledger_v0_1/claim.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_claim_ledger_cli.cmake
        )

        add_test(
            NAME cellular_finance_joint_cohort_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_joint_cohort_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DCOHORT=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic/cohort.cfg
                -DBLOCKED=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/joint-cohort-v0.1-synthetic/blocked.cfg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_joint_cohort_cli.cmake
        )

        foreach(evidence_cli_mode IN ITEMS
                enforcing
                report-only
                invalid-date
                missing-date-value
                out-of-range-date)
            add_test(
                NAME cellular_finance_evidence_gate_${evidence_cli_mode}
                COMMAND
                    ${CMAKE_COMMAND}
                    "-DPROGRAM=$<TARGET_FILE:naturalehia_evidence_gate_cli>"
                    "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                    -DDOSSIER=${CMAKE_CURRENT_SOURCE_DIR}/reference_projects/believer-wilson/dossier.cfg
                    -DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/reference_projects/believer-wilson/evidence_manifest.tsv
                    -DMODE=${evidence_cli_mode}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_evidence_cli.cmake
            )
        endforeach()

        add_test(
            NAME cellular_finance_evidence_gate_claim_population
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_evidence_gate_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/claim-population-evidence-cli
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_claim_population_evidence_cli.cmake
        )

        add_test(
            NAME cellular_finance_ten_claim_instrument_family_cli_synthetic
            COMMAND
                ${CMAKE_COMMAND}
                "-DPROGRAM=$<TARGET_FILE:naturalehia_instrument_family_cli>"
                "-DPROGRAM_EMULATOR=${cellular_finance_program_emulator}"
                -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/scenarios/ten-claim-instrument-v1-synthetic
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_ten_claim_instrument_cli.cmake
        )
    endif()
endif()
