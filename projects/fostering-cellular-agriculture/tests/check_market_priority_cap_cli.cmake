# SPDX-License-Identifier: MIT

include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

if(NOT DEFINED PROGRAM OR NOT DEFINED FIXTURE)
    message(FATAL_ERROR "PROGRAM and FIXTURE are required")
endif()

set(portfolio "${FIXTURE}/portfolio.cfg")
set(polytope "${FIXTURE}/event-polytope.cfg")
set(participation "${FIXTURE}/success-participation.cfg")
set(base_stack "${FIXTURE}/capital-stack.cfg")
set(priority_cap "${FIXTURE}/priority-cap.cfg")

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "market priority-cap fixture failed (${result}):\n${error_output}")
endif()

foreach(fragment IN ITEMS
        "SYNTHETIC ROBUST MARKET PRIORITY-CAP ADEQUACY TERM"
        "Finite physical-probability cash-priority sensitivity only"
        "Fixed structure and at-par meaning"
        "fixed underlying success participation q: 0.892857"
        "fixed junior first-loss capital A: 12.000000 DEMO million"
        "fixed aggregate commitment and stack detachment K: 20.000000 DEMO million"
        "fixed market principal notional M=K-A: 8.000000 DEMO million"
        "at par: M is subscribed as principal at month zero"
        "additional cost calls: pool costs are separate pro-rata investor contributions"
        "total market contributions can exceed M without changing principal notional"
        "varied term B: the market claim's lifetime priority allocation cap"
        "cash-transfer identity: increasing B can only transfer modeled non-principal cash"
        "base-reference B: 1.000000 DEMO million"
        "contractual ceiling for B: 1.000000 DEMO million"
        "tested finite B grid: 0.000000, 0.080000, 0.500000, 0.533333, 1.000000 DEMO million"
        "fixed junior annual physical-measure hurdle: 0.000000 percent"
        "fixed market annual physical-measure hurdle: 0.000000 percent"
        "Analysis basis"
        "portfolio label: Four-state frontier synthetic project cash paths"
        "complete project-cash scenario count: 4"
        "common-process-shock | Both projects are impaired by the declared shared process shock"
        "scalable non-principal cash-source kinds: commercial"
        "Finite-grid scope, raw records, and fixed eligibility"
        "tested candidate count C: 5 (maximum 1,024)"
        "raw portfolio cash records: 28"
        "raw portfolio auxiliary records: 8"
        "raw portfolio records R: 36 = cash + auxiliary"
        "probability work: 180 = C * S * (S + E + 1)"
        "cash-path work: 2180 = C * (R + N * S * (H + 1) + 2 * S * (H + 1))"
        "combined structural work: 2360 / 4000000"
        "fixed-structure declared mandate count: 7"
        "cap-sensitive market declared mandate count: 4"
        "junior concession mandate declared: yes"
        "fixed structure eligible across tested B: yes"
        "Declared mandates by economic role"
        "Fixed-structure eligibility mandates"
        "Cap-sensitive market adequacy mandates"
        "Junior concession mandate"
        "Every tested priority-cap candidate"
        "Candidate 0 | B=0.000000 | fixed market notional M=8.000000"
        "Candidate 1 | B=0.080000 | fixed market notional M=8.000000"
        "Candidate 2 | B=0.500000 | fixed market notional M=8.000000 | tags=previous-tested-before-market-adequate,previous-tested-before-balanced"
        "Candidate 3 | B=0.533333 | fixed market notional M=8.000000 | tags=minimum-tested-market-adequate,minimum-tested-balanced"
        "Candidate 4 | B=1.000000 | fixed market notional M=8.000000 | tags=base-reference,contractual-ceiling"
        "junior expected contributions | minimum=12.120000 | central=12.120000 | maximum=12.120000 DEMO million"
        "market expected contributions | minimum=8.080000 | central=8.080000 | maximum=8.080000 DEMO million"
        "market expected principal distributions | minimum=7.600000 | central=7.920000 | maximum=7.960000 DEMO million"
        "market NPV | minimum (robust)=0.000000"
        "worst expected market principal loss: 5.000000 percent of M"
        "worst market principal-loss ES95: 50.000000 percent of M | 4.000000 DEMO million"
        "worst market principal-loss ES99: 50.000000 percent of M | 4.000000 DEMO million"
        "worst market principal impairment probability: 10.000000 percent"
        "worst market negative-NPV probability: 10.000000 percent"
        "worst market NPV-shortfall ES95: 51.000000 percent of M | 4.080000 DEMO million"
        "worst market NPV-shortfall ES99: 51.000000 percent of M | 4.080000 DEMO million"
        "market principal-cash WAL | minimum=1.842105 | central=1.898990 | maximum=1.922111 years"
        "market NPV | minimum (robust)=0.420000"
        "junior NPV | minimum (robust)=-0.420000"
        "junior NPV concession=max(0,target-robust NPV): 0.420000 DEMO million"
        "Priority-cap selection and tested brackets"
        "status: minimum-tested-balanced-cap-found"
        "market-adequate candidate indices: 3,4"
        "balanced candidate indices: 3,4"
        "minimum tested market-adequate B: candidate 3 | B=0.533333 DEMO million"
        "previous tested B before market adequacy: candidate 2 | B=0.500000 DEMO million"
        "minimum tested balanced B: candidate 3 | B=0.533333 DEMO million"
        "previous tested B before balance: candidate 2 | B=0.500000 DEMO million"
        "base-reference candidate: 4 | B=1.000000 DEMO million"
        "contractual-ceiling candidate: 4 | B=1.000000 DEMO million"
        "minimum means only the lowest passing explicitly tested B"
        "Every-candidate endpoint witness ledger"
        "candidate 3 | market principal-loss ES95 | maximum | value=4.000000"
        "candidate 3 | market NPV-shortfall ES95 | maximum | value=4.080000 DEMO million"
        "candidate 3 | market NPV-shortfall ES99 | maximum | value=4.080000 DEMO million"
        "candidate 3 | market principal-cash WAL | maximum | value=1.922111"
        "Across-grid structural, principal, and transfer audit"
        "base stack was not mutated: true"
        "market contributions invariant: true"
        "market principal cash invariant: true"
        "market principal risk invariant: true"
        "market principal WAL invariant: true"
        "market non-principal cash nondecreasing: true"
        "junior non-principal cash nonincreasing: true"
        "market cash gained equals junior cash surrendered: true"
        "aggregate cash invariant: true"
        "pool-hurdle NPV invariant: true"
        "Interpretation boundary and false-claim ledger"
        "Only B varies."
        "Principal-loss metrics and WAL are structurally invariant to B"
        "Core model limitation: Finite synthetic physical-measure cap grid only."
        "continuous_minimum_or_optimized_contract_is_claimed=false"
        "market_hurdle_is_solved_or_empirically_calibrated=false"
        "expected_investor_return_or_annualized_yield_is_estimated=false"
        "fair_value_issue_price_or_market_spread_is_estimated=false"
        "investor_demand_or_suitability_is_established=false"
        "legal_form_enforceability_or_regulatory_treatment_is_validated=false"
        "capital_mobilization_or_crowding_in_is_established=false"
        "calibrated_execution_authorized=false")
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "priority-cap output is missing:\n${fragment}\n\n${output}")
    endif()
