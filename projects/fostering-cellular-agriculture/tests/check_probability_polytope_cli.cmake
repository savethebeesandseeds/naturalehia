# SPDX-License-Identifier: MIT

if(NOT DEFINED PROGRAM OR NOT DEFINED PORTFOLIO OR NOT DEFINED POLYTOPE)
    message(FATAL_ERROR "PROGRAM, PORTFOLIO, and POLYTOPE are required")
endif()

execute_process(
    COMMAND "${PROGRAM}" --event-polytope "${PORTFOLIO}" "${POLYTOPE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "event-probability-polytope fixture failed with ${result}:\n"
        "${error_output}")
endif()

set(required_fragments
    "SYNTHETIC EVENT-PROBABILITY POLYTOPE ANALYSIS"
    "  measure: physical P sensitivity over fixed joint cash paths"
    "  solver scope: audited floating-point linear and upper-tail fixed-path risk"
    "  calibrated_execution_authorized=false"
    "Scenario probability box"
    "  common-loss | 0.000000% | 2.000000% | 100.000000%"
    "Named event constraints"
    "event | definition | lower | central | upper | explicit members"
    "  any-project-impairment | At least one project has principal impairment by the horizon | 30.000000% | 38.000000% | 50.000000% | common-loss"
    "culture-loss-scaleup-success"
    "culture-success-scaleup-loss"
    "  common-process-shock | Both projects are impaired by the declared shared process shock | 1.000000% | 2.000000% | 10.000000% | common-loss"
    "  culture-platform-impairment | Culture-platform principal is impaired by the horizon | 12.000000% | 20.000000% | 30.000000% | common-loss"
    "  scaleup-impairment | Bioprocess-scaleup principal is impaired by the horizon | 12.000000% | 20.000000% | 30.000000% | common-loss"
    "Audited floating-point linear fixed-path pool ranges"
    "  pool expected draws | 20.000000 | 20.000000 | 20.000000 | DEMO million"
    "  pool expected receipts | 19.400000 | 21.600000 | 22.590000 | DEMO million"
    "  pool expected costs | 0.200000 | 0.200000 | 0.200000 | DEMO million"
    "  pool outstanding exposure at horizon | 0.000000 | 0.000000 | 0.000000 | DEMO million"
    "  pool expected resolved principal loss at horizon | 2.480000 | 3.200000 | 4.800000 | DEMO million"
    "  pool impairment probability | 30.000000 | 38.000000 | 50.000000 | percent"
    "  pool expected NPV at declared physical-P hurdle | -0.800000 | 1.400000 | 2.390000 | DEMO million"
    "  pool negative-NPV probability | 30.000000 | 38.000000 | 50.000000 | percent"
    "  pool expected peak same-month project draw | 20.000000 | 20.000000 | 20.000000 | DEMO million"
    "  pool expected peak same-month gross funding need | 20.200000 | 20.200000 | 20.200000 | DEMO million"
    "  pool expected peak cumulative net outlay | 20.200000 | 20.200000 | 20.200000 | DEMO million"
    "Audited floating-point linear fixed-path project ranges"
    "  project bioprocess-scaleup expected draws | 10.000000 | 10.000000 | 10.000000 | DEMO million"
    "  project bioprocess-scaleup expected receipts | 9.700000 | 10.800000 | 11.680000 | DEMO million"
    "  project bioprocess-scaleup outstanding exposure at horizon | 0.000000 | 0.000000 | 0.000000 | DEMO million"
    "  project bioprocess-scaleup expected resolved principal loss at horizon | 0.960000 | 1.600000 | 2.400000 | DEMO million"
    "  project bioprocess-scaleup impairment probability | 12.000000 | 20.000000 | 30.000000 | percent"
    "  project bioprocess-scaleup expected NPV before pool costs | -0.300000 | 0.800000 | 1.680000 | DEMO million"
    "  project bioprocess-scaleup negative-NPV probability before pool costs | 12.000000 | 20.000000 | 30.000000 | percent"
    "  project culture-platform expected draws | 10.000000 | 10.000000 | 10.000000 | DEMO million"
    "  project culture-platform expected receipts | 9.700000 | 10.800000 | 11.680000 | DEMO million"
    "  project culture-platform outstanding exposure at horizon | 0.000000 | 0.000000 | 0.000000 | DEMO million"
    "  project culture-platform expected resolved principal loss at horizon | 0.960000 | 1.600000 | 2.400000 | DEMO million"
    "  project culture-platform impairment probability | 12.000000 | 20.000000 | 30.000000 | percent"
    "  project culture-platform expected NPV before pool costs | -0.300000 | 0.800000 | 1.680000 | DEMO million"
    "  project culture-platform negative-NPV probability before pool costs | 12.000000 | 20.000000 | 30.000000 | percent"
    "Audited floating-point fixed-path upper-tail ranges"
    "  Tail mass is 5% for ES95 and 1% for ES99."
    "  pool resolved principal loss ES95 | 9.600000 | 11.200000 | 16.000000 | DEMO million"
    "  pool resolved principal loss ES99 | 16.000000 | 16.000000 | 16.000000 | DEMO million"
    "  pool NPV shortfall ES95 | 7.400000 | 9.600000 | 16.200000 | DEMO million"
    "  pool NPV shortfall ES99 | 16.200000 | 16.200000 | 16.200000 | DEMO million"
    "Common aggregate-loss ES95 project attribution"
    "  bioprocess-scaleup | 4.027586 | 5.600000 | 8.000000 | DEMO million"
    "  culture-platform | 5.572414 | 5.600000 | 8.000000 | DEMO million"
    "  central tail mass | common-loss=0.020000; common-success=0.000000; culture-loss-scaleup-success=0.015000; culture-success-scaleup-loss=0.015000"
    "Common aggregate-loss ES99 project attribution"
    "  bioprocess-scaleup | 8.000000 | 8.000000 | 8.000000 | DEMO million"
    "neither the optimizing full measure nor the resulting project attribution is claimed to be unique"
    "Endpoint witness ledger"
    "maximum constraint violation | objective reconciliation error | simplex reduced-cost residual"
    "  pool expected resolved principal loss at horizon | minimum | 2.480000 | common-loss=0.010000"
    "  pool expected resolved principal loss at horizon | maximum | 4.800000 | common-loss=0.100000"
    "Upper-tail endpoint witness ledger"
    "fractional tail-mass vector | maximum constraint violation | maximum tail-mass violation"
    "  pool resolved principal loss ES95 | minimum | 9.600000 | p: common-loss=0.010000"
    "  pool resolved principal loss ES95 | maximum | 16.000000 | p: common-loss=0.100000"
    "Solver audit controls"
    "  maximum endpoint constraint violation:"
    "  maximum endpoint objective reconciliation error:"
    "  maximum endpoint simplex reduced-cost residual:"
    "  maximum tail-mass violation:"
    "  maximum tail threshold-formula reconciliation error:"
    "  maximum minimum-threshold enumeration reduced-cost residual:"
    "A chosen witness may be nonunique"
    "This is physical-P sensitivity, not a price, fair value, risk-neutral value, or investable quote."
    "ES95/99 use their own full probability and fractional-tail witnesses"
    "The same event candidate set can enter the fully funded stack through naturalehia-capital-stack --event-polytope"
)
foreach(fragment IN LISTS required_fragments)
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "event-probability-polytope output is missing:\n${fragment}\n\n"
            "Complete output:\n${output}")
    endif()
