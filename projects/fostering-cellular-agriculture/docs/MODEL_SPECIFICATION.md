# First Facility Risk Model

Status: provisional monthly-v0.2 target specification and implementation
roadmap, 2026-08-27.

This document specifies the first transparent risk model for **Fostering
Cellular Agriculture**. The model asks a deliberately narrow question:

> Given an explicitly described production facility, capital structure, and
> set of synthetic risk assumptions, how do construction, scale-up,
> production, input-cost, demand, and regulatory uncertainty affect project
> cash flow, debt service, default, investor loss, and the cost of candidate
> support instruments?

The target model is a physical-probability Monte Carlo model. It is not a
risk-neutral derivative pricer, a forecast for a real facility, or a
substitute for engineering, legal, regulatory, food-safety, tax, accounting,
or investment review. All defaults in this specification are synthetic
regression fixtures. They must not be presented as industry estimates or used
to solicit capital.

## 0. Current-v0.1 boundary

The checked C++ v0.1 is an intentionally limited **annual reference engine**.
Its current behavior is defined by [`model.hpp`](../include/naturalehia/cellular_finance/model.hpp)
and [`model.cpp`](../src/model.cpp), not by the monthly equations below. It
currently provides:

- annual project periods and a synthetic analysis horizon;
- stochastic capital cost and construction duration;
- annual utilization, biological-yield, contamination, price, variable-cost,
  and fixed-cost effects with a small persistent-factor structure;
- basic senior-debt draw, amortization, DSCR, default, and recovery mechanics;
- fixed-price physical offtake, one-way or two-way output-price support, a
  simple completion-delay payout, and an upfront fee;
- paired unsupported/supported paths; and
- summary NPV, shortfall VaR/expected shortfall, output, DSCR, debt-default,
  debt-loss, and instrument-transfer results.

Current v0.1 does **not** implement monthly construction draws, regulatory
clearance, demand-limited sales, working capital, reserves, sponsor
commitments, a full cash waterfall, multiple default definitions, dynamic
offtaker credit, correlated AR(1) operating paths, partial debt guarantees, or
the broader output suite specified here.

A separate checked module now implements a bounded **milestone-gated committed
capital facility v0.1**. It is not the integrated monthly project model in
Sections 1 through 13. It evaluates explicit deterministic construction cases,
phase-end certification decisions, provider-performance flags, source tests,
monthly commitment fees and capped PIK, a protected-workout reserve, and a
terminal completion or recovery waterfall. It enforces pathwise cash and memo-
account identities and reports a physical-P zero-NPV upfront-fee sensitivity.
It has no operating model, senior debt, invoice register, cure process,
risk-neutral valuation, empirical calibration, or legal characterization. Its
exact implemented boundary is [Milestone-Gated Committed Capital Module
v0.1](./MILESTONE_GATED_CAPITAL_V0_1.md).

Sections 1 through 13 are the provisional monthly v0.2 target model. Unless
a passage is explicitly labeled `current v0.1`, words such as “must,” “uses,”
and “reports” state a target requirement; they do not claim that the checked
annual engine already behaves that way. Implementation should proceed in
reviewable stages, preserving current-v0.1 regression behavior until an
intentional model-version change is accepted.

## 1. Scope and design commitments

The target model described below covers one greenfield facility, one qualified
output, one senior debt facility, sponsor equity, and zero or more support
instruments. It uses a monthly time step from financial close through
construction and ten years of operations. All monetary cash flows are real,
pre-tax, and denominated in EUR millions unless a row explicitly states EUR
per kilogram.

The target implementation will:

1. Use dependency-free C++20 and only the standard library.
2. Keep every equation and convention in this document traceable to code.
3. Generate an exogenous project path once and replay that same path against
   every candidate instrument. Instrument comparisons therefore use common
   random numbers.
4. Report unsupported and supported outcomes separately. Support must not
   make the underlying facility appear intrinsically more economic.
5. Preserve failed, delayed, contaminated, and zero-output paths in all
   statistics.
6. Make the random seed, model version, input set, trial count, and quantile
   convention part of every machine-readable result.
7. Reject invalid inputs and non-finite calculations rather than silently
   clipping them, except where an explicit physical bound is part of the
   stated model.

The target version deliberately excludes portfolio diversification, secondary
trading, risk-neutral pricing, taxes, inflation, foreign exchange, endogenous
market prices, refinancing, strategic behavior, and feedback between support
and operational decisions. These are later extensions, not hidden
assumptions.

## 2. Unit of account and definitions

The modeled product is `qualified_output_kg`: kilograms released by the
facility's quality system under one fixed product specification. It is not
reactor volume, gross wet biomass, final blended-food tonnage, or announced
annual capacity. A real calibration must record moisture, dry-matter, protein,
cell fraction, and quality-release criteria outside the model.

