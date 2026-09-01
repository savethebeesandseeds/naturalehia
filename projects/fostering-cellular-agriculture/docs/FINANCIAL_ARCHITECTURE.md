# Financial Architecture for Fostering Cellular Agriculture

**Public north-star summary — version 1.0**

## Aim

Fostering Cellular Agriculture will develop an open financial standard and a
focused family of instruments that can fund cellular-agriculture projects from
research through mass production.

The purpose is to help credible science cross the difficult passage into
industry. Financial engineering should make risk understandable, divide it
honestly, and connect different stages of development to capital capable of
bearing them. It cannot make weak economics valuable merely by packaging them.

## The central idea

We are not beginning with one fund, security, or legal form. We are building a
common financial interface that a project can connect to:

```text
project financial interface
    = capital schedule
    + milestone states
    + cash rights after success
    + loss and recovery after failure
    + exposure to shared risk factors
```

This interface translates unlike scientific and industrial projects into the
same financial language without forcing them to use the same technology or
instrument. It lets us calculate capital exposure, expected loss, recovery,
duration, expected return, tail risk, correlation, diversification, and value.

| Component | Financial meaning |
|---|---|
| Capital schedule | How much can be committed and drawn, when, and for how long it remains exposed |
| Milestone states | The paths from research toward production, including stop, failure, and exit states |
| Success cash flows | Repayment, licensing, royalties, revenue participation, equity-linked value, or refinancing proceeds |
| Failure and recovery | Capital lost, unused commitment preserved, recovery cash, and recovery timing |
| Common risk exposure | Shared technology, input, equipment, buyer, energy, regulatory, and funding dependencies |
| Uncertainty status | Whether an input is observed, contractually fixed, estimated, or synthetic |

The standard describes the economics of a project. It is not a laboratory or
company-management manual.

## The selected instrument family

Stage-specific research, pilot, facility, and production terms remain inside
the underlying project claims. The market-facing family is deliberately
narrow:

| Construction | Economic function |
|---|---|
| Multi-Project Milestone Participation | Callable, untranched, limited-recourse participation in a fixed disclosed pool of milestone claims and their assigned cash |
| Funded First-Loss and Priority Claims | Two fully funded claims on the same pool; loss runs junior first and principal cash runs priority first |
| Failure-Contingent Partial-Credit Guarantee | External capped payment of a stated share of final resolved principal loss on the untranched core |

The milestone-gated facility remains an underlying claim form, not a competing
public instrument. Other completion, insurance, procurement, price, or input
hedges stay research possibilities rather than version-1 variants. The two
selected variants are alternatives; combining them requires a separately
tested no-double-recovery waterfall.

The current ten-claim synthetic fixture is mechanically implemented but fails
the robust adverse-NPV and support-feasibility tests. It is defined,
measurable, and rejectable; it is not shown attractive or financeable.

The additive Capital Mobilization Frontier v0.2 makes that rejection more
specific. On the same pool it holds `B=24`, the 15%/8% junior/market hurdle
sensitivities, claim terms, cash paths, and physical probability set fixed,
then crosses five success-participation values with five junior issued-
principal boundaries. None of the 25 tested `(q,A)` candidates passes every
declared synthetic mandate. The aggregate project-outlay and contractual
asset-principal limits are both 100 in this retained at-par fixture, while the
v0.2 stack separately funds reserve and issued principal `K=100`. Asset
writeoff `L_s`, continuing principal `O_s`, and issued-principal cash shortfall
`Q_s` remain distinct; `A` layers only `Q`.

That finite-grid rejection does not prove that untested terms fail. It identifies
the next conditional channel: hold the claim and future cash fixed, then test
whether an independently evidenced buyer price and hurdle can overlap the
issuer funding floor after a separately authorized no-rights support source.
The current synthetic issue-price/support sensitivity has arithmetic overlap
only at invented 0% and 5% hurdles, not at 8% or above, and no overlap is backed
by funded or escrowed support. Its priority-cap gate is a separate, relaxed
sensitivity mandate, not the strict frontier mandate. The strict frontier still
rejects the same `q=1`, `A=20`, `M=80` point, and price or support cannot change
its fixed `Q` risk, tails, incidence, or WAL. The downstream result is not a
quote, fair value, demand, support commitment, or evidence of capital
mobilization.

Naturalehia retains no protocol fee, carried interest, instrument royalty, or
share of financed-company receipts. This open-standard commitment does not
remove the investor's need for enforceable repayment and success cash from the
underlying project claims.

