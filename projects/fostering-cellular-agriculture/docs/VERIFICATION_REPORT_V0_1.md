# Internal Software Verification Record — Annual Engine v0.1

## Status and boundary

This record documents internal software checks performed on 27 August 2026.
It is **not** independent model validation, empirical calibration, engineering
review, credit approval, legal analysis, or evidence that any proposed
instrument is financeable. The scenarios use invented `DEMO` monetary units
with an `unspecified-synthetic` basis.

The checks establish only that the current implementation builds in the tested
environment, its automated invariants pass, its configuration boundary rejects
specified malformed inputs, and its two synthetic examples run to completion.
Economic equations and limitations are defined in the
[Annual Engine v0.1 specification](./ANNUAL_ENGINE_V0_1.md).

## Tested environment

| Item | Tested value |
|---|---|
| Host/toolchain family | Windows, MSVC 19.44, MSBuild 17.14.40 |
| Selected Windows SDK | 10.0.26100.0 |
| CMake and CTest | 4.3.3 |
| Language mode | C++20, compiler extensions disabled |
| Diagnostics | `/W4 /WX /permissive-` |
| Configurations | Debug and Release |

GCC, Clang, and `clang-tidy` were not available on the local host. The
repository CI definition requests GCC, Clang, and MSVC builds in Debug and
Release, but no remote CI result is claimed in this record.

## Commands and results

```text
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev

Result: 4/4 tests passed in Debug.
```

```text
cmake --build build/dev --config Release --parallel
ctest --test-dir build/dev -C Release --output-on-failure

Result: 4/4 tests passed in Release.
```

For each 10,000-path scenario, the complete Debug and Release CLI outputs were
also compared in memory and were byte-for-byte identical on this MSVC host.
This is a local reproducibility check, not a cross-toolchain guarantee.

The four registered tests are:

1. `cellular_finance_model_tests`;
2. `cellular_finance_config_tests`;
3. the illustrative CLI scenario; and
4. the adverse CLI scenario.

## Invariants exercised

The model tests cover:

- rejection of non-synthetic status, mismatched model versions, non-finite
  inputs, excessive dispersion, overlapping output coverage, incomplete
  disabled terms, unknown enums, unsafe audit text, negative discount rates,
  implicit terminal-debt treatment, and uncapped completion protection;
- exact zero-instrument identity and fixed-seed repeatability within one build
  environment;
- a hand-calculated deterministic unlevered cash-flow case;
- visible fee sign and transfer attribution;
- sampled one-way-floor sign, default, payout-bound, and transfer-reconciliation
  checks, plus an exact deterministic annual/lifetime-cap fixture;
- downside payments for a two-way contract for difference and an exact
  deterministic absolute-cap fixture;
- finite outputs and absence of debt metrics in an unlevered case;
- reconciliation of unconditional expected lender loss to default probability
  times mean loss severity conditional on default;
- reconciliation of paired paths among within-horizon defaults avoided or
  introduced and both-default paths delayed, accelerated, or in the same
  period;
- signed negative DSCR when cash available for debt service is negative; and
- horizon-limited completion cover and explicit surviving terminal debt.

The parser tests cover normalized print/load round trips, UTF-8 BOM handling,
and rejection of unknown, duplicate, missing, malformed, governance-ineligible,
and unsupported monetary-basis inputs. Separate manual CLI checks returned exit
code 1 and clear errors for these commands:

```text
naturalehia-cellular-finance illustrative.cfg --trials 0
naturalehia-cellular-finance illustrative.cfg --seed nope
```

## Full synthetic scenario runs

Both checked scenarios used 10,000 common random paths and seed `20260827`.
The values below are software outputs, not empirical estimates.

| Metric | Illustrative case | Adverse case |
|---|---:|---:|
| Unsupported zero-factor probability-weighted steady-state CFADS, DEMO million/year | 21.1776 | -12.1200 |
| Structured zero-factor probability-weighted steady-state CFADS before debt, DEMO million/year | 23.6270 | -7.9020 |
| Mean project-NPV transfer, DEMO million PV | 11.9105 | 11.6477 |
| Mean sponsor-equity PV change, DEMO million PV | -0.6628 | -3.6137 |
| Project-negative probability change | -0.07 percentage points | +0.06 percentage points |
| Debt-default probability change | -2.16 percentage points | -0.21 percentage points |
| Within-horizon payment defaults avoided / introduced, paths | 234 / 18 | 26 / 5 |
| Defaults delayed / accelerated where both cases default, paths | 880 / 154 | 134 / 87 |
| Paired mean default-timing change where both cases default, years | +0.1680 | +0.0125 |

These results are intentionally not optimized to make the structures appear
successful. In the adverse case, support produces a positive mean transfer but
does not repair negative operating economics and slightly increases the share
of paths with negative project NPV. This is consistent with a fee paid in every
path, removal of some high-price upside through fixed offtake, and support
concentrated in paths that may remain uneconomic.

The timing counts also prevent the net default-probability change from being
read as a complete credit result. In the illustrative case, 880 paths default
later under the package and 154 default earlier even though the net default
rate falls. A postponed default is reported separately from a path with no
structured-case payment default inside the modeled horizon; the latter is not
claimed to be a permanent cure.

## Reconciliation and audit checks

- Unsupported and structured evaluations use identical exogenous paths.
- Mean paired project-NPV change reconciles to modeled net instrument receipts.
- Fixed-offtake repricing, net price-support settlement, completion-delay
  payout, upfront fee, gross positive support payout, and signed net instrument
  transfers after a prior-period default are reported separately.
- Threshold movements are shown in both directions as path counts rather than
  being hidden inside a rounded net probability.
- Unconditional expected lender loss is distinguished at the default date and
  at present value; mean loss severity conditional on default is separate.
- Monetary label, monetary basis, model version, seed, trial count, scenario
  source note, and synthetic-input status are explicit configuration fields.

## Residual verification and model gaps

The following work remains before any decision use:

- independent equation and code review by parties not responsible for the
  implementation;
- verified GCC and Clang results, sanitizer runs, static analysis, and broader
  platform reproducibility evidence;
- golden cash-flow tables covering construction, debt amortization, recovery,
  contract caps, contamination timing, and default-period priority in more
  detail;
- a consolidated stakeholder waterfall after default, including the legal
  receiver of contract payments and prevention of double recovery;
- calibrated counterparty default, collateral, liquidity, taxes, inflation,
  working capital, refinancing, terminal value, and behavior;
- parameter estimation from complete reference-facility evidence, including
  failures, followed by back-testing and sensitivity/model-risk review; and
- independent financing-additionality and animal-product-displacement methods.

Until those gaps are closed, version 0.1 remains a synthetic research engine
for exposing assumptions and testing mechanisms—not for pricing, solicitation,
capital allocation, underwriting, or claims of animal-welfare impact.
