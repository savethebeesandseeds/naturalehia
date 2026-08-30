# Annual Reference Engine v0.1

Status: checked implementation description, 2026-08-27.

This document describes the behavior of the C++ code in `model.cpp` and
`config.cpp`. If this document and the checked source disagree, the source is
controlling and the disagreement is a defect to report. The more detailed
monthly framework in `MODEL_SPECIFICATION.md` is a target for a later version,
not current behavior.

## Purpose and hard boundary

The annual engine is a transparent synthetic scenario tool for one production
facility. It compares the same exogenous facility paths with and without one
configured package of contract terms. It is intended to reveal assumptions,
settlement mechanics, transfers, downside, and missing evidence.

It is not:

- a forecast, valuation opinion, credit rating, derivative price, or offer;
- a bankability or investment decision model;
- a construction, process, food-safety, regulatory, tax, accounting, or legal
  model;
- proof that an instrument changes funding, construction, adoption, or animal
  suffering; or
- calibrated to a company, technology, product, species, buyer, or jurisdiction.

The engine rejects a scenario unless `scenario.synthetic_inputs=true` and
`scenario.model_version=0.1.0`.

## Units, timing, and sign convention

- Time is annual. Financial close is time zero; modeled cash flows occur at
  annual period ends.
- Monetary amounts ending in `_million` use millions of the declared currency
  label.
- Prices and variable costs are input-currency units per kilogram.
- Output is millions of kilograms, so multiplying output by a per-kilogram
  price yields millions of currency units.
- The scenario declares `real`, `nominal`, or `unspecified-synthetic` as its
  monetary basis. The engine records but cannot independently verify that
  declaration. Every scenario must use one internally consistent convention.
- A positive instrument transfer is a receipt by the project. A fee or project
  payment to a counterparty is negative.
- All present values use the configured project discount rate. Sponsor cash
  flow is not discounted at a separately calibrated equity hurdle rate.

## Common random paths

Each Monte Carlo trial creates one exogenous facility path. The engine replays
that exact path first with empty contract terms and then with the configured
terms. It does not redraw construction, production, contamination, price, or
cost risk for the structured case.

This common-random-number design makes the paired difference attributable to
the modeled contract cash flows and financing mechanics. It does not establish
causality outside the model.

The pseudorandom engine is `std::mt19937_64`. Standard-normal draws use a stated
Box–Muller transform. A model version, complete input file, trial count, and
seed are therefore necessary parts of a reproducibility record. Exact sequences
are asserted only within a supported build environment; the economic
invariants must also hold independently of a particular random sequence.

## Facility path

### Construction cost and duration

Realized installed cost and construction duration use mean-one lognormal
multipliers:

```text
realized_capex = base_capex
                 * exp(sigma_capex*z_capex - 0.5*sigma_capex^2)

realized_duration = planned_duration
                    * exp(sigma_duration*z_duration
                          - 0.5*sigma_duration^2).
```

Construction spend is spread uniformly across realized duration. If duration
extends beyond the analysis horizon, the remaining capex is charged at the
horizon as an explicit terminal project and sponsor obligation. The model does
not represent construction commitments, contractor milestones, contingencies,
abandonment, or a construction-funding default.

### Dependence structure

One persistent technical factor and one persistent market factor are drawn for
each path. A driver combines its persistent and idiosyncratic draws as:

```text
z = loading*z_persistent
    + sqrt(1 - loading^2)*z_idiosyncratic.
```

The same technical factor affects capex, duration, utilization, yield,
contamination, and operating cost. The same market factor affects output price
and part of variable cost. This is a small illustrative dependence structure,
not an empirical correlation model. Common process, supplier, buyer,
regulatory, and financing shocks remain materially under-modeled.

### Annual operation

For a period partly occupied by construction, `operating_fraction` is the
fraction after realized construction completion. There is no output before
completion.

Utilization is logit-normal and remains between zero and one:

```text
utilization = logistic(logit(steady_state_utilization)
                       - sigma_utilization*z_technical).
```

The configured steady-state utilization is the zero-factor conditional value,
not generally the unconditional Monte Carlo mean when dispersion is nonzero.

Biological yield is a mean-one lognormal multiplier. Ramp is an intercept at
commercial operation plus an increment based on operating age:

```text
ramp = clamp(ramp_at_commercial_operation
             + annual_ramp_increment*operating_age, 0, 1)

attempted_output = nameplate_output
                   * operating_fraction
                   * ramp
                   * utilization
                   * yield_multiplier.
```

The configured annual contamination probability is adjusted by the technical
factor on a logit scale. Partial-year exposure is then:

```text
p_partial = 1 - (1 - p_full_year)^operating_fraction.
```

The configured probability is the full-year zero-factor conditional value, not
generally the unconditional event frequency when logit dispersion is nonzero.

