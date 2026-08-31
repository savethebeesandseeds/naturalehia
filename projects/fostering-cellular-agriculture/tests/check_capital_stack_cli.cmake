include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${PORTFOLIO}" "${AMBIGUITY}" "${PARTICIPATION}" "${STACK}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "capital-stack CLI failed (${result}): ${error}")
endif()

foreach(required_text IN ITEMS
        "SYNTHETIC FULLY FUNDED CAPITAL STACK"
        "first-loss-residual"
        "intermediate"
        "senior"
        "every probability mix feasible within the candidate set meets expected-NPV hurdle: yes"
        "maximum nominal net-cash error: 0.000000"
        "Lower senior loss is redistribution, not value creation")
    string(FIND "${output}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "capital-stack CLI output did not contain: ${required_text}\n${output}")
    endif()
endforeach()