## How an instrument can create value

Five mechanisms are worth testing:

1. **Staging limits wasted capital.** A failed project loses only the capital
   released before failure, rather than an entire commitment.
2. **Success participation supports high-risk development.** Fixed interest
   alone may be unaffordable when failures are frequent. Royalties, revenue
   participation, equity-linked value, or commercialization payments allow
   successful projects to compensate for unsuccessful ones.
3. **Pooling can reduce idiosyncratic tail risk.** It does not reduce expected
   loss by itself and does not protect against shared technological, input,
   regulatory, demand, energy, or capital-market shocks.
4. **Defined risk-transfer legs can remove a narrow financing blockage.** A
   price floor, qualified-output commitment, completion support, or guarantee
   has value only when it offsets an identified exposure and has a real payer.
5. **A common standard reduces repeated financial translation.** Comparable
   exposure and payoff records can lower analysis costs and allow projects to
   enter portfolios without hiding their differences.

## The value test

Every proposed instrument must identify where investor cash actually comes
from: customer payments, project revenue, licensing, royalties, equity value,
refinancing, recoveries, or an explicit third-party contribution. Payment-in-
kind returns enlarge a claim but do not create cash.

An instrument is promising only if, under transparent assumptions:

- expected cash flows compensate for expected loss, time, illiquidity, and
  common-factor risk;
- severe but plausible loss remains bearable for the capital assigned to it;
- successful-project upside is sufficient to balance development failures;
- pooling produces a measurable diversification benefit after correlation;
- the structure improves funding availability, tenor, or cost relative to
  financing projects separately; and
- all investor claims reconcile to the underlying project cash flows and any
  explicit outside support.

Tranching can give investors different loss and return positions, but it only
redistributes the pool’s result. It should be considered after the untranched
pool shows economic value, not used to manufacture the appearance of safety.

The implemented capital-stack test makes that statement computational. The
complete commitment is subscribed at par into a zero-yield reserve so staged
draws cannot leave senior protection imaginary. Project principal and unused
reserve pay senior first; actual non-principal receipts pay bounded priorities
and then the first-loss residual. The report shows each tranche's loss,
exhaustion, expected NPV, cash multiple, tail shortfall, and common-witness
weighted-average life. It estimates neither a market spread nor fair value.

The implemented capital-mobilization frontier then tests the two terms that
most directly divide the pool's economics: the share `q` of declared success
cash admitted to the structure and the funded junior first-loss amount `A`.
For every declared pair it rebuilds the two-claim waterfall, applies one stated
market mandate over the entire event-probability set, and retains each adverse
endpoint's own probability witness. It reports all feasible and nondominated
pairs; it does not choose a security, infer investor demand, or count modeled
feasibility as capital mobilized.

The first implemented term test varies only the share of already-declared
non-principal success cash. It solves for the smallest share that meets a stated
worst expected NPV across the entire supported probability envelope. If the
required share lies above the configured 100% right, the instrument fails this
test; unused source capacity, tranching, and an optimistic central probability
do not close the gap.

The next test fixes that success share and solves the smallest proportion of
final pool loss that an outside provider would have to absorb. Gross project
loss remains visible. It then compares the investor's maximum affordable
upfront premium with the provider's conservative claim-only break-even floor.
A missing price overlap is published as an explicit catalytic-support gap;
risk transfer is not allowed to masquerade as newly created value.

The provider price-ladder sensitivity then retains that same claim and adds
disclosed incremental collateral carry, economic-capital charge, expenses,
and target profit. Capital and collateral principal are not expenses. The
result is compared with the conservatively payable investor ceiling, and any
shortfall is separated into provider premium support and investor-target
restoration. It is premium adequacy under physical probabilities, not fair
value or a provider solvency result.

## What we are building

