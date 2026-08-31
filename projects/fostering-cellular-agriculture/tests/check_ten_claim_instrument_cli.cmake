include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

if(NOT DEFINED PROGRAM OR NOT DEFINED FIXTURE)
    message(FATAL_ERROR "PROGRAM and FIXTURE are required")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND}
        "${FIXTURE}/portfolio.cfg"
        "${FIXTURE}/ambiguity.cfg"
        "${FIXTURE}/success-participation.cfg"
        "${FIXTURE}/capital-stack.cfg"
        "${FIXTURE}/loss-protection.cfg"
        "${FIXTURE}/provider-price.cfg"
        "${FIXTURE}/provider-credit.cfg"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "ten-claim instrument-family fixture failed with ${result}:\n${error_output}")
endif()

# This is one cross-engine fixture test. The exact fragments pin the common
# ten-claim boundary, the unsupported pool, both and only both variants, and
# the explicitly adverse provider-credit result without presenting any value
# as calibrated probability or fair value.
set(required_fragments
    "SYNTHETIC TEN-CLAIM CELLULAR-AGRICULTURE INSTRUMENT FAMILY"
    "ONE CORE MULTI-PROJECT ASSET / TWO TRANSPARENT VARIANTS"
    "  project claims: 10 (required exactly 10)"
    "  declared joint scenarios: 9"
    "  shared adverse risk groups: 5"
    "  instrument variants: 2 (authorized maximum 2)"
    "  contractual reference principal: 100.000000 DEMO million"
    "UNSUPPORTED CORE - NO EXTERNAL CREDIT ENHANCEMENT"
    "  all-in investor cash contributed (outlays plus pool costs) | 88.303000 | 94.624000 | 99.503500 | DEMO million"
    "  principal cash receipts | 57.989200 | 75.123400 | 88.386050 | DEMO million"
    "  non-principal success cash | 34.130000 | 46.525500 | 55.633000 | DEMO million"
    "  investor receipts | 92.119200 | 121.648900 | 143.700550 | DEMO million"
    "  outstanding principal at horizon | 1.150000 | 8.520000 | 18.740000 | DEMO million"
    "  realized principal loss | 2.152500 | 9.980600 | 21.525350 | DEMO million"
    "  principal impairment probability | 16.000000 | 37.000000 | 60.000000 | percent"
    "  NPV at the declared physical-P hurdle | -18.717674 | 0.661828 | 15.440326 | DEMO million"
    "  negative-NPV probability | 28.000000 | 42.000000 | 60.000000 | percent"
    "  principal-loss ES95 | 19.074000 | 70.803000 | 90.000000 | DEMO million"
    "  principal-loss ES99 | 23.150000 | 90.000000 | 90.000000 | DEMO million"
    "  NPV-shortfall ES95 | 28.015930 | 81.033859 | 85.236300 | DEMO million"
    "  NPV-shortfall ES99 | 40.784017 | 85.236300 | 85.236300 | DEMO million"
    "  peak same-month gross funding need | 33.870000 | 35.688500 | 37.309000 | DEMO million"
    "  peak cumulative net outlay | 88.147600 | 94.533000 | 99.487400 | DEMO million"
    "  explicit external support in the unsupported core: zero in every declared path"
    "Ten-claim concentration table"
    "  pairwise loss correlations defined: 45 of 45"
    "  biological-process | 2.000000 | 13.000000 | 32.000000 | 0.262600 | 4.141500 | 11.056800"
    "  scale-up-commissioning | 2.000000 | 13.000000 | 32.000000 | 0.463000 | 4.842900 | 12.660000"
    "  supplier-media | 1.000000 | 12.000000 | 30.000000 | 0.106750 | 4.760700 | 12.695000"
    "  regulatory-qualification | 0.000000 | 11.000000 | 28.000000 | 0.000000 | 4.120200 | 11.200500"
    "  buyer-product-acceptance | 2.000000 | 13.000000 | 32.000000 | 0.359400 | 5.378100 | 14.075700"
    "no independence assumption is introduced"
    "VARIANT 1 - FULLY FUNDED FIRST-LOSS / MARKET-PRIORITY CLAIM"
    "  funded-first-loss-participation | first-loss residual | 0.000000 | 20.000000 | 20.000000 | 20.200000 | 36.760000 | 6.217500 | 16.000000/37.000000/60.000000 | -7.111619/-0.854614/4.117883 | 4.825000/4.825000/4.825000"
    "    distribution sources min/central/max - underlying principal cash: 8.000000/11.600000/14.400000 million\; non-principal success cash: 16.864000/25.160000/31.756000 million\; returned unused funded reserve: 0.000000/0.000000/0.000000 million"
    "    total distributions min/central/max: 24.864000/36.760000/46.156000 million\; realized principal loss min/central/max: 2.089500/6.217500/11.464850 million"
    "    principal-loss ES95 min/central/max: 17.814000/20.000000/20.000000 million\; ES99: 20.000000/20.000000/20.000000 million\; unresolved exposure min/central/max: 0.271250/2.182500/5.129500 million"
    "    negative-NPV probability min/central/max: 28.000000/42.000000/60.000000% \; NPV-shortfall ES95 min/central/max: 19.279305/20.200000/20.200000 million\; ES99: 20.200000/20.200000/20.200000 million"
    "  market-priority-claim | priority | 20.000000 | 100.000000 | 80.000000 | 80.800000 | 91.264900 | 3.763100 | 2.000000/17.000000/42.000000 | -25.733095/-13.926874/-5.274392 | 4.003747/4.069270/4.154239"
    "    distribution sources min/central/max - underlying principal cash: 49.989200/63.523400/73.986050 million\; non-principal success cash: 17.266000/21.365500/23.877000 million\; returned unused funded reserve: 1.496500/6.376000/12.697000 million"
    "    total distributions min/central/max: 75.375200/91.264900/102.766550 million\; realized principal loss min/central/max: 0.063000/3.763100/10.060500 million"
    "    principal-loss ES95 min/central/max: 1.260000/50.803000/70.000000 million\; ES99: 3.150000/70.000000/70.000000 million\; unresolved exposure min/central/max: 0.810050/6.337500/14.572300 million"
    "    negative-NPV probability min/central/max: 100.000000/100.000000/100.000000% \; NPV-shortfall ES95 min/central/max: 20.455134/69.507663/72.861678 million\; ES99: 31.502617/72.861678/72.861678 million"
    "VARIANT 2 - THIRTY-PERCENT FAILURE-CONTINGENT PARTIAL CREDIT"
    "  coverage fraction: 30.000000%"
    "  contractual maximum provider exposure: 30.000000 DEMO million"
    "  modeled maximum claim: 27.000000 DEMO million"
    "  Full-provider-performance economics before investor premium"
    "  full-performance external support cash | 0.645750 | 2.994180 | 6.457605 | DEMO million"
    "  total investor cash receipts including external support | 98.074740 | 124.643080 | 145.074685 | DEMO million"
    "  residual unprotected principal loss | 1.506750 | 6.986420 | 15.067745 | DEMO million"
    "  residual principal-loss ES95 | 13.351800 | 49.562100 | 63.000000 | DEMO million"
    "  residual principal-loss ES99 | 16.205000 | 63.000000 | 63.000000 | DEMO million"
    "  residual principal impairment probability (unchanged) | 16.000000 | 37.000000 | 60.000000 | percent"
    "  investor NPV before premium | -14.925982 | 2.699617 | 16.389832 | DEMO million"
    "  supported NPV-shortfall ES95 | 24.221747 | 66.577659 | 66.860554 | DEMO million"
    "  supported NPV-shortfall ES99 | 37.114993 | 66.860554 | 66.860554 | DEMO million"
    "  peak gross funding need before settlement (unchanged) | 33.870000 | 35.688500 | 37.309000 | DEMO million"
    "  Unsupported / full-provider-performance comparison"
    "  total investor cash receipts | 92.119200/121.648900/143.700550 | 98.074740/124.643080/145.074685"
    "  NPV-shortfall ES95 | 28.015930/81.033859/85.236300 | 24.221747/66.577659/66.860554"
    "  Supported values assume full provider performance and are before any investor premium."
    "  investor maximum non-negative premium: none"
    "  claim-only robust break-even floor: 5.307681 DEMO million"
    "  claim-only premium feasibility gap: 20.233663 DEMO million"
    "  provider all-in floor: 13.541350 DEMO million"
    "  total all-in catalytic gap: 28.467332 DEMO million"
    "  expected nominal payout | 0.645750 | 2.994180 | 6.457605 | DEMO million"
    "  nominal payout ES95 | 5.722200 | 21.240900 | 27.000000 | DEMO million"
    "  nominal payout ES99 | 6.945000 | 27.000000 | 27.000000 | DEMO million"
    "  positive-claim probability | 16.000000 | 37.000000 | 60.000000 | percent"
    "  provider default probability | 1.840000 | 4.510000 | 8.620000 | percent"
    "  actual support received PV | 0.439487 | 1.955945 | 4.159825 | DEMO million"
    "  expected unsecured exposure in default atoms | 0.000000 | 0.156477 | 0.449510 | DEMO million"
    "  investor counterparty-credit loss PV | 0.000000 | 0.081844 | 0.235112 | DEMO million"
    "  credit-stressed investor NPV before premium | -15.161094 | 2.617773 | 16.389832 | DEMO million"
    "  central counterparty-credit loss PV ES95 / ES99: 1.636882 / 6.621378 DEMO million"
    "  central claim/default correlation: 0.327111"
    "  robust minimum claim-PV delivery ratio: 0.870321"
    "  stressed total all-in catalytic gap: 28.702444 DEMO million"
    "Mechanically verified here: deterministic engine replay and zero reconciliation controls."
    "Empirical project or provider evidence supplied by this fixture: none."
    "Unknown: calibrated probabilities, forecast accuracy, market price, fair value"
    "Pooling, tranching, and protection do not improve projects or create operating cash"
    "  core maximum cash reconciliation error: 0.000000 DEMO million"
    "  funded-variant maximum monetary reconciliation error: 0.000000 DEMO million"
    "  protection maximum monetary reconciliation error: 0.000000 DEMO million"
    "  provider-price maximum monetary reconciliation error: 0.000000 DEMO million"
    "  provider-credit maximum monetary reconciliation error: 0.000000 DEMO million"
)

