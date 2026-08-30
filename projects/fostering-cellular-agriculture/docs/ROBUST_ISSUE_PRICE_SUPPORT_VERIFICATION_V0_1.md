# Robust Issue-Price Support v0.1 — Verification Record

Status: core evaluator, strict configuration parser, six-input CLI, and
synthetic verification implemented. Full strict Debug and Release verification
recorded 2026-08-30.

## Verified scope

The implemented layer begins after the project pool, physical probability
polytope, selected success participation, funded two-claim stack, and finite
market non-principal priority-cap term are fixed. It asks one bounded
conditional question:

> At each independently supplied investor hurdle, what is the maximum gross
> month-zero price supported by the fixed market claim, and does that investor
> boundary overlap the issuer's full-principal funding floor under declared
> no-rights issue-support capacity?

The answer is a finite sensitivity, not a discovered hurdle, price quote,
fair value, offer, placement result, or support commitment. The economic
definition and limitations are in the
[term specification](ROBUST_ISSUE_PRICE_SUPPORT_TERM_V0_1.md). This layer
retains the selected structure verified in the
[market priority-cap record](ROBUST_MARKET_PRIORITY_CAP_VERIFICATION_V0_1.md),
which in turn sits on the
[capital-mobilization frontier](ROBUST_CAPITAL_MOBILIZATION_FRONTIER_VERIFICATION_V0_1.md),
[capital-stack](CAPITAL_STACK_VERIFICATION_V0_1.md), and
[event-polytope](EVENT_PROBABILITY_POLYTOPE_VERIFICATION_V0_2.md) engines.

The implementation under test consists of:

- the [public core API](../include/naturalehia/cellular_finance/robust_issue_price_support.hpp);
- the [core evaluator](../src/robust_issue_price_support.cpp);
- the [configuration API](../include/naturalehia/cellular_finance/robust_issue_price_support_config.hpp);
- the [closed-schema parser and normalized printer](../src/robust_issue_price_support_config.cpp);
- the [six-input CLI](../apps/issue_price_support_cli/main.cpp);
- the [core tests](../tests/robust_issue_price_support_tests.cpp);
- the [configuration tests](../tests/robust_issue_price_support_config_tests.cpp); and
- the [CLI golden checker](../tests/check_issue_price_support_cli.cmake).

The reloadable synthetic input is
[`issue-price.cfg`](../scenarios/market-priority-cap-v0.1-synthetic/issue-price.cfg).
The CLI loads that sixth record after the five fixed upstream inputs and can
emit all six normalized configurations for byte-stable replay.

## Public result contract

The public evaluator first runs the supplied priority-cap evaluator. When a
junior-concession mandate exists, it uses only the upstream minimum tested
balanced candidate; otherwise it uses only the minimum tested market-adequate
candidate. It rebuilds that exact selected stack and changes only the market
claim's annual hurdle for each declared case. Caller inputs are not mutated.

An unavailable upstream selection is a completed economic result with overall
status `PriorityCapSelectionUnavailable`. In that result, selected candidate
index and selected `B` are both absent, and no artificial index zero or
`B=0` result is produced. Other runs report `NoFinanceableWindow` or
`FinanceableWindowFound`.

Each hurdle result retains:

- its source type, as-of date, evidence references, and declared relation to
  the reference price;
- robust, central, and maximum physical market-claim NPV at par;
- raw and admissible investor price boundaries and the issuer floor;
- optional financeable-window endpoints;
- optional minimum support and shortfall, which are absent when the raw robust
  price ceiling is negative;
- reference-price physical NPV, contribution, distribution, cash-multiple,
  return-fraction, negative-NPV probability, and NPV-shortfall-tail metrics
  when the reference is numerically eligible;
- principal loss, impairment, tail, and WAL metrics with contractual notional
  `M` retained as the denominator; and
- sparse monthly cash, NPV-shift, source-and-use, physical-probability,
  contractual-cash, junior-cash, and principal-risk audits.

Only a hurdle declared `Independent` can produce investor adequacy or enter a
financeable-case index. `ModelImpliedFromReferencePrice` and `Unresolved`
cases remain calculable reconciliation sensitivities, but receive
`HurdleNotIndependentOfReferencePrice` and cannot validate the price used to
derive them.

## Exact source-and-use identities

Let:

```text
M     fixed contractual market principal notional
P     gross buyer cash for the entire claim at month zero
F     declared issuer cost paid from issue sources at month zero
C     buyer-direct cost paid outside the subscription reserve
G     maximum declared non-repayable issue-support capacity
S_obs independently evidenced support cash actually settled to the issue
```

The modeled support requirement is arithmetic:

```text
S_req(P) = M + F - P
P + S_req(P) = M + F
modeled reserve entry = M
modeled issuer-cost payment = F
C remains outside the reserve
```

The observed settled-source identity is separate:

```text
evidenced settled-primary buyer cash + S_obs = M + F
```

The evaluator never substitutes computed `S_req` for actual `S_obs`. A
`SettledPrimary` price with buyer-cash and settlement evidence, together with
an exact evidenced support draw when positive support is required, can establish
only that issue sources settled and reconcile. Source equality does not prove
destination uses. Observed primary funding additionally requires separate
subscription-reserve-deposit evidence and, when `F>0`, issuer-cost-payment
evidence under a non-placeholder use-evidence record. The observed reserve and
cost fields remain absent without those controls. A settled secondary price is
buyer-to-seller cash and never enters the project reserve. It is numerical
evidence only when an external record explicitly normalizes it to month-zero
cash for the entire original fixed claim; even then it cannot establish project
funding.

Version 0.1 admits no support repayment, participation, security, recovery,
or other claim right. Side rights or non-cash consideration make the all-cash
price shift unidentified and fail closed. The required reference bases are
exactly:

```text
quantity basis = full contractual market principal
price basis    = gross buyer cash at month zero
quantity       = M within the disclosed money tolerance
```

Claim ID, currency, and monetary basis must match the fixed upstream inputs.

## Price window and downside equations

For supplied annual effective hurdle `h` and fixed par cash path
`CF_par,s,t`, replacing the par subscription by `P` while adding `C` shifts
every scenario NPV by the same deterministic month-zero amount:

```text
NPV_s(P,h) = NPV_par,s(h) + M - P - C

P*(h) = M - C + min[p in Q] E_p[NPV_par,s(h)]
U(h)  = min(M + F, P*(h))
L     = M + F - G
```

At an independent hurdle, the modeled window conditional on full performance
of `G` exists exactly when `0 <= L <= U(h)`. When `P*(h)>=0`:

```text
G_min(h) = M + F - U(h)
support shortfall = max(0, G_min(h) - G)
```

When `P*(h)<0`, no non-negative issue price satisfies the investor hurdle.
`G_min` and support shortfall are therefore not applicable rather than being
reported as a fictitious support cure.

Money classification uses:

```text
tol(a,b) = 1e-10
         + 256 * machine_epsilon * max(1, abs(a), abs(b))
```

Contractual input domains `P<=M+F`, `G<=M+F`, and `S_obs<=G` are enforced
exactly. Computed near-zero and near-touching endpoints are canonicalized so a
reported window never has its upper endpoint microscopically below its lower
endpoint. This tolerance is a floating-point classification control, not an
economic materiality threshold.

## Exact synthetic hand fixture

The selected upstream fixture fixes:

```text
q = 25/28      A = 12      K = 20      M = 8      B = 8/15
market pool-cost call at month zero = 0.08
G = 1.50       P_ref = 6.50       F = C = 0
```

Market distributions are:

```text
common loss          4 at month 12
common success       128/15 at month 24
either single loss   2 at month 12 plus 98/15 at month 24
```

With `d=1+h`, the retained 0%–20% event witnesses give:

```text
P_robust*(h)  = 0.80/d + 7.28/d^2 - 0.08
P_central*(h) = 0.80/d + (2866/375)/d^2 - 0.08
P_maximum*(h) = 1.02/d + 7.468/d^2 - 0.08
```

Only the robust boundary governs the conditional price window:

| Supplied hurdle | Robust ceiling | `G_min` | Modeled window with `G=1.50` | Robust NPV at `P_ref=6.50` |
|---:|---:|---:|---|---:|
| 0% | 8.000000 | 0.000000 | `[6.500000,8.000000]` | 1.500000 |
| 5% | 7.285079 | 0.714921 | `[6.500000,7.285079]` | 0.785079 |
| 10% | 6.663802 | 1.336198 | `[6.500000,6.663802]` | 0.163802 |
| 15% | 6.120378 | 1.879622 | none; shortfall 0.379622 | -0.379622 |
| 20% | 5.642222 | 2.357778 | none; shortfall 0.857778 | -0.857778 |

These are synthetic physical-measure sensitivities. The table neither says
that 10% is an observed return nor that the modeled support is committed.

## Reference-price downside oracle

At `h=10%`, all-in scenario contribution is `6.58`: gross price `6.50` plus
the market claim's `0.08` pool-cost call. The path NPVs are approximately:

```text
common success       128/15 / 1.10^2 - 6.58 =  0.472341598
either single loss   2/1.10 + 98/15/1.10^2 - 6.58 = 0.637630854
common loss          4/1.10 - 6.58 = -2.943636364
```

