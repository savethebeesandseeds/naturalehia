# Provider Counterparty-Credit Stress v0.1 Verification

**Verification date:** 29 August 2026

**Status:** synthetic mechanics verified; empirical validation not performed

## Verified scope

This record covers the C++20 provider counterparty-credit stress engine, its
strict sixth input, the human-readable CLI, and the two-project synthetic
fixture. The module sits above the existing pooled-loss-protection and provider
price-ladder calculations. It changes claim collection after provider default;
it does not change project loss, contractual coverage, or the provider's
full-performance price.

Verified files include:

- `include/naturalehia/cellular_finance/provider_credit_stress.hpp`;
- `include/naturalehia/cellular_finance/provider_credit_stress_config.hpp`;
- `src/provider_credit_stress.cpp` and
  `src/provider_credit_stress_config.cpp`;
- `apps/provider_credit_stress_cli/main.cpp`;
- `tests/provider_credit_stress_tests.cpp`,
  `tests/provider_credit_stress_config_tests.cpp`, and
  `tests/check_provider_credit_stress_cli.cmake`; and
- `scenarios/two-project-provider-credit-stress-synthetic.cfg`.

## Build and test environment

The final isolated build used:

```text
Windows 10 SDK             10.0.26100.0
Visual Studio              2022 Build Tools
MSVC                       19.44.35227.0
CMake / CTest              4.3.3
C++ standard               C++20, extensions off
compiler policy            /W4 /WX /permissive-
isolated binary directory  b/pcv
```

Both configurations completed independently:

```text
Debug    31/31 tests passed
Release  31/31 tests passed
```

An earlier Release attempt from a much longer isolated directory reached the
Windows 260-character MSBuild path limit for generated `.tlog` files. It was an
environment-path failure, not a compile or model failure. Reconfiguration at
the exact shorter directory above produced a clean full Release build and test
run.

## Accounting and state controls

The direct core tests establish that:

- every credit scenario exactly matches one original portfolio scenario;
- the credit and protection inputs name the same provider exactly;
- fixed conditional provider weights sum to one and are collapsed inside each
  project scenario before ambiguity projection;
- provider default changes only collection, never gross project loss or the
  contractual claim;
- the provider all-in price is identical with and without default states;
- direct payment, collateral, delayed recovery, and ultimate unpaid claim sum
  to the contractual claim in every atom;
- full-claim PV equals received-claim PV plus counterparty credit-loss PV;
- collateral benefits the investor only when the credit input explicitly says
  the price-ladder base is pledged and its yield remains in the pledged
  account;
- priced but unpledged collateral produces no credit benefit;
- excess collateral is capped at the claim and creates no investor windfall;
- a fully performing provider exactly reconstructs full claim delivery, has no
  incremental credit-support gap, and reports no modeled default atom;
- full nominal recovery after a delay creates a positive PV loss at a positive
  investor hurdle; and
- a zero-claim portfolio leaves both delivery ratios absent rather than
  manufacturing a ratio.

The ambiguity tests independently switch the binding probability witness for
default frequency and counterparty loss. Provider outcomes are never promoted
to separately movable ambiguity atoms. The robust delivery ratio minimizes one
common-measure ratio; it is not a quotient of unrelated range endpoints.

## Tail and wrong-way controls

Central distributions are built from the expanded physical atoms
`p_s * theta_sj`. Weighted quantiles use the lower cumulative definition and
ES95/ES99 average the worst five and one percent, including fractional boundary
mass. Tests cover loss, unpaid claim, and unsecured-exposure tails.

Wrong-way risk uses claim severity and the provider-default indicator on the
same expanded atoms. Centered, compensated moments avoid subtracting two large
rounded raw moments. A near-constant million-scale regression confirms that
claim/default correlation remains within `[-1,1]` and recovers perfect positive
dependence when default occurs only on the marginally larger claim.

Probability reconciliation controls and monetary reconciliation controls are
reported separately; their units are not mixed.

