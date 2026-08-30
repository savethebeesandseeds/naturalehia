# SPDX-License-Identifier: MIT

if(NOT DEFINED PROGRAM OR NOT DEFINED FIXTURE)
    message(FATAL_ERROR "PROGRAM and FIXTURE are required")
endif()

set(config "${FIXTURE}/hurdle-evidence.cfg")

execute_process(
    COMMAND "${PROGRAM}" "${config}" --print-normalized
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "hurdle-evidence fixture failed (${result}):\n${error_output}")
endif()

foreach(fragment IN ITEMS
        "SYNTHETIC CANDIDATE ROBUST HURDLE EVIDENCE ENVELOPE"
        "classification: synthetic candidate"
        "selected evidence tier: settled_comparable"
        "selected transaction-conditioned/model-conditioned rate set: S_1 with 2 disjoint component(s)"
        "component 1 | [9.000000%, 10.000000%]"
        "component 2 | [11.000000%, 12.000000%]"
        "eligible-interval hull (outer diagnostic only; gaps remain excluded): [8.000000%, 14.000000%]"
        "S_0 through S_k"
        "Leave-one-cluster-out diagnostics"
        "every identified set is closed, disjoint, and canonical: true"
        "every selected component meets its coverage threshold: true"
        "every selected gap is below its coverage threshold: true"
        "mechanical_candidate_set_only=true"
        "market_hurdle_is_point_identified=false"
        "annual_expected_holding_period_return_is_inferred=false"
        "fair_value_market_price_demand_or_placement_is_established=false"
        "empirical_hurdle_evidence_release_authorized=false"
        "source_identifiers_or_assertion_booleans_authenticate_documents=false"
        "calibrated_execution_authorized=false"
        "Normalized robust-hurdle-envelope configuration"
        "hurdle_envelope.maximum_contaminated_clusters=1")
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "hurdle-evidence output is missing:\n${fragment}\n\n${output}")
    endif()
endforeach()

# The selected set must retain the unsupported 10--11 percent gap. A connected
# selected component spanning that gap would be an economic, not cosmetic, bug.
foreach(forbidden IN ITEMS
        "component 1 | [9.000000%, 12.000000%]"
        "component 3 |")
    string(FIND "${output}" "${forbidden}" forbidden_position)
    if(NOT forbidden_position EQUAL -1)
        message(FATAL_ERROR
            "hurdle-evidence output improperly filled or altered the selected gap: ${forbidden}")
    endif()
endforeach()

# The canonical formatter must produce a reloadable, byte-stable configuration.
string(FIND "${output}" "hurdle_envelope.model_version=" normalized_start)
if(normalized_start EQUAL -1)
    message(FATAL_ERROR "could not locate normalized hurdle-envelope input")
endif()
string(SUBSTRING "${output}" ${normalized_start} -1 normalized_config)

string(MD5 suffix "${PROGRAM}|${FIXTURE}")
set(temp_config
    "${CMAKE_CURRENT_BINARY_DIR}/hurdle-evidence-${suffix}.cfg")
file(WRITE "${temp_config}" "${normalized_config}")

execute_process(
    COMMAND "${PROGRAM}" "${temp_config}" --print-normalized
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_output
    ERROR_VARIABLE replay_error
)
if(NOT replay_result EQUAL 0)
    file(REMOVE "${temp_config}")
    message(FATAL_ERROR
        "normalized hurdle-evidence replay failed (${replay_result}):\n${replay_error}")
endif()
string(FIND "${replay_output}" "hurdle_envelope.model_version=" replay_start)
if(replay_start EQUAL -1)
    file(REMOVE "${temp_config}")
    message(FATAL_ERROR "could not locate replayed normalized configuration")
endif()
string(SUBSTRING "${replay_output}" ${replay_start} -1 replay_config)
file(REMOVE "${temp_config}")
if(NOT normalized_config STREQUAL replay_config)
    message(FATAL_ERROR
        "normalized hurdle-evidence configuration is not byte-stable")
endif()

# Exit codes are part of the operational boundary: invocation errors differ
# from rejected evidence/configuration.
execute_process(
    COMMAND "${PROGRAM}"
    RESULT_VARIABLE usage_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(NOT usage_result EQUAL 1)
    message(FATAL_ERROR "bad CLI usage returned ${usage_result}, expected 1")
endif()

execute_process(
    COMMAND "${PROGRAM}" "${FIXTURE}/does-not-exist.cfg"
    RESULT_VARIABLE missing_result
    OUTPUT_QUIET
    ERROR_VARIABLE missing_error
)
if(NOT missing_result EQUAL 2)
    message(FATAL_ERROR
        "missing configuration returned ${missing_result}, expected 2")
endif()
string(FIND "${missing_error}"
    "calibrated_execution_authorized=false" missing_boundary)
if(missing_boundary EQUAL -1)
    message(FATAL_ERROR
        "missing configuration did not preserve the execution boundary")
endif()
