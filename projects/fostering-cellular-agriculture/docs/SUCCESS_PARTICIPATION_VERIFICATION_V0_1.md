# Robust Success-Participation v0.1 Verification

**Verification date:** 2026-08-29  
**Scope:** reusable ambiguity projection, participation-term kernel, strict
configuration, synthetic fixture, command-line report, and full-project
regression suite.

## Result

The implementation passed the complete 22-test project suite in MSVC Debug and
Release with warnings treated as errors. The published synthetic report
reconciles to the independent hand calculation and reports the economically
correct result: no fraction in the supported contractual range `[0,1]` reaches
the zero robust-NPV target.

This verifies deterministic mechanics and internal reconciliations. It does
not validate any real cellular-agriculture cash flow, probability, legal right,
investor hurdle, market price, or fair value.

## Environment

| Item | Verified value |
|---|---|
| Operating system | Microsoft Windows NT 10.0.26200.0 |
| CMake / CTest | 4.3.3 |
| Generator | Visual Studio 17 2022 |
| Windows SDK | 10.0.26100.0 |
| Compiler | MSVC 19.44.35227.0 |
| Language | C++20, extensions off |
| Compile policy | `/W4 /permissive- /WX` |
| Configurations | Debug and Release |

The isolated build was configured with:

```powershell
cmake -S . -B .codex-success-participation-build `
  -DNATURALEHIA_CELLULAR_FINANCE_BUILD_CLI=ON `
  -DNATURALEHIA_CELLULAR_FINANCE_WARNINGS_AS_ERRORS=ON
```

Debug and Release each completed `22/22` CTest cases. The suite includes all
pre-existing facility, staged-capital, portfolio, adapter, evidence, ambiguity,
parser, and CLI regressions as well as the new term tests.

An initial highly parallel Release build reported `MSB6006` for one test
linker invocation without an underlying linker diagnostic. The exact target
immediately built serially with zero warnings and errors; a complete serial
Release build and all 22 Release tests then passed. No source change was used
to clear it. This is recorded as a transient build-environment event, not
silently omitted.

## Independent fixture calculation

The fixture has two projects, each drawing `10` at month zero; pool cost is
`0.2`. A successful project returns `10` principal plus `3q` selected
commercial participation at month 24. A failed project returns `2` principal
at month 12. The declared hurdle is zero.

The four scenario NPVs are therefore:

```text
common loss                         -16.2
common success                      -0.2 + 6q
culture loss / scale-up success      -8.2 + 3q
culture success / scale-up loss      -8.2 + 3q
```

Under the strict probability envelope, the exact expected-NPV table is:

| Term | Minimum | Central | Maximum |
|---|---:|---:|---:|
| `q=0` | -5.00 | -3.40 | -2.68 |
| `q=1` | -0.80 | 1.40 | 2.39 |

For this fixture the same adverse witness binds throughout `[0,1]`:

```text
common loss                         0.10
common success                      0.50
culture loss / scale-up success     0.20
culture success / scale-up loss     0.20
```

Thus the robust lower NPV is `-5.00 + 4.20q`. Zero would require
`q = 25/21 = 1.190476...`, outside the supported contract. The engine correctly
reports `unattainable-at-full-participation`, evaluates the maximum honest term
at `q=1`, and retains the `0.80` shortfall. It does not clamp an infeasible root
to 100% or use unused source capacity as investor cash.

The central probability mix alone crosses zero at `17/24 = 0.708333...`. The
CLI labels this as context only. It is not the conservative term answer.

## Verified core controls

### Cash-right transformation

- `q=0` sets only selected non-principal receipt amounts to zero while retaining
  their principal component.
- Unselected receipts, including any unselected non-principal amount, remain
  unchanged.
- `q=0.5` interpolates selected cash exactly.
- `q=1` returns the configured portfolio without rewriting its cash schedule.
- Negative, above-one, NaN, and infinite fractions fail closed.
- A selected receipt whose principal component exceeds its cash amount is
  rejected strictly, even inside the broader portfolio input tolerance.