## Strict input and CLI controls

The `provider_credit.*` schema is closed and reloadable. It rejects unknown,
duplicate, missing, malformed, non-finite, out-of-range, oversized, and unsafe
inputs. It bounds nested counts and recovery delay, requires unique safe IDs,
normalizes only weights already within the strict sum tolerance, and rejects a
provider ID that differs from the protection input.

Performing atoms require canonical zero collateral, recovery, and delay terms.
Unpledged collateral requires zero realization in every atom. Pledged
collateral requires the retained-yield assertion. Legal-enforceability and
market-CVA/fair-value claims must remain false. Normalized CLI output was
machine-checked for all six inputs, including every provider state key and
value. Missing the sixth file or supplying an unknown option returns the
documented usage error.

The settlement ledger includes direct payment, collateral nominal/PV,
unsecured exposure, recovery fraction, recovery month and PV, unpaid claim,
received PV, credit-loss PV, and investor NPV. A reviewer can therefore
reproduce timing-sensitive rows from the CLI without hidden fields.

## Hand reconciliation of the repository fixture

At the idealized exact selected coverage `g = 1/6`, settlement month 24, and a
zero investor hurdle:

```text
price-ladder collateral base                         1.666666667
grown pledged collateral at settlement               1.734000000
recognized collateral in default, 75%                1.300500000

single-loss contractual claim                        1.333333333
single-loss unsecured EAD                            0.032833333
single-loss delayed recovery, 25%                    0.008208333
single-loss ultimate unpaid claim                    0.024625000

common-loss contractual claim                        2.666666667
common-loss unsecured EAD                            1.366166667
common-loss delayed recovery, 25%                    0.341541667
common-loss ultimate unpaid claim                    1.024625000
```

Conditional default probabilities are `1%` in common success, `10%` in each
single-loss state, and `50%` in common loss. Under the central project weights:

```text
provider-default probability                         0.052200000
default-and-positive-claim probability               0.046000000
expected counterparty credit-loss PV                 0.011132750
expected claim receipt PV                            0.522200583
central claim-PV delivery ratio                      0.979126094
claim/default correlation                            0.298388367
E[claim | default] / E[claim]                        2.681992337
```

The physical-probability envelope produces:

```text
expected credit-loss PV minimum                      0.005837250
expected credit-loss PV maximum                      0.052216250
robust minimum claim-PV delivery ratio               0.903017969
```

The full-performance provider all-in floor remains `1.378666667`. The
full-performance all-in support gap is the same amount. Counterparty stress
adds `0.052216250` of investor-target restoration, so the stressed total gap is
`1.430882917` DEMO million. The engine does not assert that this additional
support exists.

## Corrections made during review

Independent review caused the implementation to:

- allow priced but unpledged collateral while assigning it zero investor
  credit benefit;
- show direct payment and ultimate unpaid claim explicitly;
- distinguish unconditional `E[U * 1_D]` from conditional `E[U | D]`;
- disclose collateral and recovery PV and recovery month in the CLI ledger;
- make the default-modeled flag depend on an actual configured default atom;
- bind the sixth input to the exact protection-provider ID;
- split probability and monetary residual controls;
- replace cancellation-prone raw-moment correlation with centered compensated
  arithmetic; and
- keep the public brief at exactly 1,000 words after adding the credit result.

## Residual boundary

Passing tests verifies code paths, identities, strict parsing, and the stated
synthetic arithmetic. It does not validate a provider, probability, recovery,
collateral agreement, legal claim, cost, price, or investability.

Version 0.1 is a physical-measure settlement stress. It does not model
pre-settlement default time, hazard curves, rating migration, market CVA/DVA,
replacement cost, dynamic margin, close-out or netting, collateral market risk,
multiple providers, contagion, tax, currency, accounting, regulation, capital
adequacy, or legal enforceability. Real use requires named-party evidence and
qualified legal, valuation, accounting, regulatory, and credit review.
