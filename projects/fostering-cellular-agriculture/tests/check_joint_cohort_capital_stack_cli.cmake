include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

if(NOT DEFINED PROGRAM OR NOT DEFINED COHORT OR NOT DEFINED BLOCKED OR
        NOT DEFINED PARTICIPATION OR NOT DEFINED STACK OR
        NOT DEFINED STACK_Q037)
    message(FATAL_ERROR
        "PROGRAM, COHORT, BLOCKED, PARTICIPATION, STACK, and STACK_Q037 are required")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} --joint-cohort "${COHORT}" "${PARTICIPATION}"
        "${STACK}" --print-normalized
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "joint-cohort capital-stack fixture failed with ${result}:\n${error_output}")
endif()

foreach(fragment IN ITEMS
        "JOINT-COHORT CANDIDATE INPUT TO CAPITAL STACK"
        "calibrated_execution_authorized=false"
        "This synthetic candidate is conditional on the declared population"
        "Included unknown rows remain in N"
        "ES95 and ES99 are loss-tail statistics"
        "included denominator N: 20"
        "matured / unknown / excluded: 18 / 2 / 2"
        "bound portfolio SHA-256:"
        "bound raw ledger SHA-256:"
        "term files: strict normalized inputs outside the cohort"
        "Primary scenario-probability outer set"
        "portfolio reference lies inside every primary bound: true"
        "Underlying project risk at selected q before tranching"
        "Common-witness maximum-pool-tail loss attribution"
        "impairment probability min/central/max"
        "negative NPV probability min/central/max"
        "Every scalar minimum and maximum has its own feasible probability witness"
        "ES95 tail probability=0.050000; maximum pool ES="
        "ES99 tail probability=0.010000; maximum pool ES="
        "maximum probability measure: common-loss="
        "fractional tail mass: common-loss="
        "Project contributions reconcile to the shown pool ES total"
        "another ES-optimal attribution may also exist"
        "SYNTHETIC FULLY FUNDED CAPITAL STACK"
        "selected q meets that target across every probability mix feasible within the candidate set: no"
        "first-loss-residual"
        "intermediate"
        "senior"
        "Lower senior loss is redistribution, not value creation"
        "Normalized semantic renderings"
        "not a directly reloadable hash-consistent cohort package"
        "Normalized joint-cohort configuration (retains original raw-file hashes)"
        "Normalized bound portfolio configuration"
        "Normalized generated probability-envelope configuration"
        "Normalized semantic rendering of authoritative ledger rows"
        "Normalized success-participation configuration"
        "Normalized capital-stack configuration")
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "joint-cohort capital-stack output is missing:\n${fragment}\n\n${output}")
    endif()
endforeach()

# The ordinary fixture uses q=1. This second run protects the reporting bridge:
# receipts and NPV must be recomputed for the non-unit participation fraction
# that actually enters the waterfall, while draws and loss stay unchanged.
execute_process(
    COMMAND ${PROGRAM_COMMAND} --joint-cohort "${COHORT}" "${PARTICIPATION}"
        "${STACK_Q037}"
    RESULT_VARIABLE q037_result
    OUTPUT_VARIABLE q037_output
    ERROR_VARIABLE q037_error
)
if(NOT q037_result EQUAL 0)
    message(FATAL_ERROR
        "q=0.37 joint-cohort capital-stack fixture failed with ${q037_result}:\n${q037_error}")
endif()
foreach(fragment IN ITEMS
        "Underlying project risk at selected q before tranching"
        "underlying success participation q: 0.370000"
        "culture-platform | 10.000000/10.000000/10.000000 | 2.081398/8.592000/10.740000"
        "bioprocess-scaleup | 10.000000/10.000000/10.000000 | 2.081398/8.592000/10.740000")
    string(FIND "${q037_output}" "${fragment}" q037_position)
    if(q037_position EQUAL -1)
        message(FATAL_ERROR
            "q=0.37 output is missing ${fragment}:\n${q037_output}")
    endif()
endforeach()
string(FIND "${q037_output}"
    "culture-platform | 10.000000/10.000000/10.000000 | 2.325585/9.600000/12.000000"
    stale_q1_project_position)
if(NOT stale_q1_project_position EQUAL -1)
    message(FATAL_ERROR
        "q=0.37 project table must not reuse q=1 cohort receipts")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} --joint-cohort "${BLOCKED}" "${PARTICIPATION}"
        "${STACK}"
    RESULT_VARIABLE blocked_result
    OUTPUT_VARIABLE blocked_output
    ERROR_VARIABLE blocked_error
)
if(NOT blocked_result EQUAL 3)
    message(FATAL_ERROR
        "statistically blocked cohort must exit 3, got ${blocked_result}:\n${blocked_error}")
endif()
foreach(fragment IN ITEMS
        "JOINT-COHORT CANDIDATE INPUT TO CAPITAL STACK"
        "Primary outer set: BLOCKED"
        "repeated non-excluded cluster_id"
        "calibrated_execution_authorized=false")
    string(FIND "${blocked_output}" "${fragment}" blocked_position)
    if(blocked_position EQUAL -1)
        message(FATAL_ERROR
            "blocked cohort output is missing ${fragment}:\n${blocked_output}")
    endif()
endforeach()
string(FIND "${blocked_output}"
    "SYNTHETIC FULLY FUNDED CAPITAL STACK" unexpected_stack_position)
if(NOT unexpected_stack_position EQUAL -1)
    message(FATAL_ERROR "a blocked cohort must not produce a capital stack")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} --joint-cohort "${COHORT}" "${PARTICIPATION}"
    RESULT_VARIABLE usage_result
    OUTPUT_VARIABLE usage_output
    ERROR_VARIABLE usage_error
)
if(NOT usage_result EQUAL 2)
    message(FATAL_ERROR "missing cohort-mode terms must exit 2")
endif()
string(FIND "${usage_error}" "usage:" usage_position)
if(usage_position EQUAL -1)
    message(FATAL_ERROR "cohort-mode syntax errors must print usage")
endif()
string(FIND "${usage_error}" "calibrated_execution_authorized=false"
    usage_boundary_position)
if(usage_boundary_position EQUAL -1)
    message(FATAL_ERROR "usage output must retain the no-authorization boundary")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} --joint-cohort "${COHORT}.missing"
        "${PARTICIPATION}" "${STACK}"
    RESULT_VARIABLE error_result
    OUTPUT_VARIABLE runtime_output
    ERROR_VARIABLE runtime_error
)
if(NOT error_result EQUAL 1)
    message(FATAL_ERROR "missing cohort config must exit 1")
endif()
string(FIND "${runtime_error}" "calibrated_execution_authorized=false"
    runtime_boundary_position)
if(runtime_boundary_position EQUAL -1)
    message(FATAL_ERROR "runtime errors must retain no-authorization boundary")
endif()
