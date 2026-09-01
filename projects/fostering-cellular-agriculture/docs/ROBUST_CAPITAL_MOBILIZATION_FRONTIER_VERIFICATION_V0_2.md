# Robust Capital-Mobilization Frontier v0.2 — Verification Supplement

Status: complete warnings-as-errors C++20 Emscripten 6.0.5 Release build and
Node-driven test suite passed 72/72 with zero failures, 1 September 2026.

This is an additive record. It does not overwrite the legacy
[v0.1 verification](ROBUST_CAPITAL_MOBILIZATION_FRONTIER_VERIFICATION_V0_1.md)
and does not revise the hand/oracle results reported there.

## Verified scope

The supplement verifies four v0.2 boundaries:

1. the five-input evaluator copies one Capital Stack v0.2 template and changes
   only `q` and junior issued principal `A`;
2. every market principal-risk mandate and result is populated from issued-
   principal cash shortfall `Q`, not from legacy asset-loss layering;
3. the v0.2 parser and normalized output use explicit `Q` and issued-principal
   language and reject the legacy risk-key namespace; and
4. the ten-claim same-pool command-line fixture evaluates all 25 declared
   candidates and returns the checked empty feasible set.

The complete Release build passed with warnings treated as errors, including
the issue-price browser target with its target-local `NODERAWFS` override. The
full Node-driven CTest run passed 72/72. Within that complete record, the v0.2
frontier core, parser, and command-line regressions all passed. No native
Windows model build is used or claimed by this record.

## Implementation and test surfaces

| Surface | Focused symbol or test | Verified role |
|---|---|---|
| public API | `RobustCapitalMobilizationFrontierSummary` | publishes both model versions, the `Q`-family flag, three separate aggregate limits, candidates, selections, audits, and non-claims |
| validation | `require_v02_base_stack_template(...)` | requires Capital Stack v0.2, exactly two ordered claims, matching IDs/cap/hurdles, and base `q`/`A` in the grids |
| candidate evaluation | `evaluate_candidate(...)` | dispatches v0.2 market risk to `E[Q]/M`, Q-ES95/M, Q-ES99/M, and `Pr[Q>0]` |
| core unit | `test_v02_frontier_uses_issued_principal_cash_shortfall_q()` | proves asset `L` and liability `Q` can diverge and that the frontier binds only `Q` |
| parser unit | `test_v02_schema_uses_q_and_issued_principal_language()` | checks v0.2 parse/print/parse stability, required labels, and cross-version rejection |
| CLI regression | `tests/check_capital_mobilization_frontier_v02_cli.cmake` | checks model versions, aggregate limits, `Q` labels, empty selections, false-claim ledger, and normalized five-file output |
| fixture | `scenarios/ten-claim-instrument-v1-synthetic/capital-mobilization-frontier-v0.2.cfg` | declares the checked finite grid and synthetic mandate |

The source compatibility names are documented rather than hidden. In v0.2,
the `maximum_market_expected_loss_fraction` member receives the public
`maximum_market_expected_issued_principal_cash_shortfall_fraction` term and is
populated from market `E[Q]/M`; the corresponding ES and incidence members are
routed the same way. The CLI and normalized configuration expose only the
honest v0.2 labels.

## Asset/liability separation oracle

The focused core test contains two one-state examples.

### Asset loss without issued-liability shortfall

```text
project outlay / funded reserve = 8
contractual asset-principal limit = 10
principal cash returned = 8
asset writeoff L = 2
total issued-principal shortfall Q = 0
junior A = 1
market M = 7
```

All four zero-`Q` market mandates pass. This verifies that the frontier does not
substitute the two units of asset writeoff for market issued-principal
shortfall.

### Issued-liability shortfall without asset loss

```text
project outlay / funded reserve = 12
contractual asset-principal limit = 10
principal cash returned = 10
asset writeoff L = 0
total issued-principal shortfall Q = 2
junior A = 1
market Q = 1
market M = 11
```

The direct Capital Stack v0.2 result and frontier candidate agree:

```text
market E[Q]/M       1/11
market Q ES95/M     1/11
market Q ES99/M     1/11
market Pr[Q>0]      1
```

A 5% maximum expected-`Q` mandate therefore fails and the frontier returns no
feasible candidate. Every legacy realized-principal-loss placeholder is zero
in this example, so this test would fail if the v0.2 frontier accidentally
routed the old risk family.

