# Project Claim Ledger v0.1 — Verification Record

## Status and boundary

This record documents local software verification performed on 30 August
2026. It covers the deterministic one-project, one-claim ledger, its bound
package loader, command-line report, and three canonical fixtures. It is not
independent model validation, legal review, source authentication, empirical
calibration, a credit opinion, a market price, or an investment
recommendation.

The ledger's verified job is narrower: keep transaction cash, contractual
balances, settlement paths, guarantees, conversion, and the information-time
boundary internally consistent before any claim is sent to a project,
portfolio, return, or pricing model. Its rules are defined in the
[Project Claim Ledger v0.1](PROJECT_CLAIM_LEDGER_V0_1.md).

The verified package model keeps two configurations separate: a frozen
decision-time snapshot used for ex-ante arithmetic, and an optional full
retained-state snapshot used only for backtest reconstruction. Later evidence
cannot be imported into the decision snapshot merely because it is present in
the directory.

## Tested environment

| Item | Tested value |
|---|---|
| Host and generator | Windows, Visual Studio 17 2022 |
| Compiler and build tools | MSVC 19.44, MSBuild 17.14.40 |
| Selected Windows SDK | 10.0.26100.0 |
| CMake and CTest | 4.3.3 |
| Language mode | C++20, compiler extensions disabled |
| Diagnostics | `/W4 /WX /permissive-` |
| Configurations | Debug and Release |

GCC, Clang, sanitizers, and remote CI were not run in this local record. No
cross-toolchain reproducibility claim is made.

## Commands and results

The development build was configured and compiled with the project's strict
warnings-as-errors preset, then the complete suite was run:

```text
cmake --preset dev
cmake --build --preset dev --parallel
ctest --test-dir build/dev -C Debug --output-on-failure

Result: 67/67 tests passed in Debug (87.62 seconds).
```

The independently configured optimized build retained warnings as errors:

```text
cmake -S . -B build/release -DBUILD_TESTING=ON
  -DNATURALEHIA_CELLULAR_FINANCE_BUILD_CLI=ON
  -DNATURALEHIA_CELLULAR_FINANCE_WARNINGS_AS_ERRORS=ON
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
cmake --build build/release --config Release --parallel
ctest --test-dir build/release -C Release --output-on-failure

Result: 67/67 tests passed in Release (34.28 seconds).
```

Three registered tests directly cover this component:

1. `cellular_finance_claim_ledger_tests` exercises ledger mechanics and
   adversarial accounting cases;
2. `cellular_finance_claim_ledger_package_tests` exercises the closed package
   format, hashes, source resolution, and all three fixtures; and
3. `cellular_finance_claim_ledger_cli` checks deterministic reporting and the
   three explicit readiness requirements.

The other 64 tests guard the existing financial engines and adapters against
integration regressions.

The package regression additionally exercises the on-demand one-scenario
full-path evidence snapshot. In the focused seam run it passed in both Debug
(`5.47` seconds) and Release (`4.91` seconds).

## Independent hand reconstruction

The complete synthetic fixture uses invented values only. Monetary amounts are
millions of the declared synthetic unit; the terminal period is month 12.

| Item | Hand reconstruction | Engine result |
|---|---:|---:|
| Buyer cash outflow at issue | `9.0 price + 0.2 direct cost` | 9.200000 |
| Investor cash flow at issue | `-9.0 - 0.2 + 0.3 cash fee` | -8.900000 |
| Expected terminal receipts | `0.8 x 11 + 0.2 x (2 + 4)` | 10.000000 |
| Expected total receipts | `0.3 + 10.0` | 10.300000 |
| NPV at 10% | `-8.9 + 10 / 1.1` | 0.190909 |
| Expected principal loss | `0.2 x 4` | 0.800000 |
| Principal-cash WAL | all principal cash at month 12 | 12.000000 |
| Expected-cash rate preimage | `10 / 8.9 - 1` | 0.123596 |
| Peak expected EAD | `0.8 x 11 + 0.2 x 10` | 10.800000 |
| Expected provider claim and cash | `0.2 x 4` | 0.800000 |

