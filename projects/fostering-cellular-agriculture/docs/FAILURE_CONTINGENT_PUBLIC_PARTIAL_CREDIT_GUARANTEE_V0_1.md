# Failure-Contingent Public Partial-Credit Guarantee v0.1

## Instrument in one sentence

A public provider pays one sixth of an eligible pool's final resolved
contractual principal loss, after declared recoveries, with a legal maximum
payment of `3.333333` on `20` of reference principal.

This is a first-dollar quota-share guarantee attached to the investment claim.
It is not a fund, a tranche, an upfront public loan, or a promise that a
technical setback by itself produces cash.

## Status and purpose

This document applies the existing
[pooled principal-loss protection engine](POOLED_LOSS_PROTECTION_TERM_V0_1.md)
to the existing two-project synthetic participation pool. Its purpose is to
state one concrete, institution-readable instrument and expose who bears its
cost.

Version 0.1 is synthetic contract-mechanics research. It is not a forecast,
fair value, market quote, rating, offer, investment recommendation, legal
opinion, appropriation, or evidence that a public provider exists or can pay.

## Core terms

| Term | Synthetic v0.1 term |
|---|---:|
| Reference projects | `culture-platform` and `bioprocess-scaleup` |
| Aggregate legal reference principal, `K` | `20.000000` DEMO million |
| Attachment, `A` | `0.000000` |
| Public quota share, `g` | `1/6`, displayed as `0.166667` |
| Legal support cap, `C` | `3.333333` DEMO million |
| Configured cap value | `3.333333333333334` DEMO million |
| Loss determination and settlement | month `24` |
| Investor upfront premium | `0.000000` DEMO million |
| Synthetic robust gross-claim-only gap before provider costs | `0.800000` DEMO million |
| Synthetic provider | `synthetic-catalytic-provider` |

The configured cap is rounded slightly upward so the mathematical one-sixth
term remains inside the floating-point support domain. The solver reports a
certified failing/passing bracket and displays the contractual term as
`0.166667`; the economic term stated here is exactly `1/6`.

Attachment zero means the provider participates from the first dollar of
eligible loss. It does not mean the provider absorbs the entire first-loss
piece: the investor retains five sixths of every eligible loss. Positive
attachment and detachment terms are outside the current protection engine.

## Exact reference loss and claim

For project `i` in joint scenario `s`, let `W_si` be contractual principal
that is finally written off on a resolved path after all declared in-horizon
principal cash, recovery-principal cash, underlying guarantee-principal cash,
and conversion extinguishment. Then:

```text
L_s = sum_i W_si
X_s = g L_s = (1/6) L_s
```

`L_s` excludes:

- interest shortfall, fees, negative NPV, and missed target return;
- a buyer's purchase premium, discount, or direct transaction cost;
- principal that remains outstanding on a continuing path;
- unrealized conversion units;
- the payment `X_s` itself; and
- any recovery received after the modeled month-24 horizon.

The synthetic term supports the fixed percentage by restricting it to:

```text
g <= C / K
```

It does not use `min(g L_s, C)` as a scenario formula. With `K = 20` and
`C = 10/3`, a provider issuing the term would commit `gK = 10/3`, even though the largest
loss in the four modeled scenarios is only `16` and the largest modeled claim
is therefore only `8/3`.

A real contract must identify the exact normalized claim ledgers and retained
package hashes that constitute the reference portfolio. A general statement
that a facility or research program “failed” is not the payment trigger.

## Cash-flow sequence

In the synthetic fixture:

1. At month 0, the investor supplies `20.000000` to the two project claims and
   pays `0.200000` of pool cost. The public guarantee creates no project cash.
2. At month 12, a failed project returns its declared `2.000000` recovery.
3. At month 24, a successful project returns `13.000000`, including
   `10.000000` classified as principal.
4. After those underlying cash flows are applied, final resolved contractual
   principal writeoff is determined.
5. The public provider pays `X_s = L_s/6` to the protected investor claim at
   month 24.

The public payment is booked as external support cash. It is not project
revenue, returned project principal, recovery, commercial success cash, or a
reduction of reported gross loss. Investor residual-loss disclosure is
`L_s - X_s`.

