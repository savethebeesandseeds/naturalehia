# Financial Instrument Family v0.1

## Purpose

This appendix defines a coherent ladder of financial claims for cellular
agriculture from research to repeat production. It answers six questions for
each instrument: what risk is financed, when the instrument is economically
eligible, how capital is drawn, where investor cash comes from, how failure is
allocated, and what would falsify the investment thesis.

The ladder is a set of stage-appropriate forms attached to the common
representation in [`PROJECT_FINANCIAL_INTERFACE_V0_1.md`](PROJECT_FINANCIAL_INTERFACE_V0_1.md).
The shorter [`FINANCIAL_ARCHITECTURE.md`](FINANCIAL_ARCHITECTURE.md) remains
the public explanation. This appendix separates pilot and first-industrial
forms because their payoff capacity is materially different.

This is a financial-engineering specification, not a management, governance,
legal, regulatory, or offering document.

## Design rules

1. **The claim must fit the repayment source.** Pre-revenue science cannot
   service ordinary debt. Seasoned production should not surrender unlimited
   equity upside merely because earlier-stage projects need it.
2. **Every receipt must identify an external payer or realizable asset.**
   Product revenue, licensing cash, an acquisition, asset recovery, or explicit
   third-party support can supply cash. Issuance proceeds and payment in kind
   cannot.
3. **A commitment is not an exposure.** Draws, funded exposure, undrawn
   availability, irreversible spend, and recovery are modeled separately.
4. **Packaging cannot create cash.** Staging can avoid financing a failed path;
   pooling and priority can redistribute risk and loss, but neither creates
   customer cash.
5. **Instrument state and project state remain separate.** A scientific delay
   is not automatically a payment default, and conversion or refinancing does
   not make the underlying project successful.
6. **No stage is promoted by vocabulary.** A first industrial plant does not
   become a repeat facility because it is placed in a debt vehicle.

For project `i`, scenario `omega`, and month `t`, let `D_i,t` be the draw,
`E_i,t` funded economic exposure, `Z_i,t` external project cash available to
financing claims, and `Rec_i,t` distributable recovery cash. An instrument is a
state-contingent allocation rule:

```text
Investor CF_i,t(omega)
    = -D_i,t(omega)
      + allocation( Z_i,t(omega), Rec_i,t(omega), project states, terms )
```

The allocation across all instruments may not exceed the cash and recovery
actually available to them.

## The stage ladder

| Stage | Core instrument | Principal risk being financed | Natural return source |
|---|---|---|---|
| Research | Capped research participation | Scientific success and commercialization | Licensing, royalty, commercialization, or exit cash |
| Pilot and demonstration | Milestone-funded convertible preferred | Scale reproducibility and next-stage value | Commercial cash, redemption from real liquidity, or conversion value |
| First industrial | Milestone-gated construction facility with explicit term-out | Completion, commissioning, ramp, and refinancing | Contracted project cash and a modeled takeout |
| Repeat production | Qualified-output-backed production note | Operating, buyer, input, and residual technology risk | Cash available for debt service from qualified production |
| Compatible multi-project set | Untranched participation pool | Combined project loss and common-factor shocks | Sum of underlying instrument receipts |
| Reconciled, seasoned pool | Explicit loss layers, if useful | Allocation of pool tail loss | Same pool receipts, redistributed by priority |

Narrow completion, demand, price, input, interest-rate, currency, or guarantee
overlays may be attached to a core claim. They are not replacement instruments.

## 1. Capped research participation

**Financed risk.** Capital funds a defined research program whose outcomes are
highly dispersed, whose duration is uncertain, and whose near-term operating
cash is normally absent. The investor bears scientific failure,
commercialization failure, long duration, and dilution or rights-enforcement
risk. Fixed interest alone does not match this exposure.

**Economic eligibility.** The financed risk unit, capital schedule, terminal
research outcomes, and commercialization rights must be identifiable. At least
one plausible success path must lead to an attachable cash source: license
milestones, royalties, a capped share of defined product revenue, or cash exit
proceeds. A project with only social value and no such payer may deserve grant
or philanthropic capital, but it is not yet an investable participation claim.

**Draw rule.** Capital is committed up to a limit and released in stages.
Future availability is cancelled after a terminal failure. A milestone changes
the allowed draw; it does not retroactively prove a probability estimate.

