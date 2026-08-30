# Pooled Principal-Loss Protection Term v0.1

## Purpose

This module tests one risk-transfer contract around the untranched cellular-
agriculture participation pool:

> Can an outside provider absorb a stated share of final realized principal
> loss, and is there any upfront premium that both the investor and provider
> can accept under the same probability uncertainty?

It is financial engineering, not project management. It does not change which
facilities are funded, their technical outcomes, their cash rights, their
recoveries, or their gross losses. It places part of an already-measured loss
with a separate balance sheet and shows the economics of both sides.

Version 0.1 is synthetic-only. It is a transparent contract and falsification
kernel, not evidence that a guarantor exists or that the result is an
investable quote.

## Contract definition

The underlying success-participation fraction `q` is fixed explicitly. For
joint scenario `s`, let:

```text
B_s(q) = investor NPV of the unchanged underlying pool
L_si   = final resolved principal loss of project i
L_s    = sum_i L_si
```

`L_s` is measured at the portfolio horizon after all declared in-horizon
principal receipts and recoveries. Exposure on a project that is still
continuing at the horizon remains outstanding; it is not converted into a
covered loss.

For coverage fraction `g`:

```text
X_s(g) = g L_s
0 <= g <= 1
```

`X_s(g)` is an external protection payment. It is never classified as returned
principal or project recovery, and it never reduces reported gross project or
pool loss. The investor's memo residual loss is `L_s - X_s(g)`.

The configured monetary support cap `C` limits which exact percentage can be
promised. If `K` is aggregate contractual reference principal, the maximum
supported coverage is:

```text
g_cap = min(1, C / K)
```

The engine searches only `[0, g_cap]`. It does not calculate `min(g L_s, C)`,
because that would silently deliver a smaller effective percentage in the most
severe paths. Basing the domain on contractual reference principal rather than
the largest modeled loss also prevents the finite scenario table from
understating legal notional. The report separates contractual maximum exposure
`g K` from the largest claim found in the modeled paths.

For an explicit-contractual-ledger Portfolio v0.2 project, `K` uses the stated
`principal_limit_million`. Investor cash commitment, purchase price, and buyer
direct costs do not enlarge protected principal. Legacy Portfolio v0.1 treats
each draw as an equal principal addition, so aggregate commitment and aggregate
reference principal happen to be equal there. That compatibility rule must not
be generalized into cost-basis protection.

Version 0.1 requires settlement at the portfolio horizon. The underlying data
does not yet contain a loss-determination date, notice period, cure process, or
post-horizon recovery true-up, so an earlier payment would use information that
has not yet crystallized in the model.

## Investor and provider value

Let:

```text
D_I = (1 + investor physical hurdle)^(horizon / 12)
D_P = (1 + provider physical hurdle)^(horizon / 12)
```

Before premium, the investor's complete scenario NPV is:

```text
Z_s(g) = B_s(q) + X_s(g) / D_I
```

The admitted physical-probability set is:

```text
P = { p : sum_s p_s = 1 and lower_s <= p_s <= upper_s }
```

For investor target `T`, the signed premium headroom is:

```text
H_I(g) = min over p in P of sum_s p_s Z_s(g) - T
```

When `H_I(g)` is negative, the investor cannot pay a non-negative premium and
still meet the target. When it is non-negative, it is the maximum upfront
premium admitted by this physical-NPV test.

The provider's claim-only robust break-even floor is:

```text
F_P(g) = max over p in P of sum_s p_s X_s(g) / D_P
```

This is not fair value and not a market premium. It omits provider funding and
collateral costs, capital, expenses, tax, legal costs, profit, default, payment
delay, exclusions, disputes, subrogation, and wrong-way risk.

A non-negative model price interval exists only when:

```text
H_I(g) >= 0 and F_P(g) <= H_I(g)
```

Otherwise the disclosed premium feasibility gap is:

```text
gap(g) = max(0, F_P(g) - H_I(g))
```

A positive gap is the minimum catalytic or concessionary value needed within
this narrow model before provider costs. It is not hidden inside probabilities
or relabeled as diversification.

At quoted upfront premium `P_0`:

```text
investor NPV_s = Z_s(g) - P_0
provider NPV_s = P_0 - X_s(g) / D_P
```

The premium is an investor-to-provider transfer at month zero. The claim is the
opposite provider-to-investor transfer at the horizon. Neither is project cash.

## Why the two probability witnesses differ

The investor asks whether every admitted probability mix meets its target. The
provider asks whether its premium covers expected payout under every admitted
mix. Those two conservative endpoints may be attained by different probability
witnesses.

The engine therefore projects each complete scenario objective. It never adds
an independently minimized underlying NPV to an independently maximized or
minimized claim. Every reported endpoint retains its own feasible scenario
weights.

Under an identical ambiguity set and identical payout discounting:

```text
min E[B + X] - max E[X] <= min E[B]
```

Protection can move tail risk to a provider whose balance sheet or mandate is
better suited to bear it, but a fully compensated provider cannot manufacture
robust aggregate value. If the underlying pool is short of its target, an
honest result may be that no bilateral premium works.

