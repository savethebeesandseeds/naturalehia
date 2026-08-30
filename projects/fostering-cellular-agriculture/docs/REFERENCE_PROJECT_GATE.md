# Reference-Project Evidence Gate

**Project:** Fostering Cellular Agriculture  
**Specification:** version 0.2  
**Effective date:** 27 August 2026  
**Implementation:** `evidence_gate.hpp`, `evidence_gate.cpp`, and
`naturalehia-evidence-gate`

## Purpose

The evidence gate is a deterministic control over what the project may do with
a reference-project dossier. It asks:

> Which analytical uses are supported by the exact records admitted as of an
> explicit evaluation date?

It does not score a company, authenticate source truth, price an instrument, or
replace technical, legal, regulatory, tax, accounting, credit, or impact
review. Repetition does not turn a public claim into an underwriting input.

Version 0.2 is intentionally fail-closed. It adds atomic legal and asset
boundaries, conjunctive source groups, partial-evidence status, current
evaluation dates, hash-checked retained copies, reviewer and approval
fields, conflict status, and resolvable adverse-evidence history.

## Four gates, no blended score

A strength in one area cannot compensate for a missing obligation, absent run
ledger, unresolved lien, or unsupported animal-impact claim.

| Gate | Decision controlled | What a pass permits |
|---|---|---|
| Reference boundary | Is the current entity, controller, asset, product, process, site, IP, jurisdiction, regulatory boundary, and financing purpose defined? | Organize controlled work around one exact reference project. |
| Model calibration | Are operations, failures, physical balances, costs, design, capex, schedule, supply, utilities, O&M, insurance, closure, and independent review evidenced? | Calibrate a controlled project model, subject to separate model validation. |
| Cross-structure diligence | Are the financing failure, rejected ordinary alternatives, regulation, demand, acceptance, credit, capital, professional analysis, completion, recovery, and shared mechanics evidenced? | Complete the common evidence checklist needed before separate structure-specific review. It does not approve any instrument. |
| Animal-impact claims | Are additionality, displacement, inputs, financing, output and sales, buyer substitution, attribution, welfare conversion, and assurance evidenced? | Conduct an eligibility review for a narrowly scoped observed report. |

The highest allowed use remains sequential:

```text
public research / question formation
    -> defined reference / data acquisition
    -> controlled model calibration
    -> cross-structure diligence checklist complete
    -> impact reporting eligibility review
```

Every report also prints:

```text
EXECUTION READINESS: NOT ASSESSED BY THIS GATE
```

A pass is not bankability, investment approval, a rating, enforceability,
regulatory approval, an executable term sheet, a fair-value opinion, or an
offering document.

## Compiled requirements

The following 57 requirements are compiled into the C++ library. Editing a
dossier cannot lower them.

### Reference boundary — 11 requirements

| Identifier | Requirement |
|---|---|
| `REF-LEGAL-OPERATOR` | Current legal operator |
| `REF-FINANCING-OBLIGOR-AUTHORITY` | Financing obligor and authorized controller |
| `REF-FACILITY-SITE` | Facility and site boundary |
| `REF-ASSET-PERIMETER` | Owned, leased, shared, and collateral assets |
| `REF-PRODUCT-SPEC` | Financeable product specification |
| `REF-PROCESS-BOUNDARY` | Process and unit-operation boundary |
| `REF-JURISDICTION` | Operating jurisdiction |
| `REF-GOVERNING-LAW` | Governing law and dispute forum |
| `REF-USE-OF-FUNDS` | Defined financing use of funds |
| `REF-REGULATORY-FACILITY-SCOPE` | Facility regulatory scope |
| `REF-IP-RIGHTS-CONTINUITY` | IP title, freedom to operate, and insolvency continuity |

### Model calibration — 15 requirements

| Identifier | Requirement |
|---|---|
| `TEC-COMPLETE-RUN-HISTORY` | Complete successful run history |
| `TEC-FAILED-RUNS` | Failed and contaminated run history |
| `TEC-MASS-BALANCE` | Batch mass balance and yield |
| `TEC-UTILITY-COST` | Metered utilities and consumables |
| `TEC-INDEPENDENT-REVIEW` | Independent technical review |
| `CON-DESIGN-BASIS` | Issued design basis |
| `CON-CAPEX-QUOTES` | Vendor-backed capex and commitments |
| `CON-SCHEDULE` | Resource-loaded construction and commissioning schedule |
| `CON-CONTINGENCY` | Quantified cost and schedule contingency |
| `FIN-OPERATING-LEDGER` | Operating ledger and unit economics |
| `TEC-CRITICAL-SUPPLY` | Critical-input supply, concentration, and termination |
| `CON-UTILITY-CAPACITY` | Utility, wastewater, tariff, and curtailment capacity |
| `TEC-OM-RESILIENCE` | O&M, staffing, data integrity, cyber, and continuity |
| `FIN-INSURANCE` | Insurance limits, exclusions, claims, and interruption cover |
| `CON-DECOMMISSIONING` | Closure, decommissioning, remediation, and assurance |

