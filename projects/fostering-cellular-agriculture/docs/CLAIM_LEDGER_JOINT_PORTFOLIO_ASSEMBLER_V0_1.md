# Claim-Ledger Joint-Portfolio Assembler v0.1

Status: implemented and mechanically verified, 30 August 2026.

## What it does

The assembler turns two or more independently verified Project Claim Ledger
packages into one Portfolio v0.2 distribution. It is the missing bridge between
“we can normalize one real claim” and “we can measure a pool's dependence,
loss, liquidity, expected return, and diversification.”

It does not generate scenarios. An analyst must supply every complete joint
state and its physical probability. The assembler re-verifies each package,
selects one marginal path for every project in every joint state, and refuses
the portfolio unless the joint table reproduces every project's original
marginal distribution.

In financial terms, pooling may change covariance, tail loss, peak liquidity,
and diversification. It may not change a project's stand-alone probability,
expected loss, expected receipts, or expected NPV.

## Authoritative boundary

```cpp
[[nodiscard]] ClaimLedgerJointPortfolioResult
assemble_claim_ledger_joint_portfolio(
    const std::vector<ClaimLedgerJointPortfolioAssetInput>& assets,
    const ClaimLedgerJointPortfolioTerms& terms);
```

Each asset contains a loader-verified `ClaimLedgerPackage` and the same explicit
adapter terms required by the one-claim v0.2 bridge: target project identity,
stage, physical hurdle sensitivity, receipt-source allocations, external cash
budgets, and marginal factor tags.

Each joint-state declaration contains:

- one safe joint-scenario ID;
- one non-negative physical probability;
- a retained `probability_basis_id`;
- explicit joint factor tags; and
- exactly one `(project_id, marginal_scenario_id)` selection for every asset.

The result keeps three probability stages separate. `declared` is the analyst's
input, `configured` is the value passed into Portfolio after the assembler's
unit-sum normalization, and `evaluated` is Portfolio's authoritative normalized
weight used in every reported expectation. Reconciliation and returned
selection lineage use the evaluated measure; the earlier two stages remain
available for audit rather than being overwritten.

There is no public overload that accepts mutable adapter results or a bare
`PortfolioConfig`. The package entry point reloads and hash-checks every root
through the one-claim adapter before assembly.

## The financial conservation rule

Let `q_j` be the supplied probability of joint state `j`. Let `s_i(j)` be the
marginal state selected for project `i`, and let `p_i,k` be that project's
verified normalized probability of marginal state `k`. Admission requires:

```text
sum of q_j over every j where s_i(j) = k  =  p_i,k
```

for every project and every marginal state. The equality is checked with
compensated sums against Portfolio's evaluated normalized measure. A positive
rare state cannot disappear under an absolute rounding tolerance.

The assembler never substitutes:

```text
q_j = product of project marginal probabilities.
```

Independence is one possible coupling, not a default. A missing combination is
permitted only when its explicitly supplied probability is zero and all
marginals still reconcile.

## Hand-reconciled non-independent example

The two package fixtures have these one-project marginals:

| Project | Perform | Fail | Principal loss on failure |
|---|---:|---:|---:|
| A | 0.80 | 0.20 | 4.0 |
| B | 0.70 | 0.30 | 4.0 |

The supplied joint coupling is:

| Joint state | Probability |
|---|---:|
| both perform | 0.60 |
| A performs, B fails | 0.20 |
| A fails, B performs | 0.10 |
| both fail | 0.10 |

These weights are not the independent product weights. They reproduce A's
`0.80/0.20` and B's `0.70/0.30` exactly. The evaluated expected principal loss
is `0.20 × 4 + 0.30 × 4 = 2.0`; pooling does not change that sum. The pool loss
standard deviation is `sqrt(7.2) = 2.683282`, and loss correlation is positive.

A second valid coupling assigns `0.70/0.10/0.00/0.20` to the same four states.
Project expected loss and NPV remain unchanged, while pool loss volatility and
correlation change. This is the economic purpose of the assembler: dependence
becomes an explicit risk input rather than an accidental consequence of file
construction.

## Hard admission gates

The v0.1 assembler fails closed unless all of these conditions hold:

1. There are 2–128 package-derived assets and bounded joint dimensions.
2. Project IDs, package IDs, package-root hashes, claim IDs, and economic
   cluster IDs are unique. Two records of one financing cannot masquerade as
   diversification.
3. Every economic-cluster boundary is defined, and every package uses the same
   admission basis. Synthetic mechanics cannot contaminate a controlled
   expected-return pool without being visible and rejected.
