# Market Observation and Hurdle Evidence Set v0.1 — Verification Record

Status: core evaluator, strict configuration parser, reporting CLI, and
synthetic verification implemented. Full strict Debug and Release verification
recorded 2026-08-30.

## Verified scope

The implemented layer answers one bounded financial-engineering question:

> Given independently constructed, economically eligible price-observation
> records, which annual-effective expected-cash discount-rate regions are not
> ruled out by the required number of observation clusters?

The output is an exact closed set. It is not an average yield and does not
pretend that a transaction reveals the buyer's beliefs or private hurdle. Each
input interval is already conditioned on an external physical expected-cash
model, full dated claim cash, buyer cash and cost, and any evidenced
comparability bridge. The engine performs eligibility classification,
log-gross-return interval normalization, evidence-tier separation, exact
coverage-set construction, and finite audits. The economic definition and
evidence requirements are in the
[term specification](MARKET_OBSERVATION_AND_HURDLE_EVIDENCE_V0_1.md).

The implementation under test consists of:

- the [public core API](../include/naturalehia/cellular_finance/robust_hurdle_envelope.hpp);
- the [core evaluator](../src/robust_hurdle_envelope.cpp);
- the [configuration API](../include/naturalehia/cellular_finance/robust_hurdle_envelope_config.hpp);
- the [closed-schema parser and canonical printer](../src/robust_hurdle_envelope_config.cpp);
- the [reporting CLI](../apps/hurdle_evidence_cli/main.cpp);
- the [core tests](../tests/robust_hurdle_envelope_tests.cpp);
- the [configuration tests](../tests/robust_hurdle_envelope_config_tests.cpp); and
- the [CLI golden checker](../tests/check_hurdle_evidence_cli.cmake).

The reloadable synthetic input is
[`hurdle-evidence.cfg`](../scenarios/market-priority-cap-v0.1-synthetic/hurdle-evidence.cfg).
It is invented test data. Names that resemble evidence identifiers and
SHA-256 values in that file are assertions supplied to the engine; neither the
parser nor evaluator retrieves, hashes, verifies, or authenticates a document.

## Public result contract

One input record represents one already de-duplicated economic-observation
cluster. The engine preserves financially ineligible records and reports their
reasons. An eligible interval wholly outside the declared rate domain remains
an eligible empty set and can falsify agreement. It is not silently discarded.

Eligible records remain separated into this ordered hierarchy:

```text
1. settled identical target claim
2. settled comparable claim
3. executable transaction-anchored two-sided quote
```

The highest tier containing an eligible cluster controls. A lower tier is a
challenger diagnostic only; it cannot be pooled with, or outvote, the
controlling tier. This remains true when the controlling tier's predeclared
contamination budget is inadmissible. In that case the primary result is
`insufficient eligible clusters for declared contamination budget`, not a
fall-through to more numerous but weaker evidence.

Within a tier containing `n` eligible clusters and a predeclared discordance
budget `k`, the required coverage is `q=n-k`, with `0<=k<n/2`. For declared
closed domain `D`, the exact set is:

```text
S_k = { h in D : at least n-k eligible cluster intervals contain h }
```

The engine publishes `S_0` through `S_k`, each disjoint component, endpoint
coverage and boundary witnesses, maximum attained coverage, the interval hull
as an outer diagnostic only, binding and informative records, and
leave-one-cluster-out sensitivities. It preserves singleton intersections,
touching endpoints, empty sets, and unsupported gaps.

The summary's set-audit booleans are recomputed from the source intervals. The
audit checks every unique interval endpoint and every adjacent open cell,
rather than merely restating the constructed component list. It verifies that
selected points and cells meet the declared threshold and excluded points and
gaps do not.

## Exact synthetic oracle

The canonical hand fixture declares the domain `D=[0%,100%]`, three settled
comparable clusters, and `k=1`:

```text
H_1 = [ 8%, 10%]
H_2 = [ 9%, 12%]
H_3 = [11%, 14%]
```

With no discordance allowance, all three clusters must overlap. They do not:

```text
S_0 = empty
```

With one allowed discordant cluster, at least two of the three clusters must
contain the rate:

```text
S_1 = [9%, 10%] union [11%, 12%]
```

The exact coverage oracle is:

| Region | Cluster coverage | Included in `S_1` |
|---|---:|---:|
| below 8% or above 14% | 0 | no |
| 8% to below 9% | 1 | no |
| 9% through 10% | 2 | yes |
| above 10% to below 11% | 1 | no |
| 11% through 12% | 2 | yes |
| above 12% through 14% | 1 | no |