### Cross-structure diligence — 22 requirements

| Identifier | Requirement |
|---|---|
| `REG-PRODUCT-SCOPE` | Current product regulatory scope |
| `REG-OPEN-CONDITIONS` | Open regulatory conditions and controls |
| `FIN-FINANCING-FAILURE-DIAGNOSIS` | Reconciled financing history and independently reviewed binding constraint |
| `FIN-CAPITAL-ALTERNATIVES` | Direct capital-provider evidence and rejected ordinary alternatives |
| `COM-EXECUTED-OFFTAKE` | Executed offtake or reservation obligation |
| `COM-ACCEPTANCE` | Contractual and operational output acceptance protocol |
| `COM-COUNTERPARTY-CREDIT` | Counterparty credit evidence |
| `FIN-SOURCES-USES` | Reconciled sources and uses |
| `FIN-TERM-SHEET` | Current controlled financing terms |
| `FIN-TAX-ANALYSIS` | Independent tax characterization |
| `FIN-ACCOUNTING-ANALYSIS` | Independent accounting and valuation analysis |
| `FIN-LEGAL-ANALYSIS` | Independent legal and regulatory characterization |
| `CON-COMPLETION-TEST` | Contractual completion and cure test |
| `FIN-CLAIMS-RECOVERY` | Claims, liens, enforcement, insolvency waterfall, and recovery |
| `FIN-RECOVERY-VALUATION` | Claims register, recovery values, costs, and distribution waterfall |
| `FIN-SPONSOR-EQUITY` | Executed sponsor equity and proof of funds |
| `INS-MILESTONE-MECHANICS` | Staged-capital milestone, draw, cure, and workout mechanics |
| `INS-PRICE-INDEX-GOVERNANCE` | Benchmark, transaction history, basis, and manipulation controls |
| `INS-PRICE-SETTLEMENT-MECHANICS` | Physical notional, settlement, cap, fallback, and disruption mechanics |
| `INS-SUPPORT-PROVIDER-CREDIT` | Provider authority, capacity, and collateral |
| `INS-SUPPORT-CLAIMS-PROTOCOL` | Executed trigger, claims, exclusions, and recovery protocol |
| `INS-COLLATERAL-CLOSEOUT` | Collateral, liquidity, netting, close-out, and recovery controls |

This is a common diligence checklist, not an instrument-approval gate. It does
not select or validate a structure-specific supplement. Milestone capital,
qualified-output reservations, price floors/CfDs, and completion or credit
support each still require separate legal, economic, credit, accounting, tax,
operational, and model review. A pass only establishes that the common
cross-structure evidence set is complete under this schema.

### Animal-impact claims — 9 requirements

| Identifier | Requirement |
|---|---|
| `IMP-FINANCING-ADDITIONALITY` | Pre-registered financing-additionality method |
| `IMP-DISPLACEMENT-METHOD` | Displacement, substitution, leakage, and rebound method |
| `IMP-ANIMAL-INPUT-BASELINE` | Actual animal-derived input baseline |
| `IMP-OBSERVED-FINANCING` | Observed financing decision, funding, terms, timing, and counterfactual |
| `IMP-OBSERVED-OUTPUT-SALES` | Reconciled released, sold, and paid output and cultivated fraction |
| `IMP-BUYER-SUBSTITUTION` | Direct buyer conventional-purchasing and substitution evidence |
| `IMP-ATTRIBUTION-RESULT` | Observed attribution, leakage, rebound, and uncertainty result |
| `IMP-OUTCOME-ASSURANCE` | Independent observed-outcome assurance and correction release |
| `IMP-WELFARE-CONVERSION` | Welfare conversion, yield, coproduct allocation, and uncertainty |

A method alone cannot pass this gate. Observed financing and market outcomes
are separately required. “Animals saved” remains prohibited unless the exact
observed evidence, conversion method, attribution, uncertainty, and independent
review support that phrase.

## Manifest schema 0.2

The manifest is strict tab-separated data with exactly 29 columns:

```text
record_id	requirement_id	assertion_status	source_class	verification	applicability	source_date	access_date	next_review_date	record_owner	source_uri	retained_copy	retained_sha256	document_version	extract_reference	confidentiality	adverse_evidence	resolution_status	resolved_by	resolution_date	resolution_authority	resolution_basis	decision_use	verified_by	verification_date	verification_procedures	approved_by	conflict_status	limitations
```

Unknown requirement IDs, duplicate record IDs, unknown values, missing fields,
surrounding whitespace, incoherent dates, unsafe paths, and invalid sentinel
combinations are rejected.

### Controlled vocabulary

- `assertion_status`: `supports`, `partial`, `contradicts`, `mixed`,
  or `gap`.
