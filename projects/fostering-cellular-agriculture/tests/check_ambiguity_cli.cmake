if(NOT DEFINED PROGRAM OR NOT DEFINED PORTFOLIO OR NOT DEFINED AMBIGUITY)
    message(FATAL_ERROR "PROGRAM, PORTFOLIO, and AMBIGUITY are required")
endif()

execute_process(
    COMMAND "${PROGRAM}" "${PORTFOLIO}" "${AMBIGUITY}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "probability-envelope fixture failed with ${result}:\n${error_output}")
endif()

# Exact hand table for the four fixed cash paths and the bounds:
# success .50-.70; each single loss .10-.25; common loss .01-.10.
set(required_fragments
    "SYNTHETIC PHYSICAL-PROBABILITY ENVELOPE ANALYSIS"
    "Not a forecast, fair value, market price, rating, term sheet, offering document, or investment recommendation."
    "  measure: physical P sensitivity over fixed joint cash paths"
    "  lower-bound sum: 0.70999999999999996"
    "  upper-bound sum: 1.3"
    "  common-success | 50.000000% | 62.000000% | 70.000000%"
    "  common-loss | 1.000000% | 2.000000% | 10.000000%"
    "  expected project draws | 20.000000 | 20.000000 | 20.000000 | DEMO million"
    "  expected investor receipts | 19.400000 | 21.600000 | 22.590000 | DEMO million"
    "  expected pool costs | 0.200000 | 0.200000 | 0.200000 | DEMO million"
    "  expected outstanding principal at horizon | 0.000000 | 0.000000 | 0.000000 | DEMO million"
    "  expected realized principal loss | 2.480000 | 3.200000 | 4.800000 | DEMO million"
    "  probability of any realized principal impairment | 30.000000 | 38.000000 | 50.000000 | percent"
    "  expected NPV at declared physical-P hurdle | -0.800000 | 1.400000 | 2.390000 | DEMO million"
    "  probability of negative NPV | 30.000000 | 38.000000 | 50.000000 | percent"
    "  principal-loss ES95 | 9.600000 | 11.200000 | 16.000000 | DEMO million"
    "  principal-loss ES99 | 16.000000 | 16.000000 | 16.000000 | DEMO million"
    "  NPV-shortfall ES95 | 7.400000 | 9.600000 | 16.200000 | DEMO million"
    "  NPV-shortfall ES99 | 16.200000 | 16.200000 | 16.200000 | DEMO million"
    "  expected peak same-month gross funding need | 20.200000 | 20.200000 | 20.200000 | DEMO million"
    "Underlying project financial ranges"
    "  project: culture-platform"
    "  expected receipts | 9.150000 | 10.800000 | 11.790000 | DEMO million"
    "  principal impairment probability | 11.000000 | 20.000000 | 35.000000 | percent"
    "Common-witness pool-loss ES95 attribution"
    "  culture-platform | 4.800000 | 5.600000 | 8.000000 | DEMO million"
    "  bioprocess-scaleup | 4.800000 | 5.600000 | 8.000000 | DEMO million"
    "These are additive attributions under three shared measures"
    "  commercial | 18.200000 | 20.800000 | 21.970000 | 18.200000 | 20.800000 | 21.970000 | DEMO million"
    "  recovery | 0.620000 | 0.800000 | 1.200000 | 0.620000 | 0.800000 | 1.200000 | DEMO million"
    "  expected realized principal loss | minimum | 2.480000 | common-loss=0.010000; common-success=0.700000; culture-loss-scaleup-success=0.145000; culture-success-scaleup-loss=0.145000"
    "  expected realized principal loss | maximum | 4.800000 | common-loss=0.100000; common-success=0.500000; culture-loss-scaleup-success=0.200000; culture-success-scaleup-loss=0.200000"
    "  maximum endpoint probability error: 0.000000"
    "  maximum central metric reconciliation error: 0.000000 DEMO million or raw probability units"
    "  ES95 maximum project-contribution reconciliation error: 0.000000 DEMO million"
    "Every endpoint has its own feasible witness."
    "Neither the central value nor its range is risk-neutral value, fair value, or an investable quote."
)

foreach(fragment IN LISTS required_fragments)
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "probability-envelope fixture output is missing:\n${fragment}\n\n"
            "Complete output:\n${output}")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" "${PORTFOLIO}" "${AMBIGUITY}"
        --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized probability-envelope run failed with "
        "${normalized_result}:\n${normalized_error}")
endif()
foreach(heading IN ITEMS
        "Normalized portfolio configuration"
        "Normalized probability-envelope configuration")
    string(FIND "${normalized_output}" "${heading}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "normalized output is missing: ${heading}")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" "${PORTFOLIO}" "${AMBIGUITY}" --unknown-option
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
    COMMAND "${PROGRAM}" "${PORTFOLIO}"
    RESULT_VARIABLE missing_ambiguity_result
    OUTPUT_VARIABLE missing_ambiguity_output
    ERROR_VARIABLE missing_ambiguity_error
)
if(NOT missing_ambiguity_result EQUAL 2)
    message(FATAL_ERROR
        "a missing envelope must exit 2, got ${missing_ambiguity_result}")
endif()
string(FIND "${missing_ambiguity_error}" "usage:"
    missing_ambiguity_usage_position)
if(missing_ambiguity_usage_position EQUAL -1)
    message(FATAL_ERROR "a missing envelope must print usage on stderr")
endif()
