# SPDX-License-Identifier: MIT

include("${CMAKE_CURRENT_LIST_DIR}/resolve_program_command.cmake")

if(NOT DEFINED PROGRAM OR NOT DEFINED FIXTURE)
    message(FATAL_ERROR "PROGRAM and FIXTURE are required")
endif()

set(portfolio "${FIXTURE}/portfolio.cfg")
set(polytope "${FIXTURE}/event-polytope.cfg")
set(participation "${FIXTURE}/success-participation.cfg")
set(base_stack "${FIXTURE}/capital-stack.cfg")
set(priority_cap "${FIXTURE}/priority-cap.cfg")
set(issue_price "${FIXTURE}/issue-price.cfg")

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        "${issue_price}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "issue-price-support fixture failed (${result}):\n${error_output}")
endif()

foreach(fragment IN ITEMS
        "SYNTHETIC ROBUST ISSUE-PRICE SUPPORT TERM"
        "Finite physical-probability funded-cash-claim sensitivity only"
        "Input evidence labels"
        "portfolio.synthetic_inputs=true"
        "event_polytope.synthetic_inputs=true"
        "success_participation.synthetic_inputs=true"
        "base_capital_stack.synthetic_inputs=true"
        "market_priority_cap.synthetic_inputs=true"
        "issue_price_support.synthetic_inputs=true"
        "all_fixture_inputs_are_synthetic=true"
        "Fixed claim, issue sources, and selected priority cap"
        "overall status: financeable-window-found"
        "upstream priority-cap status: minimum-tested-balanced-cap-found"
        "fixed underlying success participation q: 0.892857"
        "fixed junior first-loss capital A: 12.000000 DEMO million"
        "aggregate commitment and stack detachment K: 20.000000 DEMO million"
        "fixed market claim principal M=K-A: 8.000000 DEMO million"
        "selected market lifetime priority non-principal cap B: 0.533333 DEMO million | candidate 3"
        "reference status: internal_candidate"
        "settled-secondary full-claim month-zero normalization asserted: false"
        "gross buyer issue-price input P: 6.500000 DEMO million"
        "issuer cost F: 0.000000 DEMO million"
        "buyer-direct cost C outside the reserve: 0.000000 DEMO million"
        "maximum non-repayable support capacity G: 1.500000 DEMO million"
        "actual settled issue support S_obs: 0.000000 DEMO million"
        "issuer funding floor max(0,M+F-G): 6.500000 DEMO million"
        "support status: synthetic_candidate"
        "subscription-reserve deposit evidenced: false"
        "issuer-cost payment evidenced: false"
        "issue-use evidence record id: none"
        "Finite work and cross-case invariants"
        "upstream priority-cap work units: 2360"
        "hurdle-stack work units: 2360"
        "reference-projection work units: 1440"
        "scenario-month audit work units: 1500"
        "combined structural work: 7660 / 4000000"
        "base stack was not mutated: true"
        "only the market hurdle changed across cases: true"
        "every contractual-cash and principal-risk invariant holds: true"
        "modeled financeable hurdle-case indices: 0,1,2"
        "funded/escrowed-support-covered hurdle-case indices: none"
        "literal-zero hurdle-case index: 0"
        "Every supplied hurdle case"
        "Hurdle case 0 | id=hurdle-0-percent | annual effective hurdle=0.000000 percent | source=synthetic_sensitivity | relation=independent | status=financeable-price-window"
        "Hurdle case 1 | id=hurdle-5-percent | annual effective hurdle=5.000000 percent | source=synthetic_sensitivity | relation=independent | status=financeable-price-window"
        "Hurdle case 4 | id=hurdle-20-percent | annual effective hurdle=20.000000 percent | source=synthetic_sensitivity | relation=independent | status=investor-and-issuer-requirements-do-not-overlap"
        "raw robust investor price ceiling P*: 8.000000 DEMO million"
        "issuer funding floor: 6.500000 DEMO million"
        "modeled conditional financeable window exists: true"
        "modeled window lower bound: 6.500000 DEMO million"
        "modeled window upper bound: 8.000000 DEMO million"
        "minimum no-rights support for overlap: 0.714921 DEMO million"
        "modeled conditional financeable window exists: false"
        "modeled window lower bound: not applicable"
        "modeled window upper bound: not applicable"
        "modeled overlap endpoint exists without support: true"
        "documented support commitment covers overlap: false"
        "funded/escrowed support capacity covers overlap: false"
        "funded/escrowed-support-covered price window exists: false"
        "reference price numerically eligible: true"
        "Reference-price hypothetical-primary projection"
        "modeled required support S_req=M+F-P: 1.500000 DEMO million"
        "modeled joint investor/funding term adequate: true"
        "actual observed settled support S_obs: 0.000000 DEMO million"
        "observed issue sources settled and modeled identity reconciled: false"
        "observed settled primary funding completed: false"
        "observed issue sources: not applicable"
        "Fixed future-cash physical principal risk"
        "expected principal cash distribution | minimum=7.600000 | central=7.920000 | maximum=7.960000 DEMO million"
        "expected principal-loss fraction | minimum=0.500000 | central=1.000000 | maximum=5.000000 percent of M"
        "principal-loss ES95 | minimum=0.800000 | central=1.600000 | maximum=4.000000 DEMO million"
        "worst principal-loss ES95 fraction: 50.000000 percent of M"
        "principal impairment probability | minimum=1.000000 | central=2.000000 | maximum=10.000000 percent"
        "principal-cash WAL | minimum=1.842105 | central=1.898990 | maximum=1.922111 years"
        "Case invariants"
        "physical probability polytope unchanged: true"
        "market contractual cash unchanged: true"
        "market principal risk unchanged: true"
        "issue funding identity reconciles: true"
        "Endpoint witness ledger for this hurdle"
        "hurdle case 0 | market principal-loss ES95 | maximum | value=4.000000 DEMO million"
        "hurdle case 0 | market principal-cash WAL | maximum | value=1.922111"
        "hurdle case 0 | reference investor NPV | minimum | value=1.500000"
        "own physical p: common-loss=0.100000"
        "own tail mass y: common-loss=0.050000"
        "Interpretation boundary and full false-claim ledger"
        "A modeled conditional price window is arithmetic"
        "It is not capital readiness, an executable price, or investor demand"
        "A documented support commitment is distinct from funded or escrowed capacity"
        "S_req=M+F-P is a modeled source identity. S_obs is separately evidenced settled cash"
        "Settled source cash is not completed primary funding unless the subscription-reserve deposit"
        "Settled-secondary buyer-to-seller cash never enters the project reserve"
        "A model-implied or unresolved hurdle cannot establish a financeable window"
        "They are not a price, discount curve, expected investor yield, IFRS 9 ECL, Basel EL"
        "does not prove financeability, additionality, deployment, displacement, or animal-welfare impact"
        "market_hurdle_is_discovered_or_empirically_calibrated=false"
        "fair_value_or_accounting_value_is_estimated=false"
        "market_consistent_discount_curve_or_pricing_measure_is_used=false"
        "bid_offer_executable_price_spread_or_rating_is_produced=false"
        "investor_demand_suitability_or_placement_is_established=false"
        "support_provider_authority_or_budget_is_established=false"
        "support_counterparty_or_performance_risk_is_modeled=false"
        "legal_enforceability_tax_or_regulation_is_established=false"
        "capital_mobilization_or_financing_additionality_is_proven=false"
        "animal_product_displacement_or_welfare_impact_is_proven=false"
        "calibrated_execution_authorized=false")
    string(FIND "${output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "issue-price-support output is missing:\n${fragment}\n\n${output}")
    endif()
endforeach()

# Every declared hurdle must retain its own economic row and physical witness
# ledger, rather than only the financeable or reference-price cases.
foreach(index RANGE 0 4)
    string(FIND "${output}" "Hurdle case ${index} |" case_position)
    string(FIND "${output}"
        "hurdle case ${index} | market expected principal cash | minimum"
        witness_position)
    if(case_position EQUAL -1 OR witness_position EQUAL -1)
        message(FATAL_ERROR
            "hurdle case ${index} is missing its case or physical witness ledger")
    endif()
endforeach()

# The opt-in JSON path must be deterministic, browser-parseable, and use the
# v0.2 issued-principal cash-shortfall vocabulary for the same-pool fixture.
# The default v0.1 human report above remains the byte-stable legacy surface.
set(v02_fixture
    "${CMAKE_CURRENT_LIST_DIR}/../scenarios/ten-claim-instrument-v1-synthetic")
set(v02_portfolio "${v02_fixture}/portfolio.cfg")
set(v02_polytope "${v02_fixture}/event-polytope-v0.2.cfg")
set(v02_participation "${v02_fixture}/success-participation.cfg")
set(v02_stack "${v02_fixture}/capital-stack-v0.2.cfg")
set(v02_cap "${v02_fixture}/market-priority-cap-v0.2.cfg")
set(v02_issue "${v02_fixture}/issue-price-support-v0.2.cfg")

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${v02_portfolio}" "${v02_polytope}"
        "${v02_participation}" "${v02_stack}" "${v02_cap}"
        "${v02_issue}"
    RESULT_VARIABLE v02_human_result
    OUTPUT_VARIABLE v02_human_output
    ERROR_VARIABLE v02_human_error
)
if(NOT v02_human_result EQUAL 0 OR NOT v02_human_error STREQUAL "")
    message(FATAL_ERROR
        "v0.2 issue-price-support human report failed (${v02_human_result}):\n${v02_human_error}")
