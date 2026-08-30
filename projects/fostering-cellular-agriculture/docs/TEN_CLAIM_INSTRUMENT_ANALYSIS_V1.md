# Ten-Claim Instrument Analysis

**Synthetic comparison version 1.0 — 30 August 2026**

This document applies one probability envelope and one set of fixed joint cash
paths to the unsupported core and both permitted variants. All amounts are
`DEMO million`. Every cash amount, probability, recovery, hurdle, support
term, and provider state is invented for mechanics. None is calibration, fair
value, a forecast, a market quote, a rating, an offer, or evidence that a
project or provider can perform.

## Exactly ten synthetic candidate claims

| # | Claim | Stage | Limit | Draws at months 0/12/24 | Performing receipt | Shared exposures |
|---:|---|---|---:|---|---|---|
| 1 | Cell-line and media platform | Research | 6 | 2.4 / 1.8 / 1.8 | 11.40 at month 60 | Biology; supplier |
| 2 | Serum-free media platform | Research | 7 | 2.8 / 2.1 / 2.1 | 12.95 at month 60 | Biology; supplier |
| 3 | Cultivated beef pilot | Pilot | 8 | 2.8 / 2.8 / 2.4 | 14.00 at month 54 | Biology; regulatory; buyer |
| 4 | Cultivated poultry pilot | Pilot | 9 | 3.15 / 3.15 / 2.7 | 15.75 at month 54 | Biology; scale-up; buyer |
| 5 | Continuous-perfusion demonstration | Demonstration | 10 | 3 / 3 / 4 | 16.50 at month 48 | Scale-up; supplier |
| 6 | Food-grade scaffold pilot | Pilot | 7 | 2.45 / 2.45 / 2.1 | 11.90 at month 54 | Supplier; regulatory; buyer |
| 7 | First-industrial beef facility | First industrial | 15 | 3.75 / 5.25 / 6 | 23.25 at month 48 | Scale-up; regulatory; buyer |
| 8 | First-industrial poultry facility | First industrial | 14 | 3.5 / 4.9 / 5.6 | 21.70 at month 48 | Scale-up; supplier; buyer |
| 9 | Cultivated-fat demonstration | Demonstration | 8 | 2.4 / 2.4 / 3.2 | 13.20 at month 48 | Biology; regulatory; buyer |
| 10 | Repeat modular production line | Repeat production | 16 | 3.2 / 4.8 / 8 | 24.00 at month 42 | Scale-up; supplier; regulatory; buyer |

The limits total 100. Performing receipts total 164.65: 100 principal and 64.65
synthetically assigned non-principal participation. Factor notionals overlap
and must not be added: biology 38, scale-up 64, supplier/media 60,
regulatory/qualification 54, and buyer/acceptance 77.

## Joint dependence, not independent defaults

`P` performs; `D` is continuing exposure with no horizon receipt; `E`
stops after the first draw; `L` stops after two draws; `C` is fully drawn
and resolves with 10% recovery. `E` and `L` recover a declared fraction of
the cash actually drawn.

| Joint state | Central weight | Ten-claim path | Realized loss | Continuing exposure |
|---|---:|---|---:|---:|
| All perform | 58% | `PPPPPPPPPP` | 0 | 0 |
| Biological/process shock | 7% | `LDELPPPPLP` | 13.130 | 7 |
| Scale-up/commissioning shock | 7% | `PPPLLPLLPL` | 23.150 | 0 |
| Supplier/media shock | 6% | `DLPPDEPLPD` | 10.675 | 32 |
| Regulatory/qualification delay | 5% | `PPDPPDDPDD` | 0 | 54 |
| Buyer/product-acceptance shock | 7% | `PPDLPDLLLD` | 17.970 | 31 |
| Biology and scale-up compound | 4% | `LDECLPLLLL` | 35.560 | 7 |
| Supplier, regulatory, and buyer compound | 4% | `DLDLDCCCLC` | 58.005 | 24 |
| Systemic commercialization freeze | 2% | `CCCCCCCCCC` | 90.000 | 0 |

The box envelope permits each state to move inside a declared range while all
weights still sum to one. Endpoint vectors are feasible adverse witnesses, not
confidence intervals. Central pairwise realized-loss correlations range from
0.228251 to 0.938114. Common shocks remain material.

## Unsupported core

| Metric | Minimum | Central | Maximum |
|---|---:|---:|---:|
| Investor cash contributed, including cost | 88.303000 | 94.624000 | 99.503500 |
| Principal cash receipts | 57.989200 | 75.123400 | 88.386050 |
| Non-principal success cash | 34.130000 | 46.525500 | 55.633000 |
| Investor receipts | 92.119200 | 121.648900 | 143.700550 |
| Continuing principal exposure | 1.150000 | 8.520000 | 18.740000 |
| Realized principal loss | 2.152500 | 9.980600 | 21.525350 |
| Impairment probability | 16% | 37% | 60% |
| NPV at 8% physical hurdle | -18.717674 | 0.661828 | 15.440326 |
| Negative-NPV probability | 28% | 42% | 60% |
| Principal-loss ES95 | 19.074000 | 70.803000 | 90.000000 |
| Principal-loss ES99 | 23.150000 | 90.000000 | 90.000000 |
| NPV-shortfall ES95 | 28.015930 | 81.033859 | 85.236300 |
| NPV-shortfall ES99 | 40.784017 | 85.236300 | 85.236300 |
| Peak same-month project draw | 33.679000 | 35.607500 | 37.294500 |
| Peak same-month gross funding need | 33.870000 | 35.688500 | 37.309000 |
| Peak cumulative net outlay | 88.147600 | 94.533000 | 99.487400 |