endforeach()

# Every tested cap must be present in both the economic ledger and witness
# ledger. This also guards against reporting only the selected candidate.
foreach(index RANGE 0 4)
    string(FIND "${output}" "Candidate ${index} | B=" candidate_position)
    string(FIND "${output}"
        "candidate ${index} | aggregate fully funded NPV | minimum"
        witness_position)
    if(candidate_position EQUAL -1 OR witness_position EQUAL -1)
        message(FATAL_ERROR
            "candidate ${index} is missing from the ledger or witnesses")
    endif()
endforeach()

# Scope B=1 and require the complete twelve-mandate decision ledger plus the
# exact opposing market/junior NPV transfer.
string(FIND "${output}" "Candidate 4 |" candidate_four_start)
string(FIND "${output}" "Priority-cap selection" candidate_four_end)
if(candidate_four_start EQUAL -1 OR candidate_four_end EQUAL -1)
    message(FATAL_ERROR "could not delimit the B=1 candidate")
endif()
math(EXPR candidate_four_length
    "${candidate_four_end} - ${candidate_four_start}")
string(SUBSTRING "${output}" ${candidate_four_start}
    ${candidate_four_length} candidate_four)
foreach(fragment IN ITEMS
        "market NPV | minimum (robust)=0.420000"
        "junior NPV | minimum (robust)=-0.420000"
        "minimum robust aggregate NPV: pass"
        "minimum market robust NPV margin: pass"
        "maximum market expected principal loss: pass"
        "maximum market principal-loss ES95: pass"
        "maximum market principal-loss ES99: pass"
        "maximum market principal impairment probability: pass"
        "maximum market negative-NPV probability: pass"
        "maximum market NPV-shortfall ES95: pass"
        "maximum market NPV-shortfall ES99: pass"
        "maximum market WAL: pass"
        "maximum junior first loss A: pass"
        "maximum junior NPV concession: pass")
    string(FIND "${candidate_four}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "B=1 ledger is missing: ${fragment}")
    endif()
endforeach()

# Normalized output must expose all five reloadable files. Extract the five
# configurations, reload them together, and require byte-stable normalized
# rendering across the complete five-file suffix.
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized priority-cap run failed (${normalized_result}):\n${normalized_error}")
endif()
foreach(fragment IN ITEMS
        "Normalized portfolio configuration"
        "portfolio.model_version=0.1.0"
        "Normalized event-probability-polytope configuration"
        "polytope.model_version=0.2.0"
        "Normalized success-participation configuration"
        "participation.model_version=0.1.0"
        "Normalized base-capital-stack configuration"
        "capital_stack.model_version=0.1.0"
        "capital_stack.underlying_success_participation_fraction=0.8928571428571429"
        "Normalized robust-market-priority-cap configuration"
        "priority_cap.model_version=0.1.0"
        "market_priority_cap_grid.count=5"
        "market_priority_cap_grid.1.amount_million=0"
        "market_priority_cap_grid.5.amount_million=1")
    string(FIND "${normalized_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "normalized output is missing: ${fragment}")
    endif()
endforeach()

string(FIND "${normalized_output}" "portfolio.model_version=" portfolio_start)
string(FIND "${normalized_output}"
    "\nNormalized event-probability-polytope configuration" portfolio_end)
string(FIND "${normalized_output}" "polytope.model_version=" polytope_start)
string(FIND "${normalized_output}"
    "\nNormalized success-participation configuration" polytope_end)
string(FIND "${normalized_output}" "participation.model_version=" participation_start)
string(FIND "${normalized_output}"
    "\nNormalized base-capital-stack configuration" participation_end)
string(FIND "${normalized_output}" "capital_stack.model_version=" stack_start)
string(FIND "${normalized_output}"
    "\nNormalized robust-market-priority-cap configuration" stack_end)
string(FIND "${normalized_output}" "priority_cap.model_version=" cap_start)
if(portfolio_start EQUAL -1 OR portfolio_end EQUAL -1 OR
        polytope_start EQUAL -1 OR polytope_end EQUAL -1 OR
        participation_start EQUAL -1 OR participation_end EQUAL -1 OR
        stack_start EQUAL -1 OR stack_end EQUAL -1 OR cap_start EQUAL -1)
    message(FATAL_ERROR
        "could not delimit all five normalized priority-cap inputs")
endif()

math(EXPR portfolio_length "${portfolio_end} - ${portfolio_start}")
math(EXPR polytope_length "${polytope_end} - ${polytope_start}")
math(EXPR participation_length
    "${participation_end} - ${participation_start}")
math(EXPR stack_length "${stack_end} - ${stack_start}")
string(SUBSTRING "${normalized_output}" ${portfolio_start}
    ${portfolio_length} normalized_portfolio)
string(SUBSTRING "${normalized_output}" ${polytope_start}
    ${polytope_length} normalized_polytope)
string(SUBSTRING "${normalized_output}" ${participation_start}
    ${participation_length} normalized_participation)
string(SUBSTRING "${normalized_output}" ${stack_start}
    ${stack_length} normalized_stack)
string(SUBSTRING "${normalized_output}" ${cap_start} -1 normalized_cap)
string(SUBSTRING "${normalized_output}" ${portfolio_start} -1 normalized_suffix)

string(MD5 suffix "${PROGRAM}|${FIXTURE}")
set(temp_portfolio
    "${CMAKE_CURRENT_BINARY_DIR}/priority-cap-${suffix}-p.cfg")
set(temp_polytope
    "${CMAKE_CURRENT_BINARY_DIR}/priority-cap-${suffix}-e.cfg")
set(temp_participation
    "${CMAKE_CURRENT_BINARY_DIR}/priority-cap-${suffix}-q.cfg")
set(temp_stack
    "${CMAKE_CURRENT_BINARY_DIR}/priority-cap-${suffix}-s.cfg")
set(temp_cap
    "${CMAKE_CURRENT_BINARY_DIR}/priority-cap-${suffix}-b.cfg")
file(WRITE "${temp_portfolio}" "${normalized_portfolio}\n")
file(WRITE "${temp_polytope}" "${normalized_polytope}\n")
file(WRITE "${temp_participation}" "${normalized_participation}\n")
file(WRITE "${temp_stack}" "${normalized_stack}\n")
file(WRITE "${temp_cap}" "${normalized_cap}\n")
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${temp_portfolio}" "${temp_polytope}"
        "${temp_participation}" "${temp_stack}" "${temp_cap}"
        --print-normalized
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_output
    ERROR_VARIABLE replay_error
)
if(NOT replay_result EQUAL 0)
    message(FATAL_ERROR
        "normalized priority-cap inputs did not reload (${replay_result}):\n${replay_error}")
endif()
string(FIND "${replay_output}" "portfolio.model_version=" replay_start)
if(replay_start EQUAL -1)
    message(FATAL_ERROR "replay omitted its normalized five-file suffix")
endif()
string(SUBSTRING "${replay_output}" ${replay_start} -1 replay_suffix)
if(NOT normalized_suffix STREQUAL replay_suffix)
    message(FATAL_ERROR
        "priority-cap normalized five-file print-load-print is not byte stable")
endif()

# Economic no-solution statuses are successful reports, not process errors.
file(READ "${priority_cap}" no_adequate_terms)
string(REPLACE
    "mandate.minimum_market_robust_npv_margin_fraction=0"
    "mandate.minimum_market_robust_npv_margin_fraction=1"
    no_adequate_terms "${no_adequate_terms}")
set(no_adequate_cap
    "${CMAKE_CURRENT_BINARY_DIR}/priority-cap-${suffix}-none.cfg")
file(WRITE "${no_adequate_cap}" "${no_adequate_terms}")
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${no_adequate_cap}"
    RESULT_VARIABLE no_adequate_result
    OUTPUT_VARIABLE no_adequate_output
    ERROR_VARIABLE no_adequate_error
)
if(NOT no_adequate_result EQUAL 0)
    message(FATAL_ERROR
        "no-adequate result must exit zero (${no_adequate_result}):\n${no_adequate_error}")