One Bernoulli event can reduce that year's qualified output by the configured
loss fraction. Variable cost remains payable on attempted output:

```text
qualified_output = attempted_output*(1 - contamination_loss_if_event)

base_revenue = qualified_output*spot_price

variable_cost = attempted_output*variable_cost_per_kg

base_CFADS = base_revenue - variable_cost - fixed_opex.
```

Spot price, variable cost, and fixed operating cost use mean-one lognormal
multipliers. All qualified output is assumed sold. There is no demand,
acceptance, inventory, spoilage, product mix, maintenance capex, working
capital, tax, or regulatory operating constraint in v0.1.

## Modeled contract legs

The package may contain four independent cash-flow legs. Their legal
classification and counterparty are not modeled.

### Fixed-price physical offtake

For its non-overlapping share of qualified output, the engine replaces spot
price with a fixed contract price and records only the incremental value:

```text
offtake_repricing = qualified_output*offtake_share
                    *(contract_price - spot_price).
```

This is not take-or-pay, capacity reservation, a minimum-volume commitment, or
a complete sales model. Buyer default, rejection, termination, credit support,
and resale are absent. The term can reduce project value in high-spot paths.

### Output-price support

For a separate eligible share:

```text
raw_settlement = qualified_output*support_share*(strike - spot_price).
```

A one-way floor replaces negative settlements with zero. A two-way difference
contract retains both signs. The annual and lifetime limits are
**absolute-settlement caps**: in a two-way contract, both provider payments and
project repayments consume the same capacity. This is an explicit v0.1
contract convention, not a universal market standard.

### Completion-delay cover

The cover is a simple parametric payout, not a claim that a legally effective
guarantee, insurance policy, or creditworthy guarantor exists:

```text
observation_time = min(realized_duration, analysis_horizon)
covered_delay = max(0, observation_time - trigger)
payout = min(cover_cap, payout_rate*covered_delay).
```

It settles in the annual bucket containing completion or, for a build extending
past the horizon, at the horizon using only elapsed delay. The engine treats the
payout as unrestricted project cash. It does not model fault, exclusions,
deductibles, claims verification, restricted use, recourse, clawbacks, or
counterparty default.

This leg can reward delay unless a real contract assigns controllable risk,
uses independent certification, excludes sponsor-caused delay, and includes an
appropriate deductible, long stop, and remedies.

### Upfront fee

The project and sponsor pay the configured fee at time zero. It reduces net
project receipts and sponsor cash-flow present value. It is never reported as a
positive support payout.

The report reconciles total signed project receipts to fixed-offtake repricing,
net price-support settlement, completion-delay payout, and the negative fee.
Gross positive support payout excludes physical-offtake repricing because the
latter is a product-sale term.

## Project value

Unprotected project NPV is the present value of base CFADS less construction
spend. Structured project NPV adds signed contract transfers. Debt draws,
service, and recovery do not enter project NPV.

The exogenous operating path continues for project-NPV purposes after a
financing default. This is an unlevered asset view, not a receiver or insolvency
cash-flow model. There is no modeled terminal asset value.

## Debt and sponsor cash flow

### Funding

Senior commitment equals a fixed fraction of base capex. During construction,
the lender funds the same fraction of each capex draw until commitment is
exhausted. The sponsor funds the remainder and every overrun. Sponsor capacity
is unlimited in v0.1; there is no sources-and-uses or construction-funding
default.

Interest on opening debt plus half of the current draw is capitalized before
debt service starts. Service begins in the first annual index at or after
realized construction completion. The opening balance is amortized using a
level annual payment over the configured tenor.

### DSCR and payment default

```text
DSCR = instrument_adjusted_CFADS / scheduled_debt_service
payment_cash = max(0, instrument_adjusted_CFADS).
```

DSCR is signed; negative CFADS therefore produces negative DSCR. A path's
minimum DSCR is conditional on at least one scheduled-payment observation, and
the report separately states the fraction of paths with such an observation.

“Debt payment default within horizon” means the first annual scheduled senior
payment for which nonnegative CFADS is insufficient. There is no reserve,
liquidity facility, equity cure, grace period, waiver, restructuring, or
distribution lockup. At that event:

- the sponsor is treated as losing the project and receives no later project
  cash flow;
- recovery is the configured fraction of exposure after current payment;
- lender loss is reported both as a date-of-default amount and discounted to
  time zero; and
- debt balance is removed.

Across `N` paths, the date-of-default expected loss is unconditional:

```text
EL_at_default_dates = sum_i(loss_i_at_default) / N,
```

where a non-default path contributes zero. Mean loss severity conditional on
default divides the same undiscounted sum by the number of defaulted paths. The
present-value expected loss first discounts each path's loss from its own
default date and then divides by `N`. None of these measures is a regulatory
risk weight, accounting impairment estimate, market spread, or calibrated LGD.