| Symbol | Meaning | Unit or domain |
| --- | --- | --- |
| `t` | Month index; financial close is `t = 0` | Nonnegative integer |
| `K0` | Base installed project cost before overrun | EUR million |
| `K` | Realized installed project cost | EUR million |
| `C0` | Planned construction duration | Months |
| `C_build` | Realized construction-completion month | Month index |
| `C_reg` | Regulatory-clearance month | Month index |
| `COD` | Commercial-operation month, `max(C_build, C_reg)` | Month index |
| `Q_nameplate` | Annual attempted-output capacity at full utilization | kg/year |
| `q_attempt,t` | Attempted production during month `t` | kg |
| `q_qualified,t` | Quality-released production | kg |
| `q_sold,t` | Accepted and sold production | kg |
| `P_t` | Net realized market price before support | EUR/kg |
| `CFADS_t` | Cash flow available for debt service | EUR million |
| `DS_t` | Contractual senior interest plus principal due | EUR million |
| `B_t` | Senior principal outstanding after month `t` | EUR million |

Multiplying kilograms by EUR/kg produces EUR. Divide by `1,000,000` exactly
once when placing the result in an EUR-million cash-flow account.

## 3. Timeline, construction, and commercial operation

### 3.1 Realized capital cost

The realized installed cost is

```text
K = K0 * M_capex,

M_capex = exp(log(mean_capex_multiplier)
              - 0.5*sigma_capex^2
              + sigma_capex*Z_capex_bad).
```

Without truncation, `mean_capex_multiplier` is the arithmetic mean. Positive
`Z_capex_bad` means a worse overrun. No post-draw value may replace the sampled
cost with a management estimate.

Construction takes

```text
D_build = round(clamp(delay_location
                      + delay_scale*Z_delay_bad,
                      0, max_build_delay))

C_build = C0 + D_build.
```

`round` means nearest integer with half values away from zero. Because the
argument is nonnegative, this agrees with `floor(x + 0.5)`.

Monthly construction weights use a normalized sine curve:

```text
raw_w_t = sin(pi*(t + 0.5)/C_build),   0 <= t < C_build
w_t     = raw_w_t / sum(raw_w_j)
capex_t = K*w_t.
```

This curve is only a transparent fixture. A real project must replace it with
an independently reviewed sources-and-uses schedule. The target implementation
must ensure that monthly construction spend sums to `K` within floating-point
tolerance.

### 3.2 Regulatory clearance and long stop

The target monthly model samples an independent categorical regulatory delay:

```text
D_reg in {0, 12, 24} months
C_reg = C0 + D_reg
COD   = max(C_build, C_reg).
```

The categorical probabilities are inputs. Independence is a simplifying
assumption, not a claim that regulatory and technical delays are unrelated.
Alternative fixed-delay scenarios must be reported because a Gaussian
correlation model is not credible evidence for regulatory tail risk.

If `COD` is later than `long_stop_month`, the project enters completion
default unless an enabled instrument expressly extends the long stop. A
facility produces no output before both construction completion and regulatory
clearance. From `C_build` until `COD`, it pays the declared monthly standby
cost.

### 3.3 Ramp and attempted production

For operating age `a_t = t - COD + 1`, measured in months:

```text
tau_ramp = base_ramp_months * exp(sigma_ramp*Z_ramp_bad)

utilization_t = target_utilization
                * (1 - exp(-a_t/tau_ramp))

q_attempt,t = (Q_nameplate/12) * utilization_t.
```

`q_attempt,t = 0` when `a_t <= 0`. Positive `Z_ramp_bad` slows the ramp. This
definition separates utilization from biological yield and quality release;
those quantities must not be multiplied into an input called utilization.

### 3.4 Qualified and sold output

For every operating month:

```text
yield_t = clamp(exp(-0.5*sigma_yield^2
                    + sigma_yield*Z_yield_good,t),
                yield_min, yield_max)

availability_t = clamp(base_availability
                       + sigma_availability*Z_availability_good,t,
                       availability_min, 1)

p_contamination,t = logistic(logit(base_contamination_probability)
                             - contamination_sensitivity
                               * Z_availability_good,t)

I_contamination,t ~ Bernoulli(p_contamination,t)

loss_t = contamination_loss_fraction * I_contamination,t

q_qualified,t = q_attempt,t * yield_t * availability_t * (1 - loss_t)

demand_t = (base_annual_demand/12)
           * exp(-0.5*sigma_demand^2
                 + sigma_demand*Z_demand_good,t)

q_sold,t = min(q_qualified,t, demand_t).
```