## Version and fixed-template controls

The focused core coverage rejects:

- frontier v0.2 entering the legacy four-input evaluator;
- frontier v0.1 entering the five-input evaluator; and
- a v0.2 base stack whose fixed market priority cap differs from the frontier
  term record.

The production validator also requires exactly one junior residual claim and
one market priority claim; matching claim identifiers and hurdle terms; zero
junior non-principal priority cap; and the base template's `q` and `A` in the
tested grids. Candidate construction copies the validated base stack and
changes only those two fields.

Common grid controls inherited from v0.1 remain active: empty or duplicate
grids, non-finite `q`, `q>1`, `A<=0`, `A>=K`, duplicate claim IDs, unsupported
hurdles, non-finite targets, invalid mandate ranges, non-synthetic status, more
than 1,024 candidates, and excessive structural work fail closed.

## Parser and normalized-output controls

The closed parser requires `frontier.model_version=0.2.0` and the public keys:

```text
junior_issued_principal_grid.*
mandate.maximum_market_expected_issued_principal_cash_shortfall_fraction
mandate.maximum_market_issued_principal_cash_shortfall_es95_fraction
mandate.maximum_market_issued_principal_cash_shortfall_es99_fraction
mandate.maximum_market_principal_cash_shortfall_probability
mandate.maximum_junior_issued_principal_million
```

The parser test verifies byte-stable parse/print/parse output, checks that the
v0.2 normalized form contains those keys, and checks that it contains none of
`principal_loss`, `impairment`, or `catalytic_first_loss`. It then proves both
directions of namespace closure: a legacy principal-loss key in v0.2 is
unknown, and a v0.2 `Q`-incidence key in v0.1 is unknown.

The CLI regression verifies that `--print-normalized` includes both the
complete Capital Stack v0.2 input and complete frontier v0.2 input, including
the five-point junior issued-principal grid and exact serialized mandate
values.

## Ten-claim same-pool fixture

### Declared work

```text
q = {0.5, 0.625, 0.75, 0.875, 1}
A = {10, 20, 30, 40, 50} DEMO million
cross-product = 25 candidates
fixed B = 24 DEMO million
fixed junior hurdle = 15 percent
fixed market hurdle = 8 percent
junior target NPV = 0
```

The input files are all explicitly synthetic. The retained Portfolio uses the
legacy at-par accounting convention but is passed through the separately
versioned Capital Stack v0.2 bridge. Therefore the checked aggregate project-
outlay limit, aggregate contractual asset-principal limit, and funded reserve
and issued-principal detachment `K` are each 100 in this fixture. The equality
is fixture-specific and is not asserted as a model identity.

### Declared mandate

| Metric | Required boundary |
|---|---:|
| robust aggregate NPV | absent |
| market robust NPV margin/M | at least 0 |
| worst market `E[Q]/M` | at most 0.10 |
| worst market Q ES95/M | at most 0.50 |
| worst market Q ES99/M | at most 0.60 |
| worst market `Pr[Q>0]` | at most 0.35 |
| worst market `Pr[NPV<0]` | at most 0.35 |
| worst market NPV-shortfall ES95/M | at most 0.60 |
| worst market NPV-shortfall ES99/M | at most 0.70 |
| worst market WAL | at most 10 years |
| junior issued principal `A` | at most 50 |
| junior NPV concession | at most 50 |

### Verified selection result

```text
frontier model version                         0.2.0
capital-stack model version                    0.2.0
tested candidate count                         25
feasible candidate indices                     none
nondominated feasible candidate indices        none
minimum tested feasible q                      none
least tested feasible A by q                   none
capital_mobilization_is_established             false
calibrated_execution_authorized                 false
```

The economic no-solution is a successful report, not a parse or process error.

### Disclosed candidate checks

| `(q,A;M)` | Robust market NPV | Worst `E[Q]/M` | Q ES95/M | Q ES99/M | Max `Pr[Q>0]` | Max `Pr[NPV<0]` |
|---|---:|---:|---:|---:|---:|---:|
| `(1,20;80)` | -25.733095 | 27.377875% (`21.9023`) | 87.5% (`70`) | 87.5% (`70`) | 60% | 100% |
| `(1,50;50)` | -5.569641 | approximately 12.161% | 80% (`40`) | 80% (`40`) | 28% | 54% |