endif()
foreach(fragment IN ITEMS
        "status: no-tested-market-adequate-cap"
        "market-adequate candidate indices: none"
        "balanced candidate indices: none"
        "minimum tested market-adequate B: none"
        "minimum tested balanced B: none")
    string(FIND "${no_adequate_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "no-adequate report is missing: ${fragment}")
    endif()
endforeach()

file(READ "${priority_cap}" fixed_ineligible_terms)
string(REPLACE
    "mandate.minimum_robust_aggregate_npv_million=0"
    "mandate.minimum_robust_aggregate_npv_million=100"
    fixed_ineligible_terms "${fixed_ineligible_terms}")
set(fixed_ineligible_cap
    "${CMAKE_CURRENT_BINARY_DIR}/priority-cap-${suffix}-fixed.cfg")
file(WRITE "${fixed_ineligible_cap}" "${fixed_ineligible_terms}")
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${fixed_ineligible_cap}"
    RESULT_VARIABLE fixed_ineligible_result
    OUTPUT_VARIABLE fixed_ineligible_output
    ERROR_VARIABLE fixed_ineligible_error
)
if(NOT fixed_ineligible_result EQUAL 0)
    message(FATAL_ERROR
        "fixed-ineligible result must exit zero (${fixed_ineligible_result}):\n${fixed_ineligible_error}")
