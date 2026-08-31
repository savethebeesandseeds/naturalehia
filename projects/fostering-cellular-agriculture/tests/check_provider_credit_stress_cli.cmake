include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

if(NOT DEFINED PROGRAM OR NOT DEFINED PORTFOLIO OR
        NOT DEFINED AMBIGUITY OR NOT DEFINED PARTICIPATION OR
        NOT DEFINED PROTECTION OR NOT DEFINED PROVIDER_PRICE OR
        NOT DEFINED PROVIDER_CREDIT)
    message(FATAL_ERROR
        "PROGRAM, PORTFOLIO, AMBIGUITY, PARTICIPATION, PROTECTION, PROVIDER_PRICE, and PROVIDER_CREDIT are required")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${PROTECTION}" "${PROVIDER_PRICE}"
        "${PROVIDER_CREDIT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "provider counterparty-credit fixture failed with ${result}:\n${error_output}")
endif()

# Hand reconciliation at q=1 and the certified g~=1/6 protection point.
# Price-ladder collateral grows from 5/3 to 1.734 at month 24. In a default,
# 75% is recognized, so 1.3005 is applied before 25% unsecured recovery.
# Conditional provider default probabilities rise from 1% in common success to
# 10% in either single loss and 50% in common loss, making credit risk wrong-way
# without turning provider states into independently optimized ambiguity atoms.
set(required_fragments
    "SYNTHETIC PROVIDER COUNTERPARTY-CREDIT STRESS"
    "Fixed conditional physical-P provider states only; not CVA, fair value, a market quote, a rating, legal proof, capital validation, or an offering."
    "  provider id (protection/credit exact match): synthetic-catalytic-provider"
    "  selected coverage fraction g: 0.166667"
    "  claim settlement month: 24"
    "  gross contractual claim changed by credit stress: no"
    "  provider price reduced for default: no"
    "  conditional provider-state weights independently optimized: no"
    "  provider default measure: physical stress probability"
    "  provider price basis: full performance in every pricing scenario"
    "  contractual maximum exposure: 3.333333 DEMO million"
    "  modeled maximum claim: 2.666667 DEMO million"
    "  price-ladder collateral base explicitly pledged: 1.666667 DEMO million"
    "  pledged collateral grown to settlement: 1.734000 DEMO million"
    "Pledge and retained yield are input assertions, not facts inferred from the price ladder."
    "  provider default probability | 4.100000 | 5.220000 | 9.500000 | percent"
    "  positive claim and provider default probability | 3.400000 | 4.600000 | 9.000000 | percent"
    "  expected direct provider payment | 0.361333 | 0.458667 | 0.613333 | DEMO million"
    "  expected collateral applied | 0.044217 | 0.059823 | 0.117045 | DEMO million"
    "  expected unsecured exposure in default atoms (E[U*1_D]) | 0.007783 | 0.014844 | 0.069622 | DEMO million"
    "  expected delayed unsecured recovery | 0.001946 | 0.003711 | 0.017405 | DEMO million"
    "  expected ultimate unpaid claim | 0.005837 | 0.011133 | 0.052216 | DEMO million"
    "  expected full claim PV | 0.413333 | 0.533333 | 0.800000 | DEMO million"
    "  expected support received PV | 0.407496 | 0.522201 | 0.747784 | DEMO million"
    "  expected counterparty credit loss PV | 0.005837 | 0.011133 | 0.052216 | DEMO million"
    "  investor NPV before premium | -0.052216 | 1.922201 | 2.797496 | DEMO million"
    "  hypothetical investor NPV if paying unchanged provider price | -1.430883 | 0.543534 | 1.418829 | DEMO million"
    "  counterparty credit loss PV | 0.011133 | 0.101963 | 0.000000 | 0.000000 | 0.024625 | 1.024625 | 0.222655 | 1.024625 | DEMO million"
    "  ultimate unpaid claim | 0.011133 | 0.101963 | 0.000000 | 0.000000 | 0.024625 | 1.024625 | 0.222655 | 1.024625 | DEMO million"
    "  central provider default probability: 5.220000 percent"
    "  central positive-claim-and-default probability: 4.600000 percent"
    "  claim-weighted provider default rate: 0.140000"
    "  claim/default covariance: 0.046827 DEMO million"
    "  claim/default correlation: 0.298388"
    "  expected contractual claim given default: 1.430396 DEMO million"
    "  expected unsecured exposure given default: 0.284361 DEMO million"
    "  claim-at-default / unconditional-mean multiplier: 2.681992"
    "  central claim-PV delivery ratio: 0.979126"
    "  robust minimum claim-PV delivery ratio | 0.903018 | common-loss=0.100000; common-success=0.700000; culture-loss-scaleup-success=0.100000; culture-success-scaleup-loss=0.100000"
    "  unchanged full-performance provider all-in floor: 1.378667 DEMO million"
    "  provider price change caused by default stress: 0.000000 DEMO million"
    "  stressed investor signed premium headroom: -0.052216 DEMO million"
    "  stressed investor maximum non-negative premium: none"
    "  provider premium support required: 1.378667 DEMO million"
    "  investor target restoration required: 0.052216 DEMO million"
    "  base full-performance all-in support gap: 1.378667 DEMO million"
    "  incremental counterparty-credit support gap: 0.052216 DEMO million"
    "  stressed total all-in support gap: 1.430883 DEMO million"
    "common-loss/defaults | 0.500000 | 0.010000 | no | 2.666667 | 0.000000 | 0.750000 | 1.300500 | 1.300500 | 1.366167 | 0.250000 | 0.341542 | 30 | 0.341542 | 1.024625 | 1.642042 | 1.024625"
    "culture-loss-scaleup-success/defaults | 0.100000 | 0.018000 | no | 1.333333 | 0.000000 | 0.750000 | 1.300500 | 1.300500 | 0.032833 | 0.250000 | 0.008208 | 30 | 0.008208 | 0.024625 | 1.308708 | 0.024625"
    "  maximum expected counterparty credit loss PV | 0.052216"
    "  maximum gross project-loss change: 0.000000 DEMO million"
    "  maximum conditional-weight sum error: 0.000000"
    "  expanded central probability sum error: 0.000000"
    "  maximum default-waterfall error: 0.000000 DEMO million"
    "  maximum credit-loss error: 0.000000 DEMO million"
    "  maximum conditional-collapse error: 0.000000 DEMO million"
    "  maximum central probability-projection error: 0.000000"
    "  maximum central monetary-projection error: 0.000000 DEMO million"
    "  support-gap decomposition error: 0.000000 DEMO million"
    "  maximum probability-witness reconciliation error: 0.000000"
    "  maximum monetary-witness reconciliation error: 0.000000 DEMO million"
    "  robust delivery-ratio objective residual: 0.000000 DEMO million"
    "  maximum endpoint probability error: 0.000000"
    "Conditional provider states are averaged within each original project scenario before ambiguity projection."
    "Provider failure changes only claim delivery; it does not reprice the provider, reduce gross project loss, or create DVA."
    "Required support is disclosed explicitly; no such support is assumed to exist merely because the model computes it."
)