At `(1,20)`, the market claim fails the robust NPV, expected-`Q`, both `Q`-tail,
`Q`-incidence, and negative-NPV screens. At `(1,50)`, the larger junior layer
brings `Pr[Q>0]` inside its 35% limit, but expected `Q`, both tail limits, and
negative-NPV probability still fail. These rows are diagnostics, not a claim
that only one mandate binds elsewhere in the grid.

## CLI label and non-claim regression

The ten-claim CLI check requires the public report to contain:

- both model versions;
- all three separately named aggregate limits;
- `K` only for funded reserve and issued-principal detachment;
- the junior **issued-principal** grid `A`;
- expected `Q`, Q-ES95, Q-ES99, and `Pr[Q>0]` mandate/result labels;
- the empty feasible and minimum-`q` selections;
- the statement that `A` is a `Q` layer, not causal asset-loss attribution;
- `capital_mobilization_is_established=false`; and
- `calibrated_execution_authorized=false`.

The same regression forbids the legacy CLI phrases “expected market principal
loss,” “principal-loss ES95,” “principal impairment probability,” and
“catalytic first-loss grid” in a v0.2 report.

## Downstream issue-price/support check

The separate issue-price/support module was checked on the same fixed
`q=1`, `A=20`, `M=80` claim after the finite priority-cap sensitivity selected
`B=24`. Its inputs declare `F=0`, maximum synthetic no-rights support `G=20`,
and settled support `S_obs=0`, producing an issuer floor of 60.

The downstream priority-cap file deliberately has a separate, relaxed
issue-price sensitivity mandate. Its 30% expected-`Q`, 90% Q-tail, 60%
`Pr[Q>0]`, and five-year WAL limits are not the strict frontier's 10%,
50%/60%, 35%, and ten-year limits. The structured output now carries this
scope, label and source note, says it is not the Capital Mobilization Frontier
mandate, and says price or support changes none of the fixed-risk metrics. The
same `q=1`, `A=20`, `M=80` point therefore remains rejected by the strict
25-candidate frontier even when it passes the separate sensitivity screen.

| Independent synthetic hurdle | Robust price ceiling `P*` | Conditional window |
|---:|---:|---|
| 0% | 74.575200 | `[60,74.575200]` |
| 5% | 60.955502 | `[60,60.955502]` |
| 8% | 54.266905 | none |
| 10% | 50.313857 | none |
| 15% | 41.898625 | none |
| 20% | 35.170807 | none |

The output status `financeable-window-found` means only that at least one
supplied arithmetic hurdle case overlaps the synthetic issuer floor. It also
reports `funded/escrowed-support-covered hurdle-case indices: none`. No support
funding, escrow, commitment, authority, budget, or settlement is evidenced,
and no price or hurdle is empirical. The relaxed sensitivity screen is not a
frontier pass, and the result is not a live financing window.

## Review findings and correction history

The v0.2 supplement closes three risks that the legacy term could not close:

1. it requires a Capital Stack v0.2 template rather than regenerating a legacy
   at-par stack from Portfolio commitment;
2. it routes market principal risk from `Q` and proves `L`/`Q` divergence with
   two opposite hand examples; and
3. it removes asset-loss and catalytic-first-loss language from the v0.2
   configuration and CLI while retaining the v0.1 four-input source contract
   for callers that recompile. Public result layouts gained fields; binary ABI
   compatibility with an older compiled client is not claimed.

The legacy v0.1 spec, fixture, evaluator, normalized schema, and verification
record remain available and unchanged.

## Residual limitations

The verified work is deterministic synthetic mechanics. It does not verify:

- empirical probability bounds or market mandates;
- a continuous optimizer or any term outside the declared grid;
- fair value, an executable issue price, market spread, rating, demand,
  suitability, placement, or liquidity;
- reserve custody, legal enforceability, tax, accounting, prudential treatment,
  or regulatory capital;
- support-provider authority, budget, funding, escrow, or performance;
- project or financing additionality; or
- qualified production, animal-product displacement, or welfare impact.

Different reported endpoints can have different probability witnesses. The
empty feasible set is a checked finite-grid rejection, not a universal
impossibility result and not authorization to execute another term.
