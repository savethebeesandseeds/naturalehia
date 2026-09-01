# Callable-Capital and Warehouse Funding Integrity Bridge v0.1

Status: executable synthetic reference specification, 2026-09-01. The C++20
engine and closed configuration grammar are tested only through
Emscripten/WebAssembly. No live transaction, calibrated provider model, or
public quantitative bridge result is claimed in v0.1.

## Purpose

Capital Stack v0.2 assumes that its complete asset-acquisition and primary-
funding limit `R` is subscribed at par in month zero and held in a lossless,
zero-yield reserve until used. That boundary makes the tranche protection real,
but it can be expensive. In the retained ten-claim synthetic analysis, expected
prefunding drag at the pool's 8% hurdle is `8.095413–10.122936`, with a central
value of `8.983578` million.

This bridge asks a narrower next question:

> Can the same dated eligible uses be funded with a smaller initial cash
> balance, settled callable capital, and a warehouse facility, while preserving
> real funded protection and reporting every new funding dependency and cost?

Version 0.1 is an **aggregate funding-feasibility test** around one frozen
[Capital Stack v0.2](CAPITAL_STACK_ASSET_LIABILITY_BRIDGE_V0_2.md) asset program.
It is not a replacement tranche waterfall. It does not change the asset-side
`L/O` ledgers, liability-side `Q`, attachment or detachment points, cash-source
classification, selected participation fraction, project paths, or scenario
definitions. It does not allocate bridge cash to individual capital-stack
tranches.

A passing result means only that the declared resources meet the declared
cash uses and protection tests on the tested paths. It does not mean that the
resources will be available in a live transaction or that any claim is fairly
priced, calibrated, legally enforceable, or investable.

## 1. Frozen asset and capital-stack boundary

For scenario `s` and month `t`, the bridge imports without reinterpretation:

```text
R       = Capital Stack v0.2 acquisition and primary-funding limit
A_s,t   = claim-acquisition and primary-project-funding cash due
C_s,t   = buyer-direct cost due outside R
G_s,t   = pool cost due outside R
P_s,t   = actual underlying principal cash
N_s,t   = actual underlying non-principal cash
L_s,t   = contractual asset principal written off
O_s,t   = contractual asset principal still outstanding
```

The bridge also imports the fixed capital-stack terms and their lineage. It
must reject a caller that changes a project path, date, amount, source kind,
principal ledger, scenario, probability bound, tranche boundary, or selected
`q` while changing the funding plan.

`A` is warehouse-eligible only when the warehouse term identifies the
corresponding asset and use as eligible. `C`, `G`, warehouse interest, and
warehouse fees are cash uses but do not create eligible asset cost basis,
contractual asset principal, issued tranche principal, or funded protection.
Version 0.1 does not capitalize those amounts into the borrowing base.

## 2. Declared funding resources

The bridge keeps six external funding purposes separate:

1. **initial controlled cash** actually paid into the vehicle;
2. **callable capital** subject to a fixed limit, notice period, expiry,
   permitted purpose, and provider;
3. **warehouse advances** subject to a fixed commitment, eligibility rule,
   advance rate, borrowing base, maturity, interest, fees, and provider; and
4. **cost-support cash** restricted to buyer-direct costs, pool costs, and
   facility charges;
5. **protection-replenishment cash** that has settled and qualifies as
   subordinated protection; and
6. **takeout cash** that has actually settled by the bridge end date.

A request, notice, approval, commitment, expected refinancing, indicative term
sheet, or forecast issuance is not cash. It may be recorded as contingent
capacity, but it enters the cash ledger only when settled into the controlled
account. A provider failure is never replaced by an unnamed provider or an
assumed sponsor cure.

Every request has exactly one final outcome at its fixed due month:
`settled-in-full`, `final-partial-settlement`, or `failed`. A missing outcome is
incomplete input and the configuration is rejected. It is not interpreted as
cash, default, or an indefinitely pending obligation. A failed or partially
failed warehouse amount consumes facility capacity; it cannot be redrawn
without an explicit reinstatement mechanism, which v0.1 does not have.