Variable production cost is charged on attempted output, not sold output.
This preserves the cash cost of failed batches and unsold production. The
provisional monthly v0.2 target has no inventory carry-over; unsold qualified
output has zero salvage and
incurs the configured disposal cost. A later inventory model must not be added
without also modeling shelf life, storage, working capital, and quality expiry.

Net market price and attempted-output variable cost are

```text
P_t = base_price
      * exp(-0.5*sigma_price^2 + sigma_price*Z_price_good,t)

c_var,t = base_variable_cost
          * exp(-0.5*sigma_input_cost^2
                + sigma_input_cost*Z_input_cost_bad,t).
```

Fixed operating cost and maintenance capital are zero before COD. The standby
cost is the only target-model operating cash outflow between construction
completion and COD.

## 4. Stochastic structure and correlation

### 4.1 Reproducibility

The target implementation uses `std::mt19937_64` and standard-library normal,
uniform, Bernoulli, and discrete distributions. The default seed is fixed.
Driver order is part of the versioned model contract. Every trial generates
all exogenous random variables—including offtaker-credit variables—before any
instrument is evaluated. Instruments may not consume random values.

The target reference implementation remains single-threaded so results are
bit-for-bit repeatable on the same conforming implementation and
standard-library environment. Cross-library bit identity is not promised
because C++ does not specify distribution algorithms. Golden tests should
therefore include a deterministic zero-risk fixture in addition to seeded
Monte Carlo summaries.

### 4.2 Factor construction

Correlation is represented through named factors instead of a hard-coded
Cholesky decomposition. This makes the assumed economic links visible and
guarantees a positive-semidefinite correlation matrix when every row's squared
loadings sum to at most one.

For driver `i`, draw independent standard normals for the common factors
`F_scale`, `F_market`, `F_input` and idiosyncratic `epsilon_i`:

```text
Z_perm,i = lambda_i,scale*F_scale
           + lambda_i,market*F_market
           + lambda_i,input*F_input
           + sqrt(1 - sum_f(lambda_i,f^2))*epsilon_i.
```

The signs are semantic: a positive scale factor worsens cost and delay, while
reducing yield and availability through negative loadings.

The synthetic loadings are:

| Driver | Scale | Market | Input |
| --- | ---: | ---: | ---: |
| `capex_bad` | `0.65` | `0.00` | `0.15` |
| `delay_bad` | `0.70` | `0.00` | `0.10` |
| `ramp_bad` | `0.60` | `0.00` | `0.05` |
| `yield_good` | `-0.55` | `0.05` | `-0.05` |
| `availability_good` | `-0.45` | `0.05` | `-0.10` |
| `demand_good` | `0.00` | `0.75` | `0.00` |
| `price_good` | `0.00` | `0.60` | `0.10` |
| `input_cost_bad` | `0.05` | `-0.05` | `0.75` |
| `offtaker_credit_good` | `0.00` | `0.55` | `0.00` |

These values are synthetic. They encode hypotheses to stress, not measured
correlations.

For time-varying drivers, generate a second, independent set of common-factor
and idiosyncratic innovations with the same loading table. With common
autoregressive coefficient `rho`:

```text
X_i,0 = correlated_standard_normal_innovation_i,0

X_i,t = rho*X_i,t-1
        + sqrt(1 - rho^2)
          * correlated_standard_normal_innovation_i,t

Z_i,t = sqrt(permanent_weight_i)*Z_perm,i
        + sqrt(1 - permanent_weight_i)*X_i,t.
```

`X_i,t` is stationary standard normal, and `Z_i,t` is marginally standard
normal. Construction cost, construction delay, and ramp use their permanent
drivers directly. The operating drivers use the combined values above.

| Time-varying driver | Permanent weight |
| --- | ---: |
| Yield | `0.70` |
| Availability and contamination | `0.50` |
| Demand | `0.65` |
| Market price | `0.60` |
| Variable input cost | `0.70` |
| Offtaker credit | `0.80` |

The Gaussian factor model has thin joint tails. It must be accompanied by
explicit coupled stress scenarios. A real calibration may replace it with
empirical resampling or another reviewed dependence model, but it may not add
an undocumented correlation premium.

### 4.3 Offtaker credit event

The offtaker-credit path is generated even when no offtake contract is active:

```text
base_monthly_offtaker_hazard
  = 1 - pow(1 - base_annual_offtaker_hazard, 1.0/12.0)

p_offtaker_default,t = logistic(logit(base_monthly_offtaker_hazard)
                               - credit_sensitivity
                                 * Z_offtaker_credit_good,t).
```

Default is absorbing. After default, contractual offtake payments are
multiplied by the configured unsecured recovery fraction. Spot-market sales,
if enabled, remain limited by market demand. The synthetic hazard is not a
credit rating or a substitute for counterparty underwriting.