**Payoff.** A simple form pays the lesser of a contractual cap and a defined
share of realized commercialization cash:

```text
Receipt_t = min( remaining claim cap,
                 royalty_t + license participation_t + exit participation_t )
```

The cap can be expressed as a multiple of funded capital. A fixed
commercialization payment may supplement the participation, but accrued
notional is not cash. The claim should expire or reach its cap so that a
successful project is not burdened indefinitely.

**Real external return source.** Cash paid by a licensee, customer, strategic
buyer, or acquirer. A later financing round is repayment liquidity only to the
extent that the later investor is purchasing rights supported by future
project economics.

**Loss and recovery.** On failure, undrawn commitment is cancelled. Funded
capital is impaired by the amount not recovered from a sale of transferable
rights, equipment, or other realizable assets after workout costs. Unproven IP
has zero assumed recovery unless an explicit scenario supports a buyer and
price.

**Metrics and falsification tests.** Report expected and maximum draw,
probability of capital impairment, expected capital shortfall, recovery timing,
cash-multiple and IRR distributions, duration, NPV at declared hurdles, and
loss expected shortfall. For present-value investment `I`, success probability
`p`, failure recovery `R_f`, and net winner cash `W`, the no-premium break-even
winner payoff is:

```text
W_break_even = (I - (1 - p) R_f) / p
```

The claim fails its first economic test if realistically obtainable success
rights cannot produce that payoff after time, dilution, collection risk, and
costs. It also fails if the apparent diversification rests on many projects
sharing the same biological platform or commercialization counterparty.

## 2. Milestone-funded pilot convertible preferred

**Financed risk.** Pilot and demonstration capital bears scale reproducibility,
yield, contamination, unit-cost, product-qualification, and next-stage funding
risk. There may be early revenue, but not enough stable cash for conventional
amortization.

**Economic eligibility.** A pilot budget and state path must be defined, and
success must create commercial cash, a financeable next stage, or enterprise
value in which the investor can participate. The model states the conversion
basis, dilution assumptions, senior claims, and any cash redemption source.

**Draw rule.** Preferred capital is funded by pilot milestones. Failure stops
future draws. Delay extends duration and may require a separately modeled
capital call; it does not silently increase the commitment.

**Payoff.** The investor receives a preferred return from available cash and,
at a defined financing or exit state, either redemption or conversion value.
A non-participating form can be represented as:

```text
Exit receipt = max( cash redemption actually payable,
                    conversion fraction * distributable equity value )
```

If the claim participates after receiving a preference, that additional leg
must be shown explicitly rather than hidden in a headline preferred return.
Conversion changes the payoff function; it does not create equity value.

**Real external return source.** Pilot product or licensing receipts, a cash
acquisition, or value created in a commercially financeable enterprise. A new
round may provide redemption cash, but a model that requires perpetual new
issuance without underlying value is circular.

**Loss and recovery.** The preference receives only its stated share of net
asset, IP, or sale recoveries after claims senior to it. Undrawn capital is
cancelled. A contractual liquidation preference above realizable value is a
claim amount, not a recovery forecast.

**Metrics and falsification tests.** Report expected draw, follow-on liquidity
need, capital-impairment probability, expected shortfall, conversion
probability, dilution sensitivity, value and cash-multiple distributions, and
time to liquidity. Solve for the enterprise or exit value required to meet the
investor hurdle after senior claims and dilution. The instrument is falsified
if this break-even value is outside credible success scenarios, if redemption
depends only on an unspecified future round, or if a debt-like promised return
is presented without cash capable of paying it.

## 3. First-industrial milestone facility with explicit term-out

**Financed risk.** This facility finances construction, commissioning,
qualification, and early ramp of the first industrial implementation. Its
central risks are cost overrun, completion delay, failed output qualification,
weak ramp performance, buyer non-performance, and inability to refinance the
construction exposure.

**Economic eligibility.** Sources and uses must balance. The facility needs a
dated draw schedule, completion and qualification states, downside capital
need, and an explicit term-out. A committed takeout or modeled refinancing
must ultimately be supported by qualified-output cash, contracted demand,
assets, or named outside support. Assumed refinancing at par is not itself a
repayment source.