At exactly 10% and 11%, closed-endpoint semantics retain coverage two. The
open interval between them has coverage one. The reported selected set must
therefore contain two components. The hull `[9%,12%]` would fabricate support
in the 10%–11% gap and is forbidden as the primary result. The wider eligible
interval hull `[8%,14%]` is printed only as an explicitly labelled outer
diagnostic.

The synthetic CLI result selects the settled-comparable tier with `q=2` and
reproduces the two components to six printed decimal places. It also reports
that the input is synthetic, the result is only a mechanical candidate set,
and empirical evidence release is not authorized.

## Eligibility and normalization controls

The evaluator and parser fail closed around the economic basis. The regression
scope covers:

- settled execution, buyer cash, settlement, orderly-market, arm's-length,
  claim-quantity, connected-preimage, and supporting-record assertions;
- complete same-time, same-size, same-currency executable bid and ask evidence,
  a transaction-market anchor, strictly decreasing expected cash over the
  interval, and the required ask-to-lower/bid-to-upper rate mapping;
- the annual-effective all-in buyer-cash discount rate on physical expected
  full-claim cash, including expected loss and timing;
- the robust minimum over the declared physical-probability set as the only
  primary operator in v0.1; fixed-vector calculations remain report-only;
- independence from the target reference price and exclusion of promised
  yield, coupon, target-price-implied rate, unresolved return bases, side
  rights, and non-cash consideration;
- all eight comparability axes, exactly one bounded adjustment for each
  adjusted axis, and an exact jointly feasible total interval rather than a
  componentwise box hull;
- log-gross-return transformation with finite, ordered source and adjustment
  bounds, strict transformed-rate and finite identity guards, domain clipping,
  and a no-double-count declaration for expected loss, recovery, and timing;
  and
- a frozen universe, predeclared inclusion and de-duplication rules, unique
  cluster identifiers, same-dealer/same-window clustering, bounded record and
  adjustment counts, and checked structural work.

The date controls distinguish execution, settlement, observation, and
analysis dates. A settled record with settlement after the analysis as-of date
is retained but ineligible. A prior-date observation cannot mark the
observation-date/market-regime axis as matched. A prior-date comparable can
enter only through an evidenced bounded adjustment on that axis, including an
evidenced jointly feasible total adjustment interval. Observations after the
analysis date are also ineligible, and impossible calendar dates fail
validation.

## Parser, canonical replay, and CLI controls

The parser uses a closed versioned `key=value` schema. It rejects unknown,
duplicate, and missing keys; incorrect observation or adjustment counts;
unsafe or oversized identifiers and text; surrounding whitespace and embedded
BOMs; invalid booleans, enums, dates, integers, and non-finite or out-of-range
numbers; oversized files; and input, output, and file-loading failures.

Before printing, observations are ordered by stable record ID and adjustments
by stable adjustment ID. The Debug CLI golden regression extracts the printed
normalized configuration, reloads it, prints it again, and requires byte-for-
byte equality. This verifies canonical print-load-print replay rather than
merely comparing two in-memory structures.

The CLI golden checker also requires the exact two selected components and the
three set-audit flags, rejects a connected `[9%,12%]` selected component,
checks the false-claim flags, and distinguishes operational exits:

```text
0  completed report
1  invalid invocation
2  input, loading, parsing, or configuration failure
3  evaluation or report-output failure
```

All reported paths retain `calibrated_execution_authorized=false`.

## Independent audit and correction record

Independent mathematical review confirmed the hand oracle: the three-way
intersection is empty, the two-of-three set is exactly
`[9%,10%] union [11%,12%]`, and replacing it by its hull would be wrong.

Independent implementation review identified one P1 hierarchy defect before
this record was finalized. The initial selection logic could fall through from
a nonempty higher-quality tier when that tier's `k` was inadmissible, allowing
a lower tier to become primary. That contradicted the specified evidence
hierarchy. The logic was corrected so the highest nonempty tier controls and
reports insufficient evidence. A dedicated regression now combines one direct
settled-identical record with three eligible settled comparables under `k=1`;
it requires the direct tier to remain selected, no primary identified set to
be emitted, and the lower-tier calculation to remain only a diagnostic.

The same review led to explicit settlement/as-of and market-regime date guards
and to non-tautological set audits over endpoints and adjacent cells. Dedicated
tests cover future settlement, stale matched observations, an evidenced
prior-date bridge, selected component coverage, and excluded-gap coverage.