Only common loss has negative NPV. Its admitted probability endpoints give:

```text
negative-NPV probability   minimum 0.01, central 0.02, maximum 0.10

NPV-shortfall ES95         minimum 0.588727273
                           central 1.177454545
                           maximum 2.943636364

NPV-shortfall ES99         2.943636364 at all three endpoints
```

Each probability, expectation, and tail endpoint retains its own feasible
physical-probability witness. The engine does not splice independently
optimized endpoints into one synthetic scenario forecast.

## Evidence and readiness axes

The implementation deliberately separates six conclusions:

1. `modeled_financeable_price_window_exists` is arithmetic conditional on
   full performance of declared capacity `G`.
2. `documented_support_commitment_covers_overlap` requires a non-synthetic
   `ContractuallyCommitted`, `FundedOrEscrowed`, or `SettledToIssue` record and
   a positive support need within a modeled window.
3. `funded_support_capacity_covers_overlap` and
   `funded_support_covered_price_window_exists` require
   `FundedOrEscrowed` or `SettledToIssue`; a contractual commitment alone is
   not called funded.
4. `observed_issue_sources_settled_and_reconciled` requires the coherent
   settled-primary buyer/support source identity.
5. observed reserve and issuer-cost amounts require their own use-side
   evidence; source equality alone leaves them absent.
6. `observed_primary_funding_completed` requires reconciled settled sources,
   evidenced reserve deposit, and evidenced issuer-cost payment when `F>0`.

`modeled_overlap_exists_without_support` identifies the arithmetic case
`G_min=0`: at least one support-free price endpoint exists. Other prices in the
window may still require support. It does not claim funding readiness, buyer
demand, or placement.
Synthetic and nonbinding support never produce commitment or funded-capacity
conclusions. The engine does not estimate or price provider default,
performance, appropriation, timing, or enforcement risk.

Transaction settlement evidence and hurdle provenance are independent axes.
A factual settled transaction does not make a synthetic or circular hurdle
independent, and an independently supplied hurdle does not prove settlement.

## Structural-work boundary

Let:

```text
N = scenarios
E = probability events
J = projects
T = horizon_months + 1
W = N + E + 1
R = counted portfolio cash and auxiliary records
D = priority-cap candidates
H = hurdle cases

grid(x) = x*N*W + x*R + x*J*N*T + x*N*2*T
```

The complete preflight is:

```text
upstream priority-cap work = grid(D)
hurdle-stack work          = grid(H)
reference-projection work = H*8*N*W
scenario-month audit work  = H*N*3*T

total = their checked sum <= 4,000,000
```

The three scenario-month passes are the sparse market ledger, across-hurdle
market contractual-cash comparison, and across-hurdle junior-cash comparison.
Every product and sum is overflow-checked before economic evaluation.

For the canonical fixture:

```text
N=4, E=4, J=2, T=25, W=9
R=36 = 28 cash records + 8 auxiliary records
D=5, H=5

upstream priority-cap work     2,360
hurdle-stack work              2,360
reference-projection work      1,440
scenario-month audit work      1,500
total structural work          7,660 / 4,000,000
```

This is a deterministic resource guard, not an elapsed-time estimate or an
economic bound.

## Validation and parser controls

The direct API and strict parser reject malformed or unidentified economics,
including:

- other than 1–256 unique finite hurdle cases, omitted literal zero,
  negative rates, negative zero, or rates above the allowed endpoint `10`;
- unsafe identifiers or text, impossible calendar dates, settlement before
  execution, status-incoherent dates or evidence, and placeholder normalized
  terms for any non-internal factual price status;
- non-finite, negative, or above-guardrail monetary inputs; checked-sum
  overflow; exact violations of `P<=M+F`, `G<=M+F`, or `S_obs<=G`;
- mismatched claim quantity, ID, currency, or monetary basis;
- any noncanonical quantity or price basis, side right, non-cash
  consideration, repayable support, support claim right, project-revenue
  treatment, or use of support for future costs or project losses;
- a settled secondary reference used numerically without explicit external
  normalization to the full month-zero claim;
- synthetic inputs labeled as observed price, support, or hurdle evidence;
  and
- combined structural work above 4,000,000 units.

The parser uses a closed versioned `key=value` schema with bounded file, line,
entry, identifier, text, and hurdle counts. It rejects unknown, duplicate, and
missing keys; surrounding whitespace; embedded BOMs; non-finite numerics;
invalid enums and booleans; stream failures; and output failures. Its
normalized representation is canonical rate/ID order and is intended to be
byte-stable after print/reload/print.

## Test inventory and observed status

The core regression covers:

