# Claim-Ledger Joint-Portfolio Assembler v0.1 — Verification Record

## Status and boundary

This record documents local software verification performed on 30 August 2026.
It covers the package-to-Portfolio assembler, five SHA-256-bound synthetic
fixtures, its integration with Portfolio v0.2, and fail-closed admission and
resource controls. It is not external model validation, source authentication,
legal review, empirical calibration, fair value, a rating, or an investment
recommendation.

The verified job is narrow but financially important: two or more independently
verified one-claim marginal distributions can enter one explicit joint measure
without silently inventing independence, changing a stand-alone probability,
duplicating an economic right, merging cash budgets, or moving cash between
calendars.

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
Result: 66/66 tests passed in Debug (33.73 seconds).

cmake --build build/release --config Release --parallel
ctest --test-dir build/release -C Release --output-on-failure
Result: 66/66 tests passed in Release (16.42 seconds).
```

GCC, Clang, sanitizers, remote CI, and cross-platform deterministic replay were
not run in this local record. No cross-toolchain claim is made.

## Financial conservation oracle

The ordinary two-claim fixture has marginal failure probabilities of `0.20`
and `0.30`, with principal loss of `4.0` in either failure state. Its supplied
joint weights are `0.60/0.20/0.10/0.10`, not the independent product weights.
The hand result is:

```text
expected pool principal loss = 0.20 × 4 + 0.30 × 4 = 2.0
pool loss variance           = 7.2
pool loss standard deviation = sqrt(7.2) = 2.683281573
```

A second admissible coupling changes dependence while preserving both
marginals. Tests require each assembled project's expected draws, receipts,
principal loss, and NPV to equal its separately evaluated marginal result.
They also reconstruct every selected path, monthly pool cash row, contractual-
principal roll-forward, finite cash-source budget, provider identity, and
package/entry lineage from the verified marginal inputs.

## Adversarial controls exercised

The focused test rejects or detects:

- incomplete joint selections, unsafe or duplicate IDs, non-finite, negative,
  or non-unit-sum weights, unknown marginal states, and altered marginals;
- duplicate project, package, root, claim, or economic-cluster identities;
- one cross-asset `cash_source_id` alias while preserving two separately named
  rights of the same provider as two visible budgets;
- model, currency, monetary-basis, hurdle, horizon, information-cut, period,
  or actual calendar mismatches;
- inferred independence, pool costs, loss layers, and more than 64 effective
  factor tags;
- loss of an explicitly positive `1e-15` marginal state through an absolute
  tolerance or zero-mass joint row;
- changes between declared, configured, and evaluated joint probabilities, and
  between configured and final evaluated one-claim marginal probabilities;
- loss of overlap provenance between declared and marginal factor tags;
- exact-bound-plus-one and integer-overflow attempts against aggregate retained
  marginal and expanded joint resource limits; and
- discrepancies in project results, monthly cash, principal, source budgets,
  provider lineage, or expanded cash and decision-entry lineage.

Five hash-valid fixtures are used: two ordinary claims, one rare-state claim,
one claim whose numeric periods match but whose actual dates are shifted, and a
four-state claim that exercises Portfolio's final marginal normalization. The
shifted fixture confirms that matching “month 12” labels do not make two
different calendars economically simultaneous. The rounding fixture confirms
that reconciliation targets the authoritative evaluated marginal while
retaining the earlier configured value separately.

## Internal review and corrections

Separate read-only code, test-gap, and evidence-integration review passes were
performed during development. They led to aggregate retained-marginal and
expanded-output resource guards that count copied factor instances, true
joint-term preflight before package reload, distinct declared and effective
factor lineage, distinct declared/configured/evaluated probability lineage,
authoritative final-marginal reconciliation, rare-positive-state tests, a real
hash-valid calendar mismatch, deeper cash and principal conservation tests, and
narrower public-guarantee costing language. These were internal adversarial
reviews, not independent third-party validation.

## Residual limitations and next evidence step

- The assembler accepts an analyst-supplied complete coupling; it does not
  estimate joint probabilities, correlations, recoveries, or pricing measures.
- Exact source-ID uniqueness cannot prove that differently named rights are not
  economically shared. Joint allocation of a shared guarantee or collateral
  pool requires a later schema.
- One native currency, monthly timing, common model basis, and resolved finite
  claim paths are inherited from the one-claim adapter.
- Package hashing authenticates retained bytes, not their truth, legal
  enforceability, seniority, collectability, or economic completeness.
- The assembled result remains `synthetic_inputs=true`, including if marginal
  packages later become controlled, because its joint coupling is still an
  unevidenced declaration.

The next admissible evidence object is a fully loaded, Evidence-Gate-admitted
**Partial-Credit Claim-Loss Cohort Binder**. It must retain the complete issued
or at-risk frame, terms, claim filings and decisions, dated borrower and
provider cash, recoveries, terminal resolutions, and open cases. Provider
costs, subrogation, and post-payment recoveries require a separate future
provider-cost package. Dependence and common-factor identities are tested
separately downstream. Until the population and its methods pass those
boundaries, the assembler supports transparent scenarios and stress envelopes,
not empirical probabilities, market prices, or proof of financeability.
