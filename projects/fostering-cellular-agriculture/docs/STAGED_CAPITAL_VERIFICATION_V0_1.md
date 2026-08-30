# Internal Software Verification Record — Staged Capital v0.1

**Verification date:** 29 August 2026  
**Scope:** milestone-gated committed-capital state machine, cash and memo
ledgers, source tests, provider-performance stress, recovery waterfall,
weighted summaries, physical-P fee sensitivity, strict configuration, CLI, and
checked synthetic fixture  
**Result:** passed the checks below; residual limitations remain

This is a software verification record. It does not validate a project,
certificate, probability, recovery, legal obligation, market price, credit,
transaction, or animal-impact claim.

## Verified build

| Item | Value |
|---|---|
| Host | Windows x64 |
| Generator | Visual Studio 17 2022 |
| MSVC compiler | 19.44.35227.0 |
| CMake | 4.3.3 |
| Language | C++20, extensions disabled |
| Diagnostics | MSVC `/W4 /permissive- /WX` |
| Configurations | Debug and Release |
| Debug commands | `cmake --build --preset dev`; `ctest --preset dev` |
| Release commands | `cmake --build build/release --config Release`; `ctest --test-dir build/release -C Release --output-on-failure` |
| Result | 12 / 12 passed in Debug and 12 / 12 passed in Release |

## Mechanic and accounting controls

The staged-capital tests cover:

1. A deterministic completion hand table for sponsor contribution, provider
   draw, claim, terminal receipt, sponsor residual, and NPV.
2. Monthly effective PIK, claim capping, immediate cancellation of unused
   current-phase cap, and commitment fee only on future modeled availability.
3. Atomic sponsor, provider, and cost-to-complete stops before phase cash moves.
4. Exclusion of unreachable current-phase capacity from future sources and the
   provider-share constraint on deployable future sources.
5. End-of-phase final milestone failure after the funded phase has incurred
   cost, time, fee, and PIK.
6. Protected-reserve use at stop, recovery cash applied first to any remaining
   safe-workout shortfall, then provider claim, then sponsor residual.
7. Separate unrecovered funded principal and full PIK-bearing claim writeoff.
8. Delayed recovery, provider hurdle discounting, and negative fee sensitivity
   retained as a rebate rather than silently floored.
9. Later provider nonperformance preserving prior funded claim under the stated
   economic convention, without implying setoff or legal priority.
10. Every cash entry balancing across sponsor, ProjectCo unrestricted,
    provider, protected reserve, and external accounts.
11. Pathwise closure of ProjectCo cash, reserve cash, undrawn commitment, and
    funded-claim memo accounts.

## Probability, fee, and numerical controls

- Declared positive case weights may sum within `1e-12` of one. Paths retain
  declared weights; all outcomes, distributions, conditional statistics, and
  paired fee calculations normalize the raw sum, which remains reported.
- An adversarial near-one weight sum with a `1e9`-million upfront fee preserves
  normalized event weights and the fee-adequacy identity.
- Provider nonperformance governs every strictly positive draw obligation;
  occurrence weights likewise count every strictly positive modeled amount.
- Weighted inverse-CDF boundaries and fractional expected-shortfall tails use
  probability arithmetic rather than a monetary tolerance.
- Large money values pass at exact source boundaries and reject a material
  amount immediately outside the scale-aware tolerance.
- The fee basis replays every physical case at the same normalized relative
  weight with provider cash performance held true. No adverse physical state is
  dropped because the provider failed to fund it in the actual-case view.

## Parser, serialization, and resource controls

- Unknown, duplicate, missing, non-finite, incoherent, and non-synthetic inputs
  are rejected.
- Normalized configuration uses classic locale, decimal/default-float format,
  `max_digits10`, and canonical booleans. It round-trips tiny doubles even when
  the caller supplied fixed, hexadecimal, signed, localized, padded formatting.
  Caller flags, precision, width, fill, and locale are restored.
- Phase/case identifiers and currency labels use a bounded ASCII-safe token
  grammar. Free text rejects terminal controls and is length bounded.
