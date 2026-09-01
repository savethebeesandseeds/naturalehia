if(NOT DEFINED PROGRAM OR NOT DEFINED PROGRAM_EMULATOR OR NOT DEFINED FIXTURE)
    message(FATAL_ERROR "PROGRAM, PROGRAM_EMULATOR, and FIXTURE are required")
endif()

set(portfolio "${FIXTURE}/portfolio.cfg")
set(polytope "${FIXTURE}/event-polytope-v0.2.cfg")
set(participation "${FIXTURE}/success-participation.cfg")
set(stack "${FIXTURE}/capital-stack-v0.2.cfg")
set(frontier "${FIXTURE}/capital-mobilization-frontier-v0.2.cfg")

execute_process(
    COMMAND ${PROGRAM_EMULATOR} ${PROGRAM}
        "${portfolio}" "${polytope}" "${participation}" "${stack}"
        "${frontier}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "v0.2 capital-mobilization frontier fixture failed (${result}):\n${error_output}")
endif()

set(required_fragments
    "frontier model version: 0.2.0"
    "capital-stack model version: 0.2.0"
    "aggregate project outlay limit: 100.000000 DEMO million"
    "aggregate contractual asset-principal limit: 100.000000 DEMO million"
    "funded reserve and issued-principal stack detachment K: 100.000000 DEMO million"
    "tested junior issued-principal grid A: 10.000000, 20.000000, 30.000000, 40.000000, 50.000000 DEMO million"
    "maximum market expected issued-principal cash shortfall Q: 0.100000 fraction of market notional"
    "maximum market issued-principal cash-shortfall Q ES95: 0.500000 fraction of market notional"
    "maximum market issued-principal cash-shortfall Q ES99: 0.600000 fraction of market notional"
    "maximum market Pr[Q>0]: 0.350000 probability"
    "worst expected market issued-principal cash shortfall Q:"
    "worst market issued-principal cash-shortfall Q ES95:"
    "worst market Pr[Q>0]:"
    "feasible candidate indices: none"
    "minimum tested feasible q: none"
    "A is a junior issued-principal cash-shortfall layer, not causal attribution of asset loss"
    "capital_mobilization_is_established=false"
    "calibrated_execution_authorized=false"
)
foreach(fragment IN LISTS required_fragments)
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "v0.2 frontier output is missing:\n${fragment}\n\n${output}")
    endif()
endforeach()

set(forbidden_labels
    "worst expected market principal loss:"
    "worst market principal-loss ES95:"
    "worst market principal impairment probability:"
    "maximum market expected principal loss:"
    "maximum market principal impairment probability:"
    "tested catalytic first-loss grid A:"
)
foreach(label IN LISTS forbidden_labels)
    string(FIND "${output}" "${label}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR
            "v0.2 frontier output used a forbidden legacy label: ${label}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_EMULATOR} ${PROGRAM}
        "${portfolio}" "${polytope}" "${participation}" "${stack}"
        "${frontier}" --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized v0.2 frontier run failed (${normalized_result}):\n${normalized_error}")
endif()
set(normalized_fragments
    "Normalized base-capital-stack configuration"
    "capital_stack.model_version=0.2.0"
    "Normalized capital-mobilization-frontier configuration"
    "frontier.model_version=0.2.0"
    "junior_issued_principal_grid.count=5"
    "mandate.maximum_market_expected_issued_principal_cash_shortfall_fraction=0.10000000000000001"
    "mandate.maximum_market_principal_cash_shortfall_probability=0.34999999999999998"
)
foreach(fragment IN LISTS normalized_fragments)
    string(FIND "${normalized_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "normalized v0.2 frontier output is missing: ${fragment}")
    endif()
endforeach()
