# Portfolio Calibration and Probability-Uncertainty Standard v0.1

**Status:** financial-engineering research standard; no real project is yet
calibrated under it.

## 1. Purpose

The participation pool already answers a mechanical question: if these dated
project cash paths occur with these joint weights, what are the pool's cash,
exposure, loss, liquidity, NPV, and tail-risk results?

This standard addresses the harder question: **what probability statements can
the available evidence honestly support?** Its first implementation uses a
probability envelope around a fixed set of joint cash paths. It reports the
range of each financial result over every probability vector admitted by that
envelope. It does not turn sparse evidence into a falsely precise forecast.

The distinction is fundamental:

```text
scenario construction:  what can happen, when cash moves, and why
probability calibration: how strongly the evidence supports each joint state
valuation:               what an investor would pay for the resulting claim
```

Version 0.1 advances the second line. It is still a physical-probability risk
analysis, not a risk-neutral valuation, fair value, rating, or market quote.

## 2. The financial object being calibrated

The calibrated object is not a generic cultivated-meat company and not the
probability that “cellular agriculture succeeds.” It is one disclosed pool of
specific claims, at a stated observation date and horizon, with:

- named projects, stages, technologies, products, sites, and counterparties;
- dated capital draws and the milestone conditions governing later draws;
- dated investor receipts traced to commercial, licensing, sale, recovery,
  refinancing, fee, or explicit-support sources;
- resolved failures distinguished from continuing exposure;
- common-factor descriptions that explain joint paths; and
- a complete outcome set that includes adverse, delayed, and unresolved paths.

Every probability therefore needs an event, population, time interval,
conditioning information, and source. “PD = 20%” is not a usable input.

## 3. Why a probability envelope comes before a fitted distribution

Cellular agriculture has short operating histories, changing process designs,
few industrial-scale observations, publication and survivor bias, and strong
shared exposures. A point estimate can conceal more than it reveals. Version
0.1 instead declares, for each complete joint scenario `i`:

```text
lower_i <= probability_i <= upper_i
sum_i probability_i = 1
```

A central probability remains visible as the analyst's base case, but every
published expectation is accompanied by its exact minimum and maximum over the
admitted set. The base case must sit inside every bound. Lower bounds need not
sum to one and upper bounds need not sum to one; feasibility requires:

```text
sum_i lower_i <= 1 <= sum_i upper_i
```

The cash path attached to a scenario is never altered by this calculation.
Probability uncertainty cannot manufacture revenue, improve recovery, delay a
draw, or repair an omitted failure.

This box-and-simplex envelope is deliberately modest. It can express defensible
uncertainty about the weights of an already complete joint-state table. It
cannot by itself express moment constraints, conditional-factor restrictions,
causal structure, or an omitted scenario. Those require a richer ambiguity set
or a revised joint table, not wider undocumented bounds.

## 4. Required calibration layers

### 4.1 State and horizon definitions

Before estimating frequency, define each financially distinct state. At
minimum distinguish:

- no draw, partial draw, full draw, and cancelled availability;
- milestone pass, fail, delay, waiver, and unresolved status;
- technical success without qualified commercial output;
- qualified output without contracted or collected customer cash;
- continuing exposure at the horizon versus a resolved loss;
- recovery amount, legal source, delay, cost, priority, and collectability;
- refinancing as a new financing source rather than operating value; and
- counterparty nonperformance, including wrong-way states where support is
  weakest when the project most needs it.

Two states may be merged only if the investor cash flows and the risk decision
remain immaterially different after documented testing.

### 4.2 Project marginal evidence

For each project and milestone transition acquire the numerator, denominator,
observation window, cohort definition, censoring rule, and all exclusions. The
minimum evidence set is:

| Quantity | Minimum evidence | Financial use |
|---|---|---|
| milestone transition and delay | complete dated project or run histories, including failed and unresolved observations | draw availability, duration, stranded spend |
| technical yield and contamination | run-level batch genealogy, release results, deviations, downtime, maintenance, and measurement uncertainty | pass/fail state and operating cash path |
| construction completion and cost | reconciled scope, commitments, change orders, contingency draw, schedule updates, commissioning tests | draw timing, cost-to-complete, completion state |
| qualified production and acceptance | product-specific release, tender, rejection, return, and buyer-acceptance records | eligible commercial volume |
| price and collection | executed terms, invoices, credits, payment timing, defaults, and concentration | commercial receipt amount and timing |
| recovery | collateral inventory, priority, enforceability, workout cost, sale evidence, and time to cash | recovery cash rather than assumed LGD |
| licensing, sale, or refinancing | executed comparable cash terms with stage, rights, seniority, date, and selection rule | non-operating receipt paths |

