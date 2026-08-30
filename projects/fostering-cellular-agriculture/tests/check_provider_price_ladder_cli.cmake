if(NOT DEFINED PROGRAM OR NOT DEFINED PORTFOLIO OR
        NOT DEFINED AMBIGUITY OR NOT DEFINED PARTICIPATION OR
        NOT DEFINED PROTECTION OR NOT DEFINED PROVIDER_PRICE)
    message(FATAL_ERROR
        "PROGRAM, PORTFOLIO, AMBIGUITY, PARTICIPATION, PROTECTION, and PROVIDER_PRICE are required")
endif()

execute_process(
    COMMAND "${PROGRAM}" "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${PROTECTION}" "${PROVIDER_PRICE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "provider price-ladder fixture failed with ${result}:\n${error_output}")
endif()

# At q=1 and the certified g~=1/6 point, the robust expected claim is 0.8.
# The independently disclosed additions are 0.08 variable expense,
# 0.138666667 collateral carry, 0.21 economic-capital charge, 0.05 fixed
# expense, and 0.10 target profit. The all-in floor is therefore
# 1.378666667 DEMO million while investor premium headroom is zero.
set(required_fragments
    "SYNTHETIC PROVIDER PRICE-LADDER SENSITIVITY"
    "Physical-probability premium adequacy only; not fair value, a market quote, a rating, capital validation, or an offering."
    "  provider id: synthetic-catalytic-provider"
    "  selection mode: reported-investor-target-passing"
    "  protection solver status: certified-interior-bracket"
    "  selected coverage fraction g: 0.166667"
    "  investor target met before premium: yes"
    "  cost basis: contractual maximum exposure"
    "  provider performance: assumed in every scenario"
    "  configured monetary support cap: 3.333333 DEMO million"
    "  contractual maximum exposure: 3.333333 DEMO million"
    "  modeled maximum claim: 2.666667 DEMO million"
    "  collateral principal base: 1.666667 DEMO million"
    "  allocated risk-capital base: 1.000000 DEMO million"
    "  Collateral principal and allocated capital stock are not premium expenses."
    "  robust expected claim PV: 0.800000 DEMO million"
    "  variable claim expense: 0.080000 DEMO million"
    "  claim plus variable expense subtotal: 0.880000 DEMO million"
    "  collateral funding carry PV: 0.138667 DEMO million"
    "  incremental economic-capital charge PV: 0.210000 DEMO million"
    "  fixed upfront expense: 0.050000 DEMO million"
    "  provider robust cost-recovery floor: 1.278667 DEMO million"
    "  target underwriting profit: 0.100000 DEMO million"
    "  provider robust all-in floor: 1.378667 DEMO million"
    "  cost recovery | 0.853333 | 0.985333 | 1.278667 | DEMO million"
    "  all-in revenue requirement | 0.953333 | 1.085333 | 1.378667 | DEMO million"
    "  status: provider-all-in-floor-exceeds-investor-ceiling"
    "  investor signed premium headroom: 0.000000 DEMO million"
    "  investor maximum non-negative premium: 0.000000 DEMO million"
    "  robust price interval lower bound: none"
    "  robust price interval upper bound: none"
    "  cost-recovery support gap: 1.278667 DEMO million"
    "  provider premium support required: 1.378667 DEMO million"
    "  investor target restoration required: 0.000000 DEMO million"
    "  total all-in support gap: 1.378667 DEMO million"
    "  robust all-in bilateral interval exists: no"
    "  investor minimum NPV before premium | 0.000000 | common-loss=0.100000; common-success=0.500000; culture-loss-scaleup-success=0.200000; culture-success-scaleup-loss=0.200000"
    "  provider maximum all-in requirement | 1.378667 | common-loss=0.100000; common-success=0.500000; culture-loss-scaleup-success=0.200000; culture-success-scaleup-loss=0.200000"
    "  maximum price-ladder sum error: 0.000000 DEMO million"
    "  support-gap decomposition error: 0.000000 DEMO million"
    "  maximum transformed-range error: 0.000000 DEMO million"
    "  maximum endpoint probability error: 0.000000"
    "The collateral carry and economic-capital charge are independently supplied, incremental, nonduplicative pricing allowances."
    "Their bases are exposure-sizing bases, not debt/equity funding shares; capital does not reduce collateral in v0.1."
    "Supplied net rates must already reflect any capital funding, premium float, investment income, collateral reuse, and treasury offsets."
    "Allocated capital stock is not charged as an expense; the model does not establish regulatory or economic capital sufficiency."
    "A positive gap is required catalytic support, not hidden in probabilities or called diversification."
)

foreach(fragment IN LISTS required_fragments)
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "provider price-ladder fixture output is missing:\n${fragment}\n\n"
            "Complete output:\n${output}")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${PROTECTION}" "${PROVIDER_PRICE}"
        --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized provider price-ladder run failed with "
        "${normalized_result}:\n${normalized_error}")