- exact hand formulas, canonical ordering, selected `B=8/15`, all four work
  subtotals, and total `7,660`;
- the 0%, 5%, 10%, 15%, and 20% boundaries and the 10% reference downside
  oracle;
- `P=M`, `P=0`, `G=0`, `G=M`, positive `F`, positive `C`, a negative raw
  ceiling, a ceiling above complete issue uses, and support-gap N/A behavior;
- future pool-cost calls, multiple distribution dates, sparse-ledger
  reconciliation, base-stack immutability, and fixed principal denominators;
- a one-base-currency-unit market claim and decimal-equivalent quantity match;
- cases bracketing the exact event-witness switch at `h=34/15` and the allowed
  upper endpoint `h=10`;
- internal, committed, funded, settled-primary, unnormalized-secondary,
  normalized-secondary, independent, model-implied, and unresolved evidence
  paths;
- upstream no-selection without a fake selected index or cap;
- exact money domains, canonical bases, dates, IDs, side-rights, currency, and
  money guardrails; and
- checked fail-closed combined resource accounting.

The configuration regression covers closed-schema parsing, canonical
round-trip output, enum/status mapping, evidence coherence, settlement and
normalization rules, real dates, strict numerics, unsafe and oversized input,
stream failures, output failures, and file-loading failures.

Only the following executions are recorded as observed in this verification
record:

| Configuration and scope | Result |
|---|---:|
| Strict manual MSVC Debug compile/link/run of the core and core test (`/std:c++20 /EHsc /W4 /WX /permissive-`) | passed, 1 executable |
| Integrated strict Debug issue-price core, parser, and CLI CTest scope | passed, 3/3 |
| Integrated strict Release issue-price core, parser, and CLI CTest scope | passed, 3/3 |
| Full-project strict Debug suite | passed, 57/57 in 27.22 seconds |
| Full-project strict Release suite | passed, 57/57 in 12.88 seconds |
| Six-file normalized print-load-print replay | passed in Debug and Release CLI golden regression |
| Economic no-solution, non-independent hurdle, secondary evidence-only, settled-source-only, fully evidenced settlement, usage/load/analysis exits, and broken stdout | passed in Debug and Release CLI golden regression |
| Independent mathematical and implementation-oracle audit | clean; no remaining P1/P2 finding |

## Reproducible build and test commands

From the repository root in a Visual Studio 2022 developer shell, a clean
strict verification build can be prepared with:

```powershell
cmake -S . -B build/issue-price-verify -G "Visual Studio 17 2022" -A x64 `
  -DNATURALEHIA_CELLULAR_FINANCE_WARNINGS_AS_ERRORS=ON
```

The issue-price-specific Debug scope is reproducible with:

```powershell
cmake --build build/issue-price-verify --config Debug --parallel
ctest --test-dir build/issue-price-verify -C Debug `
  -R "issue_price_support" `
  --output-on-failure
```

The verified Release scope is reproducible with:

```powershell
cmake --build build/issue-price-verify --config Release --parallel
ctest --test-dir build/issue-price-verify -C Release `
  -R "issue_price_support" `
  --output-on-failure
```

Omitting `-R` reproduces the full 57-test project suites recorded above.

## False-claim ledger

The public summary keeps these conclusions false:

```text
market hurdle discovered or empirically calibrated          false
fair value or accounting value estimated                    false
market-consistent discount curve or pricing measure used    false
bid, offer, executable spread, or rating produced            false
investor demand, suitability, or placement established       false
support-provider authority or budget established by engine   false
support counterparty or performance risk modeled             false
legal enforceability, tax, or regulation established         false
capital mobilization or financing additionality proven       false
animal-product displacement or welfare impact proven         false
```

Coherent source and use evidence proves only the specifically recorded cash
facts. Evidence identifiers are assertions in this engine, not document
authentication. Settlement does not calibrate the synthetic upstream cash paths or physical
probabilities, establish an investor hurdle, validate fair value, prove
provider performance before settlement, or demonstrate causal additionality.

## Residual limitations

Version 0.1 uses one flat annual effective hurdle per finite case. It does not
model a term structure, spread, liquidity, bid/offer, taxes, fees beyond the
declared `F` and `C`, legal rights, accounting classification, regulatory
capital, rating behavior, placement, bargaining, investor demand, support
counterparty default, appropriation risk, or secondary normalization. It does
not interpolate or optimize a hurdle.

All canonical inputs are invented `DEMO` data. Passing arithmetic shows only
that one fixed synthetic claim and one independently declared hurdle have a
conditional price-window overlap under the supplied physical probability set
and support capacity. It is not evidence that a real transaction is
financeable or that any capital provider will perform.