## 5. Operating cash flow

All operating cash-flow equations are evaluated before financing.

```text
market_revenue_t = q_sold,t * P_t / 1,000,000

variable_opex_t = q_attempt,t * c_var,t / 1,000,000

disposal_opex_t = (q_qualified,t - q_sold,t)
                  * disposal_cost / 1,000,000

fixed_opex_t = annual_fixed_opex/12

maintenance_capex_t = K0 * annual_maintenance_fraction/12

operating_cash_before_wc_t = market_revenue_t
                             - variable_opex_t
                             - disposal_opex_t
                             - fixed_opex_t
                             - maintenance_capex_t.
```

Working capital is calculated from current-month run rates:

```text
WC_t = market_revenue_t * ar_days/30
       + variable_opex_t * inventory_days/30
       - variable_opex_t * ap_days/30

delta_WC_t = WC_t - WC_t-1

CFADS_pre_support,t = operating_cash_before_wc_t - delta_WC_t.
```

Before COD, `CFADS_pre_support,t` is zero except for standby cost, which is
negative. At the final modeled operating month, remaining working capital is
released. Decommissioning cost and terminal value default to zero and must be
shown explicitly if later enabled.

Non-credit support such as a price floor, take-or-pay shortfall payment, or
operating grant is added separately:

```text
CFADS_post_operating_support,t = CFADS_pre_support,t
                                 + operating_support_t.
```

The model reports both values. It never relabels support as product revenue.

## 6. Construction financing, debt service, and default

### 6.1 Sources during construction

For each construction month, eligible senior debt funds
`debt_draw_fraction` of construction spend until the debt commitment is
exhausted. Grants and overrun support are applied according to their explicit
waterfall. Sponsor equity funds the residual and the initial debt-service
reserve, subject to the sponsor commitment.

Before the scheduled service-start month, interest is capitalized when debt
commitment remains:

```text
monthly_rate = pow(1 + annual_debt_rate, 1.0/12.0) - 1

interest_t = monthly_rate * (B_t-1 + debt_draw_t)

B_t = B_t-1 + debt_draw_t + capitalized_interest_t.
```

Any pre-service interest that cannot be capitalized is an equity requirement.
Debt draws end at the scheduled service-start month even if construction is
late. This exposes delay risk instead of assuming that lenders automatically
extend commitments.

A construction funding default occurs when required sponsor cash exceeds the
remaining sponsor commitment after enabled construction support. A completion
default occurs when `COD` exceeds the applicable long stop. The model records
these separately.

### 6.2 Scheduled amortization

At the service-start month, freeze the amortizing opening balance `B0`. With
`N` remaining monthly payments, level debt service is

```text
A = B0/N                                      when monthly_rate = 0

A = B0*monthly_rate
    / (1 - pow(1 + monthly_rate, -N))         otherwise.
```

For each payment month:

```text
interest_due_t  = monthly_rate * B_t-1
principal_due_t = min(B_t-1, A - interest_due_t)
DS_t            = interest_due_t + principal_due_t
B_t             = B_t-1 - principal_paid_t.
```

The last scheduled payment clears any residual principal caused by
floating-point rounding. No negative principal is permitted.

### 6.3 Cash waterfall and default definitions

The monthly operating waterfall is:

1. Receive market revenue and non-credit operating support.
2. Pay operating cost, maintenance capital, and working-capital change.
3. Use unrestricted project cash and then the debt-service reserve.
4. Apply any contractually available liquidity facility.
5. Determine the project's debt-service shortfall.
6. Apply a debt-service guarantee directly to the lender.
7. Record any residual missed payment as payment default.
8. If no default remains, refill the reserve to its target.
9. Apply the distribution-lockup test; distribute only residual permitted
   cash to equity.

Three different events must be reported:

- `project_distress`: project cash, reserve, and permitted liquidity are
  insufficient for scheduled debt service before a lender guarantee.
- `payment_default`: a positive debt-service shortfall remains after the
  guarantee.
- `lender_loss`: recovery and guarantee proceeds are less than outstanding
  principal plus due interest after a payment or completion default.

A guarantee can therefore reduce lender default or loss without pretending
that the project avoided distress.

Monthly debt-service coverage is

```text
DSCR_pre_support,t  = CFADS_pre_support,t / DS_t
DSCR_post_support,t = CFADS_post_operating_support,t / DS_t
```

only when `DS_t > 0`. Negative CFADS produces a negative DSCR. Months without
scheduled debt service are excluded rather than assigned infinite DSCR. A
rolling 12-month DSCR is also reported once twelve payment months exist. A
DSCR below the lockup threshold blocks distributions but is not itself a
payment default.

