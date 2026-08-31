# Fostering Cellular Agriculture: The Financial Instrument in Brief

**Public economic description — version 1.0**

## What we are building

Cellular agriculture needs patient capital while research, pilot work, and
first industrial facilities still have high failure risk and late cash. The
core instrument is a **Multi-Project Milestone Participation**: one disclosed
claim on several project claims, with later funding released only when stated
milestones are met.

This is financial engineering, not fund-management design. The instrument
defines what the investor owns, where cash can come from, what counts as loss,
and how shared failures are measured.

Naturalehia retains no protocol fee, carried interest, instrument royalty, or
share of financed-company receipts for publication or adoption of the open
standard. This does not remove the investor's need for enforceable repayment
and success cash from the financed project claims.

## The asset

An investor owns a pro-rata, limited-recourse right to:

- a fixed schedule of cellular-agriculture project claims that satisfy the
  stated inclusion conditions;
- principal repayment and specifically assigned commercial, licensing,
  royalty, capacity, offtake, or exit cash;
- identified cash recoveries; and
- in a supported variant, a separately identified external payment.

The investor does not automatically own the facilities, company equity,
company value, speculative intellectual property, unused payer capacity, or a
manager's promise.

Each project uses the same financial interface:

```text
cash required and milestone dates
+ principal created, repaid, converted, written off, or still outstanding
+ assigned success cash and its source
+ cash recovery after failure
+ exposure to shared biological, scale-up, supplier, regulatory, and buyer risk
```

Investor cash and contractual principal are separate. In the synthetic example
they happen to be equal at each primary funding draw because that is the stated
at-par term. Unknown is not zero. Continuing principal is exposure, not cash
and not yet a realized loss.

## How money moves

The core has callable commitments. Project funding is released at milestones;
failure can stop later draws. Actual project receipts and recoveries pass
through pro rata, after explicit pool costs.

```text
investor return = dated cash received - dated cash paid

opening principal + principal created
  = principal cash returned
  + evidenced conversion or other extinguishment
  + principal still outstanding
  + resolved principal loss

therefore continuing principal is exposure, not loss
pooling changes the distribution of resolved loss
packaging creates no cash
```

Success participation is capped by rights and source budgets already written
into each claim. Refinancing is liquidity, not operating value. Payment in kind
is not cash.

## One core and two variants

| Construction | What it changes | What it cannot do |
|---|---|---|
| Multi-Project Milestone Participation | Combines disclosed milestone claims and makes project, liquidity, loss, return, and dependence measurable. | It cannot improve project cash or remove common shocks. |
| **Variant 1:** Funded First-Loss and Priority Claims | Fully funds the commitment and allocates loss first to a 0–20 junior claim; principal cash pays the 20–100 priority claim first. | It reallocates cash and loss; it does not create value or diversification. |
| **Variant 2:** Failure-Contingent Partial-Credit Guarantee | An external provider pays a fixed share of final resolved loss, subject to a cash cap. | It does not cure gross loss, cover continuing exposure, or make provider capacity free. |

The two variants are alternatives in version 1. Combining them needs a new
no-double-recovery waterfall.

## Why an institution might consider it

The conditional attraction is not a promise of yield. It is a standardized,
limited-recourse claim with milestone-controlled funding, several named cash
sources, explicit loss accounting, and visible exposure to both project and
common risks. A real institution might consider it if enforceable success cash
and genuine pooling produce adequate return at its independently supported
hurdle, or if a separately funded risk bearer makes the remaining terms
acceptable.

The investment thesis is falsified if the cash rights are absent or overlap,
milestones do not stop wasteful funding, common shocks erase diversification,
adverse NPV remains inadequate, recoveries are unevidenced, or the protection
provider cannot legally and financially perform. The present synthetic example
fails the adverse-NPV and premium-feasibility tests.

## The ten-claim test

The implemented example has exactly ten claims, 100 of aggregate contractual
reference principal, three milestone dates, and nine complete joint outcomes.
It includes research platforms, pilots, demonstrations, first-industrial
facilities, and repeat production. Its probabilities, cash, recoveries,
hurdles, and provider terms are all synthetic. The fixture models dated draws
and path-contingent stops, not real milestone predicates or certificates.