Failures, aborted runs, abandoned sites, rejected products, and transactions
that did not close remain in the dataset. A press release, capacity target,
survey, non-binding expression of interest, or seller asking price is not a
frequency or payoff observation.

### 4.3 Dependence and common-factor evidence

The pool's purpose is risk distribution, so dependence is a first-order input.
For every pair or group of projects, map shared exposure to:

- cell line, media formulation, scaffold, bioreactor architecture, and
  contamination-control method;
- supplier, equipment vendor, critical input, laboratory, and contract
  manufacturer;
- site, utility system, grid, water, logistics route, and workforce;
- regulator, product category, qualification method, and legal interpretation;
- customer, price index, market segment, distribution channel, and substitute;
- insurer, guarantor, support provider, lender, and refinancing market; and
- sponsor liquidity, intellectual property, key personnel, and data platform.

Each declared joint scenario must state the common shock or conditional chain
that produces it. Independence is admitted only when the evidence and mechanism
support it; absence of observed co-failure in a small sample is not evidence of
independence. A correlation copied from public equities or another industry is
a transfer assumption and must be separately stressed.

Useful acquisition records include common-vendor failure logs, simultaneous
site outages, batch genealogy across projects, regulatory decision histories,
buyer concentration and credit migration, commodity and energy basis histories,
and sponsor-wide capital-call records. Dependence should be estimated and
challenged at the same adverse horizon used for the claim, not only through
contemporaneous average correlations.

### 4.4 Conditional payoff, recovery, and timing evidence

Success probability and success payoff are not independent inputs. Revenue,
recovery, and duration must be measured conditional on the state that produces
them. Required checks include:

- no commercial receipt before technical qualification and contractual
  eligibility;
- no recovery above available collateral or legal claim after costs and delay;
- no project value credited twice through operating cash and an exit value;
- no support cash without a named obligor, trigger, cap, term, and credit state;
- no refinancing receipt without a new creditor and a supportable takeout
  balance; and
- no terminal value for a continuing claim unless the method and uncertainty
  are separately disclosed.

The calibration package must preserve the joint relationship between the event
probability, cash amount, and time to receipt. Independently choosing an
optimistic success rate, high success payoff, short duration, and strong
recovery from different samples is prohibited unless their joint consistency
is demonstrated.

## 5. From evidence to central weights and bounds

For each joint scenario, the calibration record must contain:

1. a precise event definition and mapping from raw observations;
2. observation counts, exposure time, unresolved cases, and exclusions;
3. source identifiers and retained extracts;
4. sampling and measurement uncertainty;
5. cohort differences between the evidence and the target project;
6. any transfer, scaling, or expert judgment and its direction;
7. the central weight, lower bound, upper bound, and method; and
8. a challenger range or adverse alternative.

The interval is not automatically a statistical confidence interval. It may be
the conservative union of sampling uncertainty, transfer uncertainty, model
alternatives, and specified expert ranges. Its meaning must be labeled. When a
bound is judgmental, the record names the fact that would move or retire it.

Central weights and bounds are constructed together across the complete joint
table. Marginal intervals pasted independently onto joint states can be
infeasible or can contradict observed common shocks. The final table must
reconcile to every admitted marginal constraint and remain feasible under the
sum-to-one rule.

### Evidence hierarchy

Use the closest complete evidence available, in this order, while retaining
contrary observations:

1. controlled target-project histories under the current process and scope;
2. same-technology, same-stage controlled comparables;
3. adjacent-stage or component evidence with an explicit transfer model;
4. external industry or market proxies with wide transfer uncertainty; and
5. clearly labeled hypotheses used only for sensitivity or stress.

More observations do not outrank a smaller but materially closer cohort without
analysis. Changes in design, scale, product, operator, site, or regulation can
break exchangeability.

### Sparse and zero-event data

Zero observed failures does not imply zero failure probability. Report the
exposure base and an uncertainty bound; include a plausible adverse joint state.
For rare events, combine engineering failure modes, relevant external data, and
conservative statistical bounds without presenting the result as directly
observed frequency. If the evidence cannot distinguish central probabilities,
keep a wide envelope or use deterministic stress rather than forcing a fitted
distribution.