Upon uncured default, operations stop in the target monthly model. The lender
receives the configured fraction of outstanding principal as recovery, plus
remaining guarantee proceeds subject to their cap. Construction and operating
recovery fractions are separate synthetic inputs.

## 7. Candidate support instruments

Every instrument is disabled by a zero limit in the default unsupported run.
Payouts are tracked by instrument and month. Caps are cumulative nominal
amounts unless explicitly described as present values.

### 7.1 Capital grant

A grant pays a fixed fraction of eligible construction spend, subject to a
cumulative cap:

```text
grant_t = min(grant_fraction*eligible_capex_t,
              remaining_grant_cap).
```

It is applied before senior debt and equity. Both its gross payout and its
effect on financeability are reported.

### 7.2 Cost-overrun facility or guarantee

The provider pays only after realized cumulative cost exceeds base cost plus a
sponsor deductible:

```text
eligible_overrun_t = max(0,
                         cumulative_capex_t
                         - K0
                         - overrun_deductible)

overrun_support_t = min(eligible_overrun_t
                        - cumulative_overrun_support_t-1,
                        remaining_overrun_cap).
```

Negative incremental values are floored at zero. The contract does not pay
for operating losses.

### 7.3 Price floor or contract for difference

For eligible accepted volume `q_eligible,t`:

```text
floor_payment_t = q_eligible,t
                  * max(0, strike_price - P_t)
                  / 1,000,000.
```

Eligible volume is capped monthly and cumulatively. A two-way contract for
difference additionally requires the project to return
`q_eligible,t*max(0, P_t - strike_price)/1,000,000` when price is above strike.
One-way and two-way contracts are distinct instrument types and must not share
a label.

### 7.4 Take-or-pay offtake

The buyer pays the contract price for accepted delivery and, subject to its
terms, a shortfall amount up to minimum contracted volume. Contract allocation
replaces market revenue for the allocated kilograms; it is not added to market
revenue for the same output:

```text
contract_delivered_t = min(q_qualified,t, contract_volume_t)

spot_available_t = q_qualified,t - contract_delivered_t
spot_sold_t      = min(spot_available_t, demand_t)

delivery_cash_t = contract_delivered_t
                  * contract_price / 1,000,000

spot_cash_t = spot_sold_t * P_t / 1,000,000

take_or_pay_cash_t = max(0, contract_volume_t - contract_delivered_t)
                     * shortfall_payment_rate
                     / 1,000,000.
```

The target monthly model treats make-up rights, quality disputes, and resale as
absent. Offtaker default reduces contract cash by the configured recovery
fraction. Spot sales may occur only for output not allocated to the contract,
and remain demand-limited. The report separates ordinary payment for delivered
product from support-like shortfall payment.

### 7.5 Liquidity facility or contingent subordinated capital

After project cash and reserve, a capped facility may fund a debt-service or
construction shortfall. Each draw becomes a subordinated balance accruing the
configured real rate. It is repaid after senior debt and reserve refill but
before common-equity distributions. Expected draws, peak balance, repayments,
and unpaid terminal balance are reported. Calling a repayable draw a grant is
not permitted.

### 7.6 Partial debt-service guarantee

After project resources are exhausted:

```text
guarantee_t = min(coverage_fraction*debt_service_shortfall_t,
                  remaining_guarantee_cap).
```

The guarantee pays the lender, not project revenue. The target monthly model
assumes no recovery by the guarantor from the project; a recovery claim must be
added as an explicit subordinated cash flow in a later version.

### 7.7 Long-stop extension

An extension changes only the applicable completion deadline and, if its
terms say so, the debt-service start. These are separate switches. Extending a
deadline must not silently add debt commitment, equity, or capitalized
interest capacity.

### 7.8 Output- or yield-linked cover

The target engine may calculate a hypothetical payout against independently
verified qualified output:

```text
output_cover_t = cover_rate
                 * max(0, covered_output_floor_t - q_qualified,t)
                 / 1,000,000.
```

`cover_rate` is denominated in EUR per kilogram.

It is research-only and disabled in the initial comparison set. A real
insurance interpretation requires credible loss history, exclusions,
deductibles, claims verification, moral-hazard controls, licensing, and
capital. Expected modeled payout is not a fair premium.

## 8. Valuation and risk outputs

Discount monthly cash flows with

```text
monthly_discount_rate = pow(1 + annual_discount_rate, 1.0/12.0) - 1
DF_t = pow(1 + monthly_discount_rate, -t).
```

### 8.1 Project and equity value

Report at least:

```text
NPV_project_pre_support
  = sum_t DF_t * (-capex_t + CFADS_pre_support,t
                  + terminal_working_capital_release_t)

NPV_project_post_support
  = NPV_project_pre_support
    + sum_t DF_t * all_nonrepayable_support_t

NPV_equity
  = sum_t DF_t * (equity_distributions_t - equity_contributions_t).
```