foreach(fragment IN LISTS required_fragments)
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "ten-claim instrument-family output is missing:\n${fragment}\n\n"
            "Complete output:\n${output}")
    endif()
endforeach()

string(REGEX MATCHALL "VARIANT [0-9]+ -" variant_headings "${output}")
list(LENGTH variant_headings variant_heading_count)
if(NOT variant_heading_count EQUAL 2)
    message(FATAL_ERROR
        "instrument-family output must contain exactly two variant headings, got ${variant_heading_count}")
endif()

set(negative_dir
    "${CMAKE_CURRENT_BINARY_DIR}/ten-claim-instrument-family-negative")
file(MAKE_DIRECTORY "${negative_dir}")
file(READ "${FIXTURE}/portfolio.cfg" portfolio_text)

string(REPLACE
    "scenario.1.factor_tag.1=no-declared-adverse-factor"
    "scenario.1.factor_tag.1=unexpected-sixth-risk"
    extra_factor_portfolio
    "${portfolio_text}")
file(WRITE "${negative_dir}/extra-factor-portfolio.cfg"
    "${extra_factor_portfolio}")
execute_process(
    COMMAND ${PROGRAM_COMMAND}
        "${negative_dir}/extra-factor-portfolio.cfg"
        "${FIXTURE}/ambiguity.cfg"
        "${FIXTURE}/success-participation.cfg"
        "${FIXTURE}/capital-stack.cfg"
        "${FIXTURE}/loss-protection.cfg"
        "${FIXTURE}/provider-price.cfg"
        "${FIXTURE}/provider-credit.cfg"
    RESULT_VARIABLE extra_factor_result
    OUTPUT_VARIABLE extra_factor_output
    ERROR_VARIABLE extra_factor_error
)
if(NOT extra_factor_result EQUAL 1)
    message(FATAL_ERROR
        "an unrecognized sixth risk tag must exit 1, got ${extra_factor_result}")
