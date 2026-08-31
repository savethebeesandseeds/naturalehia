include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

if(NOT DEFINED PROGRAM OR NOT DEFINED PORTFOLIO OR
        NOT DEFINED AMBIGUITY OR NOT DEFINED PARTICIPATION)
    message(FATAL_ERROR
        "PROGRAM, PORTFOLIO, AMBIGUITY, and PARTICIPATION are required")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}" "${PARTICIPATION}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "success-participation fixture failed with ${result}:\n${error_output}")
endif()

# Hand calculation for the four fixed cash paths. The selected commercial
# payoff is 3 per successful project and q is confined to [0,1]. The robust
# lower NPV is -5.00 + 4.20q, so robust break-even would require q > 1.
set(required_fragments
    "SYNTHETIC SUCCESS-PARTICIPATION TERM ANALYSIS"
    "Not a forecast, fair value, market price, rating, term sheet, offering document, or investment recommendation."
    "  measure: robust physical P NPV over fixed joint cash paths"
    "  contractual rate domain: 0.000000 to 1.000000"
    "  selected scalable source kinds: commercial"
    "  payoff rule: principal component + q * selected configured non-principal receipt"
    "  robust target NPV: 0.000000 DEMO million"
    "  full-q nominal payoff | 4.200000 | 4.800000 | 5.070000 | DEMO million"
    "  full-q present-value payoff | 4.200000 | 4.800000 | 5.070000 | DEMO million"
    "  q=0 selected participation off | -5.000000 | -3.400000 | -2.680000 | DEMO million"
    "  q=1 configured full participation | -0.800000 | 1.400000 | 2.390000 | DEMO million"
    "  robust break-even inside contractual domain: no"
    "  robust NPV gap at maximum q: 0.800000 DEMO million"
    "  decision: reject this modeled term as economically insufficient at the robust target"
    "  central-measure threshold q (context only): 0.708333"
    "  expected realized principal loss | 2.480000 | 3.200000 | 4.800000 | DEMO million"
    "  probability of any realized principal impairment | 30.000000 | 38.000000 | 50.000000 | percent"
    "  commercial | 4.200000 | 4.800000 | 5.070000 | 4.200000 | 4.800000 | 5.070000 | DEMO million"
    "  common-loss | -16.200000 | 0.000000 | 0.000000 | -16.200000 | -16.200000 | DEMO million"
    "  common-success | -0.200000 | 6.000000 | 6.000000 | 5.800000 | 5.800000 | DEMO million"
    "  culture-loss-scaleup-success | -8.200000 | 3.000000 | 3.000000 | -5.200000 | -5.200000 | DEMO million"
    "  q=1 | -0.800000 | common-loss=0.100000; common-success=0.500000; culture-loss-scaleup-success=0.200000; culture-success-scaleup-loss=0.200000"
    "  maximum q=1 cash reconstruction error: 0.000000 DEMO million"
    "  maximum principal-loss reconciliation error: 0.000000 DEMO million"
    "  maximum source-capacity violation: 0.000000 DEMO million"
    "  maximum witness reconciliation error: 0.000000 DEMO million"
    "Unused scenario cash-source capacity is not an investor asset"
    "The central-measure threshold is context, not the conservative contract answer."
)

foreach(fragment IN LISTS required_fragments)
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "success-participation fixture output is missing:\n${fragment}\n\n"
            "Complete output:\n${output}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}" "${PARTICIPATION}"
        --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized success-participation run failed with "
        "${normalized_result}:\n${normalized_error}")
endif()
foreach(heading IN ITEMS
        "Normalized portfolio configuration"
        "Normalized probability-envelope configuration"
        "Normalized success-participation configuration")
    string(FIND "${normalized_output}" "${heading}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "normalized output is missing: ${heading}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}" "${PARTICIPATION}"
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
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}"
    RESULT_VARIABLE missing_term_result
    OUTPUT_VARIABLE missing_term_output
    ERROR_VARIABLE missing_term_error
)
if(NOT missing_term_result EQUAL 2)
    message(FATAL_ERROR
        "a missing participation term must exit 2, got ${missing_term_result}")
endif()
string(FIND "${missing_term_error}" "usage:" missing_term_usage_position)
if(missing_term_usage_position EQUAL -1)
    message(FATAL_ERROR "a missing participation term must print usage on stderr")
endif()