Debt draws and repayments are financing flows and do not enter unlevered
project NPV. Repayable subordinated support is also financing, not project
value. Report mean, standard deviation, median, 5th, 25th, 75th, and 95th
percentiles for each NPV. Equity IRR may be reported as diagnostic output but
must be `undefined` when cash-flow signs do not permit a unique solution.

### 8.2 Debt metrics

Report:

- Minimum monthly and rolling-12-month DSCR, pre- and post-operating support.
- 5th, 50th, and 95th percentiles of minimum DSCR across trials.
- Probability of construction funding default.
- Probability of completion default.
- Probability of project distress.
- Probability of payment default by year and over the full horizon.
- Probability of lender loss, expected lender loss, and loss given default.
- Expected default month conditional on default.
- Probability of a distribution lockup.

`PD` without a label is forbidden. Every probability must identify event,
horizon, and whether support is included.

### 8.3 VaR and CVaR

Define equity loss as

```text
L_equity = -NPV_equity.
```

For confidence level `q`:

```text
VaR_q = inf{x : Pr(L_equity <= x) >= q}

CVaR_q = mean loss in the worst (1 - q) fraction of trials.
```

The empirical implementation sorts losses ascending. `VaR_q` is element
`ceil(q*N) - 1` using zero-based indexing. For `CVaR_q`, set
`tail_start = min(N - 1, ceil(q*N))` and average elements `tail_start` through
`N - 1`. This is the strict empirical tail beyond the nearest-rank VaR, with
the maximum used when that tail would otherwise be empty. This fixed
convention avoids interpolation and floating-point tail-count ambiguity.
Report both 95% and 99% values and label them as loss amounts; a negative value
means even the stated loss quantile remains a gain.

Also calculate lender-loss VaR/CVaR and support-payout VaR/CVaR using the same
convention and separately named loss variables.

### 8.4 Support burden and effectiveness

For each instrument and package, report:

- Expected present value and undiscounted mean of gross payouts.
- Median, 95th percentile, and maximum payout.
- Probability of any payout and expected first-payout month.
- Expected peak and terminal balance for repayable support.
- Reduction in payment-default probability and expected lender loss relative
  to the exact same unsupported paths.
- Change in mean and downside equity NPV.
- `expected_support_per_incremental_survival`, defined as expected present
  value of support divided by the reduction in full-horizon payment-default
  probability, when that reduction is positive.
- Payout concentration: fraction of total simulated support paid to the worst
  1% and 5% of paths ranked by unsupported equity loss.

Expected payout is not a market price. Pricing would additionally require
provider funding cost, capital, liquidity, model uncertainty, expenses,
counterparty risk, legal characterization, and return.

## 9. Target monthly synthetic default fixture

The defaults below are proposed inputs for the target monthly program. They do
not replace or describe the checked annual v0.1 defaults, and they have not
been calibrated to a company, technology, or market.

### 9.1 Facility and operations

| Input | Synthetic default |
| --- | ---: |
| Base installed cost `K0` | `150.0` EUR million |
| Mean capex multiplier | `1.10` |
| Capex log volatility | `0.18` |
| Planned construction `C0` | `30` months |
| Build-delay location / scale / maximum | `6 / 6 / 24` months |
| Regulatory delays and probabilities | `0/12/24` months; `0.80/0.15/0.05` |
| Long-stop month | `42` |
| Annual nameplate attempted output | `3,000,000` kg |
| Target utilization | `0.90` |
| Base ramp constant / log volatility | `12` months / `0.25` |
| Base annual demand | `3,000,000` kg |
| Demand log volatility | `0.18` |
| Base net price / log volatility | `28.0` EUR/kg / `0.15` |
| Base variable cost / log volatility | `16.0` EUR/attempted kg / `0.12` |
| Annual fixed operating cost | `10.0` EUR million |
| Maintenance capital | `2.0%` of `K0` per year |
| Construction-complete standby cost | `0.35` EUR million/month |
| Disposal cost for unsold qualified output | `0.50` EUR/kg |
| Yield log volatility and bounds | `0.15`; `[0.55, 1.20]` |
| Availability base / volatility / minimum | `0.93 / 0.04 / 0.60` |
| Base monthly contamination probability | `0.025` |
| Contamination sensitivity / loss fraction | `0.65 / 0.60` |
| AR(1) coefficient `rho` | `0.65` |
| Accounts receivable / inventory / payables | `45 / 15 / 30` days |
| Operating horizon after COD | `120` months |
| Terminal value / decommissioning cost | `0 / 0` |