endif()
foreach(heading IN ITEMS
        "Normalized portfolio configuration"
        "Normalized probability-envelope configuration"
        "Normalized success-participation configuration"
        "Normalized pooled-loss-protection configuration"
        "Normalized provider price-ladder configuration")
    string(FIND "${normalized_output}" "${heading}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "normalized output is missing: ${heading}")
    endif()
endforeach()
foreach(provider_key IN ITEMS
        "provider_price.model_version=0.1.0"
        "provider_price.label=Two-project provider price-ladder synthetic sensitivity"
        "provider_price.source_note=Unvalidated synthetic incremental provider cost allowances for hand reconciliation only"
        "provider_price.synthetic_inputs=true"
        "provider_price.coverage_selection=reported-investor-target-passing"
        "provider_price.explicit_coverage_fraction=none"
        "provider_price.cost_bases_use_contractual_maximum_exposure=true"
        "provider_price.collateral_and_capital_are_held_until_settlement=true"
        "provider_price.variable_claim_expense_is_paid_at_claim_settlement=true"
        "provider_price.fixed_expense_and_target_profit_are_month_zero_values=true"
        "provider_price.incremental_cost_terms_are_separate_and_nonduplicative=true"
        "provider_price.collateral_fraction_of_contractual_maximum_exposure=0.5"
        "provider_price.collateral_annual_effective_funding_rate=0.059999999999999998"
        "provider_price.collateral_annual_effective_yield_rate=0.02"
        "provider_price.risk_capital_fraction_of_contractual_maximum_exposure=0.29999999999999999"
        "provider_price.risk_capital_annual_effective_charge_rate=0.10000000000000001"
        "provider_price.fixed_expense_upfront_million=0.050000000000000003"
        "provider_price.variable_claim_expense_fraction=0.10000000000000001"
        "provider_price.target_profit_upfront_million=0.10000000000000001"
        "provider_price.provider_default_risk_is_modeled=false"
        "provider_price.fair_value_is_claimed=false")
    string(FIND "${normalized_output}" "${provider_key}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "normalized output is missing provider key/value: ${provider_key}")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${PROTECTION}" "${PROVIDER_PRICE}"
        --unknown-option
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
    COMMAND "${PROGRAM}" "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${PROTECTION}"
    RESULT_VARIABLE missing_price_result
    OUTPUT_VARIABLE missing_price_output
    ERROR_VARIABLE missing_price_error
)
if(NOT missing_price_result EQUAL 2)
    message(FATAL_ERROR
        "a missing provider-price input must exit 2, got ${missing_price_result}")
endif()
string(FIND "${missing_price_error}" "usage:" missing_price_usage_position)
if(missing_price_usage_position EQUAL -1)
    message(FATAL_ERROR
        "a missing provider-price input must print usage on stderr")
endif()