All call and warehouse requests issued in one month form one atomic policy
action. The engine tests both facility batches before changing either
contingent ledger. If either side fails, no request in that month's cross-
facility batch is accepted. Every accepted or rejected request remains a dated
result row with provider, facility, request, amount, and facility-source
lineage. Final outcomes form a second dated result ledger; a full settlement is
retained with the same precision as a partial or failed settlement. Record IDs
identify evidence but never establish intramonth economic priority.

Calls, draws, and protection releases follow a declared policy. Under the v0.1
synthetic information convention, all facts with economic month earlier than
the decision month are observed; current-month and future facts are not.
Scenarios with the same complete observed history must issue the same sorted
multiset of facility-and-amount actions. Thus one request for `4` is not the
same policy as requests for `1` and `3`, while record IDs alone do not change an
economic action. A schedule that changes because of an unobserved event uses
hindsight and is rejected. Version 0.1 evaluates the supplied policy; it does
not optimize call or draw timing or validate live `known-at` timestamps.

## 3. Funded protection

Callable capital is not protection merely because it can be called. Warehouse
capacity is not protection merely because it is committed. The following may
count as funded protection only after cash has settled:

- paid-in capital contractually subordinated to the protected exposure;
- cash collateral in a controlled account, unavailable for another purpose;
  or
- proceeds of a support obligation after those proceeds have actually been
  paid and irrevocably subordinated.

The following do **not** count: undrawn commitments, unfunded guarantees,
letters of intent, expected grants, forecast asset revenue, unissued takeout
securities, warehouse proceeds, asset cost basis, or a provider's credit
rating. An unfunded guarantee remains provider reliance, not funded cash.

Let `J*` be the declared aggregate protection target carried from the frozen
capital-stack structure: the detachment amount of the subordinate capital
intended to protect the identified senior funding. The bridge does not choose
`J*`. Let

```text
J_paid_s,t      = cumulative qualifying subordinated cash settled
J_released_s,t  = cumulative qualifying capital returned or released
J_absorbed_s,t  = cumulative loss or ineligible use absorbed by that capital

F_s,t = J_paid_s,t - J_released_s,t - J_absorbed_s,t
```

`F_s,t` is funded protection remaining. It is a cash-funding and contractual-
subordination measure, not the market value or fair value of an asset.

The engine then separates funded protection into cash and invested components:

```text
F_s,t       = J_cash_s,t + J_invested_s,t
K_s,t       = retained asset basis capable of supporting invested protection
J_invested_s,t <= K_s,t
```

`K` increases when an eligible asset use is paid. A principal-basis return or
disposition reduces `K`; any invested protection no longer supported by `K`
must be reclassified from actual retained project receipts into segregated
protection cash before any investor distribution. An eligibility removal
reduces warehouse-eligible basis `E` but does not by itself assert that the
asset disappeared. A writeoff reduces `K` and must absorb the same-month
first-loss amount `min(writeoff, J_invested)` through an explicitly linked
protection-absorption record. Any residual writeoff is reported separately as
not absorbed by protection. These controls prevent a returned, distributed, or
written-off asset from leaving behind protection that exists only as a memo.

Define `E_s,t` as retained eligible funded cost basis:

```text
E_s,t = prior eligible funded cost basis
      + new eligible A uses
      - principal cash applied to reduce that basis
      - documented dispositions, writeoffs, and eligibility removals.
```

No mark-up, terminal value, or forecast recovery enters `E`. When warehouse
cash actually settles, and again after every basis or protection reduction,
the bridge requires

```text
F_s,t >= min(J*, E_s,t).
```

Thus the first `J*` of eligible deployment must be supported by actual
subordinate capital before senior warehouse exposure is created. If protection
is released or absorbed, a later warehouse advance requires replacement cash.
This is the aggregate analogue of the fully funded stack's loss-support
boundary; it does not allocate a realized loss through the tranche waterfall.