**Draw rule.** Draws fund only eligible costs reached on the underlying state
path and cannot exceed remaining commitment. Terminal failure cancels undrawn
availability. A separate completion reserve or sponsor contribution, if any,
is shown as a distinct source rather than negative project cost.

**Payoff.** During construction, a funded return may accrue; any payment in
kind remains non-cash. Cash receipts arise from permitted project cash,
completion support, and term-out proceeds. At the takeout month `T`:

```text
Required takeout_T = funded principal_T + cash-payable accrued claim_T
Takeout coverage_T = eligible term-out proceeds_T / Required takeout_T
```

A limited success participation may compensate for genuine first-of-a-kind
risk, but it must not consume cash already pledged to the term-out claim.

**Real external return source.** Customer payments for qualified output or
capacity, cash from a named support provider, and refinancing whose own debt
service is supported by operating cash and recoverable assets.

**Loss and recovery.** Staging preserves cancelled commitment but does not
recover sunk construction cost. On failure, exposure is allocated against net
equipment, site, contract, insurance, guarantee, or sale proceeds in stated
priority. Completion support and sponsor first-loss capital count only when
their amount and performance state are explicit.

**Metrics and falsification tests.** Report expected and maximum draw,
simultaneous liquidity need, completion and qualification probabilities,
cost-overrun and delay distributions, exposure at default, loss given default,
expected loss, recovery delay, takeout probability, takeout coverage, duration,
NPV, and tail loss. Test the future production note under downside debt-service
and loan-life coverage, not nameplate capacity. The facility is falsified if a
credible completed plant cannot support its takeout, if plausible overruns
exceed available sources, or if the investor return is principally unpaid PIK
at maturity.

## 4. Qualified-output-backed repeat-production note

**Financed risk.** The note finances replication or expansion after the
technology and facility template have operating evidence. Investors bear
remaining performance, input, energy, buyer, price, counterparty, and asset
recovery risk, but should not be unknowingly underwriting unresolved
first-industrial risk.

**Economic eligibility.** The project must supply comparable operating data,
a capital and working-capital schedule, qualified-output rather than nameplate
assumptions, enforceable revenue rights or well-supported demand scenarios,
operating cost sensitivities, and recoverable-asset assumptions. Stable cash
available for debt service must exist under more than the base case.

**Draw rule.** The note can fund at completion or through controlled capital
draws. Any construction-period exposure remains separately visible. Working
capital, reserve funding, and capitalized interest are uses of funds, not
revenue.

**Payoff.** Scheduled cash interest and amortization are paid from cash
available for debt service (`CFADS`) generated by qualified output, capacity
payments, or contracted offtake:

```text
DSCR_t = CFADS_t / scheduled debt service_t
LLCR_t = PV( CFADS available over loan life ) / debt outstanding_t
```

Excess-cash sweeps may shorten duration. They allocate real cash; they do not
raise total project cash.

**Real external return source.** Customer and offtaker payments for accepted
product or reserved capacity. Asset-sale and insurance proceeds are recovery,
not operating yield.

**Loss and recovery.** Payment shortfalls consume contractual reserves and
subordination, if present, before principal impairment in stated order.
Recoveries come from net realizable collateral and contract proceeds. A
specialized bioreactor is valued under its stressed alternative use or sale,
not replacement cost.

**Metrics and falsification tests.** Report probability of default, exposure at
default, loss given default, expected loss, DSCR and LLCR distributions,
minimum and stressed coverage, yield, duration, extension, NPV, cash multiple,
and loss ES. Solve for break-even qualified volume, realized price, unit cost,
and utilization. The note is falsified if debt service requires nameplate
output, unsupported prices, repeated equity rescue, or collateral recoveries
that disappear under the same common-factor shock causing default.

## 5. Compatible untranched participation pool

**Financed risk.** The first pool claim bears a pro rata share of all funded
losses and receives a pro rata share of all underlying receipts. Its purpose is
to test whether combining financially compatible exposures reduces
idiosyncratic tail risk after simultaneous draws, costs, and common factors.

