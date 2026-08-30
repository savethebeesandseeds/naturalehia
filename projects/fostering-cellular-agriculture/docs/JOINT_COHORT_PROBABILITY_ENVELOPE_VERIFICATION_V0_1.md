# Joint-Cohort Probability Envelope v0.1 — Verification Record

**Verification date:** 30 August 2026

**Scope:** deterministic synthetic implementation verification, not empirical
calibration or independent validation of a real cohort.

## Result

The joint-cohort core, exact-byte package loader, strict row-ledger parser,
generated probability-envelope handoff, per-project ambiguity ranges,
common-witness pool-tail attribution, direct fixed-`q` capital-stack bridge,
CLIs, fixtures, and regressions compile under the project's strict C++20
policy. The original cohort-only release recorded `40/40`; the integrated
inventory now contains 42 tests.

This verifies the stated mechanics: disclosed complete-joint-unit rows become a
conditional finite-sample physical-probability outer set, and that set is
projected through fixed participation-pool cash paths. It does not verify that
any real observations are IID, complete, correctly classified, or relevant to
a proposed pool. It does not establish a price, fair value, rating, legal form,
expected market return, or investment recommendation.

## Environment and replay

| Item | Value |
|---|---|
| Host | Windows, x64 |
| CMake | 4.3.3 |
| Generator | Visual Studio 17 2022 |
| Compiler | MSVC 19.44.35227.0, toolset 14.44.35207 |
| Windows SDK | 10.0.26100.0 |
| Language | C++20, extensions off |
| Diagnostics | `/W4 /WX /permissive-` |
| Configurations | Debug and Release |

The original cohort-only implementation build recorded:

| Configuration | Strict build | CTest result | Elapsed test time |
|---|---|---:|---:|
| Debug | passed | 40/40 | 29.40 s |
| Release | passed | 40/40 | 11.51 s |

A separate root-agent build in a second generated tree independently passed
`40/40` in Debug in 21.72 s and `40/40` in Release in 11.12 s. There were no
C++ compiler warnings or errors. MSBuild emitted environmental warning
`MSB8029` because both disposable build trees were beneath the system temporary
directory; it is not a source diagnostic.

The three cohort-only CTest cases were:

- `cellular_finance_joint_cohort_tests`;
- `cellular_finance_joint_cohort_config_tests`; and
- `cellular_finance_joint_cohort_cli_synthetic`.

The CLI regression also runs the successful, blocked, usage-error, and malformed
paths. The verified exit contract is `0`, `3`, `2`, and `1`, respectively. Every
path retains `calibrated_execution_authorized=false`.

The later financial integration adds:

- `cellular_finance_joint_cohort_capital_stack_tests`; and
- `cellular_finance_joint_cohort_capital_stack_cli_synthetic`.

The integrated CLI regression includes both `q=1` and `q=0.37`. The non-unit
case proves that selected-underlying receipts and NPV are recomputed for the
cash paths entering the waterfall while draws, exposure, and loss remain
unchanged.

The final integrated build and test record is:

| Configuration | Strict build | CTest result | Elapsed test time |
|---|---|---:|---:|
| Debug | passed | 42/42 | 26.69 s |
| Release | passed | 42/42 | 13.81 s |

There were no C++ compiler warnings or errors. The only build diagnostic was
MSBuild `MSB8029`, caused by using a disposable tree under the system temporary
directory rather than by project source.

## Controlling synthetic fixture

The five-file fixture contains:

- one hash-bound four-scenario, two-project portfolio;
- one 22-row primary cohort ledger and its binding configuration; and
- one two-row repeated-cluster ledger and its binding blocked configuration.

The primary frame has 18 matured rows, two not-yet-matured rows, and two
excluded rows. The exclusions remain disclosed outside the denominator under
one asserted outcome-blind rule frozen before eligibility. Therefore `N=20`,
the unknown count is `U=2`, and the matured counts in scenario-ID order are:

```text
common-loss                         1
common-success                     11
culture-loss-scaleup-success        3
culture-success-scaleup-loss        3
```

For family confidence `0.95`, `K=4`, and `N=20`, the independent hand formula

```text
epsilon = sqrt(log(2*K/alpha) / (2*N))
```

gives `0.356201270886...`. The generated primary bounds and unchanged declared
reference weights are:

| Joint state | Lower | Declared reference | Upper |
|---|---:|---:|---:|
| common loss | 0 | 0.02 | 0.506201270886... |
| common success | 0.193798729114... | 0.62 | 1 |
| culture loss / scale-up success | 0 | 0.18 | 0.606201270886... |
| culture success / scale-up loss | 0 | 0.18 | 0.606201270886... |

The reference is checked, not fitted. No descriptive empirical frequency is
published because the two included unknown outcomes have not matured. Goodman
is correspondingly unavailable for this fixture.

## Hand-reconciled financial result

The fixed portfolio has two DEMO-10-million commitments, a 24-month horizon,
and a zero annual declared hurdle. Project cash paths do not change when the
cohort is applied. Only the admissible scenario probabilities change.

| Metric | Minimum | Declared central | Maximum |
|---|---:|---:|---:|
| expected project draws | 20.000000 | 20.000000 | 20.000000 |
| expected investor receipts | 8.251169 | 19.200000 | 24.000000 |
| expected resolved principal loss at horizon | 0.000000 | 4.000000 | 13.124025 |
| principal impairment probability | 0.000000% | 38.000000% | 80.620127% |
| NPV using declared hurdle and physical-P weights | -11.948831 | -1.000000 | 3.800000 |
| principal-loss ES95 | 0.000000 | 14.000000 | 20.000000 |
| principal-loss ES99 | 0.000000 | 20.000000 | 20.000000 |
| NPV-shortfall ES95 | 0.000000 | 13.000000 | 20.200000 |
| NPV-shortfall ES99 | 0.000000 | 20.200000 | 20.200000 |

All monetary values are invented DEMO millions. The two project impairment
ranges are each `0% / 20% / 80.620127%`; simultaneous impairment is
`0% / 2% / 50.620127%`; any-project impairment is
`0% / 38% / 80.620127%`. Every endpoint prints its complete feasible
probability witness. Endpoint probability errors and central metric
reconciliation errors print as zero at report precision.

Each project separately reports expected draws `10 / 10 / 10`, receipts
`2.325585 / 9.600000 / 12.000000`, outstanding principal `0 / 0 / 0`, resolved
principal loss `0 / 2 / 8.062013`, and NPV before shared pool costs
`-7.674415 / -0.400000 / 2.000000`. Each scalar endpoint retains its own
probability witness. At the maximum-pool-ES measure, each project contributes
10 to both pool ES95 and ES99; the two contributions reconcile to the pool
total of 20 under one shared probability and fractional-tail witness.

The fixture's central NPV is negative and its uncertainty range is very wide.
It demonstrates conservative mechanics; it does not demonstrate an attractive
asset. The reported liquidity rows are expectations of each scenario's
pathwise peak, not a worst-path reserve requirement.

## Core and statistical controls

The dedicated core tests establish that:

- every result stays synthetic, candidate-only, and unauthorized;
- `N` includes matured, not-yet-matured, and unresolved rows;
- unknown rows remain compatible with every joint scenario;
- excluded rows remain disclosed and require a configured, asserted
  outcome-blind rule frozen before eligibility;
- date, status, taxonomy-freeze, scenario-mapping, and population-frame
  relationships fail closed;
- a repeated included cluster identifier blocks the statistical export but
  leaves descriptive counts visible;
- unique cluster IDs are reported only as a mechanical check, never proof of
  independence;
- `K=1`, zero cells, zero declared atom weights, and `epsilon>=1` are handled
  without pseudocounts;
- a declared reference outside the primary set blocks the financial export
  rather than being moved or replaced;
- Hoeffding component limits are rounded outward when converted for publication,
  and the family error rate is computed conservatively in extended precision;
- complete-cohort Goodman intervals reproduce the published 870-observation,
  ten-category numerical example and remain diagnostic only;
- Goodman never narrows or substitutes for the primary set;
- project, pair, any-project, and all-project impairment endpoints agree with
  direct indicator optimization; and
- every project's seven financial ranges agree with direct exact optimization,
  and shared ES95/ES99 project contributions reconcile to pool ES without
  presenting independent project maxima; and
- the pairwise witness-work cap omits only quadratic pair reports while
  preserving core financial and linear project outputs.

