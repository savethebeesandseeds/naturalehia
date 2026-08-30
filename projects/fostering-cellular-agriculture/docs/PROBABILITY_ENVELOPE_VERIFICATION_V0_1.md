# Physical-Probability Envelope v0.1 Verification

**Verification date:** 30 August 2026

**Scope:** internal implementation verification, not independent model
validation or empirical calibration.

## Result

The v0.1 probability-envelope core, strict parser, CLI, synthetic fixture, and
all integrated project targets compile under the project's strict warning
policy. The complete Debug and Release suites each pass `42/42` tests after
the project-range and cohort-to-capital-stack integration.

This verifies the stated finite-scenario mechanics. It does not verify any real
scenario, probability bound, cash right, recovery, counterparty, legal form,
fair value, rating, or financeability conclusion.

## Environment

The clean project-wide verification used:

| Item | Value |
|---|---|
| Host | Windows, x64 |
| CMake | 4.3.3 |
| Generator | Visual Studio 17 2022 |
| Compiler | MSVC 19.44.35227.0 toolset 14.44.35207 |
| Windows SDK selected by CMake | 10.0.26100.0 |
| Language | C++20, extensions off |
| Diagnostics | `/W4 /WX /permissive-` |
| Configurations | Debug and Release |

The focused ambiguity core was also compiled and run separately with Clang
C++20 strict diagnostics and MSVC `/W4 /WX /permissive-`; both focused test
executables passed. A Windows AddressSanitizer attempt did not reach test code:
the runtime hung at startup with no output or CPU use and was terminated. It is
therefore recorded as inconclusive, not as an ASan pass or model failure.

## Complete-suite result

The CMake build included the annual model, evidence gate, staged-capital model,
participation pool, staged adapter, probability-envelope core and parser,
success participation, provider-side terms, capital stack, joint-cohort bridge,
all CLIs, and every test executable.

```text
Debug:   42/42 passed in 26.69 s
Release: 42/42 passed in 13.81 s
```

The original three probability-envelope additions were:

1. probability-envelope core mechanics and validation;
2. probability-envelope parser, canonical printer, and guardrails; and
3. the complete synthetic CLI hand table, normalized output, and exit behavior.

## Core controls

The focused core tests cover:

- exact linear minimum and maximum expectations under component probability
  bounds and unit total mass;
- full witness reconstruction for each endpoint;
- deterministic scenario-ID ordering and tie handling;
- central-weight normalization and reconciliation to the ordinary pool result;
- total draws, receipts, pool costs, outstanding principal, realized loss,
  NPV, impairment, negative NPV, three liquidity peaks, and every nominal/PV
  external cash-source category;
- exact project-level draw, receipt, outstanding-exposure, realized-loss,
  pre-pool-cost NPV, impairment, and negative-NPV ranges, with each endpoint
  reconstructed from its own feasible probability witness;
- common-witness ES95/ES99 project attribution whose contributions and
  fractional tail masses reconcile exactly to pool ES;
- symmetric tied-tail allocation of `5/5`, asymmetric equal-objective capacity
  allocation, and invariance to scenario input order and renaming;
- central project receipts and the continuing-exposure-versus-loss boundary;
- the fact that componentwise source maxima can exceed the coherent attainable
  total-receipt maximum and therefore must not be summed;
- exact fractional-tail ES95 and ES99 at atom boundaries;
- NPV downside measured as `max(0, -NPV)`;
- degenerate ambiguity sets whose endpoints collapse to the central case;
- input permutations producing the same sorted result; and
- invalid version, non-synthetic claim, ID mismatch, duplicate ID, non-finite
  value, unsafe range, central mismatch, and infeasible lower/upper totals.

## Parser and CLI controls

The strict companion parser tests cover:

- the complete required key set and rejection of unknown, duplicate, missing,
  and malformed fields;
- comments, whitespace, one leading UTF-8 BOM, and rejection of a misplaced
  BOM;
- finite decimal conversion and probability feasibility;
- scenario, line, file, and text resource guards;
- `max_digits10` exact round trip under a non-classic caller locale; and
- full output-stream state restoration after canonical printing.

