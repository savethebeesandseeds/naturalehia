# Project Financial Interface v0.1

## Purpose

This document defines the common financial representation that
cellular-agriculture projects should expose to the instrument and portfolio
models in this repository. It is a financial interface, not an operating,
governance, diligence, legal, or regulatory standard.

Version 0.1 is the broader design specification. Its core investor-outlay,
investor-receipt, external-source-budget, explicit contractual-principal,
resolution, recovery, joint-scenario, and pool-risk subset is now implemented
in the Portfolio v0.2 schema and C++ engine, with strict reload compatibility
for legacy Portfolio v0.1 inputs. The current implementation boundary still
labels complete Portfolio configurations synthetic because hurdles, source
budgets, factor tags, and multi-project coupling are declared inputs. The
upstream
[Project Claim Ledger v0.1](PROJECT_CLAIM_LEDGER_V0_1.md) now normalizes one
executed claim's settlement cash, contractual balances, cash rights,
conversion, guarantee, covenant states, and ex-ante/backtest boundary before a
loss, return, pricing, or portfolio adapter may consume it. The implemented
[Claim-Ledger Portfolio Adapter v0.2](CLAIM_LEDGER_PORTFOLIO_ADAPTER_V0_2.md)
is the standard package-to-Portfolio project hook. State-transition provenance,
calibrated inputs, and several richer payoff fields remain design targets.
Values may be observed, contractually fixed, estimated, or synthetic, but their
status must never be confused.

## Design objective

Every project should be expressible as five linked objects:

```text
Project_i = (Capital_i, States_i, ExternalCash_i, Recovery_i, Factors_i)
```

The interface standardizes financial meaning and measurement. It does not
standardize the underlying science. A pool should seek heterogeneous technical
and commercial risks represented through homogeneous financial conventions.

## 1. Analysis basis

| Field | Meaning |
|---|---|
| `project_id` | Stable ASCII identifier for the financed exposure |
| `financed_risk_unit` | Research program, process/platform, facility, product-market exposure, or explicit combination being financed |
| `development_stage` | `research`, `pilot`, `demonstration`, `first-industrial`, or `repeat-production` |
| `time_zero` | Financial close or analysis origin |
| `period_months` | Common time step; monthly is the target convention |
| `horizon_months` | Final modeled month, including recovery and exit |
| `currency` | Currency of every nominal cash flow |
| `monetary_basis` | Nominal or constant-price basis and base date |
| `scenario_status` | `observed-calibrated`, `estimated`, or `synthetic` |
| `input_status` | Status applied to each material capital, transition, payoff, recovery, and factor input |
| `analysis_mode` | Contract mechanics, deterministic stress, physical-probability expected return, or market-consistent valuation |
| `probability_basis` | Source and meaning of scenario or transition weights |
| `discount_basis` | Hurdle, funding rate, or pricing-kernel convention used for value outputs |

Cash flows with different currencies or monetary bases must be converted by an
explicit external rule before aggregation. A probability reported from
synthetic scenario weights remains a sensitivity, not an empirical forecast.

## 2. Capital schedule

For project `i`, scenario `omega`, and month `t`:

| Symbol | Meaning |
|---|---|
| `N_i,t(omega)` | Underlying capital need in the current project state |
| `K_i,t` | Maximum capital contractually available at month `t` |
| `D_i,t(omega)` | Capital drawn at month `t` |
| `C_i,t(omega)` | Cumulative capital contributed through month `t` |
| `B_i,t(omega)` | Contractual principal outstanding after cash, conversion, and writeoff |
| `E_i,t(omega)` | Investor cash cost basis still economically at risk; distinct from contractual principal |
| `U_i,t(omega)` | Committed but undrawn availability |
| `I_i,t(omega)` | Irreversible spend with no continuing or recovery value |
| `X_i,t(omega)` | Required external liquidity or unfunded capital call |

Required identities are:

```text
0 <= D_i,t(omega) <= min(eligible capital need, available commitment)
C_i,t(omega) = C_i,t-1(omega) + D_i,t(omega)
U_i,t(omega) = opening availability - draws - cancellations
```

Capital committed, capital drawn, capital economically at risk, and capital
required in a stress are separate quantities. Irreversible spend must not be
treated as recoverable exposure. A pooled instrument must be able to fund
simultaneous draws rather than relying only on average utilization.

For a purchased or otherwise normalized claim, cash outlay and contractual
principal have separate identities:

```text
investor cash = receipts - purchase/funding outlays - direct costs

opening contractual principal + principal additions
    = principal cash + conversion + writeoff + closing principal.
```

Purchase premium, discount, direct cost, accrued yield, and principal are not
interchangeable. Paying above par can reduce return without creating principal
loss; buying below par does not reduce a contractual writeoff.

## 3. Project states and instrument states

Each scenario first supplies a time-ordered **underlying project state** path,
such as research, validated result, pilot, demonstration, construction, ramp,
operating production, technical failure, commercial failure, or exit.

An instrument then maps that project path into a separate **instrument state**:
undrawn, funded, accruing, cash-paying, impaired, recovered, converted, repaid,
or written off. A scientific setback is not automatically a payment default,
and a payment default does not rewrite the underlying scientific result.

In compact form, for project `i` and instrument `j`:

```text
Instrument cash flow_j,i,t(omega)
    = payoff_function_j(project state_i,t(omega),
                        project cash flow_i,t(omega),
                        instrument terms_j)
```

Projects may use stage-specific intermediate states, but every path must also
resolve into common financial outcomes:

- `continuing`: further capital and future success remain possible;
- `successful`: the defined success payoff becomes available;
- `failed`: future development capital stops and recovery begins; or
- `exited`: the exposure is repaid, sold, converted, or otherwise terminated.

Each transition records:

| Field | Meaning |
|---|---|
| `project_state_from`, `project_state_to` | Underlying development or production transition |
| `instrument_state_from`, `instrument_state_to` | Resulting claim or payment-state transition |
| `transition_month` | When exposure and payoff rights change |
| `capital_released` | Incremental draw permitted by the transition |
| `cash_payoff_enabled` | Contractual payoff rights activated by the transition |
| `remaining_commitment` | Availability after draw or cancellation |
| `scenario_weight` | Physical-measure weight, with provenance and status |

The finance model consumes the transition. It does not declare that a
scientific or operating milestone has been achieved.

## 4. Underlying external cash generation

The project record separates cash generated outside the financing structure
from capital supplied by investors. For each state-contingent external cash
flow, it records the month, source, amount or formula, project state, currency,
duration, ownership fraction, and any cap.

External sources may include product or capacity revenue, licensing milestones,
royalties, strategic acquisition proceeds, asset sales, and explicit insurance,
guarantee, public, philanthropic, or buyer support. Required project outflows
include capital expenditure, operating costs, taxes, working capital, and
workout costs.

```text
Underlying project cash flow_i,t
    = external commercial receipts_i,t
      + explicit external support_i,t
      - capital expenditure_i,t
      - operating costs_i,t
      - taxes_i,t
      - working-capital increase_i,t
      - required workout costs_i,t
```

Issuance proceeds, investor contributions, and PIK are financing entries, not
new underlying value. Refinancing can supply repayment liquidity, but its
ultimate economic support must still be traced to project cash, assets, or an
explicit outside provider.

## 5. Instrument attachment and investor cash flows

An instrument attaches to the underlying project through explicit terms:

- commitment and draw rule;
- fixed cash or accrued-return leg;
- revenue, royalty, equity, or success-participation leg;
- maturity, cancellation, conversion, and claim cap;
- priority, collateral, and recourse;
- recovery allocation; and
- counterparty-performance state.

Every investor receipt belongs to one explicit payoff leg:

| Leg | Symbol | Examples |
|---|---|---|
| Fixed cash income | `F_i,t` | Cash interest, commitment fee, preferred cash return |
| Principal or capital return | `Q_i,t` | Amortization, redemption, refinancing repayment |
| Revenue participation | `R_i,t` | Percentage of defined project or product revenue |
| Royalty or licensing participation | `Y_i,t` | License milestone or royalty receipt |
| Equity-linked success value | `G_i,t` | Conversion value, warrant value, exit participation |
| Demand or risk-transfer settlement | `H_i,t` | Offtake support, price floor, completion support |
| Residual proceeds | `V_i,t` | Asset sale, terminal project value available as cash |

The investor cash flow before funding outflows is:

```text
Receipts_i,t = F_i,t + Q_i,t + R_i,t + Y_i,t + G_i,t + H_i,t + V_i,t
```

For project `i`, instrument `j`, and terms `theta_j`:

```text
CF_j,i,t(omega)
    = payoff_function_j(project states_i,0:t(omega),
                        underlying project cash flows_i,0:t(omega),
                        recovery_i(omega),
                        common factors_0:t(omega),
                        theta_j)
```

The record must distinguish cash from accrued or payment-in-kind claims. A
claim is not a cash receipt until an identified payer transfers cash. The same
revenue, exit value, guarantee, or recovery cannot support two payoff legs
without an explicit priority or sharing rule.

An identified claim enters the common implementation through a verified
`ClaimLedgerPackage` and the
[Claim-Ledger Portfolio Adapter v0.2](CLAIM_LEDGER_PORTFOLIO_ADAPTER_V0_2.md).
The adapter requires separately declared finite external cash budgets for every
positive claim receipt. Documentary `source_record_id`, economic
`cash_source_id`, counterparty identity, and guarantee-provider identity remain
separate fields. The adapter preserves decision-cut entry lineage and never
uses the retained full/backtest path. It retains the accepted raw physical
weights, normalizes them once to the unit-sum Portfolio measure, and uses that
same measure for every expectation and reconciliation.

Stage affects plausible return sources:

- research normally requires success participation because it has no stable
  debt-service cash flow;
- pilot and demonstration exposure may combine staged preferred capital with
  commercialization participation;
- first-industrial exposure may add repayment from facility cash flow,
  refinancing, or contracted demand; and
- repeat production may support conventional debt-like cash flows when
  operating and demand evidence are sufficiently repeatable.

## 6. Failure and recovery

For each failed scenario, the interface records:

| Field | Meaning |
|---|---|
| `failure_month` | First terminal financial failure month |
| `failure_type` | Scientific, scale-up, completion, operating, market, counterparty, refinancing, or other explicit category |
| `exposure_at_failure` | Principal exposure immediately before recovery: unreturned draw-principal in legacy mode or contractual principal balance in explicit-ledger mode |
| `gross_recovery_cash` | Cash realized before workout and realization costs |
| `recovery_cost` | Workout, sale, enforcement, shutdown, and other realization cash |
| `recovery_month` | Month net recovery becomes distributable |
| `investor_recovery_cash` | Cash allocated to the modeled investor claim |
| `investor_recovery_principal` | The part of investor recovery cash that legally retires principal |
| `unfunded_commitment_cancelled` | Capital no longer drawable after failure |

```text
Net recovery cash = max(0, gross recovery cash - recovery cost)

Legacy principal loss
    = resolved unreturned draw-principal after principal recovery cash

Explicit-ledger principal loss
    = contractual principal writeoff posted after principal cash and conversion
```

Purchase price, buyer direct cost, unpaid interest or fees, negative NPV, and
other investor cash impairment remain outside both principal-loss definitions.
Total recovery cash cannot retire more principal than its declared principal
component.

If different instruments have different priorities, their allocated recoveries
must sum to no more than net recovery cash.

## 7. Common-factor exposure

Project count is not diversification. Each project records its direction and
material sensitivity to a compact common-factor set:

- core biology or process platform;
- media, growth factors, gases, utilities, and other critical inputs;
- bioreactor, downstream, and equipment platform;
- construction cost and schedule conditions;
- energy price and availability;
- product approval and facility authorization;
- buyer, product category, demand, and realized price;
- refinancing and capital-market access; and
- geography and macroeconomic conditions.

The initial interface may use categorical exposure (`none`, `low`, `medium`,
`high`) plus explicit shared identifiers. A calibrated implementation may later
replace or supplement these with factor loadings, conditional transition
probabilities, or joint scenarios. Independence must never be the silent
default.

## 8. Standard standalone metrics

For one selected instrument, suppressing the instrument index `j`, scenario
weights `p_omega`, and investor net cash flow `CF_i,t(omega)`:

```text
CF_i,t(omega) = Receipts_i,t(omega) - funding outflows_i,t(omega)

Expected loss_i = sum_omega p_omega * Principal loss_i(omega)

NPV_i(h) = sum_omega p_omega
             * sum_t CF_i,t(omega) / (1 + h)^(t/12)
```

`h` is a declared physical-measure hurdle sensitivity. This NPV is not fair
value or risk-neutral pricing.

Every project report should include:

- total commitment, expected draw, maximum draw, and peak liquidity need;
- probability of any funded exposure and probability of terminal failure;
- expected principal loss, conditional loss severity, and recovery timing;
- cash yield, distribution of IRR where defined, and distribution of cash
  multiple on invested capital;
- NPV at explicitly stated hurdle rates;
- loss p95/p99 and expected shortfall at 95%/99%;
- expected duration and extension stress; and
- common-factor and concentration exposures.

Debt-like claims report defined default probability, exposure at default, and
loss given default. For equity and success-participation claims, default may be
the wrong concept; report probability of capital impairment, expected capital
shortfall, and probability of negative NPV instead.

For a simplified binary research exposure with present-value investment `I`,
success probability `p`, winner payoff `W`, and failure recovery `R_f`, the
break-even winner payoff before any additional risk premium is:

```text
W_break_even = (I - (1 - p) * R_f) / p
```

This makes the essential early-stage question explicit: whether realistically
obtainable rights in successful projects can compensate for failures and time.

Expected IRR is not the IRR of expected cash flows. Both may be reported only
when clearly distinguished, and paths with no mathematically defined IRR must
remain visible.

## 9. Pool representation and diversification

For project weights or ownership shares `a_i`:

```text
CF_pool,t(omega) = sum_i a_i * CF_i,t(omega) - pool_costs_t(omega)
Loss_pool(omega) = sum_i a_i * Principal loss_i(omega)
```

Projects must be evaluated on joint scenarios or an explicit dependence model.
Marginal standalone simulations cannot be combined by assuming independence.

A transparent tail-diversification diagnostic is:

```text
Diversification benefit_alpha
    = sum_i a_i * ES_alpha(Loss_i) - ES_alpha(Loss_pool)

Diversification ratio_alpha
    = 1 - ES_alpha(Loss_pool)
          / sum_i a_i * ES_alpha(Loss_i)
```

The pool should also report expected loss, loss p95/p99, ES95/ES99, maximum
single-project exposure, factor concentrations, simultaneous draw need,
expected cash return, NPV, cash-multiple distribution, and impairment
probability. It should also attribute marginal tail loss by project and common
factor. Expected loss is not reduced merely because assets are pooled.

An untranched participation claim is the first pool object to test. Senior and
junior claims should be introduced only after the underlying pool reconciles
and demonstrates an economic reason for different loss-bearing positions.
Different stages may enter one explicitly risk-bearing participation pool only
when their payoff and factor differences remain visible. A debt-like senior
pool should not treat research, pilot, first-industrial, and seasoned operating
claims as interchangeable assets.

## 10. Instrument and pool invariants

1. Every project and instrument comparison uses the same underlying physical
   state path unless a causal financing effect is explicitly modeled.
2. Every cash receipt has a named economic source; PIK is never reported as
   cash.