4. Model version, currency, monetary basis, horizon, and hurdle are common.
5. The verified period unit, periods per year, period-origin date, decision
   date and period, and horizon date and period are identical. “Month 12” from
   different calendars is not treated as simultaneous cash.
6. Every joint row names every project exactly once, selects a known marginal
   state, has a retained probability-basis identity, and contributes to a
   unit-sum joint measure.
7. Every evaluated joint marginal equals the one-claim marginal. A unit-sum
   table that shifts probability between a project's states is rejected.
8. A `cash_source_id` may belong to only one asset. The assembler copies the
   selected finite budgets; it never prefixes, merges, enlarges, or nets them.
9. Pool costs and loss layers remain empty. V0.1 assembles the untranched asset
   pool before any separate participation, protection, or priority term.
10. Each project's expected outlay, receipts, principal loss, and NPV after
    assembly reconcile to its independently evaluated marginal result.

The implementation also preflights the joint terms before reloading any package
and checks aggregate retained marginal resources while packages are adapted.
It caps retained project-state pairs at `500,000`, retained scenario-month rows,
cash records, and combined lineage rows at `2,000,000` each, and rejects
overflow. Separate bounds cover constructed project-state work,
scenario-month work, cash records, factor tags, cash sources, and expanded
lineage before Portfolio evaluation. These are denial-of-service and bounded-
memory controls, not economic limits.

## Lineage retained

`ClaimLedgerJointPortfolioResult` keeps:

- every complete one-claim adapter result;
- package status, root hash, claim, cluster, calendar, and decision-cut lineage;
- declared, configured, and evaluated joint weights plus their basis IDs;
- every joint-to-marginal selection, including raw and normalized upstream
  marginal probabilities and their source/timing;
- joint factor tags exactly as declared and the effective canonical union of
  declared joint factors with every selected marginal factor;
- selected entry-level cash and principal lineage under both joint and marginal
  scenario IDs;
- selected external-budget payer and provider lineage under both IDs;
- marginal-probability reconciliation rows; and
- per-project financial reconciliation rows.

Package roots are structured lineage, not concatenated into
`PortfolioConfig::source_note`, whose bounded text field cannot safely hold 128
hashes. A caller must retain the assembler result beside the generic Portfolio
configuration.

## Honest boundary and next evidence step

The five current fixtures are synthetic mechanics. Package hashing proves the
identity of retained bytes, not the truth of cash rights, probabilities,
recovery, provider payment, or legal priority. The assembled Portfolio remains
`synthetic_inputs=true`, including when every marginal package is controlled,
because the joint coupling is still an analyst declaration.

The global source-ID rule prevents exact aliases; it cannot prove that two
differently named budgets are not slices of the same economic cash or
guarantee. Shared rights require a later joint-level right-and-budget allocation
schema. Same counterparties and providers with distinct rights remain allowed
and visible as concentration.

The next empirical artifact is therefore not another point estimate. It is a
fully loaded, Evidence-Gate-admitted **Partial-Credit Claim-Loss Cohort Binder**
containing the complete issued or at-risk frame, terms, claim filings and
decisions, dated borrower and provider cash, recoveries, terminal resolutions,
and open cases. Provider costs, subrogation, and post-payment recoveries require
a separate future provider-cost package. Dependence and common-factor identities
are tested separately downstream. Until the population and its methods pass
those boundaries, joint probabilities remain hypotheses or stresses, not
calibration, price, or proof of financeability.

The implementation is in
[`claim_ledger_joint_portfolio.hpp`](../include/naturalehia/cellular_finance/claim_ledger_joint_portfolio.hpp)
and
[`claim_ledger_joint_portfolio.cpp`](../src/claim_ledger_joint_portfolio.cpp).
The focused tests use five independently hash-bound synthetic packages: two
ordinary marginals, one positive `1e-15` rare-state marginal, one otherwise
compatible package on a shifted calendar, and one four-state rounding fixture
whose configured and final evaluated marginal weights differ. They cover two
valid non-independent couplings; exact authoritative marginal and project-result
preservation; monthly cash, principal, budget, provider, and expanded-lineage
conservation; declared versus configured versus evaluated probability lineage;
declared versus effective factor provenance; exact-limit and overflow resource
guards; rejection of a zero-mass erasure of the rare state; rejection of equal
numeric periods on different dates; and rejection of one cross-asset source
alias.

The strict Debug and Release record is in the
[Claim-Ledger Joint-Portfolio Assembler v0.1 Verification Record](CLAIM_LEDGER_JOINT_PORTFOLIO_VERIFICATION_V0_1.md).