- `source_class`: `none`, `regulator`, `government-disclosure`,
  `court-record`, `public-filing`, `executed-contract`,
  `operator-disclosure`, `independent-report`, `operational-record`,
  `engineering-record`, `financial-record`, `capital-provider-record`,
  `market-data-record`, `counterparty-record`, `academic-research`,
  `media`, `legal-opinion`, `accounting-analysis`, or `tax-analysis`.
- `verification`: `V0` through `V3`.
- `applicability`: `illustrative`, `adjacent`, `near`, or `exact`.
- `decision_use`: `question-only`, `corroboration`, or `gate`.
- `resolution_status`: `not-applicable`, `open`, or `resolved`.
- `conflict_status`: `not-assessed`, `none-disclosed`, `managed`, or
  `unresolved`.

A government announcement is not a regulator record, a court-docket mirror is
not a retained court order, and sponsor assertions do not become regulator
evidence merely because an agency file reproduces them.

## Qualification rules

One record qualifies as support only if all applicable controls pass:

1. its status is `supports`; `partial` can never close the requirement;
2. the dossier is `controlled-diligence`;
3. the record is explicitly authorized for `gate` use;
4. verification, exact-project applicability, and source class meet the
   compiled requirement;
5. dated source, access, verification, review, dossier, and evaluation dates
   are coherent and the review is current at the explicit evaluation date;
6. the retained copy is a regular file whose canonical path remains inside the
   dossier directory;
7. its lowercase SHA-256 matches the retained file;
8. owner, URI, document version, extract reference, verifier, verification
   procedure, and approver are present and not unresolved sentinels;
9. conflict status is cleared or managed; and
10. every compiled primary and conjunctive source group has a separate
    qualifying record.

Primary and conjunctive masks are compile-time checked to be disjoint. One
record therefore cannot satisfy both sides of a two-source requirement.

Metadata such as operator, obligor, controller, asset perimeter, governing law,
use of funds, and buyer cannot contain `NONE`, `UNKNOWN`, `TBD`,
`PENDING`, `UNRESOLVED`, `NOT_ESTABLISHED`, or decorated equivalents when its
requirement passes. The same unresolved-marker check applies to controlled
record provenance and resolution metadata. The public API revalidates all
loader invariants after in-memory mutation.

## Adverse evidence and resolution

`mixed` and `contradicts` records must be flagged adverse. Open adverse
evidence blocks its requirement.

A resolved adverse record remains in history and must:

- link to a later `supports` record for the same requirement;
- name its own adverse record ID in the resolution basis;
- state a resolution date and authority;
- link to support accessed after the adverse record and reviewed before the
  resolution date; and
- use a resolution date no later than the dossier as-of date; and
- have a resolution target that still qualifies on the evaluation date.

If the target becomes stale or otherwise fails, the adverse record is counted
as unresolved again. A controlled dossier must retain the adverse source and
extract as well as the resolving source.

## Dossier governance

Every dossier must set all four commitments to `true`:

```text
governance.negative_evidence_preserved=true
governance.public_claims_not_model_calibration=true
governance.no_bankability_claim_without_gate=true
governance.no_animal_impact_claim_without_gate=true
```

The schema is intentionally incompatible with version 0.1. An old dossier or
manifest fails instead of being silently interpreted under new rules.

## CLI behavior

```text
naturalehia-evidence-gate <dossier.cfg> <evidence_manifest.tsv>
    [--evaluation-date YYYY-MM-DD] [--report-only]
```

| Exit | Meaning |
|---:|---|
| 0 | All gates passed, or non-enforcing `--report-only` was explicitly used |
| 1 | Invalid dossier, manifest, file, or internal evidence invariant |
| 2 | Invalid command-line usage |
| 3 | Valid dossier, but one or more gates failed |

`--report-only` prints a conspicuous non-enforcing banner. It suppresses only
the process status, never the printed gate result. Invalid dates are usage
errors. The default evaluation date is the current UTC date; releases should
supply an explicit date.

## Current Wilson result

At the 27 August 2026 evaluation date, the Wilson public dossier is valid and
fails all four gates:

| Gate | Result |
|---|---:|
| Reference boundary | 0 / 11 |
| Model calibration | 0 / 15 |
| Cross-structure diligence checklist | 0 / 22 |
| Animal-impact claims | 0 / 9 |

Its highest permitted use is **PUBLIC RESEARCH / QUESTION FORMATION ONLY**.
This is not a zero assessment of the asset. It is a refusal to convert absent,
unretained, partial, conflicted, or unauthorized evidence into a positive
decision.

## Residual limitations

The recorded SHA-256 checks only file-to-row consistency while the manifest is
held fixed. A coordinated replacement of both file and manifest is not detected
without an externally anchored signed release root or equivalent control. A
hash also does not authenticate origin, signature, completeness, or truth.
Reviewer names and procedures remain assertions that must be governed outside
this executable check. The current model-readiness gate also requires exact operating history;
a future stage-aware version should distinguish pre-FID engineering stress,
construction monitoring, commissioning/ramp calibration, and operating-history
calibration without allowing near-scale evidence to prove achieved output.
