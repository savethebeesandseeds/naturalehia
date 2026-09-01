# Robust Capital-Mobilization Frontier v0.2

## Issued-principal cash-shortfall supplement

Status: implemented deterministic synthetic term, 1 September 2026. Version
0.2 is an additive five-input frontier for Capital Stack v0.2. It does not
replace the legacy
[Robust Capital-Mobilization Frontier v0.1](ROBUST_CAPITAL_MOBILIZATION_FRONTIER_V0_1.md).

## Decision question

Version 0.2 asks:

> For one validated Capital Stack v0.2 template, which tested combinations of
> success-cash participation `q` and funded junior issued principal `A` satisfy
> every independently declared market mandate?

The answer is a set of enumerated candidates. It is not a continuous optimum,
market price, rating, placement result, or claim that capital was mobilized.
An empty feasible set is a valid economic report.

## Additive version boundary

The two frontier versions deliberately have different entry contracts:

| Version | Inputs | Principal-risk family | Public grid language |
|---|---:|---|---|
| v0.1 | Portfolio, event polytope, success participation, frontier term | legacy tranche principal-loss layering | catalytic first loss |
| v0.2 | the same four records **plus** an independently normalized Capital Stack v0.2 template | market issued-principal cash shortfall `Q` | junior issued principal |

The programmatic default remains v0.1. The four-input evaluator accepts only
frontier v0.1. The five-input evaluator accepts only frontier v0.2 with Capital
Stack v0.2. Legacy principal-loss keys are rejected by the v0.2 parser, and
v0.2 `Q` keys are rejected by the v0.1 parser. The normalized v0.2 form never
prints `principal_loss`, `impairment`, or `catalytic_first_loss` labels.
The original public C++ constant
`kRobustCapitalMobilizationFrontierModelVersion` remains `0.1.0`; five-input
callers opt in with the additive
`kRobustCapitalMobilizationFrontierV02ModelVersion` value `0.2.0`.

Some C++ structure members retain compatibility names containing `loss`,
`impairment`, or `catalytic`. When the summary flag
`principal_risk_uses_issued_principal_cash_shortfall_q` is true, those members
contain only the v0.2 `Q` family described below. They never contain a legacy
asset-loss placeholder.

Those retained names support callers that recompile against this version. The
public candidate and summary structures gained fields, so this supplement does
not claim binary ABI compatibility with an older compiled client.

## Fixed inputs and two decision variables

The validated base-stack template fixes all of the following:

- the funded reserve and top issued-principal detachment `K`;
- exactly one junior residual claim and one market priority claim;
- both claim identifiers and their ordering;
- the market lifetime priority cap `B` on non-principal cash;
- both annual physical-measure hurdle sensitivities;
- all Capital Stack v0.2 asset/liability assertions; and
- the starting `q` and `A`, each of which must appear in the frontier grids.

Every candidate copies that template and changes only:

```text
q = fraction of already-declared eligible non-principal success cash
A = funded junior issued-principal boundary in Q-shortfall coordinates
```

For each pair:

```text
junior issued-principal claim   [0,A]
market priority claim           [A,K]
market issued principal M       K - A
```

The two claims cover `[0,K]` with no hidden third claim. `0 < A < K`; therefore
both claims have positive notional. `q` changes only eligible success cash. It
does not change project draws, principal, recoveries, costs, scenario states,
probability bounds, reserve funding, or claim rights other than the declared
success-cash participation. `A` reallocates liability cash shortfall and
priority. It does not change project cash or cause asset loss.

The evaluator sorts and crosses the complete declared grids. It accepts at
most 1,024 pairs and rejects a combined structural-work count above 4,000,000.
It does not interpolate between grid points.

## Four ledgers that must remain separate

The v0.2 report exposes three aggregate limits and the scenario observables
needed to prevent a false equivalence:

| Quantity | Meaning | What it is not |
|---|---|---|
| aggregate project-outlay limit | Portfolio cash-use/commitment boundary reported by the frontier | contractual principal or issued principal |
| aggregate contractual asset-principal limit | Portfolio asset-ledger principal boundary | purchase price, reserve, or value |
| `K` | separately funded acquisition/primary-funding reserve and total issued principal; also the top stack detachment | necessarily either aggregate limit above |
| buyer-direct cost | additional pro-rata investor call outside `K` | reserve funding or asset principal |

The first two aggregate quantities intentionally have no `O` or `L` shorthand
in this supplement. `L_s` and `O_s` retain their established scenario meanings:

```text
L_s = contractual asset principal explicitly written off in state s
O_s = contractual asset principal still outstanding at the horizon in state s
```

They enter unchanged from the Portfolio asset ledger. They are not allocated
causally to liability tranches.

Let `D_s` be principal cash distributable to issued claims, including returned
unused reserve only as defined by Capital Stack v0.2. The liability-side
shortfall is:

```text
Q_s = K - D_s
```

and the claim shortfalls are:

```text
Q_junior,s = min(Q_s, A)
Q_market,s = min(max(Q_s - A, 0), M)
```

Thus `L_s` can be positive when market `Q` is zero, and market `Q` can be
positive when `L_s` is zero. `Q` is not asset loss, continuing principal,
recovery, accounting impairment, IFRS 9 expected credit loss, Basel regulatory
expected loss, or a legal default determination.

## Candidate projections

For every `(q,A)` pair, the engine rebuilds the selected success cash, the
two-claim monthly waterfall, and every probability projection. It reports
minimum, central, and maximum physical-measure ranges where applicable and
retains each endpoint's own optimizing probability witness.

The market-claim decision metrics are:

```text
robust NPV margin       min_p E_p[NPV_market] / M
expected Q fraction     max_p E_p[Q_market] / M
Q ES95 fraction         max_{p,y} ES95_p(Q_market) / M
Q ES99 fraction         max_{p,y} ES99_p(Q_market) / M
Q incidence             max_p Pr_p[Q_market > 0]
negative-NPV incidence  max_p Pr_p[NPV_market < 0]
NPV-shortfall ES95/99   worst tail average of max(0,-NPV_market), divided by M
WAL                     maximum common-measure principal-cash weighted average life
```

The report also includes the fully funded aggregate NPV at the pool hurdle,
junior NPV and junior NPV concession, market expected contributions, principal
cash and total distributions, numerical reconciliations, and resource counts.
Buyer-direct and pool-cost calls enter contributions and NPV but do not enter
`K` or `M`; every `Q` and NPV-shortfall fraction divides by fixed market issued
principal `M`, not all-in investor cash.

Different metrics can have different adverse probability witnesses. A maximum
expected `Q`, maximum tail `Q`, maximum negative-NPV probability, and maximum
WAL must not be assembled into one invented state or forecast.

## Declared constraints and feasibility

The closed schema supports twelve optional mandates:

1. minimum robust aggregate NPV;
2. minimum market robust NPV margin;
3. maximum market expected issued-principal cash shortfall `E[Q]/M`;
4. maximum market `Q` ES95/M;
5. maximum market `Q` ES99/M;
6. maximum market `Pr[Q>0]`;
7. maximum market negative-NPV probability;
8. maximum market NPV-shortfall ES95/M;
9. maximum market NPV-shortfall ES99/M;
10. maximum market WAL;
11. maximum junior issued principal `A`; and
12. maximum junior NPV concession.

Every mandate is independently declared. `none` means absent and cannot
silently bind. A candidate is feasible only when all declared mandates pass.
If at least one candidate is feasible, the report also provides:

- all feasible indices;
- nondominated feasible indices;
- the minimum tested feasible `q`; and
- the least tested feasible `A` for each `q` that has one.

Pareto comparison minimizes `q`, `A`, junior NPV concession, the four market
`Q` endpoints, negative-NPV probability, NPV-shortfall ES95/99, and market WAL.
It does not apply a weighted score. NPV-surplus requirements remain explicit
feasibility gates rather than hidden rewards for extracting more project cash.

## Ten-claim same-pool result

The checked synthetic fixture uses:

```text
q grid = 0.5, 0.625, 0.75, 0.875, 1
A grid = 10, 20, 30, 40, 50 DEMO million
candidate count = 25
fixed K = 100 DEMO million
fixed B = 24 DEMO million
junior hurdle = 15 percent
market hurdle = 8 percent
```

Its aggregate project-outlay limit, aggregate contractual asset-principal
limit, and funded reserve/issued principal are each numerically 100. That
equality results from this retained legacy-at-par fixture; the v0.2 accounting
does not require the three quantities to be equal.

The mandate file requires non-negative market robust NPV margin; no more than
10% worst expected market `Q`; no more than 50%/60% Q-ES95/ES99; no more than
35% `Pr[Q>0]`; no more than 35% negative-NPV probability; no more than 60%/70%
NPV-shortfall ES95/99; WAL no longer than 10 years; `A` no greater than 50; and
junior NPV concession no greater than 50. Aggregate NPV is absent.

The verified result is:

```text
feasible candidate indices                  none
nondominated feasible candidate indices     none
minimum tested feasible q                   none
```