foreach(fragment IN LISTS required_fragments)
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "provider counterparty-credit fixture output is missing:\n${fragment}\n\n"
            "Complete output:\n${output}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${PROTECTION}" "${PROVIDER_PRICE}"
        "${PROVIDER_CREDIT}" --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized provider counterparty-credit run failed with "
        "${normalized_result}:\n${normalized_error}")
endif()
foreach(heading IN ITEMS
        "Normalized portfolio configuration"
        "Normalized probability-envelope configuration"
        "Normalized success-participation configuration"
        "Normalized pooled-loss-protection configuration"
        "Normalized provider price-ladder configuration"
        "Normalized provider counterparty-credit configuration")
    string(FIND "${normalized_output}" "${heading}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "normalized output is missing: ${heading}")
    endif()
endforeach()

set(credit_global_keys
    "provider_credit.model_version=0.1.0"
    "provider_credit.label=Two-project provider counterparty-credit synthetic stress"
    "provider_credit.source_note=Unvalidated synthetic physical-P provider performance states for hand reconciliation only"
    "provider_credit.provider_id=synthetic-catalytic-provider"
    "provider_credit.synthetic_inputs=true"
    "provider_credit.gross_contractual_claim_remains_unchanged=true"
    "provider_credit.provider_price_remains_full_performance_and_unchanged=true"
    "provider_credit.conditional_provider_state_weights_are_fixed_physical=true"
    "provider_credit.price_ladder_collateral_is_pledged_to_investor=true"
    "provider_credit.collateral_yield_remains_in_pledged_account=true"
    "provider_credit.collateral_applies_before_unsecured_recovery=true"
    "provider_credit.provider_default_occurs_at_claim_settlement=true"
    "provider_credit.provider_default_is_physical_stress_not_pricing_measure=true"
    "provider_credit.legal_enforceability_is_validated=false"
    "provider_credit.market_cva_or_fair_value_is_claimed=false"
    "provider_credit.scenario.count=4"
)
foreach(credit_key IN LISTS credit_global_keys)
    string(FIND "${normalized_output}" "${credit_key}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "normalized output is missing provider-credit key/value: ${credit_key}")
    endif()
endforeach()

set(scenario_ids
    "common-success"
    "culture-loss-scaleup-success"
    "culture-success-scaleup-loss"
    "common-loss"
)
set(perform_weights
    "0.98999999999999999"
    "0.90000000000000002"
    "0.90000000000000002"
    "0.5"
)
set(default_weights
    "0.01"
    "0.10000000000000001"
    "0.10000000000000001"
    "0.5"
)
foreach(scenario_index RANGE 1 4)
    math(EXPR list_index "${scenario_index} - 1")
    list(GET scenario_ids ${list_index} scenario_id)
    list(GET perform_weights ${list_index} perform_weight)
    list(GET default_weights ${list_index} default_weight)
    set(prefix "provider_credit.scenario.${scenario_index}")
    set(state_fragments
        "${prefix}.id=${scenario_id}"
        "${prefix}.state.count=2"
        "${prefix}.state.1.id=performs"
        "${prefix}.state.1.conditional_weight=${perform_weight}"
        "${prefix}.state.1.provider_performs=true"
        "${prefix}.state.1.collateral_realization_fraction=0"
        "${prefix}.state.1.unsecured_recovery_fraction=0"
        "${prefix}.state.1.unsecured_recovery_delay_months=0"
        "${prefix}.state.2.id=defaults"
        "${prefix}.state.2.conditional_weight=${default_weight}"
        "${prefix}.state.2.provider_performs=false"
        "${prefix}.state.2.collateral_realization_fraction=0.75"
        "${prefix}.state.2.unsecured_recovery_fraction=0.25"
        "${prefix}.state.2.unsecured_recovery_delay_months=6"
    )
    foreach(state_fragment IN LISTS state_fragments)
        string(FIND "${normalized_output}" "${state_fragment}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR
                "normalized output is missing provider-credit state key/value: ${state_fragment}")
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${PROTECTION}" "${PROVIDER_PRICE}"
        "${PROVIDER_CREDIT}" --unknown-option
    RESULT_VARIABLE bad_option_result
    OUTPUT_VARIABLE bad_option_output
    ERROR_VARIABLE bad_option_error
)
if(NOT bad_option_result EQUAL 2)
    message(FATAL_ERROR
        "an unknown option must exit 2, got ${bad_option_result}")
endif()
string(FIND "${bad_option_error}" "usage:" bad_option_usage_position)
if(bad_option_usage_position EQUAL -1)
    message(FATAL_ERROR "an unknown option must print usage on stderr")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${PROTECTION}" "${PROVIDER_PRICE}"
    RESULT_VARIABLE missing_credit_result
    OUTPUT_VARIABLE missing_credit_output
    ERROR_VARIABLE missing_credit_error
)
if(NOT missing_credit_result EQUAL 2)
    message(FATAL_ERROR
        "a missing provider-credit input must exit 2, got ${missing_credit_result}")
endif()
string(FIND "${missing_credit_error}" "usage:" missing_credit_usage_position)
if(missing_credit_usage_position EQUAL -1)
    message(FATAL_ERROR
        "a missing provider-credit input must print usage on stderr")
endif()
