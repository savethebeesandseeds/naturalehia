# SPDX-License-Identifier: MIT

if(NOT DEFINED PROGRAM OR NOT DEFINED FIXTURE)
    message(FATAL_ERROR "PROGRAM and FIXTURE are required")
endif()

set(portfolio "${FIXTURE}/portfolio.cfg")
set(polytope "${FIXTURE}/event-polytope.cfg")
set(participation "${FIXTURE}/success-participation.cfg")
set(frontier "${FIXTURE}/frontier.cfg")

execute_process(
    COMMAND "${PROGRAM}" "${portfolio}" "${polytope}"
        "${participation}" "${frontier}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "capital-mobilization frontier fixture failed (${result}):\n${error_output}")
endif()

foreach(fragment IN ITEMS
        "SYNTHETIC ROBUST CAPITAL-MOBILIZATION FRONTIER"
        "Analysis basis"
        "portfolio label: Four-state frontier synthetic project cash paths"
        "currency label: DEMO"
        "monetary basis: constant synthetic monetary units at analysis close"
        "horizon: 24 months"
        "complete project-cash scenario count: 4"
        "common-success | lower=0.000000% | central=62.000000% | upper=100.000000%"
        "common-process-shock | Both projects are impaired by the declared shared process shock | lower=1.000000% | upper=10.000000% | members=common-loss"
        "scalable non-principal cash-source kinds: commercial"
        "legacy participation-file robust NPV target: 0.000000 DEMO million (does not bind this frontier unless repeated as a frontier mandate)"
        "Fixed instrument terms"
        "fully funded junior loss-absorbing layer [0,A]"
        "fully funded priority layer [A,K]"
        "market notional M=K-A: funded principal notional; it excludes additional pro-rata pool-cost calls"
        "q: contingent participation in declared scalable success cash, not a coupon, yield, or ownership percentage"
        "tested participation grid q: 0.250000, 0.892857, 0.932143, 0.967857, 0.971429"
        "tested catalytic first-loss grid A: 8.181818, 12.000000, 14.000000, 16.000000 DEMO million"
        "tested candidate count: 20 (maximum 1,024)"
        "structural work units: 9440 / 4000000 combined deterministic bound"
        "portfolio records R: 36 (cash=28, auxiliary=8)"
        "probability projection: 720 = candidates * scenarios * (scenarios + events + 1)"
        "cash-path rebuild: 8720 = candidates * (R + projects * scenarios * (horizon + 1) + 2 * scenarios * (horizon + 1))"
        "declared mandate count: 12"
        "Candidate 0 | q=0.250000 | A=8.181818 | market notional=11.818182 | feasible=no | nondominated=no"
        "aggregate fully funded NPV | minimum (robust)=-3.600000 | central=-1.800000 | maximum=-0.990000 DEMO million"
        "Candidate 5 | q=0.892857 | A=12.000000 | market notional=8.000000 | feasible=yes | nondominated=yes"
        "catalytic NPV | minimum (robust)=-0.420000"
        "market NPV | minimum (robust)=0.420000 | central=0.820000 | maximum=0.870000 DEMO million"
        "market robust NPV margin: 5.250000 percent of market notional"
        "expected market contributions | minimum=8.080000 | central=8.080000 | maximum=8.080000 DEMO million"
        "expected market total distributions | minimum=8.500000 | central=8.900000 | maximum=8.950000 DEMO million"
        "expected market principal cash | minimum=7.600000 | central=7.920000 | maximum=7.960000 DEMO million"
        "worst expected market principal loss: 5.000000 percent of market notional"
        "worst expected market principal loss: 5.000000 percent of market notional | 0.400000 DEMO million"
        "worst market principal-loss ES95: 50.000000 percent of market notional"
        "worst market principal-loss ES95: 50.000000 percent of market notional | 4.000000 DEMO million"
        "worst market principal-loss ES99: 50.000000 percent of market notional"
        "worst market principal impairment probability: 10.000000 percent"
        "worst market negative-NPV probability: 10.000000 percent"
        "worst market NPV-shortfall ES95: 51.000000 percent of market notional"
        "worst market NPV-shortfall ES95: 51.000000 percent of market notional | 4.080000 DEMO million"
        "worst market NPV-shortfall ES99: 51.000000 percent of market notional"
        "market principal-cash WAL | minimum=1.842105 | central=1.898990 | maximum=1.922111 years"
        "catalytic NPV concession: 0.420000 DEMO million"
        "Candidate 10 | q=0.932143 | A=14.000000 | market notional=6.000000 | feasible=yes | nondominated=yes"
        "Candidate 13 | q=0.967857 | A=12.000000 | market notional=8.000000 | feasible=yes | nondominated=yes"
        "catalytic NPV | minimum (robust)=0.000000"
        "Candidate 19 | q=0.971429 | A=16.000000 | market notional=4.000000 | feasible=no | nondominated=no"
        "worst expected market principal loss: 0.000000 percent of market notional"
        "worst market NPV-shortfall ES95: 1.000000 percent of market notional"
        "worst market NPV-shortfall ES95: 1.000000 percent of market notional | 0.040000 DEMO million"
        "maximum catalytic first loss: fail"
        "feasible candidate indices: 5,9,10,13,14,17,18"
        "nondominated feasible candidate indices: 5,9,10,13,14,18"
        "minimum tested feasible q: 0.892857"
        "q=0.892857 | candidate=5 | A=12.000000 DEMO million"
        "Nondominated endpoint witness ledger"
        "Each row retains that metric's own separately optimized measure. Different p or tail vectors are not one combined stress."
        "candidate 5 | aggregate fully funded NPV | minimum | value=-0.000000 | own p: common-loss=0.100000;common-success=0.500000;culture-loss-scaleup-success=0.200000;culture-success-scaleup-loss=0.200000"
        "candidate 5 | market expected contributions | minimum | value=8.080000 | own p:"
        "candidate 5 | market expected contributions | maximum | value=8.080000 | own p:"
        "candidate 5 | market expected total distributions | minimum | value=8.500000 | own p:"
        "candidate 5 | market expected total distributions | maximum | value=8.950000 | own p:"
        "candidate 5 | market principal-loss ES95 | maximum | value=4.000000 DEMO million | own p: common-loss=0.100000"
        "candidate 5 | market principal-loss ES99 | maximum | value=4.000000 DEMO million"
        "candidate 5 | market NPV-shortfall ES95 | maximum | value=4.080000 DEMO million"
        "candidate 5 | market NPV-shortfall ES99 | maximum | value=4.080000 DEMO million"
        "own tail mass: common-loss=0.050000"
        "candidate 5 | market principal-cash WAL | maximum | value=1.922111 | own common-measure p: common-loss=0.010000"
        "Aggregate numerical audit maxima across all candidates"
        "probability-constraint violation:"
        "tail-mass violation:"
        "WAL ratio reconciliation error:"
        "explicitly enumerated finite grid, not a continuous optimum"
        "Different metrics can have different adverse probability witnesses"
        "not a guarantee, insurance policy, or a debt characterization"
        "Pool costs are additional pro-rata calls included in reported contributions and NPV."
        "Every principal-loss and NPV-shortfall fraction uses funded principal notional M, not all-in contributions."
        "Physical expected principal loss is not IFRS 9 ECL, Basel regulatory EL, accounting impairment, or legal default."
        "Core model limitation: Finite synthetic physical-measure grid only."
        "No fair value, market price, spread, rating"
        "or actual capital mobilization is established"
        "weighted_score_or_continuous_optimum_is_claimed=false"
        "fair_value_or_market_price_is_estimated=false"
        "capital_mobilization_is_established=false"
        "calibrated_execution_authorized=false")
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "frontier output is missing:\n${fragment}\n\n${output}")
    endif()