The CLI regression verifies the warning and interpretation boundary, exact
scenario bounds, pool and project financial range tables, common-witness tail
attribution, source table, endpoint witnesses, zero reconciliation residuals,
both normalized inputs, unknown-option exit `2`, and missing-envelope exit `2`.

## Hand-calculated fixture

For the four fixed two-project paths, the configured probability intervals are:

```text
both succeed:                       0.50 to 0.70
culture loss / scale-up success:   0.10 to 0.25
culture success / scale-up loss:   0.10 to 0.25
both lose:                          0.01 to 0.10
```

The checked central and robust results are:

| Metric | Minimum | Central | Maximum |
|---|---:|---:|---:|
| expected draws | 20.00 | 20.00 | 20.00 |
| expected receipts | 19.40 | 21.60 | 22.59 |
| expected pool costs | 0.20 | 0.20 | 0.20 |
| expected principal loss | 2.48 | 3.20 | 4.80 |
| impairment probability | 30% | 38% | 50% |
| expected NPV | -0.80 | 1.40 | 2.39 |
| negative-NPV probability | 30% | 38% | 50% |
| loss ES95 | 9.60 | 11.20 | 16.00 |
| loss ES99 | 16.00 | 16.00 | 16.00 |
| NPV-shortfall ES95 | 7.40 | 9.60 | 16.20 |
| NPV-shortfall ES99 | 16.20 | 16.20 | 16.20 |
| commercial receipts | 18.20 | 20.80 | 21.97 |
| recovery receipts | 0.62 | 0.80 | 1.20 |

All values are invented `DEMO` millions except probabilities. The minimum-loss
witness is `0.01/0.70/0.145/0.145` in sorted scenario order; the maximum-loss
witness is `0.10/0.50/0.20/0.20`. Both sum to one and lie inside every declared
interval.

Each project has expected receipt range `9.15 / 10.80 / 11.79`, expected loss
range `0.88 / 1.60 / 2.80`, impairment range `11% / 20% / 35%`, and
pre-pool-cost NPV range `-0.85 / 0.80 / 1.79`. Each contributes
`4.8 / 5.6 / 8.0` to pool ES95 and `8.0 / 8.0 / 8.0` to pool ES99 under the
shared minimum-pool-ES, central, and maximum-pool-ES measures.

The central expected NPV is positive, but the minimum is negative. The fixture
therefore demonstrates mechanics and a failed robustness threshold; it does not
demonstrate an attractive real instrument.

## Correction history

The first complete Debug test run passed `18/19`. The model and numeric output
were correct; the new CLI regression expected a different textual binary-double
rendering for the lower-bound sum (`0.71`). The assertion was corrected from
`0.70999999999999999` to the actual deterministic `max_digits10` output
`0.70999999999999996`. The focused CLI test and then the complete Debug suite
passed. Release subsequently passed `19/19` without another change.

The project-range integration replaced the prior arbitrary scenario-ID tie
break with a canonical rule: a partially filled exact equal-value block is
allocated pro rata to its remaining capacities. Three downstream CLI tests
initially failed because their expected text pinned the old `25%/15%` loss
witness. Their financial values were unchanged; the expected witnesses were
updated to `20%/20%`. The complete Debug suite then passed `42/42`.

## Residual limitations

- The ambiguity set constrains only individual joint-scenario probabilities
  plus their unit sum. It has no moment, marginal, conditional-factor, copula,
  or causal constraints.
- Exactness of the ES endpoints relies on fixed loss values and the
  first-order-stochastic extremal measures admitted by that set.
- Every metric endpoint is componentwise. Endpoints from different metrics may
  be mutually incompatible.
- Tail attribution is additive under the selected common witnesses but is not
  a proof that the ES-optimal measure or project attribution is mathematically
  unique. Conditional attribution bands would require separate optimization.
- Published endpoint and tail witnesses are capped at eight million doubles;
  large accepted project/scenario combinations fail before allocation if that
  output bound would be exceeded.
- The engine cannot detect an omitted joint state or prove that bounds cover
  the target population.
- Inputs remain synthetic-only in v0.1. Empirical activation should occur only
  after the calibration standard, retained evidence, validation record, and
  independent challenge are satisfied.
- Physical-probability NPV does not supply a market-consistent price, risk
  premium, capital charge, illiquidity adjustment, tax/accounting treatment, or
  legal characterization.