## Solver result

For the configured `q`, cap, and target, the engine reports zero coverage, the
maximum supported coverage, and either an exact boundary or a certified
failing/feasible coverage bracket. The reported fraction is the conservative
feasible upper endpoint.

It distinguishes:

1. the investor target already being met with no protection;
2. no realized reference loss in any modeled scenario;
3. no monetary support capacity;
4. an interior certified bracket;
5. a certified bracket at the cap boundary;
6. exactly full coverage being required; and
7. the target remaining unattainable at maximum supported coverage.

For every reported point, the output includes investor NPV, provider payout,
claim probability, payout expected shortfall at 95% and 99%, retained investor
loss, modeled maximum claim, contractual cap, premium headroom, provider floor,
price gap, endpoint witnesses, project attribution, and reconciliation controls.

## Hand-calculated synthetic result

The two-project fixture has gross pool loss of `16` in common loss, `0` in
common success, and `8` in each single-project loss. At full success
participation, its underlying expected NPV range is:

| Expected NPV at `q=1` | Minimum | Central | Maximum |
|---|---:|---:|---:|
| Before protection | -0.80 | 1.40 | 2.39 |

With zero investor hurdle, protected expected NPV before premium is:

```text
minimum = -0.80 + 4.80g
central =  1.40 + 3.20g
maximum =  2.39 + 2.48g
```

The smallest pre-premium coverage that reaches a zero robust target is
`g = 1/6`, or about `0.166667`. At that fraction:

| Provider payout measure | Minimum | Central | Maximum |
|---|---:|---:|---:|
| Expected payout | 0.413333 | 0.533333 | 0.800000 |
| Payout expected shortfall 95% | 1.600000 | 1.866667 | 2.666667 |
| Payout expected shortfall 99% | 2.666667 | 2.666667 | 2.666667 |

The largest modeled claim is `2.666667`; a contract covering one sixth of the
pool's `20` aggregate reference principal has contractual maximum exposure of
`3.333333`.

At the threshold the investor's premium headroom is zero, while the provider's
claim-only robust floor is `0.80`. More generally at `q=1`:

```text
investor headroom = -0.80 + 4.80g
provider floor    =         4.80g
premium gap       =  0.80
```

The gap does not disappear when coverage rises. Protection redistributes the
tail, but the same conservative probability set prevents it from repairing the
underlying `0.80` value shortfall at a provider-compensating premium. A viable
version therefore needs at least `0.80` of explicit catalytic support in this
fixture, before provider costs, or a real improvement in the underlying project
cash, loss, timing, cost, or evidence-supported probability bounds.

## Strict assumptions and current boundary

The protection file must state that:

- final modeled principal loss is the contractual reference amount;
- gross project loss remains visible;
- external support is assumed fully funded and performing in every modeled
  scenario; and
- premium is an external upfront investor-to-provider transfer.

These are model assertions, not evidence. The pool must be untranched in this
version so an external payment is not confused with internal loss-layer
priority. The strict parser rejects missing, duplicate, unknown, unsafe, or
non-finite terms and the normalized form reloads deterministically.

Version 0.1 protects contractual principal writeoff only. It does not protect
an investor's acquisition premium, discount, direct transaction cost, missed
interest, or target-return shortfall. A fully repaid above-par purchase can
therefore have negative investor economics and a zero protection claim.

The closed `key=value` schema is:

```text
protection.model_version=0.1.0
protection.label=...
protection.source_note=...
protection.provider_id=...
protection.synthetic_inputs=true
protection.portfolio_principal_loss_is_contractual_reference_amount=true
protection.gross_project_loss_remains_visible=true
protection.support_is_assumed_fully_funded_and_performing_in_all_scenarios=true
protection.premium_is_upfront_at_month_zero=true
protection.underlying_success_participation_fraction=...
protection.settlement_month=...
protection.support_cap_million=...
protection.provider_annual_physical_hurdle_rate=...
```

Run the synthetic analysis on Windows after building:

```powershell
.\build\dev\Debug\naturalehia-pooled-loss-protection.exe `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-probability-envelope-synthetic.cfg `
  .\scenarios\two-project-success-participation-synthetic.cfg `
  .\scenarios\two-project-pooled-loss-protection-synthetic.cfg
```

Add `--print-normalized` to append all four reloadable inputs.

The separate
[Provider Price-Ladder Sensitivity v0.1](PROVIDER_PRICE_LADDER_V0_1.md) now
retains this exact claim and adds disclosed incremental collateral carry,
economic-capital charge, expenses, and target profit. It does not change this
claim-only term result or infer provider creditworthiness.

A real transaction also requires a named and creditworthy provider, executed
obligation, reference and claims language, cap and duration, collateral or
appropriation mechanics, payment timing, exclusions, dispute process,
recoveries and subrogation, no-double-recovery controls, legal characterization,
tax, accounting, regulatory and prudential analysis, and calibrated provider
default and wrong-way states.

The economic arrangement may ultimately be characterized as a guarantee,
insurance, derivative, public support obligation, or another contract. Version
0.1 makes no legal-classification claim.
