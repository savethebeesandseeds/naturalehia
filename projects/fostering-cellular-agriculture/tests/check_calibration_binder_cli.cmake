if(NOT DEFINED PROGRAM OR NOT DEFINED BINDER)
    message(FATAL_ERROR "PROGRAM and BINDER are required")
endif()

execute_process(
    COMMAND "${PROGRAM}" "${BINDER}" --print-normalized
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "calibration binder fixture failed with ${result}:\n${error_output}")
endif()

foreach(fragment IN ITEMS
        "CALIBRATION BINDER CANDIDATE REVIEW - NO CALIBRATED EXECUTION"
        "candidate_status=structurally-checked-synthetic-candidate"
        "calibrated_execution_authorized=false"
        "project_id=synthetic-facility"
        "dossier_id=synthetic-binder-gap-dossier"
        "probability_measure=physical-P"
        "material_target_count=25"
        "lineage_row_count=25"
        "dossier_highest_allowed_use=PUBLIC RESEARCH / QUESTION FORMATION ONLY"
        "Normalized binder configuration"
        "Normalized portfolio configuration"
        "Normalized probability-envelope configuration"
        "Normalized calibration lineage")
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "calibration binder output is missing:\n${fragment}\n\n${output}")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" "${BINDER}" --unknown-option
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
