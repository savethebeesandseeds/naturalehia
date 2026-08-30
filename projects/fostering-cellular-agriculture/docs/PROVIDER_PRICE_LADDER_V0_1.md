# Provider Price-Ladder Sensitivity v0.1

## Purpose

The pooled principal-loss protection engine identifies the expected claim and
the investor's capacity to pay an upfront premium. That is necessary but not a
provider quote. A credible provider must also recover incremental funding,
capital, and operating costs and earn a stated return.

This module makes that second side visible. It is a deterministic C++20
**physical-probability premium-adequacy sensitivity**. It is not fair value, a
market quote, a provider balance sheet, a credit assessment, or proof of
regulatory-capital sufficiency.

The engine never changes the protection claim, gross project loss, project
cash, scenario probabilities, or investor target. It selects one protection
point and builds a transparent provider price ladder above its existing
claim-only floor.

## Selected protection point

The pricing input selects either:

- the protection solver's certified investor-target-passing upper endpoint;
  or
- an explicit coverage fraction inside the support-cap domain.

Reported-point mode fails when no certified passing point exists. It never
silently prices maximum coverage instead. An explicit point is freshly
projected through the full probability envelope; it is not interpolated from
published endpoints because the binding probability witness can change.

Let:

```text
g       selected proportional loss coverage
K       aggregate contractual reference principal
N = gK  contractual maximum exposure
T       settlement month
tau     T / 12 years
hP      provider annual physical-NPV hurdle
DP      (1 + hP)^tau
Xs      terminal claim in complete scenario s
Cs      Xs / DP, provider claim present value
```

`N`, the configured monetary support cap, the maximum claim observed in
modeled scenarios, collateral, and allocated capital are different quantities
and remain separately reported. Legal enforceability is not validated.

## Price ladder

Version 0.1 accepts these disclosed incremental assumptions:

```text
chi  collateral fraction of N
f    collateral annual effective funding rate
y    collateral annual effective asset yield
k    allocated economic-capital fraction of N
c    annual effective incremental economic-capital charge rate
eta  variable claim-expense fraction
E0   fixed expense already expressed at month zero
M0   target underwriting profit at month zero
```

Collateral and capital are assumed held until claim settlement. Variable claim
expense is paid with the claim. Their amounts are:

```text
collateral base A = chi N

collateral carry PV
  = A [(1 + f)^tau - (1 + y)^tau] / DP

allocated economic-capital base E = k N

incremental economic-capital charge PV
  = E [(1 + c)^tau - 1] / DP
```

For each complete scenario, provider cost recovery is:

```text
(1 + eta) Cs + collateral carry PV
              + economic-capital charge PV
              + E0
```

The robust cost-recovery floor is the largest expected value of that complete
scenario objective over every admitted probability mix. The robust all-in
floor adds `M0` once.

Because variable expense is a nonnegative constant fraction of the claim and
the other costs are deterministic, the implementation applies one positive
affine transformation to the complete claim-PV range. It preserves each
endpoint's feasible probability witness. If a later cost depends on scenarios
in another way, the complete combined cost vector must be projected again;
independently maximized components may not be added.

## Independent cost-allowance convention

The collateral and capital fractions are independent exposure-sizing bases,
not debt and equity funding shares, and they need not sum to one. Capital does
not reduce the collateral base in v0.1.

To use this convention without double counting, the analyst must assert that:

- collateral carry is a standalone net incremental allowance already adjusted
  for any premium float, capital contribution, collateral reuse, or treasury
  offset reflected in the supplied rate;
- the economic-capital charge is a separate incremental allowance already net
  of investment income and collateral benefits—not allocated capital stock, a
  gross shareholder required return, or a validated regulatory requirement;
- allocated capital is at-risk balance-sheet capacity, but its stock is not a
  premium expense and modeled claim loss is already charged;
- fixed expense excludes claim principal, variable claim expense, collateral
  carry, capital compensation, and profit;
- variable expense excludes the claim itself; and
- target profit is above cost recovery and is not duplicated in the capital
  charge.

These assertions expose the accounting convention; they do not prove the
inputs. If actual capital funds collateral, the appropriate balance-sheet
model instead charges treasury carry only on collateral not funded by capital
and credits the yield on capital-funded assets. Inputs from that convention
cannot be copied unchanged into this one.

## Investor ceiling and required catalytic support

Let `H` be investor signed headroom before premium. The protection engine
publishes a conservatively representable nonnegative premium ceiling only when
the target is met. All price feasibility and gap calculations use that
certified ceiling, not a slightly larger raw floating-point value.

A bilateral interval exists exactly when a nonnegative investor ceiling exists
and the all-in provider floor does not exceed it. Equality is a valid one-point
interval.

When no interval exists, the engine separates:

```text
provider premium support
  = amount still needed to pay the provider floor after using the certified
    nonnegative investor premium capacity

investor target restoration
  = amount needed to repair a negative pre-premium investor headroom

total all-in support gap
  = provider premium support + investor target restoration
```

This decomposition prevents a provider subsidy from being presented as if it
also repaired an underlying investor deficit. Any real catalytic payment would
need its own identified third-party cash leg.

## Hand-reconciled synthetic result

At the existing `q=1` and certified `g` near `1/6` point:

```text
contractual exposure                     3.333333
modeled maximum claim                    2.666667
robust expected claim PV                 0.800000
variable claim expense, 10%              0.080000
collateral carry, 50%; 6% less 2%, 2y   0.138667
capital charge, 30%; 10%, 2y             0.210000
fixed expense                            0.050000
robust cost-recovery floor               1.278667
target underwriting profit               0.100000
robust all-in provider floor             1.378667
investor certified premium ceiling       0.000000
total all-in support gap                 1.378667  DEMO million
```

The physical expected cost-recovery range is
`0.853333 / 0.985333 / 1.278667`; the all-in range is
`0.953333 / 1.085333 / 1.378667`. The adverse claim witness remains the adverse
all-in provider witness.

The result is economically useful because it falsifies a tempting conclusion:
loss protection may move `2.666667` of modeled tail claim exposure, but these
synthetic provider assumptions widen the premium gap from the claim-only
`0.800000` to `1.378667`. Risk transfer does not manufacture value.

## Input, controls, and boundary

The strict `provider_price.*` file has 21 required keys. It rejects unknown,
duplicate, missing, malformed, non-finite, and incoherent values; requires all
accounting assertions; and rejects claims that provider default or fair value
are modeled. Normalized output is reloadable.

The report reconciles selected coverage, the cost ladder, the affine range,
the two support components, and endpoint probability feasibility. Debug and
Release tests compile with strict warnings.

Provider performance is still assumed inside this pricing module. The separate
[Provider Counterparty-Credit Stress v0.1](PROVIDER_CREDIT_STRESS_V0_1.md) now
tests correlated settlement performance while copying this full-performance
price unchanged. Collateral enforceability, disputes, pre-settlement default,
dynamic margin, close-out, tax, interim capital release, premium-funded
collateral, currency basis, regulatory capital, and contractual price ticks
remain outside the combined v0.1 boundary and require real provider evidence.