The financial standard represents each project’s capital needs,
state-contingent payoffs, failure recovery, and common-factor exposure. The
implemented [Project Claim Ledger v0.1](PROJECT_CLAIM_LEDGER_V0_1.md) first
normalizes one underlying claim. It reconciles buyer price to borrower gross
and net proceeds, rolls principal and accrued interest separately, distinguishes
cash from due amounts and conversion, derives guarantee claims from covered
shortfalls, and prevents later backtest evidence from changing an ex-ante
expected return. An incomplete public package stays visibly incomplete instead
of being completed with zeroes. The
first implemented pool kernel calculates standalone and pooled risk and return
from explicit joint project states. Its strict reloadable scenario format and
human-readable report are implemented. The first stage-specific adapter now
translates actual milestone-gated facility paths into that interface, including
explicit allocations when repayment comes from more than one kind of cash.
A separate exact probability-envelope calculation now shows how every central
financial result moves over defensible bounds on the fixed joint-state weights,
including the probability witness for each endpoint. The accompanying
calibration standard defines the evidence needed to replace synthetic bounds.
The robust success-participation term solver now connects those two components:
it either publishes a supported contractual threshold or proves that the
existing payoff is insufficient. The pooled-loss-protection solver then tests
an external proportional loss share and both sides of its premium. In the
synthetic fixture, protection can reach investor break-even before premium but
no provider-compensating bilateral price exists; the required catalytic
support is measured rather than concealed. The price-ladder module now shows
how disclosed provider carry, capital charge, expenses, and profit widen that
gap while preserving the adverse probability witness. The
[counterparty-credit module](PROVIDER_CREDIT_STRESS_V0_1.md) then holds that
full-performance price fixed and stresses what the
investor actually collects through scenario-conditioned provider performance,
explicitly pledged collateral, unsecured recovery, and wrong-way dependence.
The [fully funded capital-stack term](CAPITAL_STACK_TERM_V0_1.md) now converts
the fixed participation pool into at-par first-loss, intermediate, and senior
claims while reconciling every distribution, cost, loss, unresolved exposure,
reserve movement, and robust duration result. The
[robust capital-mobilization frontier](ROBUST_CAPITAL_MOBILIZATION_FRONTIER_V0_1.md)
now crosses success participation and funded first loss, re-evaluates that
whole structure, and shows which tested market claims survive all declared
return and risk limits without a weighted score.

The implemented market cash-allocation layer is the
[market non-principal priority-cap term](ROBUST_MARKET_PRIORITY_CAP_TERM_V0_1.md),
not a management layer. For each fixed `(q,A)` structure, it reports the
smallest tested lifetime priority on real non-principal cash that meets the
declared physical-measure market mandate at par, then shows the reduction in
junior cash and increase in catalytic concession. It does not discover a
hurdle or market return.

The implemented
[robust issue-price support term](ROBUST_ISSUE_PRICE_SUPPORT_TERM_V0_1.md)
then leaves that claim and every physical cash path fixed. At each separately
supplied, non-circular investor hurdle, it calculates the highest gross buyer
price supported by the worst admitted probability mix and compares it with the
issuer price floor after explicit non-repayable, no-rights support. The result
is a conditional price window or a quantified financing gap. It separately
reports modeled arithmetic, documented support, funded or escrowed support,
settled issue sources, and evidenced reserve and cost uses. It does not turn a
synthetic hurdle into market calibration, a support indication into cash, or a
physical NPV boundary into fair value.

The implemented
[market-observation and hurdle-evidence set](MARKET_OBSERVATION_AND_HURDLE_EVIDENCE_V0_1.md)
is the non-circular bridge into that price engine. A transaction supplies a
price, not the buyer's beliefs. The bridge reconstructs the full expected-cash
rate interval under declared physical probabilities, maps comparable claims
through bounded jointly feasible adjustments, keeps direct transactions,
settled comparables, and executable quotes in separate evidence tiers, and
forms exact closed `S_0` through `S_k` unions from a predeclared discordance
budget. It neither averages across unsupported gaps nor substitutes a weaker
tier when better evidence is insufficient. The result remains transaction- and
model-conditioned: empirical acquisition, document authentication, and a first
controlled real transaction package are the next research layer, not claims
already achieved.

The principal outputs are exposure at default, probability and severity of
loss, expected cash return, NPV at a declared hurdle, IRR and cash multiple
distributions, duration, liquidity need, value at risk, expected shortfall,
correlated stress loss, diversification benefit, and—only where useful—loss
allocation across investor layers.

Existing evidence, facility, and staged-capital models remain technical
support. The public result of the project should remain simple: what each
instrument finances, where its return comes from, who bears loss, why it might
be valuable, and what assumptions could make it fail.

## Honest boundary

This architecture is a research program, not an offer, price, rating, or claim
that a project is financeable. A financial standard cannot substitute for real
cash-generating rights, credible probabilities, or viable production
economics. Its purpose is to make those conditions visible and to engineer
capital around them when they genuinely exist.