endif()
foreach(fragment IN ITEMS
        "risk-screen scope: separate relaxed priority-cap and issue-price sensitivity; not the Capital Mobilization Frontier mandate"
        "risk-screen label: Ten-claim separate relaxed issue-price risk sensitivity"
        "strict 25-candidate frontier rejects the same q=1, A=20, M=80 point"
        "price or support cannot cure its fixed Q risk failure"
        "price or support changes fixed principal-risk metrics: false"
        "fixed junior issued principal A: 20.000000 DEMO million"
        "funded reserve and issued-principal stack detachment K: 100.000000 DEMO million"
        "fixed market issued principal M=K-A: 80.000000 DEMO million")
    string(FIND "${v02_human_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "v0.2 issue-price-support human report is missing:\n${fragment}\n\n${v02_human_output}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${v02_portfolio}" "${v02_polytope}"
        "${v02_participation}" "${v02_stack}" "${v02_cap}"
        "${v02_issue}" --json
    RESULT_VARIABLE json_result
    OUTPUT_VARIABLE json_output
    ERROR_VARIABLE json_error
)
if(NOT json_result EQUAL 0)
    message(FATAL_ERROR
        "v0.2 issue-price-support JSON run failed (${json_result}):\n${json_error}")
endif()
if(NOT json_error STREQUAL "")
    message(FATAL_ERROR
        "successful issue-price-support JSON must not mix diagnostics into stderr:\n${json_error}")