In v0.1, `E` and every reduction are supplied declared-path assertions. The
engine bounds a declared cash-return reduction by same-month project cash and
reconciles every submitted movement, but it cannot prove that a borrower
omitted no receipt, disposition, writeoff, or eligibility change. Therefore
`E` is neither evidenced collateral nor an appraisal. A controlled-data
version must reconcile the complete asset subledger and servicer or controlled-
account records before `E` can support a live borrowing-base conclusion.

## 4. Warehouse borrowing base and provider exposure

For warehouse provider `w`, let

```text
H_w                    = committed facility limit
alpha_w                = contractual advance rate on eligible cost basis
W_s,t                  = warehouse principal outstanding after month t

BB_s,t = min(H_w,
             alpha_w * E_s,t,
             max(0, E_s,t - F_s,t)).
```

A draw request is a contingent dependency, not funded debt. At request time the
engine tests contractual availability and the prospective facility dependency
for the entire same-month warehouse request batch:

```text
W + prior called-unsettled + burned/defaulted capacity
  + sum(new same-month requests) <= H_w.
```

The request-stage output marks the protection and borrowing-base tests as not
applicable. When cash settles, actual `W_s,t` must satisfy both
`W_s,t <= BB_s,t` and the funded-protection test. The tests are repeated after
basis reductions and protection releases. This distinction permits notice
before a simultaneous asset closing without pretending that a notice is cash
or lender exposure. The last borrowing-base term prevents funded protection
from also being represented as warehouse-funded cost. A lower contractual
eligibility or provider limit may reduce `BB`; it may never increase it. Cost
basis is not collateral value, and passing this arithmetic is not an appraisal
or a lending decision.

Warehouse principal reconciles independently; v0.1 has no warehouse-principal
writeoff input:

```text
W_s,t = W_s,t-1
      + settled advances_s,t
      - principal repayments_s,t.
```

Version 0.1 does not infer a writeoff or recovery. If warehouse cash is unpaid,
it reports the outstanding amount and stops unless a separately modeled,
settled source pays it.

The warehouse provider's funded exposure is outstanding principal plus any
due and unpaid contractual amount. Its undrawn commitment is reported
separately as a contingent funding obligation. For each callable-capital
provider, settled contributions and the remaining callable commitment are
also separate. Summing them as one "exposure" would mix funded investment and
contingent obligation.

Provider concentration is reported by legal provider, including affiliates.
If the same economic source supplies callable capital, warehouse funding,
guarantees, or takeout support, those dependencies are aggregated for the
concentration memo. A favorable project path must not silently imply that the
provider performs; provider-performance states are separate scenario facts so
wrong-way dependence remains visible.

Two economic-group concentration measures remain deliberately separate:

- `cumulative_gross_settled_funding_source_hhi` groups all actual external
  settlements, including gross warehouse advances and refinancing cash. It is
  a provenance-of-funding flow measure and intentionally double-counts cash
  that later refinances earlier cash.
- `ending_economic_group_contingent_funding_dependency_hhi` groups only
  callable available plus called-unsettled amounts and the remaining warehouse
  contractual commitment at the measurement endpoint. Pending warehouse draws
  are included once, not added again to the commitment that already contains
  them.

Neither HHI is a default model, exposure-at-default calibration, covenant, or
claim that diversification has been achieved.

## 5. Monthly cash ordering

The bridge uses one conservative within-month order. Cash cannot be counted in
two roles.

1. Carry forward each disjoint cash bucket from the prior month.
2. Record every policy request and atomically test the same-month callable and
   warehouse batches before accepting either. A notice is not cash, and a
   rejected cross-facility batch leaves neither half committed.
3. Measure callable commitment fee and noncash reserve cost on the declared
   pre-settlement monthly basis. A called amount incurs reserve cost through
   its due settlement/default month even when notice availability has expired.