No project defaults are multiplied as if independent. The joint states
explicitly include biological/process, scale-up/commissioning, supplier/media,
regulatory/qualification, buyer/product-acceptance, compound, and systemic
shocks.

At the declared central mix, the unsupported core has:

All table amounts are synthetic `DEMO million` on a constant analysis-close
monetary basis. Cash amounts are nominal unless labeled NPV or PV.

| Measure | Synthetic result |
|---|---:|
| Expected investor cash paid, including pool cost | 94.624000 |
| Expected investor receipts | 121.648900 |
| Expected continuing principal exposure | 8.520000 |
| Expected resolved principal loss | 9.980600 |
| Expected NPV at the stated 8% physical hurdle | 0.661828 |
| Probability of any resolved impairment | 37% |
| Loss ES95 / ES99 | 70.803000 / 90.000000 |

Pooling reduces central ES95 by 7.914, or 10.053737%, relative to the sum of
standalone claim tails. At ES99 the benefit is zero. Common shocks therefore
remain decisive.

The full-provider-performance guarantee comparison keeps project economics
visible. Values below are at the central synthetic weights and before any
investor premium.

| Same ten-claim pool | Unsupported core | With 30% guarantee, provider performs |
|---|---:|---:|
| Investor cash paid, including pool cost | 94.624000 | 94.624000 |
| Project receipts and recoveries | 121.648900 | 121.648900 |
| External provider cash | 0 | 2.994180 |
| Total investor receipts | 121.648900 | 124.643080 |
| Gross resolved principal loss | 9.980600 | 9.980600 |
| Loss retained by investor | 9.980600 | 6.986420 |
| Continuing principal exposure | 8.520000 | 8.520000 |
| Expected NPV at 8% | 0.661828 | 2.699617 |
| Worst expected NPV in the envelope | -18.717674 | -14.925982 |

The only new dated cash is the external transfer; the NPV improvement is its
discounted value at the stated 8% hurdle, not its nominal amount. The guarantee
creates no project cash and does not remove impairment or continuing exposure.

## The honest result

The central NPV is slightly positive, but the worst expected NPV permitted by
the synthetic probability ranges is **-18.717674**. Full declared success
participation cannot reach robust break-even. The candidate core is therefore
not shown financeable.

Variant 1 makes the loss order real by funding 20 of first-loss capital.
However, prefunding drag is 8.983578 at the central mix, and neither class
meets its stated central hurdle under these invented terms.

Variant 2 pays 30% of final resolved loss, with a provider commitment cap of
30. Expected nominal provider payment is 2.994180 centrally and can reach
6.457605 under the probability envelope; modeled single-path payment reaches
27. Investor NPV
before premium improves centrally to 2.699617, but the adverse expected NPV
remains -14.925982. The investor cannot pay a non-negative premium and preserve
the target. Including provider costs produces a 28.467332 support gap; modeled
wrong-way provider credit increases it to 28.702444.
Under that credit stress, investor NPV before premium is 2.617773 centrally and
-15.161094 at the adverse endpoint, rather than 2.699617 and -14.925982 under
full provider performance.

This is not a failure of the project. It is the value of the standard: a weak
or under-supported claim can be rejected without hiding the gap in optimistic
probabilities, management language, or tranching.

The earlier two-claim public-guarantee control remains unchanged: zero investor
premium capacity and a 0.800000 claim-only catalytic gap before provider
expenses. The ten-claim example is a harder, separate sensitivity.

## What is synthetic, evidenced, and unknown

**Evidenced here:** the source code, input files, cash identities, loss
allocation, probability optimization, tails, correlations, provider exposure,
and reconciliation tests are retained and reproducible. This is evidence of
implemented mechanics only.

**Synthetic:** every ten-claim cash amount, probability, recovery, hurdle,
factor loading, provider term, and counterparty state.

**Unknown before a live issue:** authenticated contracts, actual project and
recovery data, defensible joint probabilities, investor hurdle evidence,
enforceable transfer and security rights, provider authority and credit
capacity, legal/tax/accounting treatment, market adoption, and whether the
financing actually advances industrial capacity and animal displacement.

See the
[candidate term sheet](MULTI_PROJECT_MILESTONE_PARTICIPATION_TERM_SHEET_V1.md)
and the
[ten-claim analysis](TEN_CLAIM_INSTRUMENT_ANALYSIS_V1.md).