## Synthetic two-project demonstration

The central weights and admitted bounds are invented mechanics inputs, not
empirical probabilities.

| Joint state | Probability lower / central / upper | Underlying NPV | Gross contractual principal loss | Public claim at `g=1/6` | Investor residual loss | Protected NPV before premium |
|---|---:|---:|---:|---:|---:|---:|
| Both succeed | `0.50 / 0.62 / 0.70` | `5.800000` | `0.000000` | `0.000000` | `0.000000` | `5.800000` |
| Culture platform loses; scale-up succeeds | `0.10 / 0.18 / 0.25` | `-5.200000` | `8.000000` | `1.333333` | `6.666667` | `-3.866667` |
| Culture platform succeeds; scale-up loses | `0.10 / 0.18 / 0.25` | `-5.200000` | `8.000000` | `1.333333` | `6.666667` | `-3.866667` |
| Both lose | `0.01 / 0.02 / 0.10` | `-16.200000` | `16.000000` | `2.666667` | `13.333333` | `-13.533333` |

Protection reduces the severity of loss. It does not make either single-loss
state or the common-loss state profitable.

## Investor economics

All ranges below optimize over the same bounded physical-probability set. The
minimum, central, and maximum columns are different feasible probability
mixes, not one forecast and not confidence bounds.

| Investor measure | Minimum | Central | Maximum |
|---|---:|---:|---:|
| Expected NPV before protection | `-0.800000` | `1.400000` | `2.390000` |
| Expected NPV after protection, before premium | `0.000000` | `1.933333` | `2.803333` |
| Expected gross contractual principal loss | `2.480000` | `3.200000` | `4.800000` |
| Expected public claim | `0.413333` | `0.533333` | `0.800000` |
| Expected retained principal loss | `2.066667` | `2.666667` | `4.000000` |
| Probability of a loss state | `30.000000%` | `38.000000%` | `50.000000%` |

The NPV and public-claim rows are direct current CLI outputs. Gross expected
loss is the public-claim range divided by `g = 1/6`; retained expected loss is
five sixths of gross expected loss. In this fixture every positive-claim state
also has negative protected NPV, so loss-state probability equals the reported
positive-claim probability.

At the adverse probability witness
`common-success=0.50`, each single loss `=0.20`, and common loss `=0.10`, the
one-sixth guarantee raises worst expected investor NPV from `-0.800000` to
`0.000000` before premium.

## Provider exposure and expected payout

| Provider measure | Minimum | Central | Maximum |
|---|---:|---:|---:|
| Expected nominal payout | `0.413333` | `0.533333` | `0.800000` |
| Probability of positive payout | `30.000000%` | `38.000000%` | `50.000000%` |
| Payout expected shortfall 95% | `1.600000` | `1.866667` | `2.666667` |
| Payout expected shortfall 99% | `2.666667` | `2.666667` | `2.666667` |

Additional exposure facts are:

- contractual maximum exposure: `3.333333`;
- largest modeled scenario payout: `2.666667`;
- uncommitted legal cap capacity at the one-sixth term: `0.000000`; and
- provider hurdle in the fixture: `0%`, so nominal expected payout equals
  expected payout present value.

The `0.800000` maximum expected payout is not the provider's maximum cash need.
Legal authority, appropriation, collateral, or liquidity must support the full
`3.333333` contingent liability, plus any costs and risk buffers required by a
real provider.

## Premium test and explicit catalytic subsidy

At `g=1/6` and a zero robust investor-NPV target:

```text
maximum investor-paid premium                   = 0.000000
provider claim-only robust break-even floor     = 0.800000
minimum disclosed catalytic gap before costs   = 0.800000
```

The `0.800000` is the maximum expected gross claim under the declared
synthetic probability set, valued at the fixture's 0% provider hurdle, less
the zero premium that preserves the investor threshold. It is **not** an OMB
credit-subsidy estimate, EU budgetary provision or State-aid amount,
accounting expected credit loss, regulatory capital requirement, liquidity
reserve, or market premium. Each of those requires additional rules, cash
flows, costs, buffers, or legal analysis.

No non-negative commercial premium satisfies both parties:

