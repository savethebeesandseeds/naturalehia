include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

if(NOT DEFINED PROGRAM OR NOT DEFINED PORTFOLIO OR
        NOT DEFINED AMBIGUITY OR NOT DEFINED PARTICIPATION OR
        NOT DEFINED PROTECTION)
    message(FATAL_ERROR
        "PROGRAM, PORTFOLIO, AMBIGUITY, PARTICIPATION, and PROTECTION are required")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${PROTECTION}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "pooled-loss-protection fixture failed with ${result}:\n${error_output}")
endif()

# Hand calculation for q=1 and terminal proportional protection. The robust
# investor NPV before premium is -0.8 + 4.8g, so g=1/6 reaches zero. The
# provider claim-only robust floor is 4.8g=0.8 while investor premium headroom
# is zero: no bilateral non-negative price exists without 0.8 of catalytic
# value before provider costs.
set(required_fragments
    "SYNTHETIC POOLED PRINCIPAL-LOSS PROTECTION ANALYSIS"
    "Not a forecast, fair value, market price, rating, term sheet, offering document, or investment recommendation."
    "  provider id: synthetic-catalytic-provider"
    "  underlying success-participation fraction q: 1.000000"
    "  reference amount: final resolved principal loss after in-horizon recovery"
    "  claim rule: coverage fraction g * gross pool principal loss"
    "  gross project loss remains visible: yes"
    "  continuing exposure is a covered realized loss: no"
    "  solver status: certified-interior-bracket"
    "  investor robust NPV target: 0.000000 DEMO million"
    "  aggregate contractual reference principal: 20.000000 DEMO million"
    "  legal support cap: 3.333333 DEMO million"
    "  maximum supported coverage fraction: 0.166667"
    "  reported coverage fraction: 0.166667"
    "  g=0 no protection | -0.800000 | 1.400000 | 2.390000 | DEMO million"
    "  reported g | 0.000000 | 1.933333 | 2.803333 | DEMO million"
    "  investor signed premium headroom: 0.000000 DEMO million"
    "  investor maximum non-negative premium: 0.000000 DEMO million"
    "  provider claim-only robust break-even floor: 0.800000 DEMO million"
    "  premium feasibility gap: 0.800000 DEMO million"
    "  robust non-negative bilateral price interval exists: no"
    "  at investor ceiling premium: 0.000000 DEMO million"
    "    provider robust NPV after premium: -0.800000 DEMO million"
    "  at provider floor premium: 0.800000 DEMO million"
    "    investor robust NPV after premium: -0.800000 DEMO million"
    "  expected nominal claim | 0.413333 | 0.533333 | 0.800000 | DEMO million"
    "  probability of a positive claim | 30.000000 | 38.000000 | 50.000000 | percent"
    "  nominal claim expected shortfall 95% | 1.600000 | 1.866667 | 2.666667 | DEMO million"
    "  nominal claim expected shortfall 99% | 2.666667 | 2.666667 | 2.666667 | DEMO million"
    "  contractual maximum exposure: 3.333333 DEMO million"
    "  largest claim in modeled scenarios: 2.666667 DEMO million"
    "  common-loss | -16.200000 | 16.000000 | 2.666667 | 13.333333 | -13.533333 | DEMO million"
    "  investor minimum NPV before premium | 0.000000 | common-loss=0.100000; common-success=0.500000; culture-loss-scaleup-success=0.200000; culture-success-scaleup-loss=0.200000"
    "  provider maximum expected claim PV | 0.800000 | common-loss=0.100000; common-success=0.500000; culture-loss-scaleup-success=0.200000; culture-success-scaleup-loss=0.200000"
    "  maximum underlying gross-loss change: 0.000000 DEMO million"
    "  maximum project-to-pool claim error: 0.000000 DEMO million"
    "  maximum support-cap violation: 0.000000 DEMO million"
    "  maximum witness reconciliation error: 0.000000 DEMO million"
    "Provider default, legal enforceability, collateral and funding cost, capital, expenses, tax, exclusions, subrogation, payment delay, and fair value are not modeled."
    "A positive premium gap is required catalytic support inside this narrow model before provider costs."
)

foreach(fragment IN LISTS required_fragments)
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "pooled-loss-protection fixture output is missing:\n${fragment}\n\n"
            "Complete output:\n${output}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${PROTECTION}" --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized pooled-loss-protection run failed with "
        "${normalized_result}:\n${normalized_error}")
endif()
foreach(heading IN ITEMS
        "Normalized portfolio configuration"
        "Normalized probability-envelope configuration"
        "Normalized success-participation configuration"
        "Normalized pooled-loss-protection configuration")
    string(FIND "${normalized_output}" "${heading}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "normalized output is missing: ${heading}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}"
        "${PARTICIPATION}" "${PROTECTION}" --unknown-option
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
        "${PARTICIPATION}"
    RESULT_VARIABLE missing_term_result
    OUTPUT_VARIABLE missing_term_output
    ERROR_VARIABLE missing_term_error
)
if(NOT missing_term_result EQUAL 2)
    message(FATAL_ERROR
        "a missing protection term must exit 2, got ${missing_term_result}")
endif()
string(FIND "${missing_term_error}" "usage:" missing_term_usage_position)
if(missing_term_usage_position EQUAL -1)
    message(FATAL_ERROR
        "a missing protection term must print usage on stderr")
endif()
