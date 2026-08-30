# Calibration Binder v0.1 — Verification Record

Status: deterministic synthetic verification, 29 August 2026.

## Verified boundary

Calibration Binder v0.1 was compiled as strict C++20 with MSVC
19.44.35227 using `/W4 /WX /permissive-`. Debug and Release configurations
were tested separately with CMake/CTest 4.3.3 and the Visual Studio 17 2022
generator.

This record verifies parser, confinement, hash, identity, lineage, citation,
and reporting controls. It does not validate a real facility, an evidence
source's truth, a calibration method, physical probabilities, recovery,
expected return, fair value, market price, a rating, legal enforceability, or
investment suitability.

## Controlling fixture

The fixture is deliberately small and invented:

- one project, `synthetic-facility`;
- one dossier, `synthetic-binder-gap-dossier`;
- one joint scenario with probability one;
- `DEMO` currency and `constant synthetic monetary units at analysis close`
  monetary basis;
- one DEMO-million commitment, draw, and principal receipt; and
- a public-research-only gap dossier that fails every decision gate.

The candidate report prints:

```text
candidate_status=structurally-checked-synthetic-candidate
calibrated_execution_authorized=false
dossier_highest_allowed_use=PUBLIC RESEARCH / QUESTION FORMATION ONLY
material_target_count=25
lineage_row_count=25
```

The 25 targets include currency and monetary basis. Every target has exactly
one lineage row; every row is explicitly `synthetic` and cites no evidence.
The fixture therefore tests mechanics without implying empirical support.

## Fail-closed controls

The dedicated tests establish that:

- missing, duplicate, unknown, unsafe, or oversized binder fields fail;
- the binder must be candidate-only and physical-P;
- portfolio and ambiguity files must both retain `synthetic_inputs=true`;
- v0.1 accepts exactly one portfolio project;
- declared project and dossier IDs must match the loaded artifacts exactly;
- bound paths must be portable, confined relative paths that resolve beneath
  the binder directory;
- artifact byte caps are checked before hashing, and lineage line and row
  counts are bounded;
- every raw-file SHA-256 must match before parsing and is rechecked, with its
  resolved path, immediately before success;
- material target coverage is an exact bijection: missing, duplicate, and
  orphan rows fail;
- probability, hurdle, term, capital, transition, dependence, cost, and
  ambiguous cash/recovery targets accept only their declared compatible input
  classes;
- invalid programmatic class or status enum values cannot be normalized;
- observed, contractual, derived, estimated, and transfer rows require
  controlled, current, retained, requirement-qualified gate-use citations;
- a contractual row also requires an executed-contract citation;
- public question-only evidence cannot relabel a probability as observed;
- no probability can carry a source-derived status until a bound empirical
  population and method ledger exists; and
- the CLI reports the hard authorization boundary and rejects unknown options.

The evidence qualification check reuses the evidence gate's compiled
requirement lookup, minimum verification, exact applicability, accepted source
class, retained-copy hash, current-review, conflict, adverse-evidence, and
decision-use controls. One prepared batch validates the dossier, indexes and
qualifies each retained record, and computes each of the 57 requirement
assessments once; lineage citations then use the prepared results. It also
requires the complete cited requirement to pass. Full requirement diagnostics
are stored once, while each record retains only bounded record-specific reasons
and one generic requirement-failure marker. A 2,048-record regression
concentrated on one failed requirement verifies linear reason volume.

## Test record

The full project suite contains 37 CTest cases after this integration. The
three binder additions are:

- `cellular_finance_calibration_binder_tests`;
- `cellular_finance_calibration_binder_config_tests`; and
- `cellular_finance_calibration_binder_cli_synthetic`.

Results:

| Configuration | Strict build | CTest result | Elapsed test time |
|---|---|---:|---:|
| Debug | passed | 37/37 | 14.19 s |
| Release | passed | 37/37 | 4.51 s |

These are the independent final replay times after the audit corrections. A
separate live Release CLI run reproduced the two bound IDs, 25/25 target
coverage, four failing dossier gates, the structural candidate status, and
`calibrated_execution_authorized=false`.

## Independent audit corrections

The first review found that the draft implementation omitted monetary meaning,
did not bind project/dossier scope, accepted arbitrary input classes, and let
public question-only evidence carry a source-derived status. It also identified
pre-hash resource exposure, a hash/parse replacement window, invalid enum
normalization, and overbroad `validated` wording.

Before release, the implementation was changed to include the monetary fields,
bind exact one-project scope, enforce class compatibility and controlled
evidence qualification, apply resource caps before hashing, recheck paths and
hashes after parsing, reject invalid enums, and report only a
`structurally-checked-synthetic-candidate`.

A final re-audit then found repeated per-citation dossier assessment and the
possibility that controlled evidence for an unrelated requirement could label
a probability as source-derived. The released code prepares evidence once per
binder and forbids all source-derived probability statuses until the missing
population/method ledger exists.

The last resource review found that failed requirement reasons could still be
copied into every associated record. The final implementation stores those
diagnostics once, bounds per-record reasons, and removes growing-vector
duplicate searches from the concentrated-requirement loop.

## Residual limitations

Version 0.1 binds a complete target-to-lineage map, but `method_id` remains a
label. It does not yet bind a method definition, exact extracted value and
unit, transformation code, source population, inclusion/censoring rule, or
challenger model. Target-class compatibility is necessarily broad for cash
whose economic type depends on its named source.

The binder also lacks semantic subledgers for milestone eligibility,
qualified-output-to-collected-cash, recovery costs/priority/timing, shared
factor exposure, and empirical probability populations. Final rehashing
narrows but cannot make ordinary filesystem reads an atomic snapshot. Multiple
projects will require a later multi-dossier release with an exact project-to-
dossier crosswalk.

These limitations are why structurally valid execution remains synthetic and
why every report denies calibrated execution, valuation, pricing, rating,
offering, and recommendation.