**Economic eligibility.** Every asset uses the common interface, cash
reconciles, and dependence is explicit. Compatible claims share sufficiently
coherent cash-flow, duration, and loss definitions. Research and seasoned-note
pools are distinct natural objects. A mixed-stage pool can be an explicitly
risk-bearing participation vehicle, but must preserve stage attribution and
must not be described as homogeneous senior debt.

**Draw and payoff.** The pool funds underlying draws and therefore holds enough
liquidity for correlated capital calls. Investor cash is the ownership share
of aggregate underlying cash less explicit pool costs:

```text
CF_pool,t = sum_i a_i * CF_i,t - pool_costs_t
Loss_pool = sum_i a_i * principal_loss_i
```

**Real external return source.** Only the sum of customer, licensing, exit,
recovery, and explicit support cash belonging to the underlying assets.

**Loss and recovery.** All investors in the untranched claim share aggregate
loss and recovery pro rata. Failed projects remain in the result. Expected
loss is the weighted sum of underlying expected losses; pooling does not lower
it merely by changing the wrapper.

**Metrics and falsification tests.** Report expected loss, impairment
probability, p95/p99 loss, ES95/ES99, expected return, NPV, duration, maximum
single-project exposure, factor concentrations, simultaneous draw need, and
marginal tail-loss contribution. At tail level `alpha`:

```text
Diversification benefit_alpha
    = sum_i a_i ES_alpha(Loss_i) - ES_alpha(Loss_pool)
```

Test explicit common-factor shocks, leave-one-project-out results, dominant
platform and buyer concentrations, and pool costs. The pool thesis is
falsified if modeled benefit vanishes under plausible dependence, if liquidity
cannot meet joint draws, or if costs consume the attainable diversification
benefit.

## 6. Fully funded capital stack only after the pool works

Senior, intermediate, and first-loss claims may be introduced only after the
untranched pool reconciles and demonstrates real economic usefulness. The
implemented v0.1 stack fully subscribes aggregate commitment at par in month
zero. A zero-yield, assumed-lossless reserve funds actual staged draws, pool
costs remain additional pro-rata calls, and genuinely unused commitment
returns through the principal waterfall only at the horizon. This makes each
loss layer real before project draws occur and exposes the NPV drag of
prefunding.

For pool loss `L` and a layer attaching at `A` and detaching at `B`:

```text
Layer loss(A,B) = min( max(L - A, 0), B - A )
```

Layer losses must sum exactly to pool loss when the full capital structure is
included. Actual project principal plus unused reserve pays senior first.
Unresolved funded principal occupies the loss stack but remains exposure, not
impairment. Actual non-principal cash pays senior and intermediate lifetime
allocation caps before reaching the first-loss residual. Those caps are not
coupons, guaranteed returns, PIK, or new cash rights. Every cash payment
reconciles to the fixed underlying pool. A senior layer has lower loss only
because another layer absorbs earlier loss; it does not cause projects to
perform better.

For each layer report expected loss and loss rate, impairment and exhaustion,
loss ES, project-principal cash, reserve return, non-principal cash, expected
NPV at a declared hurdle, all-in shortfall, cash multiple, weighted-average
life, unresolved exposure, and sensitivity to attachment, dependence,
recovery, and common factors. Robust WAL must divide cash-time by principal
cash under one feasible probability measure; it cannot divide independently
optimized endpoints. A layer is falsified if its apparent safety depends on
independence, excluding failed assets, calling reserve return profit, or
treating research and first-industrial claims as seasoned production notes.
External first-loss or guarantees are not subordination created inside the
pool: they are separately priced support from a named capital provider.

The mechanics, hand table, and strict term format are in
[`CAPITAL_STACK_TERM_V0_1.md`](CAPITAL_STACK_TERM_V0_1.md). Version 0.1 is an
at-par physical-scenario allocation analysis, not a spread, fair value, market
quote, rating, or investable legal form.

## 7. Narrow risk-transfer overlays

An overlay covers one defined exposure for a finite amount and period. Examples
include completion support, a qualified-output purchase floor, a capacity
reservation, an input or energy-price hedge, interest-rate or currency
protection, insurance, and an explicit public or philanthropic loss share.

Each overlay records its trigger, notional, cap, expiry, premium, settlement
timing, payer, collateral or performance assumption, and basis risk. Its net
contribution is:

```text
Net overlay CF_t = settlement from named payer_t - premium and collateral cost_t
```

Report expected settlement, stress settlement, counterparty exposure,
wrong-way risk, basis mismatch, cost, and the resulting change in project and
investor tail loss. The overlay is falsified when its trigger does not match
the financed exposure, its payer fails in the same scenario, or its cost
exceeds the financing benefit it preserves. Risk transfer can make a claim
better matched to an investor; it cannot erase the risk from the consolidated
system.

## Expected return, fair value, and price

The same instrument must be viewable under different analysis modes without
mixing their meanings.

**Physical expected-return sensitivity** uses stated real-world scenario
weights `p_omega` and an investor hurdle `h`:

```text
NPV^P(h) = sum_omega p_omega
             * sum_t CF_t(omega) / (1 + h)^(t/12)
```

When probabilities are estimated or synthetic, this is a sensitivity, not a
market price or empirical forecast. Report the input status, and show
probability, recovery, duration, price, volume, cost, and correlation ranges.
The implemented physical-probability envelope supplies the exact financial
projection of scenario-weight bounds for the current finite joint table; it
does not replace the separate calibration or pricing work.

The implemented success-participation solver applies that envelope to one
contract lever. For selected commercial, licensing/royalty, or exit receipts,
it scales only the non-principal cash already granted in each fixed scenario
and solves the smallest fraction that clears a stated worst expected NPV. The
whole combined NPV is re-optimized at each fraction because the adverse
probability witness may change. A result above the contractual maximum is
reported as infeasible, not converted into a larger unevidenced cash claim.

The implemented pooled-loss-protection solver applies a second lever after a
success-participation fraction is fixed. An assumed fully performing outside
provider pays an exact proportion of final resolved pool principal loss at the
horizon. Gross project loss does not change, continuing exposure is excluded,
and a monetary cap limits the supported percentage rather than clipping severe
path payments. For every candidate percentage, the complete investor NPV is
reprojected. The investor's signed premium headroom is compared with the
provider's worst expected claim present value. If there is no non-negative
overlap, the result is a disclosed premium gap, not a market price.

The implemented provider price ladder adds a third, deliberately narrow test.
At either the certified investor-target-passing protection point or a freshly
projected explicit percentage, it adds claim-variable expense, full-term net
collateral carry, an independently supplied incremental economic-capital
charge, fixed expense, and target profit. Both bases use contractual maximum
exposure; capital and collateral principal are not charged as expenses. The
complete claim-cost range is transformed once, so its feasible probability
witnesses remain intact. A missing all-in overlap is split between provider
premium support and any separate amount needed to restore a negative investor
target. This is a physical-probability cost sensitivity, not a balance sheet,
capital validation, provider-credit model, or market price.

The implemented provider counterparty-credit stress adds the next separate
test. For each original project scenario it declares fixed conditional
provider-performance outcomes, so provider failure can be more likely in
high-claim states without inventing an independence assumption. The gross
claim and the provider's full-performance price remain unchanged. Default
changes only the investor's collection waterfall: direct payment, explicitly
pledged collateral, delayed unsecured recovery, and unpaid claim. Conditional
provider outcomes are averaged inside each original scenario before the
probability envelope is applied; they are not independently optimized. The
result reports wrong-way dependence, exposure at default, credit-loss tails,
claim delivery, and additional support required. It remains a physical
settlement stress—not CVA, fair value, a rating, legal proof, or an investable
quote.

The implemented robust capital-mobilization frontier joins the success-cash
and capital-stack terms without turning them into a score. It crosses a finite
grid of the total scalable success-cash share `q` and funded junior first loss
`A`, creates one junior and one priority market claim for each pair, and tests
the whole pool and the market claim against separately declared robust NPV,
principal-loss, tail-loss, impairment, return-shortfall, duration, first-loss,
and catalytic-concession limits. Contributions, distributions, adverse
probability witnesses, all feasible cells, and nondominated cells stay
visible. A feasible cell is a physical-measure term candidate, not a price or
an investor commitment.