4. Book every same-month final callable and warehouse outcome as one batch,
   then add named cost support, protection replenishment, and takeout cash.
   Failed outcomes deliver zero cash but remain dated event rows; no first
   failing record can suppress a later outcome or supplement. Within this
   batch, settled callable cash first repays warehouse principal already
   outstanding before the warehouse-outcome substep; only its residual enters
   callable cash. It does not repay a new warehouse advance that settles later
   in the same substep. This is an explicit facility-priority convention, not
   priority created by a record ID.
5. Measure warehouse charges on the completed settlement batch and test
   provider capacity and the facility, protection, and borrowing-base
   conditions applicable to positive settled advances. A zero-cash failed
   draw has no funded-protection or borrowing-base test. If a settlement or
   capacity test fails, charges already crystallized on settled cash remain
   due, the whole step-6 cost batch is reported unpaid, and warehouse charges
   remain warehouse EAD.
6. Pay the noncapitalizable cost batch—`C`, `G`, commitment fees, warehouse
   interest, and warehouse fees—only from cost-support cash. The v0.1 batch is
   atomic: if support is insufficient, all uses in the batch are reported
   unpaid, cash remains in its bucket, and the smaller liquidity gap is
   reported separately.
7. Pay the month's `A` transaction from its permitted sources. This transaction
   is also atomic. Eligible-purpose cash cannot cure an ineligible-purpose
   restriction, and failure reports both the whole rejected transaction and
   the binding restricted-source gap.
8. Receive actual `P` and `N` cash only after the funding-use test. A same-month
   asset receipt cannot cure an earlier gap. Sweep those receipts to warehouse
   principal before any investor distribution.
9. Apply declared basis movements. Re-segregate any returned protection cash,
   recognize linked first-loss absorption, and retest `F` and `BB`.
10. Test all same-month protection releases as one all-or-none batch, then
    apply legal-maturity repayment, residual distributions, and close-of-month
    facility expiry. At maturity, unused warehouse proceeds, callable cash,
    and then segregated qualifying-protection cash repay senior warehouse
    principal. Protection cash used for senior repayment is reclassified from
    `J_cash` to supported `J_invested`; total funded protection is unchanged.
    The expiry month is the last notice/request month. Uncalled or undrawn
    capacity expires only after the final permissible action and only if no
    earlier step set `t*`; a failure snapshot precedes close-of-month expiry.

Version 0.1 has no reinvestment or replenishment. Asset receipts reduce
eligible basis or warehouse exposure as declared; they are not recycled into
new assets. A warehouse draw may not fund its own interest or fee. A different
commercial ordering requires a new version, because order changes exposure,
cost, and failure timing.

Takeout proceeds count only when settled and immediately repay warehouse
principal. At legal maturity, warehouse principal and due charges must be zero,
or the result is `warehouse-maturity-unpaid`. The failure record separately
shows the gross maturity obligation, cash applied in the maturity waterfall,
and the residual past-due amount; only the residual remains funded EAD. It is
not reclassified as loss or recovery. A future issuance, refinancing
expectation, post-maturity receipt, or modeled sale price is not a cure.

## 6. Funding failure and terminal semantics

At each month, define one binding permitted-purpose cash use as `X_s,t` and the
settled cash legally available for that purpose before asset receipts as
`B_s,t`. The immediate liquidity gap is

```text
shortfall_s,t = max(0, X_s,t - B_s,t).
```

For a restricted or atomic use, `shortfall` is the binding source-specific
gap—not the whole transaction. The result separately reports the complete
transaction rejected, the eligible-purpose cash still present, and the cash
buckets left untouched. Therefore, for an atomic failure, “unpaid use” can be
larger than “liquidity shortfall” without an accounting contradiction.

The first positive shortfall, protection deficit, borrowing-base breach,
unpaid warehouse amount, or missed terminal repayment sets `t*`, the first
funding-failure month. The result must retain:

- cash and provider ledgers through the last successful step;
- the due amount, available amount, and shortfall at `t*`;
- funded-protection and borrowing-base positions at `t*`;
- outstanding warehouse and callable-provider obligations; and
- the exact failure reason, all causal due-record lineages, and every relevant
  provider/facility/request/settlement shortfall. Multiple simultaneous
  provider shortfalls remain a vector; one provider is never overwritten by
  another or mislabeled as the callable provider.

Simultaneous legal-provider capacity breaches are also a sorted vector. A
single row may populate the scalar provider headline; multiple rows leave that
headline neutral and retain every provider source, used capacity, declared
capacity, and excess. Provider input order cannot select the reported cause.

Once an earlier phase sets `t*`, later scheduled phases are not labeled “due.”
Their due, paid, and unpaid fields remain zero. The narrow exception is the
step-5 settlement/capacity boundary above: charges contractually crystallized
on an already completed cash-settlement batch remain reported due and unpaid,
while the later asset-use phase remains unprocessed.

The scenario ledger retains every provider final outcome and every dated
settlement shortfall even when explicit later cash preserves feasibility. A
failure record copies all observed shortfalls through `t*` as disclosure
context. Version 0.1 does not promote an asset-use gap to
`provider-nonperformance`, even when a provider default occurs in the same
month. The gap remains `funding-use-shortfall`, while the provider default and
its dated lineage remain separately visible. A defensible but-for attribution
would require a counterfactual source-use ledger that reproduces restricted-cash
rules, senior warehouse-principal sweeps, and facility headroom. Temporal
coincidence or the gross unpaid provider amount is not sufficient.

It must **not** retain or manufacture any later successful project draw,
project receipt, takeout, distribution, cash multiple, return, or NPV. Once a
required use is not funded, the supplied later success path is no longer the
same economic path. A cure may be modeled only as an explicit, dated, settled
cash source in a new scenario.

A path with funding failure has no successful-path NPV. If any scenario
admitted to a probability set fails, expected cash, expected return, and
expected NPV for the complete bridge are unavailable; the engine must not
condition on surviving scenarios. It may report the model-implied probability
of funding failure when the probability set itself is admissible, but that is
not a loss calibration or an observed default rate.

Configuration incompleteness is different from an economic failure. An orphan,
duplicate, missing, wrongly dated, or post-maturity settlement outcome rejects
the input before scenario probabilities are evaluated. Only an explicit final
`failed` or `final-partial-settlement` outcome establishes modeled provider
nonperformance.

## 7. Path and summary outputs

For every scenario, v0.1 must report at least:

- `funding-feasible` or the first failure month and reason;
- every initial payment; every accepted or atomically rejected call and
  warehouse request; every full, partial, or failed final outcome; and every
  repayment, interest payment, fee, and takeout;
- eligible and ineligible uses paid, unpaid, and rejected;
- opening and closing controlled cash for every processed month;
- eligible funded cost basis and every eligibility reduction;
- funded protection split between segregated cash and invested basis, retained
  supporting-asset basis, required protection, headroom, reclassification,
  absorption, and first breach;
- warehouse borrowing base, headroom, outstanding principal, peak funded
  exposure, undrawn commitment, and terminal exposure;
- provider-by-provider gross settled funding, funded EAD, disjoint contingent
  obligation, burned/defaulted capacity, economic-group concentration, and
  failure state;
- residual cash and cost basis available at the bridge end date; and
- every implemented cash, warehouse-principal, warehouse-charge,
  eligible-basis, and funded-protection reconciliation residual.

Across the supplied admissible probability set, the summary reports bounded
ranges for funding-failure probability; expected funding shortfall; shortfall
expected shortfall at 95% and 99%; expected ending warehouse funded EAD; ending
warehouse funded-EAD expected shortfall at 95% and 99%; expected peak warehouse
funded EAD; and expected callable-provider defaulted amount. These are model-implied
ranges over supplied synthetic paths, not calibrated default, loss, or recovery
estimates.