### 9.2 Financing and simulation

| Input | Synthetic default |
| --- | ---: |
| Debt draw fraction of eligible construction spend | `0.45` |
| Senior debt commitment | `75.0` EUR million |
| Sponsor commitment, including reserve | `105.0` EUR million |
| Annual real senior rate | `7.5%` |
| Scheduled service start | Month `33` |
| Amortization tenor | `120` months |
| Initial debt-service reserve | `5.0` EUR million |
| Reserve target | `6` months of scheduled debt service |
| Distribution-lockup rolling DSCR | `1.20` |
| Construction / operating recovery | `0.20 / 0.35` of senior principal |
| Annual real NPV discount rate | `10.0%` |
| Base annual offtaker hazard | `1.0%` |
| Offtaker unsecured recovery | `0.25` |
| Credit sensitivity | `0.70` |
| Monte Carlo trials | `100,000` |
| Seed | `20260827` |
| Reported loss quantiles | `0.95`, `0.99` |

All support limits default to zero. Example support packages belong in named
test scenarios, not in the unsupported default fixture.

## 10. Simulation procedure

For each trial:

```text
1. Generate permanent common factors and idiosyncratic driver values.
2. Sample capex, build delay, ramp constant, and regulatory delay.
3. Build the monthly construction-spend path and determine COD.
4. Generate the complete monthly operating and counterparty path.
5. Store this exogenous path without applying an instrument.
6. Evaluate the unsupported project and capital structure.
7. Replay the identical path for each enabled instrument or package.
8. Store path-level outcomes and paired differences.
```

After all trials, aggregate distributions, default timing, DSCR, VaR/CVaR,
support burden, and paired instrument effects. The program should emit a
human-readable summary and CSV tables. At minimum, the path-level CSV contains
trial id, seed, realized capex, construction and regulatory delay, COD, total
qualified output, total sales, pre/post-support NPV, minimum DSCR, each default
flag and month, lender loss, equity loss, and support payout by instrument.

The target implementation should use small plain structs such as
`ModelConfig`, `RiskPath`, `InstrumentConfig`, `TrialResult`, and
`AggregateResult`. Fixed defaults may be compiled into `ModelConfig` initially;
a future parser must not be required to validate the first equations. Use
`double`, check every public result with `std::isfinite`, and use compensated
summation for large aggregate cash-flow and Monte Carlo sums where practical.

## 11. Validation invariants

The target monthly implementation is not accepted until automated tests
establish all of the following.

### 11.1 Domain and accounting invariants

- Invalid negative costs, rates below `-1`, probabilities outside `[0,1]`,
  nonpositive horizons, factor rows with squared loadings above one, and
  non-finite inputs are rejected.
- Construction spend sums to realized capital cost within
  `64*epsilon*max(1, K)`.
- Sources equal uses each month and over the full path within a declared
  monetary tolerance.
- Debt roll-forward satisfies opening balance plus draws plus capitalized
  interest minus principal paid equals closing balance.
- Debt, cash, output, sales, support headroom, and remaining commitments never
  become negative beyond the declared floating-point tolerance.
- `q_sold <= q_qualified <= q_attempt*yield_max` and `q_sold <= demand`.
- Output and product revenue are exactly zero before COD.
- Cumulative payouts never exceed instrument caps.
- Working-capital release never exceeds working capital previously funded.
- Equity distributions are zero after uncured default and during a lockup.

### 11.2 Deterministic and monotonic invariants

- With every volatility, delay, hazard, and contamination probability set to
  zero, Monte Carlo trials are identical to a separately calculated
  deterministic cash-flow table.
- With zero debt, debt service is zero, DSCR is `not_applicable`, and debt
  default and lender loss probabilities are zero.
- With zero support limits, supported and unsupported results are identical.
- A price floor set at or below zero never pays for a nonnegative market
  price.
- A debt-service guarantee never pays without a debt-service shortfall.
- A construction grant never changes physical output, price, demand, or
  operating cost on a fixed path.
- On a fixed exogenous path, increasing a non-recourse guarantee cap cannot
  increase payment default or lender loss.
- On a fixed exogenous path, increasing a one-way price-floor strike cannot
  reduce its payout or increase payment default.
- A zero-rate amortizing loan pays exactly opening principal divided by the
  number of payments, apart from the final rounding adjustment.

### 11.3 Statistical and risk-metric invariants

- One million driver-only draws reproduce standard-normal means, variances,
  and correlations within predeclared statistical tolerances.
- Sampled regulatory-delay frequencies lie within a binomial confidence band
  around their configured probabilities.
- Seeded runs are repeatable within the supported build environment.
- Common-path paired differences are exactly zero when two instrument
  configurations are identical.