The implemented next term is the
[robust market non-principal priority-cap term](ROBUST_MARKET_PRIORITY_CAP_TERM_V0_1.md).
It holds underlying pool cash, `q`, `A`, and the supplied physical hurdle fixed
while testing the market claim's lifetime non-principal priority cap. Raising
that cap transfers existing success cash from junior to market; it cannot
increase aggregate cash. The result reports both the minimum tested market
cap and the corresponding junior NPV or catalytic concession. It is cap
adequacy under a supplied hurdle, not market-return or price discovery.

The implemented
[robust issue-price support term](ROBUST_ISSUE_PRICE_SUPPORT_TERM_V0_1.md)
adds the first primary-issuance price boundary without changing that fixed
claim. For each independently supplied hurdle `h`, the worst admitted physical
probability gives a maximum gross buyer price `P*(h)`. The issuer side requires
enough buyer cash plus separately declared, non-repayable support to fund market
principal `M` and explicit issuer cost `F`. Their overlap is a conditional
price interval; their separation is a measured support gap. Buyer-direct cost
stays outside the reserve, and support receives no repayment, participation,
security, recovery, or future-project-cash right.

The output keeps five facts separate: arithmetic overlap, a support-free price
endpoint, documented support commitment, funded or escrowed support, and
settled sources plus evidenced uses. A secondary trade never funds the project.
A hurdle derived from the same reference price is reconciliation-only. The
engine produces neither fair value nor a quote, yield, rating, investor demand,
provider-performance valuation, or proof of financing additionality.

The implemented
[market-observation and hurdle-evidence set](MARKET_OBSERVATION_AND_HURDLE_EVIDENCE_V0_1.md)
defines which independently sourced rate cases may enter that engine. It
reconstructs a rate interval from each eligible price and declared full-claim
physical expected cash, applies only jointly feasible comparable adjustments,
keeps direct transactions, settled comparables, and executable quotes in
separate tiers, and retains the exact closed regions supported by all but a
predeclared number of discordant clusters. Unsupported gaps are not averaged
away. If the highest-quality nonempty tier is too small for that rule, the
answer is insufficient evidence; lower-quality observations remain diagnostics
and cannot become the primary set.

**Fair value** requires a market-consistent discount curve and compensation
for priced risk, represented by risk-neutral weights or a stochastic discount
factor `m`:

```text
Fair value_0 = E^P[ sum_t m_t * CF_t ]
```

If no defensible market calibration or comparable claims exist, fair value is
unavailable; it must not be replaced by the physical NPV and relabeled.
Contractual claim amount, expected cash, investor purchase price, and fair
value are four different quantities.

## Cross-stage reconciliation and the final value test

Transitions between instruments must not pledge the same cash twice. A
research participation may survive into production only within its cap and
priority. Pilot preferred may convert or be redeemed, but not both unless a
participating leg is explicit. A first-industrial facility is repaid by its
term-out; the production note then owns the rights assigned to it. At every
month and in every scenario:

```text
sum of cash allocated to claims
    <= external project cash + explicit support cash + distributable recovery
```

An instrument is valuable only when it is an economic claim on plausible
external cash, bought at a price that can compensate for loss, time,
illiquidity, and common-factor exposure. It should also fund a positive-value
transition, stop waste after failure, reduce distress cost, create useful
information, or place a defined risk with capital able to bear it.

The family fails as financial engineering if attractive returns arise only
from optimistic probabilities, circular refinancing, double-counted cash,
unfunded guarantees, assumed independence, or moving loss into a layer whose
capital is ignored. The honest result may be that a project needs grant,
strategic, or concessionary capital before it can issue an investable claim.
Making that boundary visible is part of the standard's value.

The implemented terms and hand calculations are in
[Robust Success-Participation Term v0.1](SUCCESS_PARTICIPATION_TERM_V0_1.md),
[Pooled Principal-Loss Protection Term v0.1](POOLED_LOSS_PROTECTION_TERM_V0_1.md),
[Provider Price-Ladder Sensitivity v0.1](PROVIDER_PRICE_LADDER_V0_1.md),
[Provider Counterparty-Credit Stress v0.1](PROVIDER_CREDIT_STRESS_V0_1.md), and
[Fully Funded Capital Stack v0.1](CAPITAL_STACK_TERM_V0_1.md), and
[Robust Capital-Mobilization Frontier v0.1](ROBUST_CAPITAL_MOBILIZATION_FRONTIER_V0_1.md).
