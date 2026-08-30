# Partial-Credit Claim-Loss Cohort Binder v0.1 — Verification Record

## Status and boundary

This record documents local software verification performed on 30 August 2026.
It covers the implemented in-memory mechanical-evaluation kernel, its synthetic
Claim Ledger fixtures, arithmetic reconstruction, fail-closed controls, and
integration with the repository suite.

It does **not** verify a controlled population register, real observed losses,
term comparability, source truth, census completeness, an empirical frequency,
a probability, correlation, calibration, price, rating, expected return, or
financeability. The five-file cohort loader, controlled-register hash, Evidence
Gate binding, and empirical admission policy are not implemented. The
authoritative selected-path evidence-provenance seam is implemented: resolved
rows are reloaded in one immutable Claim Ledger package operation that preserves
selected latest entry scope/status/source/date, requested cash-path status,
provider terms, and applicable covenants. The cohort requires an explicitly
complete-resolved path and records that the provenance was verified during
evaluation. It still leaves empirical, Portfolio, and calibrated-execution
authority false.

## Tested environment and result

| Item | Tested value |
|---|---|
| Host and generator | Windows, Visual Studio 17 2022 |
| Compiler and build tools | MSVC 19.44, MSBuild 17.14.40 |
| Selected Windows SDK | 10.0.26100.0 |
| CMake and CTest | 4.3.3 |
| Language mode | C++20, compiler extensions disabled |
| Diagnostics | `/W4 /WX /permissive-` |
| Configurations | Debug and Release |

The complete repository suite was compiled and run under both configurations:

```text
cmake --build build/dev --config Debug --parallel
ctest --test-dir build/dev -C Debug --output-on-failure
Result: 67/67 tests passed in Debug (87.62 seconds).

cmake --build build/release --config Release --parallel
ctest --test-dir build/release -C Release --output-on-failure
Result: 67/67 tests passed in Release (34.28 seconds).
```

The four focused seam regressions also passed in both configurations:

```text
Debug:   4/4 passed; 22.30 seconds total
         Claim Ledger package 5.47 s; cohort 0.88 s;
         Evidence Gate 15.54 s; population CLI 0.16 s
Release: 4/4 passed; 9.82 seconds total
         Claim Ledger package 4.91 s; cohort 0.74 s;
         Evidence Gate 3.94 s; population CLI 0.20 s
```

GCC, Clang, sanitizers, remote CI, and cross-platform deterministic replay were
not run in this local record. No cross-toolchain claim is made.

## Hand reconstruction of the five-member frame

The caller-declared synthetic frame contains five rows: one resolved performing
claim, one resolved loss, one triggered unresolved claim, one not-yet-matured
claim, and one excluded claim. The mechanical count result is:

```text
declared frame       = 5
included             = 4
resolved             = 2
not yet matured      = 1
unresolved           = 1
excluded             = 1
censored/open        = 2

trigger known        = 3
triggered            = 2
trigger unknown      = 1
```

The four included claims each have contractual face `10`, so aggregate face is
`40`. Face is a cap and reference denominator; it is not funded principal. For
the two resolved paths, actual principal conserves as follows (all amounts are
in the fixture's declared million-unit basis):

```text
opening principal                                      0
+ funded principal created                            18
+ capitalized fee/interest principal                   2
= resolved principal roll-forward basis               20

borrower principal cash                               10
+ recovery principal cash                              2
+ provider principal cash                              4
+ conversion                                           0
+ final principal writeoff                             4
= resolved principal disposition                      20
```

The resolved loss itself preserves each economic leg separately: principal
roll-forward basis `10`, recovery `2`, pre-support shortfall `8`, provider claim
generated/payable/paid `4`, and final residual writeoff `4`.

Two open protected claims each have an exact provider lifetime maximum-cash cap
of `4`. Consequently, gross provider principal cash is mechanically bounded by
`4..12`: the resolved `4` plus zero to `8` across the two open rows. That range
is a contractual outer envelope, not a payout estimate. Because contractual
face is only a peak-principal cap and no lifetime cumulative shortfall/writeoff
cap is verified, aggregate open pre-support shortfall and final writeoff remain
`Unknown`.

With the fixed included denominator of four, the compatible positive-outcome
frequencies are:

```text
pre-support shortfall frequency   0.50 .. 0.75
provider-cash frequency           0.25 .. 0.75
final-writeoff frequency          0.25 .. 0.75
provider cash / contractual face  0.10 .. 0.30
writeoff / contractual face       Unknown
```

These are arithmetic compatibility ranges for this synthetic declared frame.
They are not sampling intervals or estimates of future probabilities, and the
different endpoints are not asserted to share one common joint witness.

## Face-versus-principal oracle

An additional underdrawn resolved fixture changes contractual face to `12`
while retaining funded principal `9` and capitalized principal `1`. Its
principal roll-forward basis remains `10`, discharged by recovery `2`, provider
cash `4`, and writeoff `4`. The test therefore catches the financially material
error of treating undrawn commitment capacity as funded principal loss.

## Adversarial controls exercised

The focused test verifies or rejects, as applicable:

- mutable caller summaries, by reloading the declared Claim Ledger root, and a
  mismatched expected SHA-256 root;
- declared frame-count mismatch, duplicate observation IDs, and duplicate
  economic clusters anywhere in the frame, including exclusions;
- exclusions frozen after eligibility, included `not-applicable` trigger
  statuses, invalid enums, impossible date ordering, and triggered rows falsely
  left in the immature class;
- resolved trigger classifications that contradict verified claim cash;
- invented, omitted, or mismatched provider identity, including when face was
  not known at the decision cut;
- currency or monetary-basis mismatch and oversized retained evidence-ID lists;
- unknown provider amounts becoming false zeroes, and zero coverage or exact
  zero face manufacturing a positive provider payout;
- a triggered positive-shortfall classification attached to exact zero face;
- an exact positive `1e-11` face or provider term being collapsed by a
  materiality tolerance; and
- separate principal, provider-cash, and horizon-settlement reconciliation;
- resolved rows whose raw cash-path status is missing or not
  `complete-resolved`;
- unsafe requested scenario identifiers before package I/O;
- missing ordinary source-manifest metadata while preserving the two reserved
  absence markers; and
- common-versus-scenario selected-entry scope without cross-scenario leakage.

The focused result also confirms sorted observation output, propagation of
synthetic/package blockers, and distinct known-positive versus
possible-positive unpaid-claim counts.

## Residual implementation boundary and next evidence step

The current API accepts a caller-constructed in-memory frame. It does not parse
or hash-bind `cohort.cfg`, `observations.tsv`, `methods.cfg`, `dossier.cfg`, and
`evidence_manifest.tsv`; prove that all issued or at-risk claims were included;
run the Evidence Gate; authenticate classifications; retain authoritative
population/method/classification admission; or emit the full source and method
lineage in its result. It does consume authoritative selected-entry
status/source/date provenance for resolved rows and rejects a non-complete path
attestation. The regression therefore validates mechanical bookkeeping, this
provenance seam, and fail-closed boundaries only.

The five-file loader and controlled population acquisition are intentionally
deferred from the present checkpoint. If later authorized, they can consume the
implemented Claim Ledger snapshot without reparsing cash into a second ledger.
Only after population, method, classification, status, source, date, and
retained-copy admission passes may a separate study estimate marginal loss
ranges. Dependence must still be tested independently before those marginals can
inform a pooled instrument, premium, or investor-return analysis.