- For the stated loss convention, `VaR_99 >= VaR_95` and
  `CVaR_q >= VaR_q`, within numerical tolerance.
- Empirical VaR and CVaR match hand calculations for a small fixed vector of
  losses, including repeated values and a one-element tail.
- Reported default probabilities equal integer event counts divided by trial
  count; failed paths are not removed from denominators.

### 11.4 Consolidation invariant

With a common zero discount rate and no external support, tax, transaction
cost, or recovery haircut, project cash flows must equal the sum of lender and
equity cash flows. When support is active, adding the support provider's signed
cash flow restores the identity. This test detects double-counting of grants,
guarantees, debt draws, or product revenue.

## 12. Required test and stress scenarios

Each scenario uses the same synthetic facility unless the row states an input
override. Tests should assert accounting identities and expected direction;
the target implementation should freeze numerical regression outputs only
after they are independently checked.

| Scenario | Overrides | Required result |
| --- | --- | --- |
| Deterministic base | All volatilities, delays, hazards, and contamination are zero | Matches hand-built monthly schedule |
| Unlevered project | Debt commitment and draw fraction are zero | No debt metrics or lender loss; project NPV unchanged by financing |
| Certain funding failure | Realized cost above total available sources | Construction funding default at the first unfunded month |
| Long-stop breach | Fixed COD one month beyond long stop | Completion default unless an explicit extension is enabled |
| Slow ramp | Large fixed `tau_ramp` | Lower early CFADS and DSCR; physical equations remain valid |
| Biological underperformance | Low yield and availability on every operating month | Lower qualified output; attempted-output cost remains payable |
| Contamination month | Force one event | Qualified output falls by the exact declared fraction for that month |
| Demand collapse | Demand multiplier near zero | Sales are demand-limited and unsold-output disposal cost is charged |
| Input-price shock | High input-cost factor with unchanged selling price | Lower CFADS and no change to physical output |
| Coupled scale failure | Bad scale factor drives overrun, delay, slow ramp, low yield, and low availability together | Worse tail than independent one-at-a-time shocks |
| Regulatory delay | Fixed 12- and 24-month delays | No pre-clearance output; standby and debt-timing effects appear |
| Offtaker default | Force default during a take-or-pay contract | Contract cash falls to configured recovery; spot sales remain constrained |
| Price floor | Fixed prices below, at, and above strike | Formula pays exact shortfall, zero at strike, and respects cap |
| Overrun support | Costs below deductible, inside cover, and above cap | Zero, partial, and capped payouts respectively |
| Partial debt guarantee | Forced shortfall larger than coverage and cap | Distress remains visible; lender shortfall falls by exact payout |
| Liquidity facility | Temporary shortfall followed by recovery | Draw, accrued subordinated balance, and repayment follow waterfall |
| Take-or-pay | Zero delivery with solvent buyer | Shortfall payment follows declared rate and is not labeled product revenue |
| Tail-metric fixture | Fixed loss vector | VaR/CVaR match hand calculation exactly |
| Support dominance check | Replay paths with nested guarantee caps | Larger cap never raises pathwise payment default or lender loss |

The published demonstration should also compare, with common random numbers:

1. Unsupported financing.
2. A capped construction-overrun facility.
3. A capped output-price floor.
4. A partial debt-service guarantee.
5. A take-or-pay contract with counterparty default risk.
6. One blended package combining narrowly sized construction and revenue
   support.

No package should be called optimal. The comparison should show which risk it
addresses, expected provider payout, risks left with the project, and whether
it reduces distress, payment default, lender loss, or only shifts value.

## 13. Interpretation and next evidence gate

The synthetic model is useful for exposing assumptions, cash-flow identities,
settlement mechanics, and the data needed to distinguish candidate
instruments. It cannot establish that any instrument is financeable.

Before real calibration, the project needs at minimum:

- Independently reviewed construction design, vendor quotations, contingency,
  commissioning plan, and sources and uses.
- Complete repeated-run histories at relevant scale, including failed and
  contaminated batches.
- Standard definitions for attempted, harvested, qualified, accepted, and sold
  kilograms.
- Auditable media, energy, labor, maintenance, quality, and disposal costs.
- Paid buyer trials and draft or executed offtake terms rather than survey
  demand.
- A jurisdiction-specific regulatory path and long-stop analysis.
- Sponsor commitments, lender terms, reserve requirements, counterparty credit,
  recovery assumptions, and legal characterization of every support contract.
- Empirical evidence for marginal distributions, time dependence, and
  correlation—or explicit uncertainty ranges when evidence is absent.

Until those inputs exist, model results must retain the label **synthetic
scenario analysis**. The target model's success criterion is transparency and
falsifiability, not an attractive financing result.