endforeach()

foreach(forbidden_fragment IN ITEMS
        "Exact linear fixed-path pool ranges"
        "ES95/99 and capital-stack routing are unavailable")
    string(FIND "${output}" "${forbidden_fragment}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR
            "v0.2 output makes an unavailable or unsupported claim:\n"
            "${forbidden_fragment}")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" --event-polytope "${PORTFOLIO}" "${POLYTOPE}"
        --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized event-polytope run failed with ${normalized_result}:\n"
        "${normalized_error}")
endif()
foreach(fragment IN ITEMS
        "Normalized portfolio configuration"
        "Normalized event-probability-polytope configuration"
        "polytope.model_version=0.2.0"
        "scenario.1.id=common-loss"
        "event.1.id=any-project-impairment"
        "event.1.scenario.1.id=common-loss")
    string(FIND "${normalized_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "normalized output is missing: ${fragment}")
    endif()
endforeach()

# The normalized polytope section is a semantic input, not merely a display.
string(FIND "${normalized_output}" "polytope.model_version=0.2.0"
    normalized_polytope_start)
if(normalized_polytope_start EQUAL -1)
    message(FATAL_ERROR "could not locate normalized polytope bytes")
endif()
string(SUBSTRING "${normalized_output}" ${normalized_polytope_start} -1
    normalized_polytope)
string(MD5 temporary_suffix "${PROGRAM}|${PORTFOLIO}|${POLYTOPE}")
set(normalized_polytope_path
    "${CMAKE_CURRENT_BINARY_DIR}/event-polytope-normalized-${temporary_suffix}.cfg")
file(WRITE "${normalized_polytope_path}" "${normalized_polytope}")
execute_process(
    COMMAND "${PROGRAM}" --event-polytope "${PORTFOLIO}"
        "${normalized_polytope_path}"
    RESULT_VARIABLE reload_result
    OUTPUT_VARIABLE reload_output
    ERROR_VARIABLE reload_error
)
file(REMOVE "${normalized_polytope_path}")
if(NOT reload_result EQUAL 0)
    message(FATAL_ERROR
        "normalized polytope was not reloadable (${reload_result}):\n"
        "${reload_error}")
endif()
string(FIND "${reload_output}"
    "pool expected resolved principal loss at horizon | 2.480000 | 3.200000 | 4.800000"
    reload_metric_position)
if(reload_metric_position EQUAL -1)
    message(FATAL_ERROR "normalized reload changed the projected loss range")
endif()

# The opt-in prefix must not reinterpret the existing positional grammar.
get_filename_component(fixture_directory "${PORTFOLIO}" DIRECTORY)
set(legacy_ambiguity
    "${fixture_directory}/two-project-probability-envelope-synthetic.cfg")
if(NOT EXISTS "${legacy_ambiguity}")
    message(FATAL_ERROR "legacy ambiguity fixture is missing: ${legacy_ambiguity}")
endif()
execute_process(
    COMMAND "${PROGRAM}" "${PORTFOLIO}" "${legacy_ambiguity}"
    RESULT_VARIABLE legacy_result
    OUTPUT_VARIABLE legacy_output
    ERROR_VARIABLE legacy_error
)
if(NOT legacy_result EQUAL 0)
    message(FATAL_ERROR
        "legacy positional probability-envelope run changed (${legacy_result}):\n"
        "${legacy_error}")
endif()
foreach(fragment IN ITEMS
        "SYNTHETIC PHYSICAL-PROBABILITY ENVELOPE ANALYSIS"
        "  expected realized principal loss | 2.480000 | 3.200000 | 4.800000 | DEMO million"
        "  principal-loss ES95 | 9.600000 | 11.200000 | 16.000000 | DEMO million")
    string(FIND "${legacy_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "legacy output is missing: ${fragment}")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" --event-polytope "${PORTFOLIO}" "${POLYTOPE}"
        --unknown-option
    RESULT_VARIABLE unknown_result
    OUTPUT_VARIABLE unknown_output
    ERROR_VARIABLE unknown_error
)
if(NOT unknown_result EQUAL 2)
    message(FATAL_ERROR
        "an unknown event-polytope option must exit 2, got ${unknown_result}")
endif()
string(FIND "${unknown_error}" "usage:" unknown_usage_position)
if(unknown_usage_position EQUAL -1)
    message(FATAL_ERROR "an unknown event-polytope option must print usage")
endif()

execute_process(
    COMMAND "${PROGRAM}" --event-polytope "${PORTFOLIO}"
    RESULT_VARIABLE missing_result
    OUTPUT_VARIABLE missing_output
    ERROR_VARIABLE missing_error
)
if(NOT missing_result EQUAL 2)
    message(FATAL_ERROR
        "a missing event-polytope path must exit 2, got ${missing_result}")
endif()
string(FIND "${missing_error}" "usage:" missing_usage_position)
if(missing_usage_position EQUAL -1)
    message(FATAL_ERROR "missing event-polytope input must print usage")
endif()

# A coherent polytope whose scenario taxonomy does not match the portfolio
# reaches validation and must use the analysis-error exit rather than grammar.
string(REPLACE "common-loss" "scenario-not-in-portfolio"
    invalid_polytope "${normalized_polytope}")
set(invalid_polytope_path
    "${CMAKE_CURRENT_BINARY_DIR}/event-polytope-invalid-${temporary_suffix}.cfg")
file(WRITE "${invalid_polytope_path}" "${invalid_polytope}")
execute_process(
    COMMAND "${PROGRAM}" --event-polytope "${PORTFOLIO}"
        "${invalid_polytope_path}"
    RESULT_VARIABLE invalid_result
    OUTPUT_VARIABLE invalid_output
    ERROR_VARIABLE invalid_error
)
file(REMOVE "${invalid_polytope_path}")
if(NOT invalid_result EQUAL 1)
    message(FATAL_ERROR
        "an event-polytope validation error must exit 1, got "
        "${invalid_result}:\n${invalid_error}")
endif()
string(FIND "${invalid_error}"
    "event-probability-polytope analysis failed:" invalid_error_position)
if(invalid_error_position EQUAL -1)
    message(FATAL_ERROR "validation exit must identify event-polytope analysis")
endif()