The primary finite-sample construction follows
[Hoeffding (1963)](https://doi.org/10.1080/01621459.1963.10500830). The
complete-cohort challenger follows
[Goodman (1965)](https://www.stat.cmu.edu/technometrics/59-69/VOL-07-02/v0702247.pdf).

## Package, parser, and CLI controls

The package loader reads each portfolio and ledger into a bounded immutable
byte snapshot, verifies the declared SHA-256 over that snapshot, and parses
those exact bytes. It re-resolves the paths and compares the bytes again before
return. Tests cover content drift, path aliasing, unsafe or escaping paths, and
portfolio/ledger identity.

Other adversarial tests cover:

- unknown, duplicate, missing, non-finite, hard-flag, measure, sampling, and
  method fields;
- deterministic ASCII identifiers, exact status tokens, strict calendar
  dates, and invalid programmatic enum values;
- one-mebibyte cohort configuration, 16-mebibyte portfolio, 32-mebibyte ledger,
  4,096-byte configuration-line, 8,192-byte ledger-line, 100,000-row, field,
  rule, and citation-count guards;
- malformed headers, rows, lists, classifications, exclusions, citations, and
  truncated population frames;
- canonical, locale-independent semantic rendering with complete caller
  stream-state restoration; the cohort configuration retains raw-file hashes,
  so normalized portfolio and ledger bytes require rehashing before they can be
  rebound as a new package;
- a canonically printed generated probability-envelope configuration that can
  be replayed separately by the ambiguity engine;
- full financial and cash-source endpoint witnesses, lower/upper/central mass
  controls, and numerical reconciliation controls; and
- interpretation labels that distinguish parameter confidence from ES95/ES99,
  expected pathwise peaks from reserve requirements, physical probabilities
  from the declared discount hurdle, and receipts from return rates.

## Independent audit corrections

The implementation changed materially during adversarial review:

- aggregate counts were rejected in favor of a hash-bound authoritative row
  ledger;
- the loader was changed from separate hash and parse reads to hashing and
  parsing the same bounded byte snapshot;
- the visible cluster check was renamed from an IID-satisfaction claim to
  `included_cluster_ids_unique`;
- incomplete cohorts stopped publishing matured-only frequencies as a center;
- zero-probability portfolio atoms were supported without letting them distort
  central distribution tails;
- financial and cash-source witnesses, portfolio context, and replayable
  generated-envelope output were added;
- a valid but blocked report was given fail-closed exit code `3`;
- component probability limits and family alpha were made directionally
  conservative at floating-point boundaries;
- pairwise reporting received a bounded-work guard; and
- exact project financial ranges and common-witness tail attribution were
  added, with pro-rata handling of tied objective and fractional-tail blocks;
- the fixed-`q` cohort-to-waterfall bridge was added without a second portfolio,
  probability fallback, or participation re-solve; and
- selected-`q` reporting was separated from the cohort's original hash-bound
  candidate portfolio view so non-unit participation cannot display stale cash;
- normalized package output was labeled as semantic rendering rather than a
  new hash-consistent package; and
- cash, NPV, impairment, confidence, and liquidity labels were narrowed to the
  quantities actually calculated.

Independent code/architecture, financial-engineering, and institutional-claims
reviews identified and then regression-tested these corrections.

## Residual limitations and next model

- Hashes prove byte identity, not truth, comparability, correct classification,
  IID sampling, or outcome-blind analyst conduct.
- The direct complete-joint-state design becomes sparse and exponentially large
  as projects and outcome states are added.
- The box-simplex outer set deliberately discards some multinomial and
  unresolved-row coupling information.
- A declared center outside the set blocks financial output; v0.1 has no
  central-free `NA` reporting mode.
- The generated set can now reach the fixed-`q` fully funded capital-stack
  waterfall, but neither underlying nor tranche results are security prices,
  spreads, ratings, or expected market returns.
- Inputs remain entirely synthetic and cannot authorize real execution.

The next financial-engineering model is a marginal/factor event-probability
polytope. It must express project impairment, pair co-impairment, and common
bioprocess, supplier, buyer, geography, and platform events without pretending
that a real many-facility cohort observes every atom of an exponentially large
joint state table. It will reuse the same project cash interface, exact
ambiguity projections, selected-`q` bridge, and cash-conserving waterfall.
