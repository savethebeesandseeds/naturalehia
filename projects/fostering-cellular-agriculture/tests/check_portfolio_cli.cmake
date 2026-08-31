include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

if(NOT DEFINED PROGRAM OR NOT DEFINED SCENARIO)
    message(FATAL_ERROR "PROGRAM and SCENARIO are required")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${SCENARIO}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "participation-pool fixture failed with ${result}:\n${error_output}")
endif()

# These exact values describe the intended four-state synthetic fixture:
# two 10-million commitments, 20 drawn in every state, a 0.2 pool cost,
# success receipts of 13, loss recoveries of 2, and weights 0.62/0.18/0.18/0.02.
# Project and scenario IDs are intentionally not fixed here so the fixture can
# use descriptive names without weakening the economic checks.
set(required_fragments
    "SYNTHETIC PARTICIPATION-POOL ANALYSIS"
    "Not a price, fair value, term sheet, offering document, investment recommendation, or claim of validated performance."
    "  portfolio commitment: 20.000000 DEMO million"
    "  expected draws: 20.000000 DEMO million"
    "  expected outstanding principal at horizon: 0.000000 DEMO million"
    "  expected realized principal loss: 3.200000 DEMO million"
    "  probability of any realized principal impairment: 38.000000%"
    "  pool principal-loss ES95: 11.200000 DEMO million"
    "  pool principal-loss ES99: 16.000000 DEMO million"
    "  expected NPV at declared physical-P hurdle: 1.400000 DEMO million"
    "  probability of negative NPV: 38.000000%"
    "    mean=3.200000 p50=0.000000 p95=8.000000 p99=16.000000 max=16.000000"
    "    ES95=11.200000 ES99=16.000000"
    "  same-month gross funding need before receipts (DEMO million)"
    "    mean=20.200000 p50=20.200000 p95=20.200000 p99=20.200000 max=20.200000"
    "  commercial | 20.800000 | 20.800000"
    "  recovery | 0.800000 | 0.800000"
    "  ES95 | 16.000000 | 11.200000 | 4.800000 | 30.000000%"
    "  ES99 | 16.000000 | 16.000000 | 0.000000 | 0.000000%"
    "  project | stage | commitment | expected draws | expected receipts | expected outstanding"
    " | -0.125000"
    "| 0.000000 | 0.000000 | 5.800000 | 20.200000 |"
    "| 0.000000 | 8.000000 | -5.200000 | 20.200000 |"
    "| 0.000000 | 16.000000 | -16.200000 | 20.200000 |"
    "  maximum cash reconciliation error: 0.000000 DEMO million"
    "  maximum loss-layer reconciliation error: 0.000000 DEMO million"
    "Expected NPV is a physical-P sensitivity using analyst-declared joint weights and hurdle."
    "Outstanding principal belongs to continuing paths and is exposure at the horizon, not realized loss."
)

if(DEFINED EXTRA_REQUIRED_FRAGMENT)
    list(APPEND required_fragments "${EXTRA_REQUIRED_FRAGMENT}")
endif()

foreach(fragment IN LISTS required_fragments)
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "participation-pool fixture output is missing:\n${fragment}\n\n"
            "Complete output:\n${output}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${SCENARIO}" --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized participation-pool run failed with "
        "${normalized_result}:\n${normalized_error}")
endif()
string(FIND "${normalized_output}" "Normalized configuration"
    normalized_heading_position)
if(normalized_heading_position EQUAL -1)
    message(FATAL_ERROR
        "--print-normalized did not emit the normalized configuration heading")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${SCENARIO}" --unknown-option
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
    COMMAND ${PROGRAM_COMMAND}
    RESULT_VARIABLE missing_scenario_result
    OUTPUT_VARIABLE missing_scenario_output
    ERROR_VARIABLE missing_scenario_error
)
if(NOT missing_scenario_result EQUAL 2)
    message(FATAL_ERROR
        "a missing scenario must exit 2, got ${missing_scenario_result}")
endif()
string(FIND "${missing_scenario_error}" "usage:"
    missing_scenario_usage_position)
if(missing_scenario_usage_position EQUAL -1)
    message(FATAL_ERROR "a missing scenario must print usage on stderr")
endif()
