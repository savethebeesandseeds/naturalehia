# SPDX-License-Identifier: MIT

include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

if(NOT DEFINED PROGRAM OR NOT DEFINED PORTFOLIO OR NOT DEFINED POLYTOPE OR
        NOT DEFINED PARTICIPATION OR NOT DEFINED STACK OR
        NOT DEFINED AMBIGUITY)
    message(FATAL_ERROR
        "PROGRAM, PORTFOLIO, POLYTOPE, PARTICIPATION, STACK, and AMBIGUITY are required")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} --event-polytope "${PORTFOLIO}" "${POLYTOPE}"
        "${PARTICIPATION}" "${STACK}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "event-polytope capital-stack fixture failed (${result}):\n${error_output}")
endif()

foreach(fragment IN ITEMS
        "SYNTHETIC EVENT-CONSTRAINED FULLY FUNDED CAPITAL STACK"
        "fixed underlying success participation q: 1.000000"
        "declared robust underlying NPV target: 0.000000 DEMO million"
        "fixed-q worst expected underlying NPV: -0.800000 DEMO million"
        "fixed q meets target under every feasible event measure: no"
        "robust target gap: 0.800000 DEMO million"
        "Event-probability basis"
        "any-project-impairment | At least one project has principal impairment by the horizon | 30.000000% | 38.000000% | 50.000000%"
        "common-process-shock | Both projects are impaired by the declared shared process shock | 1.000000% | 2.000000% | 10.000000%"
        "underlying draw-as-needed expected NPV | -0.800000 | 1.400000 | 2.390000 | DEMO million"
        "prefunding drag | 0.000000 | 0.000000 | 0.000000 | DEMO million"
        "Tranche: first-loss-residual"
        "expected resolved principal loss at horizon | 1.200000 | 1.520000 | 2.000000 | DEMO million"
        "expected NPV at tranche hurdle | -0.540000 | 0.300000 | 0.860000 | DEMO million"
        "principal loss ES95 | 4.000000 | 4.000000 | 4.000000 | DEMO million"
        "Tranche: intermediate"
        "expected resolved principal loss at horizon | 1.220000 | 1.560000 | 2.200000 | DEMO million"
        "principal exhaustion probability | 1.000000 | 2.000000 | 10.000000 | percent"
        "principal loss ES95 | 4.400000 | 4.800000 | 6.000000 | DEMO million"
        "NPV shortfall ES95 | 2.860000 | 3.660000 | 6.060000 | DEMO million"
        "Tranche: senior"
        "robust expected-NPV hurdle met under every feasible event measure: yes"
        "expected resolved principal loss at horizon | 0.060000 | 0.120000 | 0.600000 | DEMO million"
        "principal loss ES95 | 1.200000 | 2.400000 | 6.000000 | DEMO million"
        "principal loss ES99 | 6.000000 | 6.000000 | 6.000000 | DEMO million"
        "principal cash-weighted average life (common-measure ratio) | 1.872340 | 1.919028 | 1.937626 | years"
        "Selected endpoint witness ledger"
        "pool underlying expected NPV | minimum | -0.800000 | p: common-loss=0.100000"
        "senior principal loss ES95 | maximum | 6.000000 | p: common-loss=0.100000"
        "senior WAL | minimum | 1.872340 | p: common-loss=0.100000"
        "maximum probability-constraint violation:"
        "maximum tail-mass violation:"
        "maximum WAL ratio reconciliation error:"
        "This candidate set is synthetic and uncalibrated."
        "fair_value_or_market_price_is_estimated=false"
        "legal_enforceability_is_validated=false"
        "ratings_or_regulatory_capital_are_validated=false"
        "Results are audited floating-point projections, not symbolic optima"
        "calibrated_execution_authorized=false")
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "event-polytope stack output is missing:\n${fragment}\n\n${output}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} --event-polytope "${PORTFOLIO}" "${POLYTOPE}"
        "${PARTICIPATION}" "${STACK}" --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized event-stack run failed (${normalized_result}):\n${normalized_error}")