The audit then found and corrected four P2 evidence-boundary defects. First,
`mechanical_candidate_set_only` had depended on the synthetic-data label; it is
now always true because v0.1 cannot authenticate or authorize an empirical
release. Second, a source-rate cap had rejected otherwise valid connected
preimages wholly above `D`; finite ordered source rates above `-100%` now enter
eligibility, while the downstream domain remains bounded to `[0,10]`. Direct
above- and below-domain tests and a mixed three-cluster test prove that an empty
eligible `H_j` remains in `n` and `q` rather than disappearing from coverage.
Third, direct transactions, empty selected sets, and full-domain uninformative
sets could receive a comparable-consensus label from count alone. Consensus is
now restricted to a non-direct tier with a nonempty proper identified set and
the declared minimum coverage; all unavailable cases retain an unavailable
identification basis. Fourth, an extreme valid source rate plus a negative log
adjustment could make `expm1` round to exactly `-1`, yielding an infinite
normalization audit while leaving the record eligible. Transformed endpoints
must now remain strictly above `-1`, and the identity error must be finite; an
exact `nextafter(-1,0)` adversarial regression guards the boundary.

## Test inventory and observed status

The core regression covers direct transaction-conditioned cases, touching
closed intervals and singleton results, disjoint majority sets, tier
non-pooling, the corrected no-fall-through hierarchy, uninformative full-domain
sets, eligible empty mappings retained in coverage counts, direct-versus-
comparable consensus labels, transformed-rate numerical boundaries, evidence
and comparability exclusions, settlement and quote controls, date bridges,
invalid direct-API enums, exact `k<n/2`, deterministic canonical record order,
the 128-record guardrail, and leave-one-cluster-out work accounting.

The configuration regression covers closed-schema parsing, canonical ordering
and replay, exact dynamic counts, scalar and calendar validation, safe-text and
stream controls, the 16 MiB file limit, missing files, and the canonical
three-comparable fixture.

Only the following executions are claimed as observed here:

| Configuration and scope | Result |
|---|---:|
| Strict MSVC C++20 compilation (`/std:c++20 /EHsc /W4 /WX /permissive-`) | passed in Debug and Release |
| Integrated hurdle core, parser, and CLI scope | passed in Debug and Release |
| Full-project strict Debug suite | passed, 60/60 |
| Full-project strict Release suite | passed, 60/60 |
| Canonical CLI print-load-print replay and exact synthetic oracle | passed in Debug and Release |
| Independent mathematical and implementation review | clean after one P1 and four P2 boundary corrections; dedicated regressions added |

## Reproducible build and test commands

From the project root in a Visual Studio 2022 developer shell:

```powershell
cmake -S . -B build/hurdle-evidence-verify -G "Visual Studio 17 2022" -A x64 `
  -DNATURALEHIA_CELLULAR_FINANCE_WARNINGS_AS_ERRORS=ON
cmake --build build/hurdle-evidence-verify --config Debug --parallel
ctest --test-dir build/hurdle-evidence-verify -C Debug `
  -R "hurdle_evidence|robust_hurdle_envelope" `
  --output-on-failure
```

Omitting `-R` reproduces the full Debug project suite. Release verification,
as recorded above, uses the same commands with `Release` in place of `Debug`.

## False-claim ledger

The following conclusions remain false:

```text
real transaction-consistent hurdle evidence acquired          false
market hurdle or investor belief observed                     false
market hurdle point identified                                false
annual expected holding-period return inferred                false
statistical confidence or sampling coverage estimated         false
observation independence established                          false
fair value or market value estimated                          false
clearing price, buyer demand, or placement established        false
target physical probabilities empirically calibrated          false
source identifiers or assertion booleans authenticate files   false
legal, accounting, tax, or regulatory treatment established   false
calibrated execution authorized                               false
```

The fixture contains no real transaction record. Even a future eligible
transaction would observe a price, not a buyer's private hurdle. Its mapped
rate would remain conditional on the declared physical cash and probability
model and any comparability bridge.

## Residual limitations

Version 0.1 consumes externally calculated connected rate intervals; it does
not reconstruct price from raw contracts, estimate physical probabilities,
value side rights, infer adjustments, prove adjustment dependence, retrieve
documents, verify hashes, authenticate settlement, or estimate a yield curve.
It gives no statistical interpretation to the finite observation universe and
does not establish that cluster sources are independent.

The current verification is mechanical and synthetic. It proves that the
implemented finite-set operations preserve the declared hierarchy, exact
closed coverage, unsupported gaps, and interpretation boundaries. It does not
prove that a real hurdle exists, that the modeled target claim has a market
price or fair value, that investors will demand it, or that its expected
holding return has been measured.