Two disclosed points show why the empty set is economically informative:

| `(q,A;M)` | Robust market NPV | Worst `E[Q]/M` | Q ES95/M | Q ES99/M | Max `Pr[Q>0]` | Max `Pr[NPV<0]` |
|---|---:|---:|---:|---:|---:|---:|
| `(1,20;80)` | -25.733095 | 27.377875% | 87.5% | 87.5% | 60% | 100% |
| `(1,50;50)` | -5.569641 | about 12.161% | 80% | 80% | 28% | 54% |

The second point reduces market shortfall incidence but still breaches the
expected-`Q`, both `Q`-tail, and negative-NPV screens. This is a rejection of
the supplied grid and mandate only. It does not show that all possible capital
structures fail.

## Conditional issue-price/support channel

Once the declared `q`-by-`A` grid fails, the next modeled channel is a lower
gross buyer price and/or a separately funded, no-rights issue support source.
That is evaluated by the existing
[Robust Issue-Price Support Term v0.1](ROBUST_ISSUE_PRICE_SUPPORT_TERM_V0_1.md),
which is versioned independently but dispatches the Capital Stack v0.2 `Q`
risk family when supplied the v0.2 stack.

For fixed future claim cash and market issued principal `M`, it compares:

```text
P*(h) = robust investor gross price ceiling at independent hurdle h
issuer floor = max(0, M + issuer cost F - maximum no-rights support G)
```

The ten-claim downstream fixture fixes `q=1`, `A=20`, `M=80`, selects `B=24`,
sets `F=0`, and supplies a wholly synthetic `G=20`. Arithmetic overlap exists
at the invented 0% and 5% hurdle cases, with windows `[60,74.575200]` and
`[60,60.955502]`. At the declared 8% market hurdle, `P*=54.266905`, below the
issuer floor of 60, so no overlap exists. There is also no overlap at 10%, 15%,
or 20%.

The priority-cap selection is governed by a separate, relaxed issue-price
sensitivity mandate: 30% expected `Q`, 90% Q-ES95/99, 60% `Pr[Q>0]`, and
five-year WAL limits. It is not the frontier mandate, which rejects the same
`q=1`, `A=20`, `M=80` point under 10%, 50%/60%, 35%, and ten-year limits. A
pass under the separate sensitivity limits does not reverse that rejection.
Price and support can change NPV and the funding identity but do not change
`Q`, its tails or incidence, or WAL, so they cannot cure the strict frontier's
fixed-risk failure.

No support funding, authority, budget, escrow, or settlement is evidenced;
settled support is zero. No supplied hurdle or reference price is a market
observation. The conditional windows therefore do not reverse the frontier's
rejection or establish an executable financing.

## CLI and normalized record

The checked v0.2 CLI form uses the prebuilt WebAssembly reporter through Node
inside the pinned Emscripten image; it does not compile or invoke a native
Windows executable:

```powershell
docker run --rm --name fca-frontier-v02 `
  --mount type=bind,source=/mnt/host/c/Work/Naturalehia/projects/fostering-cellular-agriculture,target=/work `
  -w /work emscripten/emsdk:6.0.5 `
  node build-wasm-v02/naturalehia-capital-mobilization-frontier.js `
    <portfolio.cfg> `
    <event-polytope.cfg> `
    <success-participation.cfg> `
    <capital-stack-v0.2.cfg> `
    <capital-mobilization-frontier-v0.2.cfg> `
    [--print-normalized]
```

`--print-normalized` appends all five complete reloadable inputs. Economic
no-solution exits successfully. Command-grammar, input/load, and cross-input or
report failures remain distinct and retain
`calibrated_execution_authorized=false`.

## Explicit non-claims and residual limitations

Version 0.2 does not estimate or establish:

- a continuous optimum or failure of untested terms;
- fair value, an issue price, market spread, discount curve, rating, bid/offer,
  annualized expected return, or investor suitability;
- empirical probability calibration or an objective forecast;
- investor demand, placement, liquidity, capital mobilization, crowding-in, or
  financing additionality;
- support-provider authority, funding, budget, escrow, enforceability, or
  performance;
- reserve custody risk, strategic behavior, servicing risk, legal form,
  security perfection, insolvency treatment, tax, accounting, or regulatory
  capital; or
- project additionality, qualified output, animal-product displacement, or
  welfare impact.

The probability set is finite and physical-measure, the mandate is invented,
and endpoints can have different witnesses. The output is a reproducible
structural rejection boundary, not authorization to execute.