3. Draws never exceed available commitment and cancellations never reappear.
4. Principal loss is mode-specific: resolved unreturned draw-principal in
   legacy mode and exact contractual writeoff in explicit-ledger mode. Investor
   cash impairment, negative NPV, and unpaid interest or fees remain separate.
5. All allocated project payoffs reconcile to available project and external
   cash without double counting.
6. Pool cash flows equal underlying cash flows less explicit costs and cannot
   be improved by dropping failed projects.
7. Losses allocated across investor layers reconcile exactly to pool loss;
   tranching changes priority, not aggregate value.
8. Common-factor exposure remains explicit in base, sensitivity, and stress
   results.
9. Observed, estimated, and synthetic inputs remain distinguishable in every
   aggregate output.
10. A reported instrument value states its measure, probability basis, hurdle
    or discount convention, horizon, currency, and monetary basis.
11. Revenue, royalty, collateral, and asset rights allocated across claims do
    not exceed 100% unless an explicit offsetting obligation is recorded.
12. A numerical correlation matrix is symmetric and positive semidefinite;
    different project names never imply independence.
13. Explicit subsidy, guarantee value, and other outside support remain
    separately visible from underlying project economics and investor return.
14. A participation-rate solver scales only a receipt leg explicitly marked as
    contractually scalable; unused project cash capacity is never inferred to
    be an investor right.
15. A claim-derived Portfolio exposure names one loader-verified package root,
    one frozen decision cut, one claim, one project mapping, and one defined
    economic-cluster boundary; later backtest state cannot rewrite it.
16. Every claim receipt consumes a separately declared external source budget.
    Documentary provenance, economic source, counterparty, and provider identity
    cannot substitute for one another. Common and scenario scope are typed
    separately, and a provider-bound budget can fund only its exact guarantee
    receipt.
17. Investor cash and contractual principal reconcile independently. Purchase
    price and direct cost affect cash return and NPV; only contractual writeoff
    is principal loss in explicit-contractual-ledger mode.
18. A boundary that normalizes an accepted probability measure retains the raw
    weights, performs the normalization once, and uses the resulting measure
    consistently for every downstream expectation.

## 11. Real structuring value

Internal transfers among the project, pool, and investors cancel when the
structure is consolidated. A useful financial arrangement must create or
preserve real economic value:

```text
Real structuring value
    = value of the underlying activity when financed
      - value of the underlying activity without the financing
      - incremental transaction, liquidity, and servicing costs
```

Real value may arise by funding a positive-value project that otherwise cannot
cross a development stage, limiting wasted follow-on capital, reducing
distress costs, producing useful information, or matching a risk with capital
that can bear it. Pooling, refinancing, and tranching alone do not manufacture
cash or value.

## 12. Relation to current implementation

The current participation-pool engine implements the common dated investor
outlay, source-budgeted receipt, explicit contractual-principal, legacy
draw-equals-principal, resolved-loss, continuing-exposure, explicit joint
scenario, pool-cost, tail-risk, and optional loss-layer subset. Portfolio v0.2
separates claim purchase price and buyer direct cost from funded principal,
capitalized principal, conversion, and writeoff. Its strict reloadable format
and CLI remain backward compatible with Portfolio v0.1 inputs. This is not yet
the complete interface described above.

The implemented
[Claim-Ledger Portfolio Adapter v0.2](CLAIM_LEDGER_PORTFOLIO_ADAPTER_V0_2.md)
is the standard project hook for an identified claim. It reloads and
hash-compares the package root, admits only synthetic-complete mechanics or an
expected-return-admissible controlled candidate, requires a defined economic
cluster, and consumes only the frozen decision core. It maps buyer price and
direct cost to investor cash, maps contractual additions, principal cash,
conversion, and writeoff to an independent principal ledger, and requires
external budgets for every receipt. Package, scenario, entry, documentary
source, cash-source, declared budget scope, counterparty, and provider lineage
remain explicit. Common budget scope cannot collide with a literal scenario
identifier, and provider-bound budgets match guarantee receipts in both
directions. Accepted Claim Ledger weights and normalized Portfolio weights are
both retained; all Portfolio expectations use the latter consistently.