For feasible paths with complete measured cash-flow boundaries, it may also
report:

- the NPV at one declared physical-measure hurdle of actual bridge funding
  cash flows;
- gross timing benefit relative to the fully funded month-zero `R` baseline;
- warehouse interest and fees, callable-capital costs, and total bridge cost;
  and
- net timing benefit after those explicit costs.

NPV is unavailable after any funding failure, after settled takeout (because
the replacement liability is outside the bridge), while any provider outcome
is unresolved, while warehouse EAD remains past due, or when a positive
callable commitment fee has no explicit recipient/claim allocation. The last
guard prevents one aggregate permanent-capital perspective from paying a fee
without also identifying whether its own member receives that fee. Callable
liquidity opportunity cost remains a separate noncash sensitivity, never cash
NPV or fair value.

The `8.983578` central prefunding-drag result is a baseline diagnostic from one
synthetic probability mix, not an amount available to pay providers and not a
forecast saving. A candidate bridge can reduce idle-cash timing cost while
having a worse result after interest, fees, provider failure, or takeout risk.

No v0.1 field is a tranche distribution, tranche loss, issue yield, expected
investor return, or price. A later version must explicitly connect successful
aggregate funding paths to the unchanged Capital Stack v0.2 waterfall and
prove that aggregate cash and source identities still reconcile.

## 8. Required reconciliations

For every processed month, including the failure month:

```text
current-month external cash inflows
  = month-zero initial qualifying protection cash
  + settled callable capital
  + settled warehouse advances
  + settled cost support
  + settled protection replenishment
  + actual asset receipts
  + settled takeout cash

opening disjoint cash-bucket balances
  + current-month external cash inflows

current-month cash uses
  = eligible asset uses
  + buyer-direct and pool costs
  + warehouse interest and fees
  + other declared bridge costs
  + warehouse principal repayments
  + protection releases and investor distributions

opening disjoint cash-bucket balances
  + current-month external cash inflows
  = current-month cash uses
  + every disjoint closing cash bucket
```

Transfers between named buckets and funded-protection components are internal
reclassifications, not new external cash sources.

Separately:

```text
ending warehouse principal
  = opening principal + settled advances - principal repayments

ending unpaid warehouse charges
  = opening unpaid charges + charges due - charges paid

total nonasset costs due
  = total nonasset costs paid + total nonasset costs unpaid

remaining funded protection
  = settled qualifying capital - releases - absorbed amount

remaining funded protection
  = segregated qualifying-protection cash + invested protection basis

invested protection basis
  <= retained protection-supporting asset basis

ending callable commitment
  = opening commitment - accepted requested amount
    - expired uncalled amount

warehouse available capacity while the request window remains open
  = facility limit - funded principal - called-unsettled amount
    - burned/defaulted capacity
```

Asset receipts remain exactly the imported `P+N` cash. Funding changes their
financing and priority before handoff; it cannot increase them. `L`, `O`, and
contractual principal remain asset-ledger facts, while warehouse exposure is a
separate financing ledger. None may be netted into another merely to make a
reconciliation pass.

## 9. Risk and return interpretation

Replacing month-zero prefunding changes risk; it does not remove it. The
economic exchange is:

```text
less idle paid-in cash
  in exchange for
call-timing risk + provider-credit risk + warehouse interest and fees
+ eligibility and borrowing-base risk + maturity and takeout risk
+ asset encumbrance and provider concentration.
```

Warehouse seniority may protect the provider while reducing cash available to
later capital. Callable investors retain a contingent obligation before cash
is called; after settlement, their paid-in capital is at risk under its actual
subordination. Neither commitment size nor provider reputation establishes
loss capacity.