endif()
string(JSON json_status GET "${json_output}" financeabilityWindow status)
string(JSON json_case_count LENGTH "${json_output}"
    financeabilityWindow cases)
string(JSON json_risk_metric_count LENGTH "${json_output}"
    financeabilityWindow riskGate metrics)
string(JSON json_case_0_status GET "${json_output}"
    financeabilityWindow cases 0 status)
string(JSON json_case_1_status GET "${json_output}"
    financeabilityWindow cases 1 status)
if(NOT json_status STREQUAL "financeable-window-found" OR
        NOT json_case_count EQUAL 6 OR
        NOT json_risk_metric_count EQUAL 5 OR
        NOT json_case_0_status STREQUAL "financeable-price-window" OR
        NOT json_case_1_status STREQUAL "financeable-price-window")
    message(FATAL_ERROR
        "v0.2 issue-price-support JSON has the wrong status/case/risk schema")
endif()
foreach(index RANGE 2 5)
    string(JSON case_status GET "${json_output}"
        financeabilityWindow cases ${index} status)
    if(NOT case_status STREQUAL
            "investor-and-issuer-requirements-do-not-overlap")
        message(FATAL_ERROR
            "only the zero- and five-percent hurdle cases may have windows")
    endif()
endforeach()
foreach(fragment IN ITEMS
        "\"financeabilityWindow\""
        "\"capitalStackModelVersion\": \"0.2.0\""
        "\"principalRiskMetricFamily\": \"issued-principal-cash-shortfall-q\""
        "\"mandateScope\": \"separate-priority-cap-issue-price-sensitivity\""
        "\"mandateLabel\": \"Ten-claim separate relaxed issue-price risk sensitivity\""
        "\"isCapitalMobilizationFrontierMandate\": false"
        "\"priceOrSupportChangesFixedRiskMetrics\": false"
        "\"label\": \"Separate sensitivity limits pass\""
        "strict 25-candidate frontier rejects the same q=1, A=20, M=80 point"
        "cannot cure its fixed Q risk failure"
        "\"selectedMarketPriorityNonprincipalCap\": 24.000000"
        "\"issuerFundingFloor\": 60.000000"
        "\"label\": \"Expected issued-principal cash shortfall Q\""
        "\"label\": \"Issued-principal cash shortfall Q ES95\""
        "\"label\": \"Issued-principal cash shortfall Q ES99\""
        "\"label\": \"Issued-principal cash shortfall probability Pr[Q>0]\""
        "\"investorCeiling\": 74.575200"
        "\"investorCeiling\": 60.955502"
        "\"investorCeiling\": 54.266905"
        "\"investorCeiling\": 50.313857"
        "\"investorCeiling\": 41.898625"
        "\"investorCeiling\": 35.170807"
        "\"minimumSupport\": 5.424800"
        "\"minimumSupport\": 19.044498"
        "\"minimumSupport\": 25.733095"
        "\"minimumSupport\": 29.686143"
        "\"minimumSupport\": 38.101375"
        "\"minimumSupport\": 44.829193"
        "\"provenance\""
        "\"nonClaimNotes\""
        "\"calibratedExecutionAuthorized\": false")
    string(FIND "${json_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "v0.2 issue-price-support JSON is missing:\n${fragment}\n\n${json_output}")
    endif()
endforeach()
foreach(forbidden IN ITEMS
        "SYNTHETIC ROBUST ISSUE-PRICE SUPPORT TERM"
        "Fixed risk mandates pass"
        "Fixed risk mandate fails"
        "\"key\": \"expectedPrincipalLossFraction\""
        "\"key\": \"principalImpairmentProbability\""
        ": NaN" ": Inf" ": -Inf" ": Infinity")
    string(FIND "${json_output}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR
            "v0.2 issue-price-support JSON contains forbidden text: ${forbidden}")
    endif()
endforeach()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${v02_portfolio}" "${v02_polytope}"
        "${v02_participation}" "${v02_stack}" "${v02_cap}"
        "${v02_issue}" --json
    RESULT_VARIABLE repeated_json_result
    OUTPUT_VARIABLE repeated_json_output
    ERROR_VARIABLE repeated_json_error
)
if(NOT repeated_json_result EQUAL 0 OR NOT repeated_json_error STREQUAL "" OR
        NOT repeated_json_output STREQUAL json_output)
    message(FATAL_ERROR
        "issue-price-support JSON must be byte-deterministic across identical runs")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${v02_portfolio}" "${v02_polytope}"
        "${v02_participation}" "${v02_stack}" "${v02_cap}"
        "${v02_issue}.missing" --json
    RESULT_VARIABLE json_failure_result
    OUTPUT_VARIABLE json_failure_output
    ERROR_VARIABLE json_failure_error
)
if(NOT json_failure_result EQUAL 2 OR NOT json_failure_output STREQUAL "")
    message(FATAL_ERROR
        "failed JSON runs must keep stdout empty so diagnostics cannot be parsed as JSON")
