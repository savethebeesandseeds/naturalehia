# Robust Issue-Price Support Term v0.1

## Status and purpose

Status: implemented and synthetically verified financial-engineering term.
Nothing in this document is a market quote, valuation opinion, offer, or
recommendation.

This term begins only after the underlying project pool, success-cash share,
funded first loss, principal waterfall, and non-principal priority cap have
been fixed. It asks:

> At each separately supplied investor hurdle, what is the highest issue
> price the fixed market claim can support across the declared physical
> probability set, and how much explicit issue-price support is required for
> the project still to receive the claim's full funded principal?

The answer is a two-sided price window, not a point estimate of fair value.
The investor boundary comes from the fixed claim's dated cash flows and a
supplied hurdle. The issuer boundary comes from the funded principal that must
enter the subscription reserve and a separately declared, non-repayable
support capacity. If the boundaries do not overlap, the result is a measured
financing gap.

## Why price is now a separate layer

The implemented priority-cap term establishes which existing pool cash belongs
to the market claim. It deliberately holds the claim at par and uses physical
probabilities. That is enough to test cash allocation and risk, but not enough
to say what an investor would pay.

Contractual principal, expected cash, issue price, investment value, and fair
value are different quantities. The International Valuation Standards describe
an income approach as converting future cash flow to one current amount and
distinguish market value from value to a particular owner; their market
approach depends on price information for identical or comparable assets
([IVSC glossary](https://ivsc.org/standards-glossary/)). IFRS 13 defines fair
value from an orderly market-participant exit-price perspective, including
market-participant assumptions about risk
([IFRS Foundation](https://www.ifrs.org/issued-standards/list-of-standards/ifrs-13-fair-value-measurement/)).

This v0.1 term does not satisfy either standard and does not claim to. It uses
a flat annual effective hurdle supplied from outside the physical project
model. Until comparable transactions, bids, offers, or another defensible
pricing calibration exist, the output is an investment-return sensitivity.

## Fixed claim and three cash providers

Let:

```text
K = fixed aggregate project commitment
A = fixed funded junior first-loss principal
M = K - A = fixed market claim principal notional
B = fixed market lifetime non-principal priority cap
P = issue price paid by the market investor for principal notional M
F = explicit issuer costs paid from gross issue funding at month zero
C = buyer-direct costs paid in addition to P at month zero
G = maximum declared non-repayable issue-price support capacity
S_obs = evidenced support cash settled to this issue, if any
S_req(P) = M + F - P = modeled support required at price P
h = supplied annual effective investor hurdle
```

The fully funded stack still places `K` in its subscription reserve at month
zero. The junior investor still funds `A`. For the market layer:

```text
market investor issue payment + modeled required support = principal + issuer costs
P + S_req(P) = M + F
0 <= S_req(P) <= G for an arithmetically fully funded candidate

amount placed in subscription reserve = M
issuer costs paid and separately reported = F
```

The market investor continues to pay its separately modeled share of pool
costs. Issue support funds only the gap between gross buyer cash and the two
declared uses `M+F`. It does not pay future pool costs, cover project losses,
create project receipts, or change the market claim's notional, priority, or
distributions.

Version 0.1 permits `0 <= P <= M+F`, so gross proceeds cannot exceed the two
declared uses. Buyer-direct cost `C` never enters the reserve. Support is
non-repayable. If a provider receives repayment, participation, security, or
recovery rights, those rights are a separate claim and must enter the complete
waterfall rather than being called support here.

For the provider, an actual support draw is an upfront expenditure, not a
probabilistic claim loss or an investment whose modeled return is hidden
elsewhere. Before settlement, provider non-performance is counterparty risk;
after settlement, the paid amount is explicit concessionary capital.

## Separate observation and decision inputs

Price observation and investor hurdle are separate inputs. The engine must
never infer one from the other.

Version 0.1 accepts one reference-price record per run. Its status is one of:

```text
internal_candidate
nonbinding_indication
binding_unsettled_subscription
executed_unsettled_primary
settled_primary
settled_secondary
```

Only `settled_primary` can evidence cash entering the issue. An executed but
unsettled primary record evidences an agreed price, not completed funding. A
settled secondary record is a buyer-to-seller transfer and contributes nothing
to the project reserve. Multiple observations must be evaluated as separate
runs; v0.1 never averages them into a clearing price.

The reference-price record identifies the exact claim and includes gross buyer
cash `P`, quantity and price basis, primary or secondary status, execution and
settlement dates, currency and monetary basis, issuer cost `F`, buyer-direct
cost `C`, side rights or non-cash consideration, source document or evidence
record, and whether buyer cash payment and settlement are evidenced. A real
use also needs a normalized term/result identifier so a quote for different
priority, duration, loss exposure, or cash rights cannot be silently
substituted. Reserve-deposit and issuer-cost-payment evidence, with a separate
use-evidence record ID, are required before source settlement can be promoted
to observed use-side funding.
Version 0.1's numerical price basis is total month-zero cash for the entire
identified market claim, and quantity must equal `M`. If side rights or
non-cash consideration are present, v0.1 rejects the record rather than
silently valuing them at zero.

A later secondary transaction normally prices the remaining claim after time,
information, and possibly cash flows have passed. It is therefore evidence-only
unless an identified external normalization restates it as month-zero cash for
the entire original claim. The engine does not perform that normalization.
Even a normalized secondary record remains buyer-to-seller cash and never
enters the project ledger. `F` remains a modeled primary-issuance input; it is
not presented as an observed use of secondary proceeds.

Support capacity has its own status: `synthetic_candidate`,
`nonbinding_indication`, `contractually_committed`, `funded_or_escrowed`, or
`settled_to_issue`. Capacity `G` and settled cash `S_obs` are separate fields.
A capacity calculation is not evidence that a provider has authority, budget,
or cash. Only settled primary buyer cash and an evidenced `S_obs` equal to the
required support draw establish the observed issue-source identity. The
support record therefore has separate cash-payment and issue-settlement
evidence booleans; status and amount alone are insufficient.

The price-window calculation is an arithmetic result conditional on all of
declared capacity `G` performing. `synthetic_candidate` and
`nonbinding_indication` never establish available capital. The output reports
support readiness separately: contractual commitment, funded or escrowed
capacity, and cash settled to the issue are different evidence states. The
engine neither estimates nor haircuts provider default, enforceability,
timing, appropriation, or other counterparty-performance risk.

Every tested hurdle remains a separately supplied decision case with:

```text
case ID
annual effective hurdle h
hurdle source type
as-of date
source reference or evidence-record ID
source note
relation to the reference price
```

Allowed hurdle source types are:

```text
same_claim_market_observation
comparable_market_observation
model_adjusted_comparable
investor_target
policy_target
synthetic_sensitivity
```

The relation is `independent`, `model_implied_from_reference_price`, or
`unresolved`. Only an independent hurdle can support investor adequacy or a
financeable-window conclusion. A hurdle solved from `P_ref` and the same cash
flows is a model-implied reconciliation, not independent evidence that the
price compensates an investor. An unresolved relation is likewise ineligible
for financeability.

Neither the price status nor the hurdle source is a numerical weight or an
automatic statement of market representativeness. An observed transaction can
still be unsuitable because its cash rights, seniority, duration, currency,
liquidity, loss exposure, size, or date differ. A model-adjusted comparable
must identify its adjustment outside this engine. IOSCO's benchmark principles
emphasize sufficient data and a transparent hierarchy between transaction
inputs and expert judgment; this project borrows that evidence discipline
without claiming to construct a benchmark
([IOSCO](https://www.iosco.org/library/pubdocs/pdf/IOSCOPD415.pdf)).

The engine never converts a candidate, indication, synthetic hurdle, policy
target, or unverified comparable into “market observed.” It prints the price
status, each hurdle's provenance, and the price/hurdle relation beside the
result.

For an internal candidate, quote, unsettled primary record, or secondary
record, the price window remains a hypothetical primary-issuance test. The
engine reports modeled funding arithmetic but keeps
`observed_primary_funding_completed=false`. A secondary trade never draws
issue support or enters the project ledger, even when buyer-to-seller cash and
settlement are evidenced.

Observed issue sources are settled and reconciled only when the price record is
`settled_primary`, both buyer-cash and settlement evidence are affirmative, and
any positive support draw is `settled_to_issue` with `S_obs=M+F-P` and both
support cash-payment and issue-settlement evidence affirmative. Validation
also requires `0<=S_obs<=G`. Observed settled sources are reconciled
independently from the modeled requirement:

```text
modeled identity:               P_ref + S_req(P_ref) = M + F
observed settled-source identity: evidenced buyer cash + S_obs = M + F
```

The first identity is arithmetic and cannot prove the second. A contractual
commitment or funded escrow can
strengthen financeability evidence but is not settlement into the modeled
reserve.

Source equality does not prove that cash reached the declared uses. The price
record therefore has separate subscription-reserve-deposit and issuer-cost-
payment evidence booleans plus a use-evidence record ID. Observed primary
funding is complete only when settled sources reconcile, reserve deposit is
evidenced, and any positive `F` is evidenced as paid. Reported reserve entry
and issuer-cost payment remain modeled uses otherwise.

An issue-price record can be factual even while the upstream v0.1 project cash
and probability inputs remain synthetic. That can establish only the stated
transaction fact. It does not calibrate the underlying model or turn a settled
price into evidence of financeability, fair value, or additionality.

## Investor price ceiling

For scenario `s`, let `CF_par,s,t` be the fixed market claim cash flow from
the implemented stack, including the original principal subscription of `M`
at month zero and every pool-cost call. Replacing the par subscription with
issue price `P` and adding buyer-direct cost `C` changes no later claim cash
and gives:

```text
NPV_s(P,h)
    = NPV_par,s(h) + M - P - C
```

For physical probability polytope `Q`:

```text
robust NPV(P,h)
    = min over p in Q of sum_s p_s NPV_s(P,h)

raw investor price ceiling P*(h)
    = M - C + robust NPV_par(h)
```

Because `P` is a deterministic month-zero cash flow, it shifts every scenario
and every probability endpoint one-for-one. No optimization or interpolation
is needed to obtain `P*(h)`. Within this discount-only term:

```text
admissible investor ceiling U(h) = min(M+F, P*(h))
```

If `P*(h) < 0`, even a zero issue price fails the supplied robust NPV hurdle.
The term reports `no-nonnegative-investor-price`; a grant that only fills
principal cannot repair the investor's remaining cost calls or the claim's
late and impaired cash.

`P*(h)` is the maximum price consistent with one supplied hurdle under one
physical probability set. It is not an observed price, a fair value, a bid,
or proof that an investor uses that hurdle. The input `h` is never printed as
the claim's yield, YTM, IRR, or expected market return.

The companion
[Market Observation and Hurdle Evidence Set v0.1](MARKET_OBSERVATION_AND_HURDLE_EVIDENCE_V0_1.md)
defines how independent transaction evidence may supply a closed, potentially
disconnected set of such `h` cases without deriving the hurdle from this
claim's proposed target price. The issue-price term must evaluate those
retained regions separately; their convex hull is not evidence.

## Issuer floor and financeable window

The project must still place `M` in the reserve and pay the separately declared
issuer cost `F`. With support capacity `G`, the minimum gross buyer price that
can complete those uses is:

```text
issuer funding floor L = M + F - G
```

At an independently sourced hurdle `h`, an arithmetic, investor-adequate
issue-price window conditional on full performance of capacity `G` exists
when:

```text
0 <= L <= U(h)

conditional financeable price window = [L, U(h)]
```

For any price in the window, modeled required support is
`S_req(P)=M+F-P`. When
`P*(h)>=0`, the minimum support capacity that could create an overlap is:

```text
G_min(h) = M + F - U(h)
```

and the remaining support shortfall is:

```text
support shortfall(h) = max(0, G_min(h) - G)
```

When `P*(h)<0`, `G_min` and support shortfall are not applicable: no
non-negative investor price meets the hurdle, so additional issue support
cannot create an overlap while this claim and its other cost calls stay fixed.

The lower bound protects the modeled funding identity. The upper bound
protects the stated investor hurdle. Neither is a negotiated price or evidence
that support will perform. Bargaining, placement cost, liquidity, support
counterparty risk, legal enforceability, tax, regulation, and investor demand
remain outside the engine.

The output keeps four conclusions separate:

```text
modeled price window exists
a price endpoint exists without support, or a documented commitment covers the overlap
funded or escrowed support capacity covers the overlap
settled issue sources reconcile
use-side evidence completes observed primary funding
```

A contractual commitment can satisfy only the documentary statement. It does
not satisfy the funded-support statement. When `G_min=0`, the output says a
support-free price endpoint exists; other prices in the window can still
require support. It does not claim that an
investor is placed or that the issue is funding-ready.

## Reference-price analysis

The term also accepts one declared reference gross issue price `P_ref` in
`[0,M+F]`. For each hurdle case it reports:

```text
robust / central / maximum physical NPV at P_ref
physical expected scenario NPV-to-all-in-contribution fraction
physical expected distributions, cash multiple, and net-return fraction
negative-NPV probability
NPV-shortfall ES95 and ES99
required modeled support M+F-P_ref
support-capacity margin G-(M+F-P_ref)
non-negative unused capacity and reference-price support shortfall
investor term adequacy
modeled full-funding adequacy
modeled joint term adequacy
observed settled-source reconciliation
observed primary funding completion
```

At the reference price:

```text
required modeled support S_req = M + F - P_ref
support-capacity margin         = G - S_req
unused support capacity         = max(0, G - S_req)
reference support shortfall     = max(0, S_req - G)
actual observed support         = S_obs, never S_req by assumption
```

`S_req` is not an actual draw. It remains a requirement for a secondary
observation and for every candidate or unsettled record. Negative capacity
margin is printed as a shortfall; unused capacity never becomes negative.

For each scenario, all-in investor contribution is `P_ref+C` plus every nominal
pool-cost call assigned to the market claim. Scenario NPV margin divides that
scenario's NPV by that scenario's all-in contribution, and its probability
range is projected from those pathwise ratios. Cash multiple and net-return
fraction use the same pathwise all-in denominator. A ratio is absent if its own
denominator is not strictly positive. Principal loss and principal impairment
continue to use contractual notional `M`. Both denominators are printed so a
discounted purchase cannot make contractual loss exposure disappear.

Each NPV, probability, and tail endpoint retains its own feasible probability
witness. Expected cash, expected NPV, negative-NPV probability, and expected
shortfall are separate optimizations and must not be combined as one forecast.

## What does and does not move

Across hurdle and price cases, the engine must prove that these remain fixed:

- project cash, scenario dates, and the physical probability polytope;
- `q`, `A`, `K`, `M`, `B`, claim IDs, and waterfall ordering;
- market principal and non-principal distributions in every state and month;
- principal loss, impairment, and principal-cash weighted-average life;
- junior contribution, cash, NPV at its own fixed hurdle, and concession; and
- the modeled issue source-and-use identity after adding required support.

Only discounting, the investor's issue payment, separately declared issue
costs, and the corresponding modeled support requirement change. Lowering `P` improves
investor NPV by exactly the extra discount, while increasing required support
by the same amount. It does not improve project performance or reduce
consolidated economic cost.

## Exact synthetic hand fixture

Use the selected priority-cap fixture:

```text
q = 25/28      A = 12      K = 20      M = 8      B = 8/15
market pool-cost call at month zero = 0.08
available non-repayable support G = 1.50
reference issue price P_ref = 6.50
issuer and buyer-direct issue costs F=C=0
```

The fixed market distributions are:

```text
common loss:       4 at month 12
common success:    128/15 at month 24
either single loss: 2 at month 12 plus 98/15 at month 24
```

Let `d=1+h`. On the displayed non-negative 0%–20% hurdle range, the retained
event polytope gives these robust, central, and maximum issue-price boundaries:

```text
P_robust*(h)  = 0.80/d + 7.28/d^2 - 0.08
P_central*(h) = 0.80/d + (2866/375)/d^2 - 0.08
P_maximum*(h) = 1.02/d + 7.468/d^2 - 0.08
```

Only the robust boundary governs the financeability window. The central and
maximum boundaries are physical-probability endpoint sensitivities, not issuer
price ceilings or alternative transaction prices.

The issuer floor is `8-1.5=6.5`. The finite synthetic hurdle cases give:

| Supplied hurdle | Robust price ceiling | Minimum capacity for overlap `G_min` | Window with `G=1.50` | Robust NPV at `P_ref=6.50` |
|---:|---:|---:|---|---:|
| 0% | 8.000000 | 0 | `[6.500000,8.000000]` | 1.500000 |
| 5% | 7.285079 | 0.714921 | `[6.500000,7.285079]` | 0.785079 |
| 10% | 6.663802 | 1.336198 | `[6.500000,6.663802]` | 0.163802 |
| 15% | 6.120378 | 1.879622 | none; support shortfall 0.379622 | -0.379622 |
| 20% | 5.642222 | 2.357778 | none; support shortfall 0.857778 | -0.857778 |

This table does not say that 10% is an observed market return. It says that if
10% were a defensible hurdle, the synthetic claim could meet it across the
supplied probability envelope only at an issue price no higher than about
`6.663802`; the project would then need at least about `1.336198` of explicit
outside issue support to receive the full `8` of market principal.

At 15%, the declared `1.50` support capacity is insufficient. Calling the
claim financeable at that hurdle would require hiding a `0.379622` funding gap
or changing another economic term.

## Finite cases and resource boundary

Version 0.1 evaluates a declared finite set of at most 256 hurdle cases. Rates
must be finite and lie in `[0,10]`, matching the public capital-stack engine;
negative rates are outside this version. Case IDs are unique. Every monetary
input `P`, `F`, `C`, `G`, and `S_obs` is finite and non-negative. Checked
arithmetic requires `M+F` and `P+C` to remain finite; `P<=M+F`, `G<=M+F`, and
`0<=S_obs<=G`. Status and cash-evidence fields must be coherent. At least one
literal zero-hurdle case is required as an undiscounted reconciliation
baseline.

Cases are evaluated independently. The term does not interpolate or binary-
search a hurdle, and it does not assume that results are monotone in `h`:
future investor cost calls can make a general contingent cash-flow stream
non-conventional.

Money comparisons use an absolute tolerance of `1e-10` in the reported
million-unit basis plus
`256 * machine_epsilon * max(1,abs(first),abs(second))`. This is a floating-
point classification control, not economic materiality.

The evaluator first runs the supplied priority-cap term and requires a
deterministic eligible selected cap. Where a junior-concession mandate exists,
the selected cap must be the minimum tested balanced cap; otherwise it is the
minimum tested market-adequate cap. It then copies that selected stack, changes
only the identified market claim's annual physical hurdle for each case, and
calls the public event-polytope stack evaluator. It does not mutate any caller
input.

The structural bound is explicit. Let `N` be scenarios, `E` probability
events, `J` projects, `T=horizon_months+1`, `W=N+E+1`, `R` counted portfolio
records, `D` tested priority-cap candidates, and `H` hurdle cases. Define:

```text
grid(x) = x*N*W + x*R + x*J*N*T + x*N*2*T

upstream priority-cap work = grid(D)
hurdle stack work          = grid(H)
reference projection work = H*8*N*W
scenario-month audit work  = H*N*3*T
total structural work      = their sum
```

`R` includes declared pool costs, cash-source monthly amounts, project draws
and receipts, plus loss-layer/scenario, cash-source, and factor-tag auxiliary
records. Every product and sum is overflow-checked before evaluation, and the
total must not exceed `4,000,000` work units. This is a deterministic resource
guard, not a runtime estimate.

If the upstream term has no eligible selected cap, the economic status is
`priority-cap-selection-unavailable` and no hurdle result is produced. That is
a completed analysis, not malformed input and not a reason to manufacture a
price case.

Normalized configuration must be byte-stable after load, print, reload, and
print. The parser is closed-schema, bounded-input, locale-independent, and
fail-closed on unknown, duplicate, missing, non-finite, incoherent, or
resource-excessive inputs.

## Falsification tests

The term fails or reports no window when:

- the fixed claim or selected `B` does not match the supplied stack;
- changing a hurdle changes any contractual cash or principal-risk result;
- modeled issue price plus required support does not reconcile exactly to
  `M+F`, with modeled uses exactly `M` for reserve and `F` for issuer cost;
- source equality is printed as observed reserve deposit or issuer-cost payment
  without the separate use-side evidence;
- support is counted as project revenue or as investor distribution;
- a recoverable support claim is omitted from the waterfall;
- an issue price above `M+F` or an unexplained proceeds surplus is admitted;
- a negative raw price ceiling is clipped to zero and called adequate;
- the physical probability witness is relabeled risk-neutral;
- a candidate, unsettled, or secondary price record is printed as settled
  primary funding;
- a synthetic, policy, or unverified hurdle is printed as market-observed or
  empirically calibrated;
- robust NPV at `P*(h)` does not reconcile to zero within the disclosed money
  tolerance;
- a price below the issuer floor or above the investor ceiling is called
  jointly adequate;
- principal loss is divided only by discounted purchase price and the fixed
  notional denominator disappears;
- different probability witnesses are combined into one scenario; or
- case count, horizon, scenario, event, or cash work exceeds the resource cap.

Tests must include `P=M`, `P=0`, `G=0`, `G=M+F`, positive `F`, positive `C`,
zero and positive `S_obs`, a reference price below the issuer floor, a negative
raw ceiling, a ceiling above complete issue uses, a one-base-currency-unit
market notional, future pool-cost calls, multiple distribution dates, a grid
ordering permutation, rates immediately around a financeable-window boundary,
rates bracketing the exact event-witness switch at `h=34/15`, and the allowed
upper endpoint `h=10`. Transaction tests must prove that an internal candidate,
unsettled primary, settled primary, and settled secondary record produce different
evidence conclusions without changing any cash-path calculation. Independent,
model-implied, and unresolved hurdle relations must retain distinct eligibility
and reason fields: model-implied and unresolved cases are never
financeability-eligible, while an independent case follows the arithmetic.
Their final window booleans may legitimately all be false.

## Interpretation boundary

The following remain false:

```text
market hurdle discovered or calibrated by this engine       false
fair value or accounting value estimated                    false
market-consistent discount curve or pricing measure used    false
YTM, IRR, model-implied break-even yield, or annualized
  expected market return produced                           false
bid, offer, executable price, spread, or rating produced    false
investor demand, suitability, or placement established      false
support-provider authority or budget established by engine  false
support-provider performance/default risk modeled or priced false
legal enforceability, tax, or regulation established        false
capital mobilization or financing additionality proven      false
animal-product displacement or welfare impact proven        false
```

Biomedical securitization research demonstrates why pooling, risk allocation,
and simulated return analysis can widen the financing design space, while its
published results remain model-dependent simulations
([Fernandez, Stein, and Lo](https://www.nature.com/articles/nbt.2374)). This
project borrows only the probability-set and investor-acceptability structure
from research on incomplete-market pricing; it does not import that
literature's additional assumptions or imply that the literature never derives
unique prices
([Carr, Geman, and Madan](https://doi.org/10.1016/S0304-405X(01)00075-7)).

The honest output can be that real project cash supports no acceptable price,
that a price works only with quantified concessionary support, or that market
evidence is too weak to choose a hurdle. Those are financial-engineering
results, not failures of presentation.