Cash NPV uses the aggregate permanent-capital perspective: funded-at-close
capital, settled calls, cost support, and protection replenishment are negative
cash flows; permitted investor distributions and protection releases are
positive cash flows. Warehouse cash is temporary external debt, not investor
contribution. NPV at the declared physical-measure hurdle is only a timing
sensitivity under supplied paths. It is not fair value. A modeled probability-
weighted return is a model output, not a market return. Provider interest and
fees are contractual cash terms, not proof that the provider receives an
adequate risk-adjusted return. A callable fee without an allocated recipient
suppresses NPV rather than silently mixing entity perspectives.

### Institutional design analogies

The treatment is deliberately recognizable without claiming that these rules
govern a future vehicle. The Bank for International Settlements describes
subscription credit lines as loans collateralized by investor capital that is
committed but not yet disbursed. That distinction supports reporting undrawn
capital as contingent support rather than settled cash
([BIS private-markets primer](https://www.bis.org/publications/private-markets-primer)).

ILPA's 2020 subscription-line guidance recommends transparency around facility
size and balance, draw duration, rates, fees, purpose, and return calculations
with and without the facility. The bridge therefore preserves both its fully
funded baseline and its after-cost funding result
([ILPA guidance](https://ilpa.org/wp-content/uploads/2020/06/ILPA-Guidance-on-Disclosures-Related-to-Subscription-Lines-of-Credit_2020_FINAL.pdf)).

The Basel liquidity framework treats committed facilities as future funding
obligations and prohibits counting the same item twice. Version 0.1 borrows
those two control ideas for reconciliation only
([Basel Framework LCR40](https://www.bis.org/committees/bcbs/basel-framework/standard/lcr/40/inforce/2019-12-15/published/2019-12-15)).
These are design analogies, not assertions of ILPA conformity, bank status,
Basel applicability, or regulatory compliance.

## 10. Evidence and live-use boundary

Version 0.1 rejects `synthetic_inputs=false`. Provider evidence booleans and
record identifiers are retained as provenance fields, but they cannot establish
facility enforceability, provider capacity, or the date on which a fact became
known. Accepting those fields as “live” would overstate the evidence gate. A
later controlled/live version requires retained, dated, independently
checkable evidence for:

- executed callable-capital and warehouse contracts, including conditions
  precedent, limits, notice, expiry, priority, fees, maturity, borrowing base,
  collateral control, and termination rights;
- the legal identity and economic affiliation of every provider;
- actual provider funding capacity, cash collateral, and settled payments;
- asset eligibility, dated uses, receipts, writeoffs, and cost-basis changes;
- controlled-account and facility statements that reconcile to the ledger;
  and
- any terminal takeout cash, as an executed and settled transaction rather
  than a forecast.

Three empirical objects must remain separate:

| Object | Minimum meaning | Prohibited substitution |
|---|---|---|
| Observed price | Dated price from an executed arm's-length issuance or trade, with size and terms | A model NPV, indicative quote, target par amount, or hurdle |
| Realized return | Ex-post return calculated from retained dated contributions and distributions through a stated measurement date | Expected cash, unresolved exposure, forecast recovery, or an observed price alone |
| Model expected return | Probability-weighted modeled cash result under a named physical-measure dataset and version | Observed market return, realized performance, fair value, or investor-required return |

A model may compare these objects only after each is independently admitted and
labeled. It may not use one to authenticate another. A live observed warehouse
spread does not calibrate project default or recovery; realized cash on one
facility does not establish an expected return for a new pool; and a modeled
expected return does not establish an issue price.

Until those contracts, provider evidence, asset records, `known-at` timestamps,
and independent checks pass a future evidence gate, every v0.1 output remains
synthetic research. A mechanically feasible path is not authorized for an
offering, investor solicitation, or capital decision.

## 11. Explicit exclusions and falsification

Version 0.1 does not establish or calculate:

```text
replacement or amendment of the Capital Stack v0.2 waterfall       false
tranche cash, loss, expected return, or suitability                  false
issue price, fair value, spread, rating, or risk-neutral value       false
empirical PD, LGD, recovery, dependence, or return calibration       false
warehouse collateral value or an appraisal                          false
provider solvency, legal enforceability, or guaranteed performance  false
security perfection, bankruptcy remoteness, or insolvency recovery  false
tax, accounting, securities-law, banking, or regulatory treatment   false
regulatory capital, liquidity coverage, or risk retention            false
hedging, derivatives, FX, interest-rate, or basis-risk treatment     false
market demand, secondary liquidity, crowding-in, or impact achieved  false
```

The bridge is falsified as a funding-feasible candidate when any due use is
unpaid; protection is only promised rather than settled; warehouse exposure
exceeds its borrowing base; a fee is financed by circular self-drawing; a
provider is silently substituted; a failed draw silently restores capacity;
returned asset cash is distributed while its protection remains only as memo
basis; a writeoff lacks its same-month first-loss allocation; the schedule uses
future information; a mandatory final provider outcome is absent; warehouse
cash settles after legal maturity; takeout is forecast rather than settled;
provider concentration is hidden or double-counted; post-shortfall success cash
is retained; failed scenarios are removed from an expected-return calculation;
or bridge funding changes an imported asset cash path.

The executable boundary remains intentionally narrow: one callable facility,
one warehouse facility, fixed lags and final outcomes, supplied joint provider
states without PD/LGD/recovery calibration, static legal-provider capacities,
no affiliate capacity pool, no intramonth information, no takeout-liability
cash flows, no provider-specific return allocation, and no legal-enforceability
opinion. Eligible-basis completeness is supplied rather than independently
established. Concentration is disclosure, not a covenant; reserve opportunity
cost is sensitivity, not price.

The honest outcome may be that full prefunding remains cheaper or safer. The
purpose of the bridge is not to force a callable-capital or warehouse solution;
it is to state exactly when less idle cash has been purchased with acceptable,
visible, and fully reconciled funding risk.

## 12. Executable and verification boundary

The normative C++20 model contract is
[`funding_bridge.hpp`](../include/naturalehia/cellular_finance/funding_bridge.hpp),
the public closed-parser contract is
[`funding_bridge_config.hpp`](../include/naturalehia/cellular_finance/funding_bridge_config.hpp),
the evaluator is
[`funding_bridge.cpp`](../src/funding_bridge.cpp), and the closed text grammar
is implemented by
[`funding_bridge_config.cpp`](../src/funding_bridge_config.cpp). The focused
adversarial suites are
[`funding_bridge_tests.cpp`](../tests/funding_bridge_tests.cpp) and
[`funding_bridge_config_tests.cpp`](../tests/funding_bridge_config_tests.cpp).

On 2026-09-01 the complete project built with C++ warnings as errors and passed
`75/75` tests under `emscripten/emsdk:6.0.5`; the funding-bridge evaluator and
parser were compiled to WebAssembly/JavaScript and executed with Node. No native
Windows compilation was used. The adversarial set includes missing and partial
provider outcomes, warehouse-only default lineage, same-history policy
partitioning, cross-facility all-or-none request batches, full outcome and
cross-month shortfall ledgers, aggregate settlement lineages, facility expiry
and maturity, burned capacity, phase-closed due/paid/unpaid fields, simultaneous
provider-capacity breaches, source-restricted atomic failures, warehouse charge
reconciliation, returned-protection custody, atomic protection releases,
same-month first-loss absorption, affiliated-provider HHI, exact frictionless
prefunding-drag parity, takeout suppression, callable-fee suppression, invalid
in-memory enums, and row permutation invariance.

That record verifies implementation consistency, not empirical truth. Version
0.1 does not yet publish a checked file-backed bridge transaction, browser
adapter, or numerical candidate result. Those are the next release gates; until
then the white paper correctly treats callable capital and warehousing as a
proposed extension rather than evidence that a particular structure is
investable.
