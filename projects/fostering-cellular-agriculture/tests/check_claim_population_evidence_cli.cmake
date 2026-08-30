# SPDX-License-Identifier: MIT

if(NOT DEFINED PROGRAM OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "PROGRAM and WORK_DIR are required")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}/retained")
file(WRITE "${WORK_DIR}/retained/population.txt" "abc")
file(WRITE "${WORK_DIR}/retained/review.txt" "abc")

file(WRITE "${WORK_DIR}/dossier.cfg"
    "dossier.schema_version=0.3.0\n"
    "dossier.id=synthetic-cli-claim-population\n"
    "dossier.as_of_date=2026-08-30\n"
    "dossier.status=controlled-diligence\n"
    "dossier.subject_kind=claim-population\n"
    "dossier.owner=synthetic-cli-owner\n"
    "population.authority_legal_name=Synthetic Program Authority\n"
    "population.program_or_book_id=synthetic-cli-book\n"
    "population.scope=Complete issued-or-at-risk protected claim register\n"
    "population.reporting_currency=TEST\n"
    "governance.negative_evidence_preserved=true\n"
    "governance.public_claims_not_model_calibration=true\n"
    "governance.no_bankability_claim_without_gate=true\n"
    "governance.no_animal_impact_claim_without_gate=true\n")

set(manifest_header
    "record_id\trequirement_id\tassertion_status\tsource_class\tverification\tapplicability\tsource_date\taccess_date\tnext_review_date\trecord_owner\tsource_uri\tretained_copy\tretained_sha256\tdocument_version\textract_reference\tconfidentiality\tadverse_evidence\tresolution_status\tresolved_by\tresolution_date\tresolution_authority\tresolution_basis\tdecision_use\tverified_by\tverification_date\tverification_procedures\tapproved_by\tconflict_status\tlimitations\n")
set(authority_row
    "POP-AUTHORITY\tFIN-CLAIM-POPULATION-FRAME\tsupports\tcapital-provider-record\tV3\texact\t2026-08-01\t2026-08-02\t2027-08-01\tsynthetic-cli-owner\tcontrolled://synthetic/population\tretained/population.txt\tba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\tsynthetic-v1\tsection-1\tcontrolled\tfalse\tnot-applicable\tNONE\tNONE\tNONE\tNONE\tgate\tSynthetic Verifier\t2026-08-02\tpopulation provenance review\tSynthetic Approver\tnone-disclosed\tsynthetic test evidence only\n")
set(independent_row
    "POP-INDEPENDENT\tFIN-CLAIM-POPULATION-FRAME\tsupports\tindependent-report\tV3\texact\t2026-08-01\t2026-08-02\t2027-08-01\tsynthetic-cli-owner\tcontrolled://synthetic/review\tretained/review.txt\tba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\tsynthetic-v1\tsection-1\tcontrolled\tfalse\tnot-applicable\tNONE\tNONE\tNONE\tNONE\tgate\tSynthetic Verifier\t2026-08-02\tindependent completeness challenge\tSynthetic Approver\tnone-disclosed\tsynthetic test evidence only\n")

file(WRITE "${WORK_DIR}/evidence_manifest.tsv"
    "${manifest_header}${authority_row}${independent_row}")
execute_process(
    COMMAND "${PROGRAM}"
        "${WORK_DIR}/dossier.cfg"
        "${WORK_DIR}/evidence_manifest.tsv"
        --evaluation-date 2026-08-30
    RESULT_VARIABLE passing_result
    OUTPUT_VARIABLE passing_output
    ERROR_VARIABLE passing_error)
if(NOT passing_result EQUAL 0)
    message(FATAL_ERROR
        "passing population profile returned ${passing_result}\n"
        "stdout:\n${passing_output}\nstderr:\n${passing_error}")
endif()
if(NOT passing_output MATCHES "PASS  FIN-CLAIM-POPULATION-FRAME" OR
   NOT passing_output MATCHES "Reference-project permissions: NOT APPLICABLE")
    message(FATAL_ERROR
        "passing population report lacks required disclosure\n${passing_output}")
endif()

file(WRITE "${WORK_DIR}/evidence_manifest.tsv"
    "${manifest_header}${authority_row}")
execute_process(
    COMMAND "${PROGRAM}"
        "${WORK_DIR}/dossier.cfg"
        "${WORK_DIR}/evidence_manifest.tsv"
        --evaluation-date 2026-08-30
    RESULT_VARIABLE failing_result
    OUTPUT_VARIABLE failing_output
    ERROR_VARIABLE failing_error)
if(NOT failing_result EQUAL 3)
    message(FATAL_ERROR
        "authority-only population profile returned ${failing_result}\n"
        "stdout:\n${failing_output}\nstderr:\n${failing_error}")
endif()
if(NOT failing_output MATCHES "FAIL  FIN-CLAIM-POPULATION-FRAME" OR
   NOT failing_output MATCHES "required conjunctive source group")
    message(FATAL_ERROR
        "authority-only population report lacks fail-closed disclosure\n${failing_output}")
endif()