- at premium zero, the investor reaches its minimum target but the provider's
  robust claim-only NPV is `-0.800000`;
- at premium `0.800000`, the provider reaches claim-only break-even but the
  investor's robust NPV becomes `-0.800000`.

The first public version therefore sets investor premium to zero and discloses
`0.800000` as the synthetic gross-claim-only gap to be funded by explicit
catalytic capital before provider expenses, liquidity, capital charges,
funding cost, profit, and provider default risk. A real public guarantor would
need its own applicable budget method and legal authority. A separate
guarantee facility could instead receive an explicit grant or capital
contribution.

This research instrument calls that disclosed value transfer **catalytic
subsidy** in an economic, not legal or accounting, sense. It is catalytic
because a public balance sheet bears a modeled claim cost that the investor
cannot pay while preserving the stated robust return threshold. It is not
hidden investor return because:

- it is disclosed outside project cash and outside project NPV before support;
- gross and retained project losses remain visible;
- it is not called diversification, recovery, commercial revenue, or fair
  value; and
- provider expected cost and full contingent exposure are reported separately.

This result does not prove that subsidy is socially optimal. It only states the
gross-claim-only public value-transfer gap under this synthetic probability set
and return target.

## Contractual principal is not investor cost basis

For a claim-ledger-normalized project, buyer price and buyer direct cost belong
to the investor cash and NPV ledger. Contractual principal additions, principal
cash, conversion, and writeoff belong to the separate principal ledger.

An above-par purchase can therefore have negative investment economics while
contractual principal is fully repaid. It produces no claim under this
guarantee. Conversely, a contractual writeoff remains the reference loss even
when buyer price differs from par. Buyer direct costs are never protected
principal.

For an explicit-contractual-ledger portfolio, the protection percentage and
cap domain use the project's `principal_limit_million`, not its investor cash
`commitment_million`. The legacy two-project fixture uses `10` of cash draw and
`10` of principal for each project, so those two amounts happen to coincide.
That fixture equality must not be generalized into an accounting rule.

## Required real-contract provisions

Before real use, transaction documents and admission controls must specify:

- the normalized claims, information cut, package hashes, currency, and
  monetary basis;
- public provider identity, legal authority, appropriation or collateral, and
  payment-right evidence;
- exact loss determination, notice, cure, dispute, and payment dates;
- exclusions, no-double-recovery, subrogation, and allocation of later
  recoveries;
- treatment of claim-level guarantees already included in underlying cash;
- provider default, payment delay, unsecured recovery, and wrong-way states;
- tax, accounting, regulatory-capital, insurance, derivatives, state-aid, and
  public-budget characterization; and
- independent probability, recovery, and dependence evidence.

The title describes the economic design. Whether a real arrangement is legally
a guarantee, insurance contract, derivative, public support obligation, or
another instrument requires jurisdiction-specific advice.

## Verification against the implemented fixture

The figures above were checked against these exact synthetic inputs:

- [`two-project-participation-pool-synthetic.cfg`](../scenarios/two-project-participation-pool-synthetic.cfg)
- [`two-project-probability-envelope-synthetic.cfg`](../scenarios/two-project-probability-envelope-synthetic.cfg)
- [`two-project-success-participation-synthetic.cfg`](../scenarios/two-project-success-participation-synthetic.cfg)
- [`two-project-pooled-loss-protection-synthetic.cfg`](../scenarios/two-project-pooled-loss-protection-synthetic.cfg)

Run the current Debug CLI on Windows with:

```powershell
.\build\dev\Debug\naturalehia-pooled-loss-protection.exe `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-probability-envelope-synthetic.cfg `
  .\scenarios\two-project-success-participation-synthetic.cfg `
  .\scenarios\two-project-pooled-loss-protection-synthetic.cfg
```

The current output matched the stated direct figures. Its underlying-loss,
project-to-pool claim, two-party settlement cash, support-cap, combined-NPV,
probability-witness, and endpoint-probability reconciliation errors all report
`0.000000` at displayed precision. No numerical mismatch was found. Derived
gross-loss and retained-loss ranges are identified as derivations above rather
than presented as direct CLI fields.