The model does not allocate later project or instrument cash among a receiver,
lender, new owner, or other estate stakeholders. Sponsor and unlevered project
views therefore do not consolidate after default. The report separately shows
expected signed net instrument transfers in periods after a prior-period debt
default so this unresolved allocation remains visible; the value may be
negative under a two-way settlement or adverse offtake repricing. A transfer in
the default year is included in that year's payment-capacity test and is not
classified as post-default.

### Terminal balance convention

If a surviving path retains debt at the analysis horizon, v0.1 assumes the
sponsor pays that balance as a terminal balloon. The amount is deducted from
sponsor cash-flow PV and reported as both a frequency and an unconditional
expected amount across all paths.
The strict input flag
`debt.assume_terminal_balance_paid_by_sponsor=true` makes this convention
visible. Version 0.1 rejects `false` because a terminal default and recovery
waterfall is not implemented.

This assumption must not be mistaken for a real sponsor guarantee or
non-recourse project-finance term.

## Reported statistics

The engine reports means, population standard deviations, linearly interpolated
5th/50th/95th percentiles, and two zero-NPV shortfall measures:

```text
shortfall = max(0, -NPV)
shortfall VaR 95% = interpolated 95th percentile of shortfall
shortfall expected shortfall = mean of the largest ceil(5%*N) shortfalls.
```

It also reports:

- negative project- and sponsor-PV probabilities;
- first-payment-shortfall probability over the declared horizon;
- conditional default-timing distributions, a paired timing-change distribution
  on paths defaulting in both cases, and counts distinguishing within-horizon
  defaults avoided or introduced from defaults delayed, accelerated, or in the
  same period;
- unconditional expected lender loss averaged across all paths at default dates
  and at present value, plus mean loss severity conditional on default;
- DSCR observation coverage and conditional minimum-DSCR distribution;
- terminal sponsor-payment frequency and amount;
- per-leg instrument transfers and gross positive support payouts; and
- raw paired counts for negative-to-nonnegative and
  nonnegative-to-negative NPV transitions.

Raw counts matter because a small probability change may represent only a few
paths. The engine does not calculate sampling confidence intervals or model
uncertainty.

## Interpretation rules

1. A positive average transfer is not newly created value and may be an explicit
   concession from a buyer, public body, philanthropist, insurer, or other
   counterparty.
2. Expected payout is not a fair premium or market price. Pricing also requires
   provider funding, capital, expenses, liquidity, counterparty credit, legal
   characterization, collateral, model risk, and return.
3. A package can improve mean project NPV while increasing the number of
   negative-NPV paths. A fee applies everywhere, fixed offtake removes upside,
   and payouts may concentrate in already deeply negative paths.
4. Support can reduce lender loss while reducing sponsor PV. Senior debt has
   first claim, and avoiding a modeled default can keep the sponsor exposed to
   later loss or a terminal payment.
5. Support cannot repair negative operating economics. The CLI's zero-factor,
   probability-weighted steady-state checkpoint reports base CFADS and
   break-even price before probabilistic results.
6. Output-linked support can reward production without adoption because v0.1
   assumes all qualified output is sold. Independent demand and buyer evidence
   are required before a real proposal.
7. Qualified output is a settlement-sensitive measure. Quality definitions,
   data controls, independent verification, dispute rules, and protection for
   adverse reporting are necessary.
8. A fixed physical path means the engine does not test whether finance changes
   debt capacity, spread, tenor, funding probability, construction date, or
   output. It measures contract effects at fixed financing terms.
9. Financed kilograms are not an animal-welfare result. The separate financing
   and market-displacement counterfactuals in the Responsible Finance Charter
   must both be supported before making an impact claim.

## Current internal software verification and next implementation gate

The checked tests cover strict configuration parsing, schema version, finite
and bounded inputs, synthetic-only governance, fixed-seed repeatability,
zero-contract identity, fee sign, per-leg transfer reconciliation, one-way
floor sampled-sign and default checks, deterministic one- and two-way cap
fixtures, two-way negative settlement, unlevered debt behavior,
signed negative DSCR, horizon-limited delay cover, DSCR observation coverage,
and explicit terminal debt reporting.

The next completed engineering slice is a separate synthetic
[Milestone-Gated Committed Capital Module v0.1](./MILESTONE_GATED_CAPITAL_V0_1.md).
It adds explicit monthly draw, fee, claim, provider-performance, protected-
reserve, and recovery mechanics without changing or claiming to validate this
annual engine. A named-facility calibration still requires the evidence in
`UNDERWRITING_DATA_STANDARD.md`; the integrated monthly sources-and-uses,
operating cash waterfall, demand, counterparty, and default architecture in
`MODEL_SPECIFICATION.md` remains future work.