endforeach()

# Scope the boundary candidate's mandate ledger and require all twelve declared
# decisions to be explicit passes.
string(FIND "${output}" "Candidate 5 |" candidate_five_start)
string(FIND "${output}" "Candidate 6 |" candidate_six_start)
if(candidate_five_start EQUAL -1 OR candidate_six_start EQUAL -1)
    message(FATAL_ERROR "could not delimit candidate 5")
endif()
math(EXPR candidate_five_length
    "${candidate_six_start} - ${candidate_five_start}")
string(SUBSTRING "${output}" ${candidate_five_start}
    ${candidate_five_length} candidate_five)
foreach(constraint IN ITEMS
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
        "maximum catalytic first loss: pass"
        "maximum catalytic NPV concession: pass")
    string(FIND "${candidate_five}" "${constraint}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "candidate 5 is missing a declared pass: ${constraint}")
    endif()
endforeach()

# Normalized output must contain all four reloadable inputs. Re-run it and
# prove the frontier semantic rendering is byte stable.
execute_process(
    COMMAND "${PROGRAM}" "${portfolio}" "${polytope}"
        "${participation}" "${frontier}" --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized frontier run failed (${normalized_result}):\n${normalized_error}")
endif()
foreach(fragment IN ITEMS
        "Normalized portfolio configuration"
        "portfolio.model_version=0.1.0"
        "Normalized event-probability-polytope configuration"
        "polytope.model_version=0.2.0"
        "Normalized success-participation configuration"
        "participation.model_version=0.1.0"
        "Normalized capital-mobilization-frontier configuration"
        "frontier.model_version=0.1.0"
        "participation_grid.1.fraction=0.25"
        "participation_grid.5.fraction=0.97142857142857142"
        "catalytic_first_loss_grid.1.amount_million=8.1818181818181817")
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
    "\nNormalized capital-mobilization-frontier configuration" participation_end)
string(FIND "${normalized_output}" "frontier.model_version=" frontier_start)
if(portfolio_start EQUAL -1 OR portfolio_end EQUAL -1 OR
        polytope_start EQUAL -1 OR polytope_end EQUAL -1 OR
        participation_start EQUAL -1 OR participation_end EQUAL -1 OR
        frontier_start EQUAL -1)
    message(FATAL_ERROR "could not delimit all four normalized frontier inputs")
endif()
math(EXPR portfolio_length "${portfolio_end} - ${portfolio_start}")
math(EXPR polytope_length "${polytope_end} - ${polytope_start}")
math(EXPR participation_length
    "${participation_end} - ${participation_start}")
string(SUBSTRING "${normalized_output}" ${portfolio_start}
    ${portfolio_length} normalized_portfolio)
string(SUBSTRING "${normalized_output}" ${polytope_start}
    ${polytope_length} normalized_polytope)
string(SUBSTRING "${normalized_output}" ${participation_start}
    ${participation_length} normalized_participation)
string(SUBSTRING "${normalized_output}" ${frontier_start} -1 normalized_frontier)

string(MD5 suffix "${PROGRAM}|${FIXTURE}")
set(temp_portfolio "${CMAKE_CURRENT_BINARY_DIR}/frontier-${suffix}-p.cfg")
set(temp_polytope "${CMAKE_CURRENT_BINARY_DIR}/frontier-${suffix}-e.cfg")
set(temp_participation "${CMAKE_CURRENT_BINARY_DIR}/frontier-${suffix}-q.cfg")
set(temp_frontier "${CMAKE_CURRENT_BINARY_DIR}/frontier-${suffix}-f.cfg")
file(WRITE "${temp_portfolio}" "${normalized_portfolio}\n")
file(WRITE "${temp_polytope}" "${normalized_polytope}\n")
file(WRITE "${temp_participation}" "${normalized_participation}\n")
file(WRITE "${temp_frontier}" "${normalized_frontier}\n")
execute_process(
    COMMAND "${PROGRAM}" "${temp_portfolio}" "${temp_polytope}"
        "${temp_participation}" "${temp_frontier}" --print-normalized
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_output
    ERROR_VARIABLE replay_error
)
if(NOT replay_result EQUAL 0)
    message(FATAL_ERROR
        "normalized frontier inputs did not reload (${replay_result}):\n${replay_error}")
endif()
string(FIND "${replay_output}" "frontier.model_version=" replay_frontier_start)
string(SUBSTRING "${replay_output}" ${replay_frontier_start} -1 replay_frontier)
if(NOT normalized_frontier STREQUAL replay_frontier)
    message(FATAL_ERROR "frontier normalized print-load-print is not byte stable")
endif()

# No feasible candidate is an economic result, not a process failure.
file(READ "${frontier}" no_feasible_terms)
string(REPLACE
    "mandate.minimum_robust_aggregate_npv_million=0"
    "mandate.minimum_robust_aggregate_npv_million=100"
    no_feasible_terms "${no_feasible_terms}")
set(no_feasible_frontier
    "${CMAKE_CURRENT_BINARY_DIR}/frontier-${suffix}-none.cfg")
file(WRITE "${no_feasible_frontier}" "${no_feasible_terms}")
execute_process(
    COMMAND "${PROGRAM}" "${portfolio}" "${polytope}"
        "${participation}" "${no_feasible_frontier}"
    RESULT_VARIABLE no_feasible_result
    OUTPUT_VARIABLE no_feasible_output
    ERROR_VARIABLE no_feasible_error
)
if(NOT no_feasible_result EQUAL 0)
    message(FATAL_ERROR
        "no-feasible frontier must exit zero (${no_feasible_result}):\n${no_feasible_error}")
endif()
foreach(fragment IN ITEMS
        "feasible candidate indices: none"
        "nondominated feasible candidate indices: none"
        "minimum tested feasible q: none"
        "least tested feasible A by q\n    none")
    string(FIND "${no_feasible_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "no-feasible report is missing: ${fragment}")
    endif()
endforeach()

# Exit taxonomy: 1 grammar, 2 input/parser, 3 compatible-input analysis.
execute_process(
    COMMAND "${PROGRAM}" "${portfolio}" "${polytope}" "${participation}"
    RESULT_VARIABLE usage_result
    ERROR_VARIABLE usage_error
)
if(NOT usage_result EQUAL 1)
    message(FATAL_ERROR "missing frontier terms must exit 1")
endif()
string(FIND "${usage_error}" "usage:" position)
if(position EQUAL -1)
    message(FATAL_ERROR "frontier grammar errors must print usage")
endif()

execute_process(
    COMMAND "${PROGRAM}" "${portfolio}" "${polytope}"
        "${participation}" "${frontier}" --unknown-option
    RESULT_VARIABLE option_result
    ERROR_VARIABLE option_error
)
if(NOT option_result EQUAL 1)
    message(FATAL_ERROR "unknown frontier options must exit 1")
endif()

execute_process(
    COMMAND "${PROGRAM}" "${portfolio}.missing" "${polytope}"
        "${participation}" "${frontier}"
    RESULT_VARIABLE input_result
    ERROR_VARIABLE input_error
)
if(NOT input_result EQUAL 2)
    message(FATAL_ERROR "frontier input/load errors must exit 2")
endif()
string(FIND "${input_error}"
    "capital-mobilization-frontier input/configuration failed:" position)
if(position EQUAL -1)
    message(FATAL_ERROR "frontier input failures need a scoped label")
endif()

file(READ "${frontier}" invalid_analysis_terms)
string(REPLACE
    "catalytic_first_loss_grid.3.amount_million=16"
    "catalytic_first_loss_grid.3.amount_million=20"
    invalid_analysis_terms "${invalid_analysis_terms}")
set(invalid_analysis_frontier
    "${CMAKE_CURRENT_BINARY_DIR}/frontier-${suffix}-analysis.cfg")
file(WRITE "${invalid_analysis_frontier}" "${invalid_analysis_terms}")
execute_process(
    COMMAND "${PROGRAM}" "${portfolio}" "${polytope}"
        "${participation}" "${invalid_analysis_frontier}"
    RESULT_VARIABLE analysis_result
    ERROR_VARIABLE analysis_error
)
if(NOT analysis_result EQUAL 3)
    message(FATAL_ERROR "frontier cross-input analysis errors must exit 3")
endif()
string(FIND "${analysis_error}"
    "capital-mobilization-frontier analysis failed:" position)
if(position EQUAL -1)
    message(FATAL_ERROR "frontier analysis failures need a scoped label")
endif()

foreach(error_text IN ITEMS "${usage_error}" "${option_error}"
        "${input_error}" "${analysis_error}")
    string(FIND "${error_text}" "calibrated_execution_authorized=false"
        position)
    if(position EQUAL -1)
        message(FATAL_ERROR "frontier errors must retain calibration boundary")
    endif()
endforeach()

file(REMOVE "${temp_portfolio}" "${temp_polytope}"
    "${temp_participation}" "${temp_frontier}" "${no_feasible_frontier}"
    "${invalid_analysis_frontier}")