For an actually IID binomial sample with zero events in `n` trials, one
transparent one-sided 95% upper bound is `1 - 0.05^(1/n)` (approximately
`3/n`). The [NIST exact-binomial reference](https://itl.nist.gov/div898/software/dataplot/refman2/auxillar/exacbino.htm)
provides the underlying interval method. Cellular-agriculture observations are
often clustered, censored, time-varying, or selectively observed, so that
formula is a diagnostic—not permission to ignore those violations.

The implemented
[Joint-Cohort Probability Envelope v0.1](JOINT_COHORT_PROBABILITY_ENVELOPE_V0_1.md)
uses a different finite-sample benchmark. Every included observation is one
complete joint-pool sampling unit. Matured observations map to one scenario;
not-yet-matured and unresolved observations remain in the denominator and are
compatible with every scenario. Under the explicitly declared IID joint-unit
assumption, a Hoeffding bound plus a union bound supplies a conservative
nonasymptotic simultaneous outer set. The module withholds the statistical
export for an obvious declared cluster repetition, leaves the portfolio's
central weights as hypotheses, and keeps all outputs synthetic and
candidate-only. It does not infer a joint distribution from separate facility
marginals.

### Physical and pricing measures

Every probability is tagged as physical `P`, pricing/risk-neutral `Q`, or an
uncalibrated stress weight. Version 0.1 accepts only physical central weights
and physical uncertainty bounds. It uses them for expected cash, expected loss,
liquidity, tail risk, and hurdle-rate NPV sensitivity.

Market-implied survival or default probabilities can contain risk and liquidity
premia. They are not automatically expected event frequencies. Conversely, a
physical probability distribution is not an arbitrage-free pricing measure.
This project therefore prohibits combining an unidentified `Q` probability
with a risky discount rate or presenting a `P`-weighted NPV as market value.

## 6. Exact financial sensitivity produced by v0.1

For fixed scenario outcome values `x_i`, any expectation is linear:

```text
E[x] = sum_i probability_i * x_i
```

The exact minimum starts with every lower bound and allocates the remaining
probability to the lowest `x_i` until each upper bound is reached. The maximum
does the same from the highest `x_i`. Scenario identifier breaks numerical
ties, making every result reproducible. Each endpoint publishes its full
probability vector as a witness.

Version 0.1 applies this method to:

- expected draws, investor receipts, pool costs, outstanding exposure, and
  realized principal loss;
- probability of any principal impairment and probability of negative NPV;
- expected NPV at the declared physical-measure hurdle;
- expected same-month draw, gross funding need, and cumulative outlay peaks;
- expected nominal and present-value receipts for every cash-source class; and
- principal-loss and non-negative NPV-shortfall ES95 and ES99.

For fixed losses under this envelope, allocating probability from lower to
higher loss produces the first-order-stochastically largest feasible loss
distribution; the reverse produces the smallest. Evaluating the same
fractional-tail expected-shortfall definition at those witnesses therefore
gives exact ES bounds. The result would not automatically remain exact under a
different ambiguity set with moment, factor, or coupling constraints.

Different metrics may have different endpoint witnesses. Componentwise maximum
commercial cash, maximum recovery cash, and maximum NPV are not necessarily one
simultaneously attainable scenario distribution. Reports must not add
independently optimized endpoints or describe them as one forecast.

Version 0.1 does not publish an “uncertain diversification benefit” formed by
subtracting separately optimized standalone and pool tails. That subtraction
could combine incompatible probability witnesses. Diversification remains a
central-case diagnostic until a joint robust comparison is implemented.

## 7. Validation and challenger tests

A calibration is not accepted merely because the input file parses. At minimum:

- reproduce raw transition, timing, payoff, and recovery counts;
- reconcile joint weights to all published marginals;
- compare predicted and realized outcomes by cohort and observation date;
- backtest only outcomes that have matured, retaining unresolved cases;
- publish calibration curves and proper probability scores on frozen cohorts,
  rather than treating rank discrimination as calibration;
- evaluate alternative cohorts, exclusions, priors, and dependence structures;
- test sensitivity to influential years, projects, transactions, and outliers;
- compare the central model with a simpler benchmark and an adverse challenger;
- document use outside the evidence population or horizon; and
- state the performance range in which the model remains fit for this specific
  financial decision.

Recalibration does not erase the prior model. Preserve the observation date,
data snapshot, prior estimates, reason for change, and resulting movement in
expected loss, NPV, liquidity, and tail loss.

## 8. Source-backed methodological precedents

This standard borrows discipline, not numerical parameters, from other fields:

- Fernández, Stein, and Lo's biomedical “megafund” research calibrated stage
  transitions from a historical oncology-program dataset and explicitly added
  common-factor dependence because independence overstates diversification and
  debt capacity. Its supplement also describes sparse preclinical data,
  valuation proxies, transfer assumptions, and sensitivity to influential
  observations. Those oncology probabilities, correlations, returns, and
  valuations are **not** cellular-agriculture inputs. See the
  [Nature Biotechnology article](https://www.nature.com/articles/nbt.2374) and
  [supplementary methods](https://media.springernature.com/original/springer-static/esm/art%3A10.1038%2Fnbt.2374/MediaObjects/41587_2012_BFnbt2374_MOESM17_ESM.pdf).
- Lo and Siah's later
  [correlated-drug-development study](https://qlsadvisors.com/wp-content/uploads/2021_Megafund_JSF.pdf)
  makes technical-success dependence explicit and shows that tail loss and
  investor returns move materially under alternative marginal-success and
  correlation assumptions. Because its correlation is assumed rather than
  historically estimated, it reinforces the need for an uncertainty envelope
  and common-cause challengers rather than supplying a number to copy.
- The Federal Reserve's current
  [model-risk guidance](https://www.federalreserve.gov/frrs/guidance/supervisory-guidance-on-model-risk-management.htm)
  treats assumptions, data limitations, aggregate dependencies, alternative
  assumptions, outcome analysis, and use beyond a model's intended purpose as
  sources of model risk. This project uses that as a validation discipline, not
  as a claim that the guidance directly governs this research instrument.
- A Bank for International Settlements study finds that uncertainty in default
  probability and asset-correlation estimates can materially increase measured
  portfolio tail risk and that parameter uncertainty is not diversified away
  merely by adding exposures. See
  [BIS Working Paper 280](https://www.bis.org/publ/work280.htm).
- The European Banking Authority's
  [PD and LGD estimation guidelines](https://eba.europa.eu/activities/single-rulebook/regulatory-activities/model-validation/guidelines-pd-estimation-lgd?version=2018)
  seek to reduce unjustified model variability while preserving risk
  sensitivity. They are a useful credit-calibration reference, not a mapping of
  every cellular-agriculture participation claim into bank-credit terminology.
- The Basel Committee's current
  [IRB validation standard](https://www.bis.org/committees/bcbs/basel-consolidated-guidelines/module/cri/30)
  recognizes that default histories alone can be inadequate for low-default
  portfolios, including project finance, and calls for justified data pooling,
  benchmarking, forward-looking adjustment, documentation, and validation.
  It informs sparse-data controls here without assigning regulatory risk
  weights to this instrument.
- Gneiting and Raftery's
  [proper-scoring-rule paper](https://sites.stat.washington.edu/people/raftery/Research/PDF/Gneiting2007jasa.pdf)
  supplies the basis for using Brier or logarithmic scores to evaluate honest
  probabilistic forecasts. Such scores require frozen, matured observations
  and do not make a transferred cohort representative.
- [Hoeffding's bounded-sum inequality](https://doi.org/10.1080/01621459.1963.10500830)
  supplies the finite-sample sampling-uncertainty benchmark used by the
  joint-cohort module. The resulting coverage statement remains conditional on
  independent, identically distributed joint units and a frozen truthful
  mapping; it does not absorb cohort transfer or cash-path uncertainty.
- [Goodman's simultaneous multinomial score construction](https://www.stat.cmu.edu/technometrics/59-69/VOL-07-02/v0702247.pdf)
  supplies the complete-cohort challenger calculation. Because the construction
  is asymptotic, the module does not use it to narrow the primary sparse-data
  envelope.

## 9. Decision boundary

A result may be described as a calibrated physical-probability range only when
the state definitions, evidence population, joint mapping, bounds, and
validation record meet this standard. Otherwise it is labeled synthetic,
hypothetical, or deterministic stress.

Even a well-supported physical probability envelope does not determine price.
An investable price still requires contractual enforceability, investor cash
rights, market risk preferences, funding and capital costs, liquidity,
expenses, tax and accounting treatment, counterparty credit, and a market or
negotiation process. Probability calibration makes the asset more legible; it
does not make an uneconomic project valuable.

The first implementation bridge is the
[Calibration Binder v0.1](CALIBRATION_BINDER_V0_1.md). It SHA-256 binds the
exact scenario artifacts and maps every material normalized input to a declared
method label, source status, evidence record or explicit `NONE`, limitation,
and retirement fact. The second bridge is the
[Joint-Cohort Probability Envelope v0.1](JOINT_COHORT_PROBABILITY_ENVELOPE_V0_1.md),
which binds a raw outcome ledger, preserves unknown outcomes, constructs a
finite-sample candidate outer set, and connects that set to investor-risk
ranges. Its strict implementation record is the
[Joint-Cohort Probability Envelope v0.1 Verification](JOINT_COHORT_PROBABILITY_ENVELOPE_VERIFICATION_V0_1.md).
Both bridges remain candidate-only and do not relax the synthetic-input guard.
The
[public evidence snapshot](PUBLIC_CALIBRATION_EVIDENCE_V0_1.md) defines
source-backed candidate-event and acquisition questions but explicitly
supplies no empirical scenario weights, recoveries, or expected returns.

The next calibration generation may add lifecycle hazard models, explicit
factor and common-cause constraints, parameter or posterior draws, challenger
model variants, and nested propagation of parameter and path uncertainty. Those
features must preserve the simple envelope as a reproducible benchmark. A more
complex model earns its place only when it changes a real financing decision
and survives effective challenge.