The fixture deliberately uses stress probabilities. It is mechanically ready
for expected-return arithmetic but is not an admissible market observation or
an empirical estimate. This distinction is present in the report rather than
being inferred from a successful calculation.

## Accounting and state invariants exercised

The ledger tests establish the following implemented controls:

- `UNKNOWN` has no numerical payload and cannot become zero through parsing,
  default construction, or reporting;
- each retained row has a unique entry ID and a stable economic-fact ID;
  latest-known versions replace rather than add, equal-time competitors fail,
  and a correction cannot change accounting kind, event group, or provider;
- primary buyer price, buyer direct costs, borrower gross and net proceeds,
  borrower third-party costs, cash fees, capitalized fees, original-issue
  discount, original-issue premium, and funded principal are separate entries
  with one joint closing equality system;
- positive discount and premium cannot coexist in one funding event, and
  pairwise-overlapping bounded identities without a common value fail;
- principal and accrued interest have separate rollforwards, while amounts due
  remain separate from amounts actually paid;
- maturity is bound to the declared calendar origin: no positive funding is
  admitted after maturity, the full pre-resolution balance must be due from
  maturity onward, and closing balances must equal outstanding due balances;
- borrower cash, recovery cash, guarantee cash, non-cash conversion, and
  writeoff reduce claims through explicit and bounded paths;
- conversion units are not money and never enter investor cash without a later
  external realization record;
- a guarantee claim is derived from the covered shortfall, deductible, cap,
  allocation, and lag, and actual provider cash cannot exceed that claim;
- provider shortfall, deductible, and cap are cumulative-lifetime mechanics;
  a later cure or other underlying resolution after allocated shortfall makes
  the provider path explicitly uncomputable rather than being guessed, and
  capitalized interest cannot create both an interest and principal claim;
- provider obligation priority is enforced by cash type: principal cash cannot
  consume an interest-payable claim, or vice versa, merely because the total
  provider cap has room;
- multiple providers cannot allocate more than 100% within either the
  principal or interest category; full principal-only and interest-only
  allocations remain independent;
- covenant breach, cure, waiver, non-exercise, consent, default, and
  acceleration are explicit states; a breach is not silently promoted into a
  cash consequence;
- records known after the decision period remain backtest evidence and cannot
  rewrite the ex-ante expected return; later scalars, scenario probabilities,
  completeness status, conversion metadata, entries, provider terms, and
  covenants are excluded or masked in the frozen decision configuration;
- a retained full/backtest configuration is evaluated separately and is
  explicitly unavailable if a later row is incomplete or contradictory;
- the same immutable package load can return one requested full-path evidence
  snapshot with selected-latest entry scope, input status, source/date,
  retained-copy state, raw cash-path status, provider terms, and applicable
  covenants without reparsing or recalculating cash;
- unsafe requested scenario IDs fail before package I/O, missing ordinary
  source metadata hard-fails, and the reserved `SYNTHETIC` and
  `NO_PUBLIC_SOURCE` markers retain no invented date;
- common and scenario-specific selected entries and covenants remain
  distinguishable without cross-scenario leakage;
- scenario probability evidence and cash-path-completeness evidence are
  separately classified and sourced;
- expected-return, NPV, and market-observation admission are separate labels:
  successful mechanics alone grants none of them;
- every scenario's primary buyer-price period must contain the declared
  funding and settlement dates, so one path cannot mask another path's timing;
- market-observation admission requires a common settlement-dated primary
  closing, observed buyer cash, observed-or-derived bridge facts,
  transaction-capable provenance, no scenario funding in or before the anchor,
  and no later correction or newly discovered primary closing fact;
- negative, non-finite, unsafe, overpaid, overlapping, unexplained, and
  out-of-horizon mechanical states fail rather than being normalized away; and