- Commercial, licensing/royalty, and exit/sale are the only scalable source
  kinds; recovery, refinancing, support, and fees cannot be swept into success
  participation.

### Robust optimization

- The prepared projector validates the portfolio and ambiguity envelope once,
  then accepts finite scalar values keyed by the exact scenario-ID set.
- Missing, extra, duplicate, unsafe, NaN, and infinite keyed values fail.
- Endpoint bounds and witness vectors use canonical scenario-ID order.
- The projector reuses the existing capped-simplex water-fill rather than
  duplicating optimization code.
- Its 10,000-scenario supported boundary is tested explicitly.
- A two-scenario case whose adverse witness changes with `q` verifies the
  combined objective. The true threshold is `2/3`; the solver returns a
  certified failing/feasible bracket narrower than `1e-12`. A shortcut based on
  a fixed initial witness would give the wrong answer.
- All five solver statuses have explicit tests: target already met at zero,
  interior certified bracket, full participation required, no selected
  participation cash, and unattainable at full participation.

### Timing and attribution

- A positive-hurdle test separates nominal from present value for month-24
  cash. At a 10% annual hurdle the scenario payoff PVs are `600/121` and
  `300/121`; robust, central, and maximum expected PVs are respectively
  `420/121`, `480/121`, and `507/121`.
- Scenario results retain q=0 NPV, q=1 NPV, reported-q NPV, nominal payoff, PV
  payoff, and cash-source attribution.
- Source-capacity verification uses a sparse event sweep proportional to actual
  cash records, not a scenario-by-source-by-horizon dense loop.

## Strict input and CLI controls

The term parser is a closed `0.1.0` schema. Tests cover unknown, duplicate, and
missing keys; count mismatch; zero and excess source counts; duplicate or
disallowed source kinds; unsupported version; false synthetic/scalability
assertions; malformed and non-finite numbers; unsafe text; misplaced UTF-8 BOM;
and file and line guardrails.

Normalized printing uses the classic locale and `max_digits10`, restores the
caller's stream state, and round-trips every supported value exactly.

The CLI regression verifies the hand table, status, central-only context,
binding witness, loss invariance, source attribution, interpretation warnings,
normalized-output mode, usage errors, and missing-term behavior.

For the distributed fixture, every reported computational control is zero to
six decimal places:

```text
q=1 cash reconstruction error       0.000000
principal-loss reconciliation error 0.000000
source-capacity violation            0.000000
witness reconciliation error        0.000000
endpoint probability error           0.000000
```

## Design corrections retained in the record

1. An early idea considered allocating unused scenario cash-source capacity.
   It was rejected because availability is not an investor receipt or ownership
   right. The implemented term scales only configured investor cash above the
   principal component.
2. `q=0` was initially described too broadly as “principal only.” It now means
   selected participation off; unselected receipts are unchanged.
3. The first capacity-control implementation iterated every month for every
   source. Pre-integration audit replaced it with a sparse event sweep so the
   control respects the engine's supported dimensions.
4. The solver never combines independently optimized base-NPV and payoff
   endpoints. It projects complete scenario NPV at every candidate fraction.

## Residual limitations

- Only MSVC was available on the active PATH for this verification; `clang++`
  and `g++` were not available for an additional compiler run.
- No market-consistent pricing, stochastic discount factor, liquidity premium,
  tax, capital charge, or secondary-market execution model is present.
- Counterparty default, legal priority, transfer restrictions, dilution,
  disputes, and enforcement costs are absent.
- The selected receipt is asserted at the source-kind level. Real calibration
  needs receipt-level legal payoff classification and evidence.
- Interval bounds constrain finite joint-scenario probabilities only. They do
  not construct missing scenarios, infer causality, or establish empirical
  coverage.
- All distributed figures are synthetic mechanics data.

The economic interpretation and the required next evidence are defined in
[Robust Success-Participation Term v0.1](SUCCESS_PARTICIPATION_TERM_V0_1.md) and
[Portfolio Calibration and Probability-Uncertainty Standard v0.1](PORTFOLIO_CALIBRATION_STANDARD_V0_1.md).
