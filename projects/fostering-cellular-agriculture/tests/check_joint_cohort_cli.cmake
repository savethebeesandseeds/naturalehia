if(NOT DEFINED PROGRAM OR NOT DEFINED COHORT OR NOT DEFINED BLOCKED)
    message(FATAL_ERROR "PROGRAM, COHORT, and BLOCKED are required")
endif()

execute_process(
    COMMAND "${PROGRAM}" "${COHORT}" --print-normalized
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "joint-cohort fixture failed with ${result}:\n${error_output}")
endif()

foreach(fragment IN ITEMS
        "JOINT-COHORT PROBABILITY ENVELOPE v0.1 - SYNTHETIC CANDIDATE"
        "calibrated_execution_authorized=false"
        "population frame count: 22"
        "monetary basis: constant synthetic monetary units at analysis close"
        "portfolio horizon: 24 months"
        "total commitment: 20.000000 DEMO million"
        "independently and identically distributed complete joint-unit draws are required"
        "conservative nonasymptotic simultaneous outer confidence set"
        "included denominator N: 20"
        "unknown U: 2"
        "excluded outside N: 2"
        "Primary conservative simultaneous outer set"
        "portfolio reference lies inside every primary bound: true"
        "Goodman diagnostic unavailable: it requires complete outcomes and K>1."
        "Exact financial ranges under the generated outer set"
        "expected project draws"
        "expected investor receipts"
        "expected terminal principal loss"
        "NPV using the declared hurdle and physical-P scenario weights"
        "expected outstanding principal at horizon"
        "principal-loss ES99"
        "NPV-shortfall ES95"
        "this confidence level governs the probability-parameter outer set; ES95/ES99"
        "expected peak cumulative net outlay"
        "Underlying project financial ranges"
        "project: culture-platform"
        "expected receipts | 2.325585 | 9.600000 | 12.000000 | DEMO million"
        "expected realized principal loss | 0.000000 | 2.000000 | 8.062013 | DEMO million"
        "Common-witness pool-loss ES95 attribution"
        "culture-platform | 0.000000 | 7.000000 | 10.000000 | DEMO million"
        "These are additive attributions under three shared measures"
        "Expected receipts by declared external source"
        "Financial endpoint probability witnesses"
        "commercial nominal receipts minimum witness"
        "Probability and reconciliation controls"
        "component lower-bound sum"
        "maximum endpoint probability error"
        "ES95 maximum project-contribution reconciliation error"
        "principal_loss_million > 0"
        "pathwise peak; they are not a worst-path reserve"
        "Joint impairment projections"
        "any-project impairment"
        "all-project impairment"
        "This synthetic candidate does not authorize calibration"
        "Normalized semantic renderings"
        "not a directly reloadable hash-consistent cohort package"
        "Normalized joint-cohort configuration (retains original raw-file hashes)"
        "joint_cohort.population_frame_count=22"
        "joint_cohort.sampling_assumption=iid-complete-joint-state-candidate"
        "Normalized portfolio configuration"
        "Normalized generated probability-envelope configuration"
        "ambiguity.model_version=0.1.0"
        "scenario.1.lower_weight="
        "Normalized semantic rendering of authoritative ledger rows")
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "joint-cohort output is missing:\n${fragment}\n\n${output}")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" "${COHORT}" --unknown-option
    RESULT_VARIABLE bad_result
    OUTPUT_VARIABLE bad_output
    ERROR_VARIABLE bad_error
)
if(NOT bad_result EQUAL 2)
    message(FATAL_ERROR "unknown option must exit 2, got ${bad_result}")
endif()
string(FIND "${bad_error}" "usage:" usage_position)
if(usage_position EQUAL -1)
    message(FATAL_ERROR "unknown option must print usage")
endif()

execute_process(
    COMMAND "${PROGRAM}"
    RESULT_VARIABLE missing_result
    OUTPUT_VARIABLE missing_output
    ERROR_VARIABLE missing_error
)
if(NOT missing_result EQUAL 2)
    message(FATAL_ERROR "missing arguments must exit 2, got ${missing_result}")
endif()
string(FIND "${missing_error}" "usage:" missing_usage_position)
if(missing_usage_position EQUAL -1)
    message(FATAL_ERROR "missing arguments must print usage")
endif()
string(FIND "${missing_error}"
    "calibrated_execution_authorized=false" missing_boundary_position)
if(missing_boundary_position EQUAL -1)
    message(FATAL_ERROR "usage exits must retain the no-authorization boundary")
endif()

execute_process(
    COMMAND "${PROGRAM}" "${BLOCKED}"
    RESULT_VARIABLE blocked_result
    OUTPUT_VARIABLE blocked_output
    ERROR_VARIABLE blocked_error
)
if(NOT blocked_result EQUAL 3)
    message(FATAL_ERROR
        "structurally valid blocked cohort must exit 3, got ${blocked_result}:\n${blocked_error}")
endif()
foreach(fragment IN ITEMS
        "Primary outer set: BLOCKED"
        "repeated non-excluded cluster_id"
        "calibrated_execution_authorized=false"
        "This synthetic candidate does not authorize calibration")
    string(FIND "${blocked_output}" "${fragment}" blocked_position)
    if(blocked_position EQUAL -1)
        message(FATAL_ERROR
            "blocked output is missing ${fragment}:\n${blocked_output}")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" "${COHORT}.missing"
    RESULT_VARIABLE error_result
    OUTPUT_VARIABLE runtime_output
    ERROR_VARIABLE runtime_error
)
if(NOT error_result EQUAL 1)
    message(FATAL_ERROR "runtime errors must exit 1, got ${error_result}")
endif()
string(FIND "${runtime_error}"
    "calibrated_execution_authorized=false" runtime_boundary_position)
if(runtime_boundary_position EQUAL -1)
    message(FATAL_ERROR "runtime errors must retain no-authorization boundary")
endif()