endif()
string(FIND "${fixed_ineligible_output}"
    "status: fixed-structure-ineligible" position)
if(position EQUAL -1)
    message(FATAL_ERROR "fixed-ineligible report has the wrong status")
endif()

# Strict exit taxonomy: 1 command grammar, 2 load/parser,
# 3 cross-input/core/report-output.
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}"
    RESULT_VARIABLE usage_result
    ERROR_VARIABLE usage_error
)
if(NOT usage_result EQUAL 1)
    message(FATAL_ERROR "missing priority-cap terms must exit 1")
endif()
string(FIND "${usage_error}" "usage:" position)
if(position EQUAL -1)
    message(FATAL_ERROR "priority-cap grammar errors must print usage")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        --unknown-option
    RESULT_VARIABLE option_result
    ERROR_VARIABLE option_error
)
if(NOT option_result EQUAL 1)
    message(FATAL_ERROR "unknown priority-cap options must exit 1")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}.missing" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
    RESULT_VARIABLE input_result
    ERROR_VARIABLE input_error
)
if(NOT input_result EQUAL 2)
    message(FATAL_ERROR "priority-cap input/load errors must exit 2")
endif()
string(FIND "${input_error}"
    "market-priority-cap input/configuration failed:" position)
if(position EQUAL -1)
    message(FATAL_ERROR "priority-cap input failures need a scoped label")