At the central measure, pooling reduces ES95 from the 78.717 sum of standalone
claim ES to 70.803: a benefit of 7.914, or 10.053737%. ES99 remains 90 both
standalone and pooled, so the measured 99% diversification benefit is zero.
The maximum ten-claim loss remains 90.

Expected project cash remains separate by source: commercial 77.1490,
licensing/royalty 40.8265, and recovery 3.6734. There is no project-level
explicit-support cash. Pool cost is 1.0. Aggregate weighted-average life is not
available in the core engine.

Principal/non-principal classification and cash-source kind are two views of
the same receipts, so those rows and source totals must not be added. Each
minimum or maximum is separately optimized over the probability envelope; the
central column alone uses the one declared central measure throughout.

The minimum-NPV witness assigns 40% to all-perform, 10% to the biology/scale-up
compound, 16% to buyer shock, 12% to regulatory delay, 10% to the
supplier/regulatory/buyer compound, and 6% to systemic freeze, with the
remaining 6% in single shocks. It produces expected NPV of -18.717674.
That is also the remaining robust NPV gap at full declared success
participation. No support provider, provider payout, investor premium,
provider floor, or provider catalytic gap applies to the unsupported core.

## Variant 1: fully funded first loss and priority

The full 100 commitment is subscribed at month zero; total calls including the
cost are 101, making that amount the initial aggregate liquidity need. Expected
prefunding drag at the pool's 8% hurdle is
8.095413–10.122936, central 8.983578.

| Metric | Funded first loss 0–20 | Market priority 20–100 |
|---|---:|---:|
| Investor contributions | 20.200000 | 80.800000 |
| Expected underlying principal cash, min / central / max | 8.000000 / 11.600000 / 14.400000 | 49.989200 / 63.523400 / 73.986050 |
| Expected return of unused funded reserve, min / central / max | 0 / 0 / 0 | 1.496500 / 6.376000 / 12.697000 |
| Expected non-principal success cash, min / central / max | 16.864000 / 25.160000 / 31.756000 | 17.266000 / 21.365500 / 23.877000 |
| Expected distributions, min / central / max | 24.864000 / 36.760000 / 46.156000 | 75.375200 / 91.264900 / 102.766550 |
| Expected loss, min / central / max | 2.089500 / 6.217500 / 11.464850 | 0.063000 / 3.763100 / 10.060500 |
| Expected continuing exposure, min / central / max | 0.271250 / 2.182500 / 5.129500 | 0.810050 / 6.337500 / 14.572300 |
| NPV at stated hurdle, min / central / max | -7.111619 / -0.854614 / 4.117883 at 15% | -25.733095 / -13.926874 / -5.274392 at 8% |
| Impairment probability, min / central / max | 16% / 37% / 60% | 2% / 17% / 42% |
| Negative-NPV probability, min / central / max | 28% / 42% / 60% | 100% / 100% / 100% |
| Principal-loss ES95, min / central / max | 17.814 / 20 / 20 | 1.260 / 50.803 / 70 |
| Principal-loss ES99, min / central / max | 20 / 20 / 20 | 3.150 / 70 / 70 |
| NPV-shortfall ES95, min / central / max | 19.279305 / 20.200000 / 20.200000 | 20.455134 / 69.507663 / 72.861678 |
| NPV-shortfall ES99, min / central / max | 20.200000 / 20.200000 / 20.200000 | 31.502617 / 72.861678 / 72.861678 |
| Principal cash WAL, min / central / max | 4.825 / 4.825 / 4.825 years | 4.003747 / 4.069270 / 4.154239 years |

Loss and exposure sum back to the unchanged gross pool at every path. Neither
class meets its central hurdle. Aggregate stack NPV at the common 8% pool
hurdle is -27.985093 / -8.321750 / 6.715820 across the minimum, central, and
maximum expected cases. Priority changes allocation, not aggregate value or
project diversification. Each robust minimum and maximum in the table has its
own probability witness; component endpoints must not be added. The central
columns use one common declared measure and reconcile exactly.

Project and factor concentration, pairwise dependence, and gross-project
diversification are unchanged from the core. There is no contingent provider,
payout, premium capacity, or provider floor in this variant; the 20 first-loss
amount is funded investor capital, not a guarantee commitment.

## Variant 2: 30% failure-contingent guarantee