endif()
foreach(fragment IN ITEMS
        "Normalized portfolio configuration"
        "portfolio.model_version=0.1.0"
        "Normalized event-probability-polytope configuration"
        "polytope.model_version=0.2.0"
        "Normalized success-participation configuration"
        "participation.model_version=0.1.0"
        "Normalized capital-stack configuration"
        "capital_stack.model_version=0.1.0")
    string(FIND "${normalized_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "normalized event-stack output is missing: ${fragment}")
    endif()
endforeach()

# Extract each semantic rendering and prove that the four printed inputs can be
# loaded together again. Headings, not comments, delimit the configurations.
string(FIND "${normalized_output}" "portfolio.model_version=" portfolio_start)
string(FIND "${normalized_output}"
    "\nNormalized event-probability-polytope configuration" portfolio_end)
string(FIND "${normalized_output}" "polytope.model_version=" polytope_start)
string(FIND "${normalized_output}"
    "\nNormalized success-participation configuration" polytope_end)
string(FIND "${normalized_output}" "participation.model_version=" participation_start)
string(FIND "${normalized_output}"
    "\nNormalized capital-stack configuration" participation_end)
string(FIND "${normalized_output}" "capital_stack.model_version=" stack_start)
if(portfolio_start EQUAL -1 OR portfolio_end EQUAL -1 OR
        polytope_start EQUAL -1 OR polytope_end EQUAL -1 OR
        participation_start EQUAL -1 OR participation_end EQUAL -1 OR
        stack_start EQUAL -1)
    message(FATAL_ERROR "could not delimit all four normalized configurations")
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
string(SUBSTRING "${normalized_output}" ${stack_start} -1 normalized_stack)
string(MD5 suffix "${PROGRAM}|${PORTFOLIO}|${POLYTOPE}|${STACK}")
set(temp_portfolio "${CMAKE_CURRENT_BINARY_DIR}/sp-${suffix}-p.cfg")
set(temp_polytope "${CMAKE_CURRENT_BINARY_DIR}/sp-${suffix}-e.cfg")
set(temp_participation "${CMAKE_CURRENT_BINARY_DIR}/sp-${suffix}-q.cfg")
set(temp_stack "${CMAKE_CURRENT_BINARY_DIR}/sp-${suffix}-s.cfg")
file(WRITE "${temp_portfolio}" "${normalized_portfolio}\n")
file(WRITE "${temp_polytope}" "${normalized_polytope}\n")
file(WRITE "${temp_participation}" "${normalized_participation}\n")
file(WRITE "${temp_stack}" "${normalized_stack}\n")
execute_process(
    COMMAND ${PROGRAM_COMMAND} --event-polytope "${temp_portfolio}"
        "${temp_polytope}" "${temp_participation}" "${temp_stack}"
    RESULT_VARIABLE reload_result
    OUTPUT_VARIABLE reload_output
    ERROR_VARIABLE reload_error
)
file(REMOVE "${temp_portfolio}" "${temp_polytope}"
    "${temp_participation}" "${temp_stack}")
if(NOT reload_result EQUAL 0)
    message(FATAL_ERROR
        "the four normalized configs did not reload (${reload_result}):\n${reload_error}")
endif()
string(FIND "${reload_output}" "robust target gap: 0.800000 DEMO million"
    reload_position)
if(reload_position EQUAL -1)
    message(FATAL_ERROR "normalized-config reload changed the instrument result")
endif()

# The opt-in mode must not alter the established positional v0.1 report.
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${STACK}"
    RESULT_VARIABLE legacy_result
    OUTPUT_VARIABLE legacy_output
    ERROR_VARIABLE legacy_error
)
if(NOT legacy_result EQUAL 0)
    message(FATAL_ERROR
        "legacy positional capital-stack mode changed (${legacy_result}):\n${legacy_error}")
endif()
foreach(fragment IN ITEMS
        "SYNTHETIC FULLY FUNDED CAPITAL STACK"
        "selected underlying success participation q: 1.000000"
        "Tranche: first-loss-residual"
        "principal loss ES95 | 4.000000 | 4.000000 | 4.000000 | DEMO million"
        "Lower senior loss is redistribution, not value creation.")
    string(FIND "${legacy_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "legacy output is missing: ${fragment}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} --event-polytope "${PORTFOLIO}" "${POLYTOPE}"
        "${PARTICIPATION}" "${STACK}" --unknown-option
    RESULT_VARIABLE unknown_result
    ERROR_VARIABLE unknown_error
)
if(NOT unknown_result EQUAL 2)
    message(FATAL_ERROR "unknown event-stack option must exit 2")
endif()
string(FIND "${unknown_error}" "usage:" usage_position)
if(usage_position EQUAL -1)
    message(FATAL_ERROR "event-stack grammar errors must print usage")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} --event-polytope "${PORTFOLIO}" "${POLYTOPE}"
        "${PARTICIPATION}"
    RESULT_VARIABLE missing_result
    ERROR_VARIABLE missing_error
)
if(NOT missing_result EQUAL 2)
    message(FATAL_ERROR "missing event-stack terms must exit 2")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} --joint-cohort "${PORTFOLIO}" "${PARTICIPATION}"
    RESULT_VARIABLE joint_grammar_result
    ERROR_VARIABLE joint_grammar_error
)
if(NOT joint_grammar_result EQUAL 2)
    message(FATAL_ERROR "existing joint-cohort grammar exit changed")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} --event-polytope "${PORTFOLIO}.missing"
        "${POLYTOPE}" "${PARTICIPATION}" "${STACK}"
    RESULT_VARIABLE runtime_result
    ERROR_VARIABLE runtime_error
)
if(NOT runtime_result EQUAL 1)
    message(FATAL_ERROR "event-stack load errors must exit 1")
endif()
string(FIND "${runtime_error}"
    "event-probability capital-stack analysis failed:" runtime_position)
if(runtime_position EQUAL -1)
    message(FATAL_ERROR "event-stack runtime errors need a mode-specific label")
endif()
string(FIND "${runtime_error}" "calibrated_execution_authorized=false"
    boundary_position)
if(boundary_position EQUAL -1)
    message(FATAL_ERROR "event-stack errors must retain the calibration boundary")
endif()