- Files, lines, phases, cases, scheduled path months, aggregate case-months,
  recovery delays, money, rates, and claim multiples have explicit limits.

## Checked synthetic fixture

The CLI fixture is synthetic and expressly not calibrated to the Wilson public
dossier. CTest locks the following outputs so executable mechanics and the
documented case table cannot silently diverge:

| Actual outcome | Weight |
|---|---:|
| Completion | 55% |
| Final milestone failure | 20% |
| Cost-to-complete funding failure | 15% |
| Sponsor funding failure | 0% |
| Provider funding failure | 10% |

Selected fixture diagnostics, in synthetic `DEMO` millions:

| Diagnostic | Result |
|---|---:|
| Expected provider draw | 44.100000 |
| Expected unrecovered funded principal | 5.750000 |
| Principal-loss p95 / ES95 | 28.000000 / 28.000000 |
| Expected PIK-bearing claim writeoff | 7.978934 |
| Expected provider terminal receipt | 43.681502 |
| Physical-P zero-NPV upfront-fee sensitivity | 8.220632 |
| Charged fee | 3.000000 |
| Fee adequacy gap | -5.220632 |
| Maximum cash-entry imbalance | 0.000000 |
| Maximum commitment/claim memo imbalance | 0.000000 |

The `8.220632` result is conditional arithmetic under declared synthetic
physical weights, the paired provider-performance convention, and a 12%
provider hurdle. It is not fair value, a risk-neutral price, a market quote, a
financing recommendation, or evidence that the facility can be financed.

## Independent review incorporated

Separate mechanism, implementation, quantitative-control, and adversarial-test
reviews were used. Their findings led to, among other changes:

- a safety-first failure waterfall and separate reserve-at-stop versus final
  unresolved shortfall measures;
- paired all-provider-performs fee cases rather than excluding provider-default
  states;
- separate cash-principal loss and funded-claim writeoff;
- normalized near-one weights and exact probability-tail handling;
- consistent strict-positive event and Boolean semantics;
- canonical, locale-independent normalized output;
- text, parser, and aggregate-work resource bounds; and
- an exact executable-fixture regression instead of a generic CLI phrase test.

## Residual limitations

- All staged-capital cases, probabilities, costs, certificates, proceeds,
  recovery delays, workout needs, rates, and hurdles are unvalidated synthetic
  inputs. The Wilson dossier remains excluded from calibration.
- The cost-to-complete test is aggregate and provider-share constrained, not a
  phase-by-phase allocation of forecast cost to future phase caps. It can pass
  a case whose later cap distribution would be insufficient.
- Certificates are inputs. There are no invoice tests, certificate-generation
  rules, cure periods, draw notices, conditions precedent, dated milestones,
  long stops, disputes, fraud controls, or independent verifier workflow.
- Sponsor construction capacity is tested, but separate sponsor liquidity or
  default on upfront fees, commitment fees, and reserve funding is not modeled.
- Provider nonperformance has no modeled damages, setoff, clawback, forfeiture,
  replacement funding, collateral, wrong-way risk, or insolvency consequence.
- Completion and recovery fields are external cash-proceeds assumptions. There
  is no asset valuation, sale process, enforcement-cost model, or legal-priority
  engine. The terminal ordering is an economic convention only.
- There is no operating model, working capital, maintenance, revenue, offtake,
  debt service, refinancing, cash sweep, tax, accounting, regulatory-capital,
  hedge, Q-measure, CVA, or portfolio layer in this module.
- No financing additionality, output adoption, substitution, animal-derived
  displacement, or reduction in animal suffering is established by these
  calculations.
- The implementation has deterministic and adversarial regression tests, but
  no formal proof, fuzzing campaign, independent model validation, or real-data
  backtest.

## Release decision

Staged Capital v0.1 is suitable as a transparent synthetic contract-mechanics
baseline for research discussion, hand-table reconciliation, and identifying
the evidence and legal terms a real facility would require. It is not approved
for underwriting, capital commitment, security valuation, accounting, rating,
solicitation, execution, or impact assurance.
