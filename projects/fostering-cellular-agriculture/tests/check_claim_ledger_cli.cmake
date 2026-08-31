include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

if(NOT DEFINED PROGRAM OR NOT DEFINED SYNTHETIC OR
        NOT DEFINED LIBERATION OR NOT DEFINED SOLAR)
    message(FATAL_ERROR
        "PROGRAM, SYNTHETIC, LIBERATION, and SOLAR are required")
endif()

function(assert_report_number report key minimum maximum)
    string(REGEX MATCH "${key}=([-+0-9.eE]+)" matched "${report}")
    if(NOT matched)
        message(FATAL_ERROR
            "synthetic claim-ledger output is missing numeric field ${key}")
    endif()
    set(actual "${CMAKE_MATCH_1}")
    if("${actual}" LESS "${minimum}" OR "${actual}" GREATER "${maximum}")
        message(FATAL_ERROR
            "synthetic claim-ledger ${key}=${actual} is outside [${minimum}, ${maximum}]")
    endif()
endfunction()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${SYNTHETIC}"
    RESULT_VARIABLE synthetic_result
    OUTPUT_VARIABLE synthetic_output
    ERROR_VARIABLE synthetic_error
)
if(NOT synthetic_result EQUAL 0)
    message(FATAL_ERROR
        "synthetic claim-ledger report failed with ${synthetic_result}:\n${synthetic_error}")
endif()

set(synthetic_fragments
    "PROJECT CLAIM LEDGER v0.1 PACKAGE REVIEW"
    "package_status=synthetic-complete"
    "package_integrity=verified"
    "core_config_ready=true"
    "core_evaluation=performed"
    "full_backtest_evaluation=performed"
    "expected_return_admissible=false"
    "observation_admissible=false"
    "mechanical_expected_cash_ready=true"
    "mechanical_npv_ready=true"
    "mechanical_rate_preimage_ready=true"
    "provider_claim_ready=true"
    "expected_buyer_cash_outflow_t0_million=9.2"
    "expected_terminal_receipts_million=10"
    "expected_total_receipts_million=10.3"
    "expected_principal_loss_million=0.8"
    "expected_total_loss_million=0.8"
    "peak_expected_ead_million=10.8"
    "expected_provider_claim_generated_million=0.8"
    "expected_provider_guarantee_cash_million=0.8"
    "synthetic package is not an admissible expected-return observation"
)
foreach(fragment IN LISTS synthetic_fragments)
    string(FIND "${synthetic_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "synthetic claim-ledger output is missing:\n${fragment}\n\n${synthetic_output}")
    endif()
endforeach()

assert_report_number("${synthetic_output}"
    "expected_investor_cashflow_t0_million" -8.900000001 -8.899999999)
assert_report_number("${synthetic_output}"
    "expected_npv_million" 0.190909089 0.190909092)
assert_report_number("${synthetic_output}"
    "expected_principal_cash_wal_months" 11.99999999 12.00000001)
assert_report_number("${synthetic_output}"
    "annual_effective_rate_preimage" 0.123595504 0.123595507)

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${SYNTHETIC}" --require-rate-preimage
    RESULT_VARIABLE synthetic_rate_result
    OUTPUT_VARIABLE synthetic_rate_output
    ERROR_VARIABLE synthetic_rate_error
)
if(NOT synthetic_rate_result EQUAL 0)
    message(FATAL_ERROR
        "complete synthetic rate-preimage requirement failed: ${synthetic_rate_error}")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${SYNTHETIC}" --require-expected-return
    RESULT_VARIABLE synthetic_return_result
    OUTPUT_VARIABLE synthetic_return_output
    ERROR_VARIABLE synthetic_return_error
)
if(NOT synthetic_return_result EQUAL 3)
    message(FATAL_ERROR
        "synthetic stress mechanics must not satisfy expected-return admission; got ${synthetic_return_result}")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${SYNTHETIC}" --require-observation-admission
    RESULT_VARIABLE synthetic_admission_result
    OUTPUT_VARIABLE synthetic_admission_output
    ERROR_VARIABLE synthetic_admission_error
)
if(NOT synthetic_admission_result EQUAL 3)
    message(FATAL_ERROR
        "synthetic observation admission must fail with 3, got ${synthetic_admission_result}")
endif()

foreach(public_claim IN ITEMS "${LIBERATION}" "${SOLAR}")
    execute_process(
        COMMAND ${PROGRAM_COMMAND} "${public_claim}"
        RESULT_VARIABLE public_result
        OUTPUT_VARIABLE public_output
        ERROR_VARIABLE public_error
    )
    if(NOT public_result EQUAL 0)
        message(FATAL_ERROR
            "valid incomplete public package failed: ${public_error}")
    endif()
    foreach(fragment IN ITEMS
            "package_status=retained-public-incomplete"
            "package_integrity=verified"
            "core_config_ready=false"
            "core_evaluation=not-performed"
            "full_backtest_evaluation=unavailable"
            "expected_return_admissible=false"
            "observation_admissible=false"
            "expected-cash rate preimage is not ready for expected-return admission"
            "UNKNOWN")
        string(FIND "${public_output}" "${fragment}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR
                "public incomplete output is missing ${fragment}:\n${public_output}")
        endif()
    endforeach()

    execute_process(
        COMMAND ${PROGRAM_COMMAND} "${public_claim}"
            --require-observation-admission
        RESULT_VARIABLE public_admission_result
        OUTPUT_VARIABLE public_admission_output
        ERROR_VARIABLE public_admission_error
    )
    if(NOT public_admission_result EQUAL 3)
        message(FATAL_ERROR
            "public observation admission must fail with 3, got ${public_admission_result}")
    endif()


    execute_process(
        COMMAND ${PROGRAM_COMMAND} "${public_claim}" --require-expected-return
        RESULT_VARIABLE public_return_result
        OUTPUT_VARIABLE public_return_output
        ERROR_VARIABLE public_return_error
    )
    if(NOT public_return_result EQUAL 3)
        message(FATAL_ERROR
            "public incomplete expected-return admission must fail with 3, got ${public_return_result}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${SYNTHETIC}" --unknown-option
    RESULT_VARIABLE bad_option_result
    OUTPUT_VARIABLE bad_option_output
    ERROR_VARIABLE bad_option_error
)
if(NOT bad_option_result EQUAL 2)
    message(FATAL_ERROR
        "unknown claim-ledger option must exit 2, got ${bad_option_result}")
endif()
string(FIND "${bad_option_error}" "usage:" usage_position)
if(usage_position EQUAL -1)
    message(FATAL_ERROR "unknown claim-ledger option must print usage")
endif()