This separation prevents an above-par purchase from becoming a false principal
loss. It does not provide cost-basis protection: loss layers and proportional
principal protection attach to contractual principal, while purchase premium,
direct cost, and negative NPV remain investor cash economics. The adapter is
monthly only, requires zero opening accrued interest, maps resolved claims only,
and leaves hurdle, budgets, factor tags, and multi-project coupling as declared
assumptions.

The implemented
[Claim-Ledger Joint-Portfolio Assembler v0.1](CLAIM_LEDGER_JOINT_PORTFOLIO_ASSEMBLER_V0_1.md)
now supplies that multi-project bridge. It re-verifies two or more package
roots, requires a common currency, basis, calendar, decision cut, horizon and
hurdle, rejects duplicate clusters and cross-asset source IDs, and accepts only
complete explicitly weighted joint states. For every project and marginal
state, the sum of evaluated joint weights selecting that state must reproduce
the one-claim normalized probability. Pooling can therefore change dependence,
tail loss and liquidity without changing a project's expected loss, receipts,
or NPV. The coupling remains synthetic until a complete cohort and method
ledger supports it.

The companion probability-envelope engine now implements exact bounds around
the fixed joint-scenario measure for cash, exposure, loss, NPV, liquidity, and
tail severity. It keeps an endpoint witness for every scalar result and does
not relabel bounded synthetic weights as empirical calibration.

The success-participation term engine now uses that envelope to solve a genuine
contract variable. It preserves every draw, cost, principal classification,
loss, source budget, and scenario; `q=0` switches off only selected
non-principal participation and `q=1` reconstructs the configured portfolio.
An interior answer is a certified failing/feasible bracket. If `q=1` is still
below the robust target, the interface reports economic insufficiency rather
than inventing a larger right.

The pooled principal-loss protection engine then fixes one `q` and places a
separate proportional claim over final resolved pool loss. It does not insert
cash into a project, change principal classification, cover continuing
exposure, or reduce gross loss. The monetary cap becomes a maximum percentage
of aggregate contractual reference principal, so a finite modeled scenario
table cannot understate contractual notional. Legacy draw-equals-principal
projects use commitment; explicit-ledger projects use their declared principal
limit. Acquisition premium and buyer direct cost are never protected principal.
The engine reprojects complete investor NPV, provider payout expectation and
payout ES, and compares investor premium headroom with a claim-only provider
floor. Provider performance is assumed; credit, collateral, funding cost,
enforceability and market pricing remain outside the implemented interface.

The fully funded capital-stack engine fixes one `q`, subscribes aggregate
commitment at par into a zero-yield reserve, and applies contiguous first-loss,
intermediate, and senior claims. It separately conserves actual project
principal, unused-reserve return, non-principal cash, cost calls, realized
loss, and continuing exposure. Its robust expected return, tail loss,
exhaustion, and common-witness weighted-average life are physical-scenario
outputs; it does not estimate a spread, fair value, rating, or legal priority.
Capital-stack v0.1 accepts only the legacy draw-equals-principal, fully funded
at-par convention. It rejects explicit-contractual-ledger portfolios, so
Claim-Ledger Portfolio Adapter output cannot enter that stack until subscription
cash, reserve cash, acquisition cost, and asset principal are represented as
separate accounts.

The annual facility engine supplies aggregate operating and financing results
for one synthetic production facility. The milestone-gated capital engine
supplies detailed staged draw, commitment, claim, failure, and recovery paths
for one facility. Its first implemented adapter translates the actual provider
paths into the common interface, preserves their weights and cash timing, and
requires explicit source allocations for completion repayments. The annual and
remaining stage-specific models must still expose and translate pathwise cash
without discarding dependence or inventing terminal value.
Only after calibrated untranched pool economics reconcile should the project
estimate market prices for investor layers or optimize a capital structure.