The guarantee pays 30% of final resolved principal loss at month 60, subject to a
legal cap of 30. It leaves 70% of gross loss with the investor and does not
cover continuing exposure.

Contractual principal remains 100. Before premium, investor contributions,
project receipts, continuing exposure, impairment probability, project and
factor concentrations, joint dependence, gross diversification, and underlying
liquidity are unchanged from the core. The provider payment is external cash at
month 60; aggregate underlying WAL remains unavailable. At the central weights,
total investor receipts before premium are 124.643080: 121.648900 from projects
and recoveries plus 2.994180 from the provider.

Unless a row expressly says provider-credit stress, the table assumes full
provider performance. The retained-loss and supported NPV-shortfall tails are
therefore full-performance results. Credit-stressed NPV-shortfall tails are not
produced by the current consolidated reporter and remain unavailable.

| Metric | Minimum | Central | Maximum |
|---|---:|---:|---:|
| Investor contributions before premium | 88.303000 | 94.624000 | 99.503500 |
| Total investor receipts including external support, full provider performance | 98.074740 | 124.643080 | 145.074685 |
| Expected provider payment, nominal at month 60 | 0.645750 | 2.994180 | 6.457605 |
| Continuing principal exposure, unchanged | 1.150000 | 8.520000 | 18.740000 |
| Principal impairment probability, unchanged | 16% | 37% | 60% |
| Expected investor NPV before premium, full provider performance | -14.925982 | 2.699617 | 16.389832 |
| Expected investor NPV before premium, provider-credit stress | -15.161094 | 2.617773 | 16.389832 |
| Expected retained principal loss | 1.506750 | 6.986420 | 15.067745 |
| Retained principal-loss ES95 | 13.351800 | 49.562100 | 63.000000 |
| Retained principal-loss ES99 | 16.205000 | 63.000000 | 63.000000 |
| Supported NPV-shortfall ES95 | 24.221747 | 66.577659 | 66.860554 |
| Supported NPV-shortfall ES99 | 37.114993 | 66.860554 | 66.860554 |
| Peak gross funding need before settlement, unchanged | 33.870000 | 35.688500 | 37.309000 |
| Provider claim ES95, nominal at month 60 | 5.722200 | 21.240900 | 27.000000 |
| Provider claim ES99, nominal at month 60 | 6.945000 | 27.000000 | 27.000000 |

The provider's contractual maximum exposure is 30 and its modeled maximum
claim is 27. Investor premium capacity is absent: signed headroom is
-14.925982. The claim-only provider break-even floor is 5.307681 and the
claim-only catalytic gap is 20.233663. Adding the declared collateral carry,
capital charge, expenses, and profit raises the provider all-in floor to
13.541350 and the total support gap to 28.467332.

Claim payouts and payout tails are nominal at month 60. Claim-only and all-in
provider floors are month-zero present values at the separate 4% provider
hurdle; investor NPV and counterparty-credit-loss PV use the portfolio's stated
8% hurdle. These bases must not be compared as if they were the same date or
discount rate. The two support components are nevertheless stated as month-zero
funding requirements under their respective disclosed hurdle constructions;
their 28.467332 sum is a budget gap, not fair-value aggregation under one common
discount curve.

Counterparty stress is deliberately wrong-way. Provider default probability is
1.84%–8.62%, central 4.51%; expected unsecured exposure in default atoms is
0–0.449510, central 0.156477; and expected investor credit-loss PV is
0–0.235112, central 0.081844. The central claim-PV delivery ratio is 0.959837
and its robust minimum is 0.870321. Credit stress increases the all-in support
gap to 28.702444.

Contractual maximum unsecured exposure is 17.579091 and modeled maximum
unsecured exposure is 14.579091. At the central probability mix,
counterparty-credit-loss PV ES95 / ES99 is 1.636882 / 6.621378.

The earlier two-claim Failure-Contingent Public Partial-Credit Guarantee v0.1
fixture remains unchanged: it has zero investor premium capacity and a
0.800000 claim-only catalytic gap before provider expenses. The ten-claim
result is a new, harder sensitivity and does not overwrite that control result.

## What the comparison proves—and does not

The engines reconcile project cash, recovery, reserve return, support transfer,
principal, loss, exposure, and tranche allocation. They show that the current
synthetic success rights do not clear the adverse probability envelope.
Pooling supplies a modest ES95 benefit, but common factors dominate the far
tail. First loss moves risk; a guarantee transfers it; neither creates cash.

Real use still requires authenticated contracts, empirical claim and recovery
data, defensible dependence bounds, investor hurdle evidence, legal
enforceability, provider authority and credit capacity, tax/accounting review,
and adoption evidence. Until then the terms are candidate-only and the correct
decision is not to call them priced, calibrated, attractive, or financeable.

## Verification record

On 30 August 2026, the complete Debug and Release suites each passed 68 of 68
tests. The focused instrument-family golden and negative-boundary regression
passed in both configurations; Debug and Release produced identical
consolidated reports. Core, funded-stack, protection, provider-price, and
provider-credit monetary reconciliation maxima were all zero at six decimals.