endif()

file(READ "${priority_cap}" invalid_analysis_terms)
string(REPLACE
    "priority_cap.market_claim_id=market-priority"
    "priority_cap.market_claim_id=missing-market-claim"
    invalid_analysis_terms "${invalid_analysis_terms}")
set(invalid_analysis_cap
    "${CMAKE_CURRENT_BINARY_DIR}/priority-cap-${suffix}-analysis.cfg")
file(WRITE "${invalid_analysis_cap}" "${invalid_analysis_terms}")
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${invalid_analysis_cap}"
    RESULT_VARIABLE analysis_result
    ERROR_VARIABLE analysis_error
)
if(NOT analysis_result EQUAL 3)
    message(FATAL_ERROR "priority-cap cross-input errors must exit 3")
endif()
string(FIND "${analysis_error}"
    "market-priority-cap analysis failed:" position)
if(position EQUAL -1)
    message(FATAL_ERROR "priority-cap analysis failures need a scoped label")
endif()

# Close the report pipe without consuming it. The fixture is intentionally
# larger than a platform pipe buffer, so the writer must observe the broken
# destination, report a scoped analysis failure, and exit 3 rather than
# silently returning success with an incomplete report.
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
    COMMAND "${CMAKE_COMMAND}" -E false
    RESULTS_VARIABLE report_output_results
    ERROR_VARIABLE report_output_error
)
if(NOT report_output_results STREQUAL "3;1")
    message(FATAL_ERROR
        "failed priority-cap report output must produce pipeline results 3;1, got ${report_output_results}")
endif()
string(FIND "${report_output_error}"
    "market-priority-cap analysis failed: failed while writing market-priority-cap report"
    position)
if(position EQUAL -1)
    message(FATAL_ERROR
        "priority-cap report-output failures need the scoped writer diagnostic")
endif()

foreach(error_text IN ITEMS "${usage_error}" "${option_error}"
        "${input_error}" "${analysis_error}" "${report_output_error}")
    string(FIND "${error_text}" "calibrated_execution_authorized=false"
        position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "priority-cap errors must retain the calibration boundary")
    endif()
endforeach()

file(REMOVE "${temp_portfolio}" "${temp_polytope}"
    "${temp_participation}" "${temp_stack}" "${temp_cap}"
    "${no_adequate_cap}" "${fixed_ineligible_cap}"
    "${invalid_analysis_cap}")
