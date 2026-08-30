# SPDX-License-Identifier: MIT

if(NOT DEFINED PROGRAM OR NOT DEFINED DOSSIER OR NOT DEFINED MANIFEST)
    message(FATAL_ERROR "PROGRAM, DOSSIER, and MANIFEST are required")
endif()
if(NOT DEFINED MODE)
    message(FATAL_ERROR "MODE is required")
endif()

set(arguments "${DOSSIER}" "${MANIFEST}")
set(expected_result 3)
set(required_output "PUBLIC RESEARCH / QUESTION FORMATION ONLY")
set(additional_required_output "")
set(required_error "")

if(MODE STREQUAL "report-only")
    list(APPEND arguments "--evaluation-date" "2026-08-27" "--report-only")
    set(expected_result 0)
    set(required_output "NON-ENFORCING REPORT MODE")
    set(additional_required_output "The printed gate result is unchanged")
elseif(MODE STREQUAL "enforcing")
    list(APPEND arguments "--evaluation-date" "2026-08-27")
elseif(MODE STREQUAL "invalid-date")
    list(APPEND arguments "--evaluation-date" "not-a-date")
    set(expected_result 2)
    set(required_output "")
    set(required_error "valid YYYY-MM-DD")
elseif(MODE STREQUAL "missing-date-value")
    list(APPEND arguments "--evaluation-date" "--report-only")
    set(expected_result 2)
    set(required_output "")
    set(required_error "valid YYYY-MM-DD")
elseif(MODE STREQUAL "out-of-range-date")
    list(APPEND arguments "--evaluation-date" "1800-01-01")
    set(expected_result 2)
    set(required_output "")
    set(required_error "valid YYYY-MM-DD")
else()
    message(FATAL_ERROR "unknown MODE: ${MODE}")
endif()

execute_process(
    COMMAND "${PROGRAM}" ${arguments}
    RESULT_VARIABLE actual_result
    OUTPUT_VARIABLE actual_output
    ERROR_VARIABLE actual_error
)

if(NOT actual_result EQUAL expected_result)
    message(FATAL_ERROR
        "${MODE}: expected exit ${expected_result}, got ${actual_result}\n"
        "stdout:\n${actual_output}\nstderr:\n${actual_error}")
endif()
if(NOT required_output STREQUAL "" AND
   NOT actual_output MATCHES "${required_output}")
    message(FATAL_ERROR
        "${MODE}: stdout lacks '${required_output}'\n${actual_output}")
endif()
if(NOT required_error STREQUAL "" AND
   NOT actual_error MATCHES "${required_error}")
    message(FATAL_ERROR
        "${MODE}: stderr lacks '${required_error}'\n${actual_error}")
endif()
if(NOT additional_required_output STREQUAL "" AND
   NOT actual_output MATCHES "${additional_required_output}")
    message(FATAL_ERROR
        "${MODE}: stdout lacks '${additional_required_output}'\n${actual_output}")
endif()