endif()
string(FIND "${extra_factor_error}"
    "unrecognized shared-risk tag" extra_factor_error_position)
if(extra_factor_error_position EQUAL -1)
    message(FATAL_ERROR
        "an unrecognized sixth risk tag must report its boundary")
endif()

string(REPLACE
    "portfolio.synthetic_inputs=true"
    "portfolio.synthetic_inputs=false"
    nonsynthetic_portfolio
    "${portfolio_text}")
file(WRITE "${negative_dir}/nonsynthetic-portfolio.cfg"
    "${nonsynthetic_portfolio}")
execute_process(
    COMMAND ${PROGRAM_COMMAND}
        "${negative_dir}/nonsynthetic-portfolio.cfg"
        "${FIXTURE}/ambiguity.cfg"
        "${FIXTURE}/success-participation.cfg"
        "${FIXTURE}/capital-stack.cfg"
        "${FIXTURE}/loss-protection.cfg"
        "${FIXTURE}/provider-price.cfg"
        "${FIXTURE}/provider-credit.cfg"
    RESULT_VARIABLE nonsynthetic_result
    OUTPUT_VARIABLE nonsynthetic_output
    ERROR_VARIABLE nonsynthetic_error
)
if(NOT nonsynthetic_result EQUAL 1)
    message(FATAL_ERROR
        "a non-synthetic input must exit 1, got ${nonsynthetic_result}")
endif()
string(FIND "${nonsynthetic_error}"
    "synthetic inputs only"
    nonsynthetic_error_position)
if(nonsynthetic_error_position EQUAL -1)
    message(FATAL_ERROR
        "a non-synthetic input must report the synthetic-only boundary")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND}
        "${FIXTURE}/portfolio.cfg"
        "${FIXTURE}/ambiguity.cfg"
        "${FIXTURE}/success-participation.cfg"
        "${FIXTURE}/capital-stack.cfg"
        "${FIXTURE}/loss-protection.cfg"
        "${FIXTURE}/provider-price.cfg"
    RESULT_VARIABLE missing_credit_result
    OUTPUT_VARIABLE missing_credit_output
    ERROR_VARIABLE missing_credit_error
)
if(NOT missing_credit_result EQUAL 2)
    message(FATAL_ERROR
        "a missing provider-credit input must exit 2, got ${missing_credit_result}")
endif()
string(FIND "${missing_credit_error}" "usage:" usage_position)
if(usage_position EQUAL -1)
    message(FATAL_ERROR
        "a missing provider-credit input must print usage on stderr")
endif()