endif()
string(FIND "${json_failure_error}"
    "issue-price-support input/configuration failed:" position)
if(position EQUAL -1)
    message(FATAL_ERROR "failed JSON runs need a scoped stderr diagnostic")
endif()

# Normalized output must expose all six reloadable files. Extract each one,
# reload them together, and require a byte-stable six-file normalized suffix.
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        "${issue_price}" --print-normalized
    RESULT_VARIABLE normalized_result
    OUTPUT_VARIABLE normalized_output
    ERROR_VARIABLE normalized_error
)
if(NOT normalized_result EQUAL 0)
    message(FATAL_ERROR
        "normalized issue-price-support run failed (${normalized_result}):\n${normalized_error}")
endif()
foreach(fragment IN ITEMS
        "Normalized portfolio configuration"
        "portfolio.model_version=0.1.0"
        "Normalized event-probability-polytope configuration"
        "polytope.model_version=0.2.0"
        "Normalized success-participation configuration"
        "participation.model_version=0.1.0"
        "Normalized base-capital-stack configuration"
        "capital_stack.model_version=0.1.0"
        "Normalized robust-market-priority-cap configuration"
        "priority_cap.model_version=0.1.0"
        "Normalized robust-issue-price-support configuration"
        "issue_price_support.model_version=0.1.0"
        "reference_price.secondary_price_normalized_to_full_month_zero_claim=false"
        "reference_price.subscription_reserve_deposit_evidenced=false"
        "reference_price.issuer_cost_payment_evidenced=false"
        "reference_price.issue_use_evidence_record_id=none"
        "support.settled_support_million=0"
        "hurdle_case.count=5"
        "hurdle_case.1.id=hurdle-0-percent"
        "hurdle_case.5.id=hurdle-20-percent")
    string(FIND "${normalized_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "normalized output is missing: ${fragment}")
    endif()
endforeach()

string(FIND "${normalized_output}" "portfolio.model_version=" portfolio_start)
string(FIND "${normalized_output}"
    "\nNormalized event-probability-polytope configuration" portfolio_end)
string(FIND "${normalized_output}" "polytope.model_version=" polytope_start)
string(FIND "${normalized_output}"
    "\nNormalized success-participation configuration" polytope_end)
string(FIND "${normalized_output}" "participation.model_version=" participation_start)
string(FIND "${normalized_output}"
    "\nNormalized base-capital-stack configuration" participation_end)
string(FIND "${normalized_output}" "capital_stack.model_version=" stack_start)
string(FIND "${normalized_output}"
    "\nNormalized robust-market-priority-cap configuration" stack_end)
string(FIND "${normalized_output}" "priority_cap.model_version=" cap_start)
string(FIND "${normalized_output}"
    "\nNormalized robust-issue-price-support configuration" cap_end)
string(FIND "${normalized_output}" "issue_price_support.model_version=" issue_start)
if(portfolio_start EQUAL -1 OR portfolio_end EQUAL -1 OR
        polytope_start EQUAL -1 OR polytope_end EQUAL -1 OR
        participation_start EQUAL -1 OR participation_end EQUAL -1 OR
        stack_start EQUAL -1 OR stack_end EQUAL -1 OR
        cap_start EQUAL -1 OR cap_end EQUAL -1 OR issue_start EQUAL -1)
    message(FATAL_ERROR
        "could not delimit all six normalized issue-price-support inputs")
endif()

math(EXPR portfolio_length "${portfolio_end} - ${portfolio_start}")
math(EXPR polytope_length "${polytope_end} - ${polytope_start}")
math(EXPR participation_length
    "${participation_end} - ${participation_start}")
math(EXPR stack_length "${stack_end} - ${stack_start}")
math(EXPR cap_length "${cap_end} - ${cap_start}")
string(SUBSTRING "${normalized_output}" ${portfolio_start}
    ${portfolio_length} normalized_portfolio)
string(SUBSTRING "${normalized_output}" ${polytope_start}
    ${polytope_length} normalized_polytope)
string(SUBSTRING "${normalized_output}" ${participation_start}
    ${participation_length} normalized_participation)
string(SUBSTRING "${normalized_output}" ${stack_start}
    ${stack_length} normalized_stack)
string(SUBSTRING "${normalized_output}" ${cap_start}
    ${cap_length} normalized_cap)
string(SUBSTRING "${normalized_output}" ${issue_start} -1 normalized_issue)
string(SUBSTRING "${normalized_output}" ${portfolio_start} -1 normalized_suffix)

string(MD5 suffix "${PROGRAM}|${FIXTURE}")
set(temp_portfolio
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-p.cfg")
set(temp_polytope
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-e.cfg")
set(temp_participation
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-q.cfg")
set(temp_stack
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-s.cfg")
set(temp_cap
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-b.cfg")
set(temp_issue
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-i.cfg")
file(WRITE "${temp_portfolio}" "${normalized_portfolio}\n")
file(WRITE "${temp_polytope}" "${normalized_polytope}\n")
file(WRITE "${temp_participation}" "${normalized_participation}\n")
file(WRITE "${temp_stack}" "${normalized_stack}\n")
file(WRITE "${temp_cap}" "${normalized_cap}\n")
file(WRITE "${temp_issue}" "${normalized_issue}\n")
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${temp_portfolio}" "${temp_polytope}"
        "${temp_participation}" "${temp_stack}" "${temp_cap}"
        "${temp_issue}" --print-normalized
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_output
    ERROR_VARIABLE replay_error
)
if(NOT replay_result EQUAL 0)
    message(FATAL_ERROR
        "normalized issue-price-support inputs did not reload (${replay_result}):\n${replay_error}")
endif()
string(FIND "${replay_output}" "portfolio.model_version=" replay_start)
if(replay_start EQUAL -1)
    message(FATAL_ERROR "replay omitted its normalized six-file suffix")
endif()
string(SUBSTRING "${replay_output}" ${replay_start} -1 replay_suffix)
if(NOT normalized_suffix STREQUAL replay_suffix)
    message(FATAL_ERROR
        "issue-price-support normalized six-file print-load-print is not byte stable")
endif()

# A modeled economic no-solution remains a successful report. Higher issuer
# uses and zero support capacity eliminate every conditional overlap without
# creating a parse or process error.
file(READ "${issue_price}" no_solution_terms)
string(REPLACE
    "reference_price.issuer_cost_million=0"
    "reference_price.issuer_cost_million=1"
    no_solution_terms "${no_solution_terms}")
string(REPLACE
    "support.maximum_support_million=1.5"
    "support.maximum_support_million=0"
    no_solution_terms "${no_solution_terms}")
set(no_solution_issue
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-none.cfg")
file(WRITE "${no_solution_issue}" "${no_solution_terms}")
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        "${no_solution_issue}"
    RESULT_VARIABLE no_solution_result
    OUTPUT_VARIABLE no_solution_output
    ERROR_VARIABLE no_solution_error
)
if(NOT no_solution_result EQUAL 0)
    message(FATAL_ERROR
        "economic no-solution must exit zero (${no_solution_result}):\n${no_solution_error}")
endif()
foreach(fragment IN ITEMS
        "overall status: no-financeable-window"
        "issuer cost F: 1.000000 DEMO million"
        "maximum non-repayable support capacity G: 0.000000 DEMO million"
        "issuer funding floor max(0,M+F-G): 9.000000 DEMO million"
        "modeled financeable hurdle-case indices: none"
        "modeled conditional financeable window exists: false")
    string(FIND "${no_solution_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "no-solution report is missing: ${fragment}")
    endif()
endforeach()

# A hurdle derived from the very price it tests remains reportable but is
# explicitly barred from establishing a financeable window.
file(READ "${issue_price}" non_independent_terms)
string(REPLACE
    "issue_price_support.synthetic_inputs=true"
    "issue_price_support.synthetic_inputs=false"
    non_independent_terms "${non_independent_terms}")
string(REPLACE
    "hurdle_case.3.reference_price_relation=independent"
    "hurdle_case.3.reference_price_relation=model_implied_from_reference_price"
    non_independent_terms "${non_independent_terms}")
set(non_independent_issue
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-nonind.cfg")
file(WRITE "${non_independent_issue}" "${non_independent_terms}")
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        "${non_independent_issue}"
    RESULT_VARIABLE non_independent_result
    OUTPUT_VARIABLE non_independent_output
    ERROR_VARIABLE non_independent_error
)
if(NOT non_independent_result EQUAL 0)
    message(FATAL_ERROR
        "non-independent hurdle report failed (${non_independent_result}):\n${non_independent_error}")
endif()
foreach(fragment IN ITEMS
        "issue_price_support.synthetic_inputs=false"
        "Hurdle case 2 | id=hurdle-10-percent"
        "relation=model_implied_from_reference_price"
        "status=hurdle-not-independent-of-reference-price"
        "modeled conditional financeable window exists: false"
        "Hurdle provenance and relation to P are independent")
    string(FIND "${non_independent_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "non-independent hurdle report is missing: ${fragment}")
    endif()
endforeach()

# A settled secondary trade can evidence buyer-seller cash and settlement but
# remains evidence-only unless explicit full-claim month-zero normalization is
# asserted. It never establishes primary project funding.
file(READ "${issue_price}" secondary_terms)
string(REPLACE
    "issue_price_support.synthetic_inputs=true"
    "issue_price_support.synthetic_inputs=false"
    secondary_terms "${secondary_terms}")
string(REPLACE
    "reference_price.status=internal_candidate"
    "reference_price.status=settled_secondary"
    secondary_terms "${secondary_terms}")
string(REPLACE
    "reference_price.execution_date=none"
    "reference_price.execution_date=2026-08-28"
    secondary_terms "${secondary_terms}")
string(REPLACE
    "reference_price.settlement_date=none"
    "reference_price.settlement_date=2026-08-29"
    secondary_terms "${secondary_terms}")
string(REPLACE
    "reference_price.source_reference=Unvalidated synthetic reference price for mechanics testing only"
    "reference_price.source_reference=Settled secondary trade record"
    secondary_terms "${secondary_terms}")
string(REPLACE
    "reference_price.evidence_record_id=none"
    "reference_price.evidence_record_id=PRICE-SECONDARY-001"
    secondary_terms "${secondary_terms}")
string(REPLACE
    "reference_price.buyer_cash_payment_evidenced=false"
    "reference_price.buyer_cash_payment_evidenced=true"
    secondary_terms "${secondary_terms}")
string(REPLACE
    "reference_price.settlement_evidenced=false"
    "reference_price.settlement_evidenced=true"
    secondary_terms "${secondary_terms}")
set(secondary_issue
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-secondary.cfg")
file(WRITE "${secondary_issue}" "${secondary_terms}")
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        "${secondary_issue}"
    RESULT_VARIABLE secondary_result
    OUTPUT_VARIABLE secondary_output
    ERROR_VARIABLE secondary_error
)
if(NOT secondary_result EQUAL 0)
    message(FATAL_ERROR
        "evidence-only secondary report failed (${secondary_result}):\n${secondary_error}")
endif()
foreach(fragment IN ITEMS
        "reference status: settled_secondary"
        "settled-secondary full-claim month-zero normalization asserted: false"
        "reference buyer cash payment evidenced: true"
        "reference settlement evidenced: true"
        "subscription-reserve deposit evidenced: false"
        "issuer-cost payment evidenced: false"
        "issue-use evidence record id: none"
        "reference price numerically eligible: false"
        "reference numerical block reason: settled-secondary price lacks explicit normalization to the full month-zero fixed claim"
        "Reference-price hypothetical-primary projection: not applicable"
        "Fixed future-cash physical principal risk"
        "Settled-secondary buyer-to-seller cash never enters the project reserve")
    string(FIND "${secondary_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "evidence-only secondary report is missing: ${fragment}")
    endif()
endforeach()

# Positive observed completion requires coherent settled-primary buyer cash
# plus the exact support cash settled to the same issue. This remains separate
# from hurdle provenance and every broader finance/additionality claim.
file(READ "${issue_price}" settled_terms)
string(REPLACE
    "issue_price_support.synthetic_inputs=true"
    "issue_price_support.synthetic_inputs=false"
    settled_terms "${settled_terms}")
string(REPLACE
    "reference_price.status=internal_candidate"
    "reference_price.status=settled_primary"
    settled_terms "${settled_terms}")
string(REPLACE
    "reference_price.execution_date=none"
    "reference_price.execution_date=2026-08-28"
    settled_terms "${settled_terms}")
string(REPLACE
    "reference_price.settlement_date=none"
    "reference_price.settlement_date=2026-08-29"
    settled_terms "${settled_terms}")
string(REPLACE
    "reference_price.source_reference=Unvalidated synthetic reference price for mechanics testing only"
    "reference_price.source_reference=Settled primary subscription record"
    settled_terms "${settled_terms}")
string(REPLACE
    "reference_price.evidence_record_id=none"
    "reference_price.evidence_record_id=PRICE-PRIMARY-001"
    settled_terms "${settled_terms}")
string(REPLACE
    "reference_price.buyer_cash_payment_evidenced=false"
    "reference_price.buyer_cash_payment_evidenced=true"
    settled_terms "${settled_terms}")
string(REPLACE
    "reference_price.settlement_evidenced=false"
    "reference_price.settlement_evidenced=true"
    settled_terms "${settled_terms}")
string(REPLACE
    "reference_price.subscription_reserve_deposit_evidenced=false"
    "reference_price.subscription_reserve_deposit_evidenced=true"
    settled_terms "${settled_terms}")
string(REPLACE
    "reference_price.issue_use_evidence_record_id=none"
    "reference_price.issue_use_evidence_record_id=ISSUE-USES-001"
    settled_terms "${settled_terms}")
string(REPLACE
    "support.status=synthetic_candidate"
    "support.status=settled_to_issue"
    settled_terms "${settled_terms}")
string(REPLACE
    "support.settled_support_million=0"
    "support.settled_support_million=1.5"
    settled_terms "${settled_terms}")
string(REPLACE
    "support.funding_evidenced=false"
    "support.funding_evidenced=true"
    settled_terms "${settled_terms}")
string(REPLACE
    "support.settlement_evidenced=false"
    "support.settlement_evidenced=true"
    settled_terms "${settled_terms}")
string(REPLACE
    "support.source_reference=Unvalidated synthetic support capacity for mechanics testing only"
    "support.source_reference=Settled issue-support transfer record"
    settled_terms "${settled_terms}")
string(REPLACE
    "support.evidence_record_id=none"
    "support.evidence_record_id=SUPPORT-SETTLED-001"
    settled_terms "${settled_terms}")
set(settled_issue
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-settled.cfg")
file(WRITE "${settled_issue}" "${settled_terms}")
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        "${settled_issue}"
    RESULT_VARIABLE settled_result
    OUTPUT_VARIABLE settled_output
    ERROR_VARIABLE settled_error
)
if(NOT settled_result EQUAL 0)
    message(FATAL_ERROR
        "settled-primary report failed (${settled_result}):\n${settled_error}")
endif()
foreach(fragment IN ITEMS
        "reference status: settled_primary"
        "support status: settled_to_issue"
        "actual settled issue support S_obs: 1.500000 DEMO million"
        "subscription-reserve deposit evidenced: true"
        "issuer-cost payment evidenced: false"
        "issue-use evidence record id: ISSUE-USES-001"
        "documented support commitment covers overlap: true"
        "funded/escrowed support capacity covers overlap: true"
        "funded/escrowed-support-covered price window exists: true"
        "actual observed settled support S_obs: 1.500000 DEMO million"
        "observed primary buyer cash completed: true"
        "observed support cash completed: true"
        "observed issue sources settled and modeled identity reconciled: true"
        "observed settled primary funding completed: true"
        "observed issue sources: 8.000000 DEMO million"
        "capital_mobilization_or_financing_additionality_is_proven=false")
    string(FIND "${settled_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "settled-primary report is missing: ${fragment}")
    endif()
endforeach()

# Settled source cash alone reconciles the cash-source arithmetic but is not
# completed primary funding until use-side reserve evidence is also present.
set(settled_sources_only_terms "${settled_terms}")
string(REPLACE
    "reference_price.subscription_reserve_deposit_evidenced=true"
    "reference_price.subscription_reserve_deposit_evidenced=false"
    settled_sources_only_terms "${settled_sources_only_terms}")
string(REPLACE
    "reference_price.issue_use_evidence_record_id=ISSUE-USES-001"
    "reference_price.issue_use_evidence_record_id=none"
    settled_sources_only_terms "${settled_sources_only_terms}")
set(settled_sources_only_issue
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-sources-only.cfg")
file(WRITE "${settled_sources_only_issue}" "${settled_sources_only_terms}")
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        "${settled_sources_only_issue}"
    RESULT_VARIABLE settled_sources_only_result
    OUTPUT_VARIABLE settled_sources_only_output
    ERROR_VARIABLE settled_sources_only_error
)
if(NOT settled_sources_only_result EQUAL 0)
    message(FATAL_ERROR
        "settled-sources-only report failed (${settled_sources_only_result}):\n${settled_sources_only_error}")
endif()
foreach(fragment IN ITEMS
        "reference status: settled_primary"
        "subscription-reserve deposit evidenced: false"
        "issue-use evidence record id: none"
        "observed issue sources settled and modeled identity reconciled: true"
        "observed settled primary funding completed: false"
        "observed amount entering subscription reserve: not applicable")
    string(FIND "${settled_sources_only_output}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "settled-sources-only report is missing: ${fragment}")
    endif()
endforeach()

# Strict exit taxonomy: 1 command grammar, 2 load/parser, and 3 compatible
# input cross-validation, evaluation, or report-output failure.
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
    RESULT_VARIABLE usage_result
    ERROR_VARIABLE usage_error
)
if(NOT usage_result EQUAL 1)
    message(FATAL_ERROR "missing issue-price input must exit 1")
endif()
string(FIND "${usage_error}" "usage:" position)
if(position EQUAL -1)
    message(FATAL_ERROR "issue-price-support grammar errors must print usage")
endif()
string(FIND "${usage_error}" "[--print-normalized|--json]" position)
if(position EQUAL -1)
    message(FATAL_ERROR "issue-price-support usage must advertise --json")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        "${issue_price}" --unknown-option
    RESULT_VARIABLE option_result
    ERROR_VARIABLE option_error
)
if(NOT option_result EQUAL 1)
    message(FATAL_ERROR "unknown issue-price-support options must exit 1")
endif()

execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        "${issue_price}.missing"
    RESULT_VARIABLE input_result
    ERROR_VARIABLE input_error
)
if(NOT input_result EQUAL 2)
    message(FATAL_ERROR "issue-price-support load errors must exit 2")
endif()
string(FIND "${input_error}"
    "issue-price-support input/configuration failed:" position)
if(position EQUAL -1)
    message(FATAL_ERROR "issue-price-support input failures need a scoped label")
endif()

file(READ "${issue_price}" invalid_analysis_terms)
string(REPLACE
    "reference_price.market_claim_id=market-priority"
    "reference_price.market_claim_id=missing-market-claim"
    invalid_analysis_terms "${invalid_analysis_terms}")
set(invalid_analysis_issue
    "${CMAKE_CURRENT_BINARY_DIR}/issue-price-${suffix}-analysis.cfg")
file(WRITE "${invalid_analysis_issue}" "${invalid_analysis_terms}")
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        "${invalid_analysis_issue}"
    RESULT_VARIABLE analysis_result
    ERROR_VARIABLE analysis_error
)
if(NOT analysis_result EQUAL 3)
    message(FATAL_ERROR "issue-price-support cross-input errors must exit 3")
endif()
string(FIND "${analysis_error}"
    "issue-price-support analysis failed:" position)
if(position EQUAL -1)
    message(FATAL_ERROR
        "issue-price-support analysis failures need a scoped label")
endif()

# Close the report pipe without consuming it. The full per-hurdle witness
# ledger exceeds a platform pipe buffer, so the producer must observe the
# broken destination and return the scoped output-failure status.
execute_process(
    COMMAND ${PROGRAM_COMMAND} "${portfolio}" "${polytope}"
        "${participation}" "${base_stack}" "${priority_cap}"
        "${issue_price}"
    COMMAND "${CMAKE_COMMAND}" -E false
    RESULTS_VARIABLE report_output_results
    ERROR_VARIABLE report_output_error
)
if(NOT report_output_results STREQUAL "3;1")
    message(FATAL_ERROR
        "failed issue-price-support report output must produce pipeline results 3;1, got ${report_output_results}")
endif()
string(FIND "${report_output_error}"
    "issue-price-support analysis failed: failed while writing issue-price-support report"
    position)
if(position EQUAL -1)
    message(FATAL_ERROR
        "issue-price-support report-output failures need the scoped writer diagnostic")
endif()

foreach(error_text IN ITEMS "${usage_error}" "${option_error}"
        "${input_error}" "${analysis_error}" "${report_output_error}")
    string(FIND "${error_text}" "calibrated_execution_authorized=false"
        position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "issue-price-support errors must retain the calibration boundary")
    endif()
endforeach()

file(REMOVE "${temp_portfolio}" "${temp_polytope}"
    "${temp_participation}" "${temp_stack}" "${temp_cap}"
    "${temp_issue}" "${no_solution_issue}" "${non_independent_issue}"
    "${secondary_issue}" "${settled_issue}"
    "${settled_sources_only_issue}" "${invalid_analysis_issue}")
