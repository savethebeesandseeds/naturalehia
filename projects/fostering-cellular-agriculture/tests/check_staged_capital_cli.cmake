include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

if(NOT DEFINED PROGRAM OR NOT DEFINED SCENARIO)
    message(FATAL_ERROR "PROGRAM and SCENARIO are required")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${SCENARIO}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "staged-capital fixture failed with ${result}:\n${error_output}")
endif()

set(required_fragments
    "  completion: 55.000000%"
    "  final milestone failure: 20.000000%"
    "  cost-to-complete funding failure: 15.000000%"
    "  sponsor funding failure: 0.000000%"
    "  provider funding failure: 10.000000%"
    "    mean=44.100000 p50=60.000000 p95=60.000000 p99=60.000000 max=60.000000"
    "    mean=5.750000 p50=0.000000 p95=28.000000 p99=28.000000 max=28.000000"
    "  unrecovered-principal expected shortfall 95: 28.000000 DEMO million"
    "    mean=7.978934 p50=0.000000 p95=38.784668 p99=38.784668 max=38.784668"
    "  expected nominal provider terminal receipt: 43.681502 DEMO million"
    "    qualified-completion | 0.550000 | completed | 36 | 36 | 60.000000 | 69.693639 | -3.109076"
    "    equipment-acceptance-failure | 0.200000 | milestone-failure | 28 | 40 | 51.000000 | 23.000000 | -30.042818"
    "    cost-to-complete-stop-after-development | 0.150000 | cost-to-complete-failure | 6 | 12 | 6.000000 | 5.000000 | -1.274459"
    "    provider-funding-failure-at-first-draw | 0.100000 | completed | 36 | 36 | 60.000000 | 69.693639 | -3.109076"
    "  equipment-acceptance-failure | 0.200000 | milestone-failure | 28 | 40 | 51.000000 | 61.784668 | 23.000000 | 28.000000 | 38.784668 | 0.000000 | 2.000000 | 0.000000"
    "physical-P zero-NPV upfront fee: 8.220632 DEMO million"
    "charged upfront fee: 3.000000 DEMO million"
    "fee adequacy gap (charged minus break-even): -5.220632 DEMO million"
    "maximum cash-entry imbalance: 0.000000 DEMO million"
    "maximum commitment/claim memo imbalance: 0.000000 DEMO million"
)

foreach(fragment IN LISTS required_fragments)
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "staged-capital fixture output is missing:\n${fragment}\n\n"
            "Complete output:\n${output}")
    endif()
endforeach()