- standard-sign expected cash must have a numerical root inside the disclosed
  annual-effective guardrail before a rate preimage is reported.

The package tests additionally reject path traversal, altered SHA-256-bound
bytes, a changed root snapshot, a forged mutable report object, unknown
configuration keys, duplicate or unresolved source IDs, duplicate retained
paths, empty or blank evidence, unsupported source URIs or provenance, wrong
TSV headers, malformed typed values, invalid source markers, unresolved
clusters, excessive rows/bytes/path-selection work/blocker output, and
incomplete observation-admission inputs. The public report reloads and verifies
the exact `claim.cfg` root before rendering, and prints round-trip numerical
values, the analysis basis, expected cash schedule, and bound fingerprints.

## Canonical package results

| Package | Integrity | Core evaluation | Observation admission |
|---|---|---|---|
| Synthetic one-claim fixture | verified | performed | blocked: synthetic stress inputs |
| Liberation Labs public fixture | verified | not performed | blocked |
| Solar Foods public fixture | verified | not performed | blocked |

For Liberation Labs, the package preserves public note terms and later
conversion context but refuses to invent the economic-cluster boundary,
settlement price, proceeds, claim balance, complete cash rights, recovery, or
ex-ante state probabilities.

For Solar Foods, the package preserves the reported draw, capitalized
commission, principal context, floating-rate and instalment terms, named
guarantee providers, and later covenant history. It refuses to transform those
facts into complete lender cash, guarantee allocations, recovery, maturity, or
May 2022 ex-ante probabilities.

Both public directories therefore remain useful, hash-bound incomplete
records. Package integrity does not imply financeability. The correct result is
`core_config_ready=false`, `core_evaluation=not-performed`, and
`observation_admissible=false`.

## CLI controls

The command-line application accepts one bound package and optional hard
requirements:

```text
naturalehia-claim-ledger <claim.cfg>
naturalehia-claim-ledger <claim.cfg> --require-expected-return
naturalehia-claim-ledger <claim.cfg> --require-rate-preimage
naturalehia-claim-ledger <claim.cfg> --require-observation-admission
```

An honest incomplete package prints package metadata, row counts, bound hashes,
and explicit blockers and exits successfully as a record review. It does not
render every retained term value; those remain in the hash-bound input tables.
A requested readiness condition that is not met exits with code 3. Invalid CLI
use exits with code 2. Structural or package errors fail the load; no partial
economic result is emitted. Output-stream failure is checked before a success
exit is returned.

## Residual limitations and next verification work

- The core represents one claim, one native currency and monetary basis, and a
  finite monthly horizon. Aggregation and currency conversion need separately
  evidenced rules.
- Scenario paths and probabilities are inputs. This component does not
  estimate technical success, dependence, recovery, or investor beliefs.
- The software checks financial identities, not the truth of a source, legal
  enforceability, security priority, bankruptcy treatment, tax, or accounting
  classification.
- Provider-claim mechanics do not establish provider solvency or collection;
  counterparty-credit stress belongs in the separate provider model.
- Provider v0.1 uses cumulative lifetime allocated shortfall, one lifetime
  deductible, and one lifetime cap. Per-occurrence deductibles and later-cure,
  cancellation, subrogation, or reallocation mechanics require a later schema.
- Conversion units have no assumed price, liquidity, dilution treatment, or
  cash realization.
- The ledger does not itself create a pool, tranche, diversification benefit,
  fair value, market hurdle, rating, investor demand, or financing
  additionality.
- A production adapter must still map a complete ledger into the common
  project interface without changing cash source, principal classification,
  resolution state, or information time.
- Independent source review, legal mapping, cross-toolchain testing, empirical
  calibration, historical backtesting, and external model validation remain
  necessary before decision use.

This verification supports the ledger as an honest normalization boundary for
continued financial-engineering research. It does not make either retained
public transaction investable.
