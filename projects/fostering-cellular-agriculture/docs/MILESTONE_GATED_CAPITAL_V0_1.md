# Milestone-Gated Committed Capital Module v0.1

Status: checked implementation description, 2026-08-29.

This document describes the behavior implemented in
[`staged_capital.hpp`](../include/naturalehia/cellular_finance/staged_capital.hpp),
[`staged_capital.cpp`](../src/staged_capital.cpp), and
[`staged_capital_config.cpp`](../src/staged_capital_config.cpp). The checked
source controls if this document and the implementation disagree.

## Purpose and hard boundary

The module evaluates one synthetic milestone-gated construction-capital
facility through a finite sequence of linear phases. At the start of each
reached phase, the sponsor and provider fund one aggregate eligible construction
use. A phase runs for a fixed number of months. Its certification decision is
applied at the end. Certification releases the next phase; final failure stops
the project.

The narrow hypothesis is that a provider can commit capital at financial close
while limiting each phase's exposure until the preceding phase has been
certified. The implementation shows the resulting cash transfers, commitment
and funded-claim mechanics, phase-stop outcomes, recovery, protected-workout
reserve, and a weighted fee sensitivity.

Version 0.1 accepts synthetic inputs only. It is not:

- a forecast, valuation opinion, fair value, market quote, offer, or advice;
- an empirical probability model or a risk-neutral pricing model;
- a finding that the provider commitment is legally enforceable;
- a construction-progress, invoice, engineering, or certification model;
- an operating-project, debt-service, refinancing, or cash-sweep model;
- evidence that a named facility is financeable or bankable; or
- evidence of qualified output, animal-product displacement, or animal-welfare
  impact.

The fixture is not calibrated to the Believer Meats Wilson dossier. The module
must not be presented as a Wilson financing proposal.

## Implemented economic form

The provider has a positive aggregate commitment divided into positive phase
caps. The provider funds a configured share of each reached phase's aggregate
eligible cost, limited by the current phase cap and remaining commitment. The
sponsor funds the rest, limited by its construction commitment.

Actual provider draws create a limited-recourse funded claim. The claim earns a
capped payment-in-kind return during each funded phase and, after a failure,
during the declared recovery delay. There is no interim repayment. At
completion, declared completion cash proceeds pay the provider claim first and
the sponsor receives any residual. On a
failed path, the protected reserve is applied at the stop date; delayed recovery
proceeds then pay any declared safe-workout shortfall before the provider claim,
and the sponsor receives any residual after both.

The model's completion priority and its failure ordering of protected safety
obligations, provider claim, and sponsor residual are economic conventions, not
legal conclusions about security, statutory priority, insolvency, or
subordination.

## Configuration schema

The strict `key=value` parser rejects unknown, duplicate, and missing keys.
Every case must contain one input record for every configured phase, including
phases the case will not reach.

### Scenario fields

```text
scenario.model_version
scenario.label
scenario.source_note
scenario.currency_label
scenario.monetary_basis
scenario.synthetic_inputs
```

The checked model requires `scenario.model_version=0.1.0` and
`scenario.synthetic_inputs=true`.

### Facility fields

```text
facility.provider_commitment_million
facility.sponsor_construction_commitment_million
facility.provider_cost_share
facility.annual_pik_rate
facility.claim_cap_multiple
facility.annual_commitment_fee_rate
facility.upfront_fee_million
facility.provider_hurdle_rate
facility.sponsor_discount_rate
facility.protected_workout_reserve_million
```

`provider_commitment_million`, `sponsor_construction_commitment_million`, and
`protected_workout_reserve_million` must be strictly positive. The provider
cost share must be strictly between zero and one. The provider commitment must
equal the sum of all phase caps. A zero-provider-commitment mode is not
implemented.

The sponsor construction commitment excludes the upfront fee, commitment fees,
and protected reserve. Those are additional sponsor cash requirements. The
engine assumes the sponsor pays all three outside the construction commitment
and records them in sponsor cash call and NPV. It has no separate sponsor-
liquidity balance or funding-default test for the upfront fee, monthly
commitment fees, or protected reserve.

### Phase fields

For phase number `N`:

```text
phase.N.id
phase.N.duration_months
phase.N.provider_stage_cap_million
```

Phase identifiers are unique. Duration is a positive number of whole months.
The phase cap is positive. There is no configured phase start date,
certification date, milestone long stop, or monthly spend schedule.

### Case fields

For case number `N`:

```text
case.N.id
case.N.weight
case.N.completion_value_million
case.N.recovery_value_million
case.N.recovery_delay_months
case.N.required_workout_cost_million
```

`recovery_value_million` is gross ProjectCo recovery cash before the modeled
safe-workout-shortfall diversion and excludes the protected reserve. Realization,
enforcement, sale, and similar costs other than the separately declared
`required_workout_cost_million` must already be deducted from this input. The
same safe-workout need must not also be netted from recovery value, or it would
be counted twice.

For phase `M` within case `N`:

```text
case.N.phase.M.actual_eligible_cost_million
case.N.phase.M.estimated_cost_to_complete_million
case.N.phase.M.certification
case.N.phase.M.provider_funds
```

Certification is exactly `certified` or `final-failure`. Provider performance
is exactly `true` or `false`. The estimated cost to complete cannot be less
than the current phase's actual eligible cost.

Every case weight is strictly positive. The raw declared sum must be within
`1e-12` of one. The weights are analyst-declared synthetic physical-measure
weights; the engine draws no random numbers and does not infer probabilities
from the cases. Individual path records retain declared weights. Every
aggregate outcome, distribution, conditional statistic, and fee calculation
uses `declared_weight / raw_declared_weight_sum`, and the summary exposes that
raw sum as `configured_case_weight_sum`.

Fee-sensitivity eligibility is not a configuration field. The actual configured
cases, including their provider-performance flags, drive the project-risk
summary. For the fee sensitivity, the engine creates a paired copy of every
case, holds all provider-performance flags true, re-evaluates the same physical
case, and retains its relative weight. A false flag in an unreached phase or a
zero-entitlement phase therefore has no effect on the actual path and is also
held true in the paired fee replay.

### Supported input domain and numerical guardrails

The validator accepts between `1` and `32` phases and between `1` and `256`
cases. Each phase duration is between `1` and `1200` months, the sum of all
configured phase durations is at most `1200` months per path, and:

```text
case_count * scheduled_path_months <= 100000.
```

Each recovery delay is at most `1200` months. Every bounded money input is at
most `1e9` million; each annual rate is at most `5.0`; and the claim cap multiple
is between `1.0` and `10.0`. These are computational-domain guardrails, not
claims that such inputs are economically reasonable.

Configuration files are limited to `16 MiB` and individual lines to `4096`
bytes. Scenario label, source note, and monetary-basis text are each limited to
`1024` bytes; phase and case identifiers to `128` bytes; and the currency label
to `32` bytes. All text rejects C0 and DEL controls. Identifiers and the currency
label use only ASCII letters, digits, hyphen, underscore, and period so they
cannot forge pipe-delimited audit rows. These are input and memory-integrity
controls, not financial assumptions.

For every case, the final phase's
`estimated_cost_to_complete_million` must equal its
`actual_eligible_cost_million` within numerical tolerance. Case weights must
sum to one within the separate probability tolerance of `1e-12`.

The scale-aware numerical tolerance used for monetary comparisons and
reconciliations is:

```text
tolerance(scale)
  = 1e-10 million
    + 64 * machine_epsilon * max(1, abs(scale)).
```

The `1e-10` floor is in the configured million-unit basis. A comparison may use
the relevant source, balance, or larger compared value as `scale`, as specified
in the code. It is a numerical reconciliation tolerance, not an event
materiality threshold. Boolean provider performance and summary occurrence
weights apply to every strictly positive modeled obligation or result.

## Exact phase timing

Financial close is month `0`. The upfront fee and protected reserve are posted
at month `0` before the first phase is evaluated.

For each reached phase, the engine uses this sequence:

1. Set the phase start to the current month.
2. Calculate provider entitlement, the required sponsor construction
   contribution, and the unused part of the current phase cap.
3. Test sponsor capacity for that current construction contribution.
4. Apply the aggregate/share-constrained remaining cost-to-complete screen,
   excluding unused current-phase capacity from future sources.
5. Test the current phase's provider-performance flag.
6. If those tests pass, post the entire sponsor contribution, provider draw,
   and aggregate eligible construction use at the phase start month.
7. Immediately cancel any unused part of the current phase's provider cap.
8. For each fixed month of phase duration, advance the clock by one month,
   accrue capped PIK, and charge the sponsor-paid commitment fee.
9. Apply the phase certification decision.
10. If certified, begin the next phase at that same phase-end month. If the
   final phase is certified, settle completion. If certification is
   `final-failure`, settle the failure after its configured recovery delay.

Thus, a six-month first phase drawn at month `0` ends at month `6`; a certified
second phase is drawn at month `6`, not month `7`. Phase duration controls only
the clock, PIK accrual, and commitment-fee months. The full aggregate phase cost
is paid at phase start; there are no monthly construction-spending inputs.

Certification is an exogenous end-of-phase decision. The finance model does not
create, verify, or time a certificate. A phase that ends in final failure has
already been funded, spent, and charged its full configured duration of PIK and
commitment fees.

## Phase release and failure logic

The implementation is a linear phase loop rather than a separately persisted
state enum. Its terminal outcomes are:

```text
completed
milestone-failure
cost-to-complete-failure
sponsor-funding-failure
provider-funding-failure
```

Phase `1` is tested at month `0`. Each later phase is reached only when the
previous phase was funded and ended `certified`. Phases cannot be skipped or
reordered.

### Current-phase entitlement and sponsor construction test

Before any current-phase cash moves:

```text
provider_draw_entitlement
  = min(actual_eligible_cost * provider_cost_share,
        current_phase_provider_cap,
        remaining_provider_commitment)

sponsor_contribution_required
  = actual_eligible_cost - provider_draw_entitlement

unused_current_phase_cap
  = max(0,
        current_phase_provider_cap - provider_draw_entitlement).
```

If the required sponsor contribution exceeds remaining sponsor construction
commitment beyond tolerance,
the outcome is `sponsor-funding-failure`. Its funding gap is the sponsor
shortfall. The project stops before the remaining cost-to-complete screen,
provider-performance test, or current phase cash moves.

This sponsor test covers the current phase's construction contribution only.
It does not test whether the sponsor has cash for the upfront fee, commitment
fees, or protected reserve; those payments are assumed performed.

### Aggregate/share-constrained remaining cost-to-complete screen

Only after the current sponsor construction test passes, the engine removes the
current phase cost from the declared estimate and screens the remaining cost:

```text
remaining_cost_estimate
  = max(0,
        estimated_cost_to_complete - actual_eligible_cost)

future_provider_commitment
  = max(0,
        remaining_provider_commitment
        - provider_draw_entitlement
        - unused_current_phase_cap)

future_provider_sources
  = min(future_provider_commitment,
        provider_cost_share * remaining_cost_estimate)

future_sponsor_sources
  = max(0,
        remaining_sponsor_construction_commitment
        - sponsor_contribution_required)

future_sources
  = future_sponsor_sources + future_provider_sources

cost_to_complete_gap
  = remaining_cost_estimate - future_sources.
```

If the gap is positive beyond tolerance, the outcome is
`cost-to-complete-failure`. The project stops before that phase's draw and use.
The unused part of the current phase cap is excluded because the module permits
only one current-phase draw; that unused capacity is unreachable at this draw
date and cannot support future costs.

This is an aggregate, provider-share-constrained screen. It is not a
phase-by-phase allocation of the remaining cost estimate, and it does not test
future estimated phase costs individually against future phase caps. It does
not add protected-reserve cash, ProjectCo cash, external financing, a grant, or
expected completion or recovery value. The estimate remains an analyst input,
not model-generated foresight.

### Provider performance and current-phase funding

If provider entitlement is strictly positive and `provider_funds=false`, the
outcome is `provider-funding-failure`. Its funding gap equals the missed provider
entitlement. This test occurs after the sponsor construction test and remaining
cost screen but before current-phase cash moves, so no sponsor or provider
construction cash is posted for that phase.

If both parties perform, their sources equal the aggregate eligible use exactly:

```text
sponsor_contribution + provider_draw_received
  = actual_eligible_cost.
```

The sponsor absorbs any phase cost not covered by provider entitlement. The
provider share is therefore a maximum share subject to the phase cap, not a
promise that the provider will fund that percentage above the cap.

### Cancellation

Immediately after the phase's single aggregate draw at its start:

```text
unused_current_phase_cap
  = max(0, provider_stage_cap - provider_draw_received).
```

That unused phase capacity is cancelled immediately, does not roll into a later
phase, and never enters the phase's commitment-fee base. On any terminal
outcome, all remaining undrawn commitment is cancelled. No commitment fee
accrues after termination or during the recovery delay.

There is no cure, waiver, re-baselining, draw resumption, replacement provider,
or restart after a failure.

## Cash ledgers

Every accepted cash entry is timestamped and balances across five accounts:

| Account | Meaning |
| --- | --- |
| `sponsor_million` | Sponsor contributions and fees paid; reserve release and terminal residual received |
| `project_unrestricted_million` | Construction sources, aggregate construction use, completion or recovery proceeds, terminal distributions |
| `provider_million` | Actual draws paid; fees and terminal repayment received |
| `protected_reserve_million` | Sponsor-funded protected reserve, workout use, and unused release |
| `external_million` | Aggregate construction-use recipient, workout-use recipient, and source of completion or recovery proceeds |

The implemented cash-flow kinds are:

```text
upfront-fee
protected-reserve-funding
sponsor-construction-contribution
provider-draw
eligible-construction-use
commitment-fee
completion-or-recovery-proceeds
workout-shortfall-use-from-recovery
provider-repayment
sponsor-residual-distribution
protected-workout-use
protected-reserve-release
```

Examples of exact postings are:

```text
sponsor construction contribution: sponsor -X, project +X
provider draw:                      provider -X, project +X
eligible construction use:         project -X, external +X
upfront or commitment fee:         sponsor -X, provider +X
reserve funding:                    sponsor -X, reserve +X
completion or recovery proceeds:   external -X, project +X
workout shortfall from recovery:    project -X, external +X
provider repayment:                project -X, provider +X
sponsor residual:                  project -X, sponsor +X
```

Each posting must sum to zero. The path result retains the event ledger and its
timestamps for audit, but v0.1 does not promise a complete exported monthly cash
statement. Construction spending is an aggregate phase-start event, not a
month-by-month schedule.

The external account is a balancing account for parties outside the modeled
sponsor/ProjectCo/provider/reserve perimeter. It is not an input for external
committed construction finance.

## Sponsor-paid fees outside ProjectCo

The sponsor pays `facility.upfront_fee_million` directly to the provider at
month `0`. It never enters ProjectCo and does not reduce provider commitment.

During every funded phase month:

```text
commitment_fee_month
  = remaining_provider_commitment
    * annual_commitment_fee_rate / 12.
```

The fee is paid directly by the sponsor. `remaining_provider_commitment` is
measured after the current phase draw and after immediate cancellation of the
unused current-phase cap. Commitment fees therefore cover only still-reachable
future-phase availability; they do not charge for unused capacity assigned to
the current phase.

An immediate pre-draw failure pays no phase commitment fees. The upfront fee is
still present because it was paid at close.

## Commitment memo rollforward

For every reached phase:

```text
opening_undrawn_commitment
  - provider_draw_received
  - cancelled_availability
  = closing_undrawn_commitment.
```

The aggregate provider commitment must equal the sum of phase caps. At a
terminal result, closing undrawn commitment is zero because the remaining
amount is cancelled.

The path reports opening and closing undrawn commitment, provider draw received,
cancelled availability, total unused commitment cancelled, and the maximum memo
rollforward imbalance.

## Funded-claim memo rollforward and PIK cap

Provider draws are added to the funded claim at the phase start. For each month
of phase duration:

```text
monthly_pik_rate
  = pow(1 + annual_pik_rate, 1/12) - 1

claim_cap
  = claim_cap_multiple * cumulative_provider_principal

next_claim
  = min(claim_cap,
        opening_month_claim * (1 + monthly_pik_rate)).
```

After a failed outcome, the same monthly accrual continues for exactly
`recovery_delay_months`, subject to the same cap. No PIK accrues after completion
or recovery settlement.

For every reached phase, the memo identity is:

```text
opening_funded_claim
  + provider_draw_received
  + contractual_return_accrued
  - provider_repayment
  - claim_writeoff
  = closing_funded_claim.
```

`contractual_return_accrued_million` is the actual capped accrual. Version 0.1
does not report a separate hypothetical uncapped accrual or PIK-denied amount.
At terminal settlement, unpaid claim is written off and closing funded claim is
zero.

## Protected-workout reserve

The sponsor funds the entire positive protected reserve at month `0`, separately
from ProjectCo and construction commitment.

On successful completion, the full reserve is returned to the sponsor at the
completion month.

On any failed outcome, at the outcome month:

```text
protected_workout_spend
  = min(protected_workout_reserve,
        required_workout_cost)

protected_reserve_shortfall_at_stop
  = max(0,
        required_workout_cost - protected_workout_reserve)

protected_reserve_release
  = protected_workout_reserve - protected_workout_spend.
```

Workout use is one aggregate event, not monthly spending. Any unused reserve is
returned immediately to the sponsor. At the outcome date,
`safety_funding_shortfall_million` is initialized to
`protected_reserve_shortfall_at_stop_million`.

On a failed path, recovery proceeds later reduce that shortage before they can
pay the provider:

```text
workout_shortfall_paid_from_recovery
  = min(recovery_value,
        protected_reserve_shortfall_at_stop)

safety_funding_shortfall
  = protected_reserve_shortfall_at_stop
    - workout_shortfall_paid_from_recovery.
```

The two shortfall fields are deliberately different. If recovery is delayed,
`protected_reserve_shortfall_at_stop_million` records the modeled outcome-date
shortage throughout the intervening period even when later proceeds reduce or
eliminate `safety_funding_shortfall_million` at settlement. The model does not
determine whether the timing or amount is legally or ethically sufficient for a
real transaction.

## Completion and delayed-recovery settlement

### Completion

If the final reached phase ends `certified`, the outcome is `completed` at that
phase's end month. The configured `completion_value_million` enters ProjectCo
from the external account in the same month.

```text
provider_repayment = min(provider_claim, completion_value)
sponsor_residual = completion_value - provider_repayment.
```

The provider is paid first and the sponsor receives the residual. Any unpaid
claim is written off. The protected reserve is returned separately to the
sponsor.

### Failure and recovery delay

Cost-to-complete, sponsor-funding, and provider-funding failures occur before
the stopped phase's draw. Milestone failure occurs at the end of a phase after
its cost, duration, fees, and PIK have been processed.

The recovery month is:

```text
recovery_month = outcome_month + recovery_delay_months.
```

PIK accrues through the delay. At the recovery month,
`recovery_value_million` enters ProjectCo and follows this waterfall:

```text
proceeds_after_protected_obligations
  = recovery_value - workout_shortfall_paid_from_recovery

provider_repayment
  = min(provider_claim, proceeds_after_protected_obligations)

sponsor_residual
  = proceeds_after_protected_obligations - provider_repayment.
```

Recovery proceeds are declared case inputs. They are not derived from asset
value, sale costs, creditor priority, or market data. The safe-workout diversion
is capped at the declared shortage at stop; it is not an independently validated
workout invoice or liability.

Despite its field name, `provider_nominal_recovery_million` is the provider's
terminal cash receipt in either state: repayment from completion value on a
completed path or repayment from the recovery value left after protected safety
obligations on a failed path. It is not a failure-only recovery metric.

Provider principal loss is:

```text
provider_principal_loss
  = max(0, cumulative_provider_draws - provider_repayment).

provider_claim_writeoff
  = provider_claim_at_exit - provider_repayment.
```

Principal loss is unrecovered funded cash principal. It is distinct from the
writeoff of the full PIK-bearing claim, which also includes any unpaid accrued
contractual return.

If a provider funds an earlier phase and later fails to fund a reached phase,
the model leaves the provider's previously funded claim intact, continues the
configured capped PIK through the recovery delay, and gives that claim the same
modeled priority after any remaining safe-workout shortfall. It models no
setoff, damages, clawback, forfeiture, replacement funding, or netting
consequence for the later provider nonperformance.

## Physical-P fee sensitivity

The implementation first evaluates every declared case with its configured
provider-performance flags for project-risk and loss reporting. It then copies
the entire configuration, sets `provider_funds=true` in every phase of every
case, and re-evaluates all cases for the provider fee sensitivity. Physical
costs, certifications, terminal values, recovery delays, workout costs, case
identifiers, and relative case weights are unchanged.

This paired all-performance replay prevents provider nonperformance from
improving the provider's own fee calculation without dropping an adverse
physical case or allowing analyst-selected exclusions. The replayed weights are
normalized by the same accepted near-one raw sum, so
`fee_sensitivity_included_weight` is `1.0`.

For each paired fee case, provider NPV before upfront fee includes provider
draws, commitment fees, and terminal provider repayment. It excludes only the
upfront fee. Discounting uses:

```text
discount_factor(t)
  = pow(1 + provider_hurdle_rate, -t/12).
```

```text
expected_provider_npv_before_upfront
  = sum_s case_weight_s
      * paired_all-performance_provider_npv_before_upfront_s

physical_measure_break_even_upfront_fee
  = -expected_provider_npv_before_upfront

expected_provider_npv_after_charged_upfront
  = expected_provider_npv_before_upfront
    + charged_upfront_fee

upfront_fee_adequacy_gap
  = charged_upfront_fee
    - physical_measure_break_even_upfront_fee.
```

The last two quantities must reconcile exactly within tolerance.

An actual provider-funding-failure case remains in outcome, loss, funding-gap,
sponsor-cash-call, stranded-spend, safety-shortfall, and commitment-utilization
summaries. Its paired fee case assumes provider performance and may therefore
reach later phases or a different terminal outcome. The paired replay is a
pricing convention, not a provider credit model or a statement that replacement
funding exists.

The break-even fee is a physical-measure, declared-hurdle sensitivity. It is not
Q-measure pricing, arbitrage-free value, IFRS or accounting fair value, a market
quote, or evidence that the charged fee is adequate after expenses, capital,
liquidity, tax, legal, or model risk.

## Implemented path and weighted outputs

The path result includes:

- outcome, stopped phase, outcome month, and recovery month;
- reached and funded flags and phase start/end months;
- eligible cost, required and actual sponsor contribution, and provider
  entitlement and received draw;
- funding gap and certification decision;
- commitment fees and commitment memo accounts;
- capped contractual return, provider repayment, claim writeoff, and funded-
  claim memo accounts;
- cumulative eligible spend, provider draws, provider peak principal and peak
  net cash outlay;
- sponsor construction contributions and total commitment fees;
- provider claim at exit, nominal and present-value recovery, principal loss,
  and sponsor residual;
- cancelled commitment, stranded spend, protected workout use, reserve release,
  protected-reserve shortfall at stop, workout shortfall paid from recovery,
  and final safety-funding shortfall;
- provider NPV before and after upfront fee and sponsor NPV;
- closing ProjectCo, reserve, commitment, and claim balances; and
- maximum cash-entry and memo-rollforward imbalance.

The weighted summary includes outcome weights, provider-draw, principal-loss,
claim-writeoff, protected-reserve-shortfall-at-stop, and final-safety-shortfall
weights; expected commitment utilization; expected provider recovery, principal
loss, and claim writeoff; conditional principal loss and claim writeoff when
applicable; the paired fee-replay weight of `1.0`; paired all-performance
provider NPVs; the break-even fee and adequacy gap; and actual-case weighted
summaries of provider draws, peak provider outlay, provider principal loss,
provider claim writeoff, sponsor total cash call, stranded spend, funding gap,
protected-reserve shortfall at stop, final safety-funding shortfall, and outcome
month.

It also retains one auditable paired fee-basis result per original case:
original case identifier and normalized weight, all-provider-performs outcome,
outcome month, terminal-settlement month, provider draws, terminal provider receipt under the
`provider_nominal_recovery_million` field, and provider NPV before and after the
upfront fee. Thus completion receipts and failure-recovery receipts are both
visible case by case. The CLI prints the case, weight, paired outcome, outcome
month, terminal-settlement month, draw, terminal receipt, and provider NPV before
fee so that the timing and pricing basis are not hidden in an aggregate. The
actual-case table exposes the same two dates.

Weighted summaries report mean, population standard deviation, discrete
weighted `p50`, `p95`, and `p99`, maximum, and upper-tail expected shortfall at
95% and 99%.

The command-line summary is aggregate. Although the API retains timestamped
cash ledger entries, v0.1 does not claim a complete exported cash-by-month
construction model.

## Exact synthetic fixture

The controlling checked fixture is
[`milestone-gated-capital-synthetic.cfg`](../scenarios/milestone-gated-capital-synthetic.cfg).
No value is sourced from the Wilson dossier.

### Common facility terms

| Configuration field | Fixture value |
| --- | ---: |
| `scenario.model_version` | `0.1.0` |
| `scenario.currency_label` | `DEMO` |
| `scenario.monetary_basis` | `constant synthetic monetary units at financial close` |
| `facility.provider_commitment_million` | `60` |
| `facility.sponsor_construction_commitment_million` | `100` |
| `facility.provider_cost_share` | `0.40` |
| `facility.annual_pik_rate` | `0.08` |
| `facility.claim_cap_multiple` | `1.50` |
| `facility.annual_commitment_fee_rate` | `0.01` |
| `facility.upfront_fee_million` | `3` |
| `facility.provider_hurdle_rate` | `0.12` |
| `facility.sponsor_discount_rate` | `0.12` |
| `facility.protected_workout_reserve_million` | `10` |

### Phase terms

| Phase | `duration_months` | `provider_stage_cap_million` |
| --- | ---: | ---: |
| `initial-engineering-and-development` | `6` | `6` |
| `design-permitting-contracts-and-procurement` | `12` | `24` |
| `equipment-delivery-installation-and-factory-acceptance` | `10` | `21` |
| `mechanical-completion-and-controlled-commissioning` | `8` | `9` |

In the completed case the phases start at months `0`, `6`, `18`, and `28`, and
the final phase ends at month `36`.

### Cases and terminal inputs

| Case | Weight | Paired fee-replay outcome | Completion value | Recovery value | Recovery delay | Workout cost |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| `qualified-completion` | `0.55` | `completed` | `210` | `0` | `0` | `0` |
| `equipment-acceptance-failure` | `0.20` | `milestone-failure` | `0` | `25` | `12` | `12` |
| `cost-to-complete-stop-after-development` | `0.15` | `cost-to-complete-failure` | `0` | `5` | `6` | `6` |
| `provider-funding-failure-at-first-draw` | `0.10` | `completed` | `210` | `0` | `0` | `1` |

### Phase inputs by case

Each cell is `actual eligible cost / estimated cost to complete /
certification / provider funds`.

| Case | Phase 1 | Phase 2 | Phase 3 | Phase 4 |
| --- | --- | --- | --- | --- |
| `qualified-completion` | `15 / 150 / certified / true` | `60 / 135 / certified / true` | `52.5 / 75 / certified / true` | `22.5 / 22.5 / certified / true` |
| `equipment-acceptance-failure` | `15 / 150 / certified / true` | `60 / 135 / certified / true` | `52.5 / 75 / final-failure / true` | `22.5 / 22.5 / certified / true` |
| `cost-to-complete-stop-after-development` | `15 / 150 / certified / true` | `70 / 160 / certified / true` | `52.5 / 75 / certified / true` | `22.5 / 22.5 / certified / true` |
| `provider-funding-failure-at-first-draw` | `15 / 150 / certified / false` | `60 / 135 / certified / true` | `52.5 / 75 / certified / true` | `22.5 / 22.5 / certified / true` |

The phase-4 row in `equipment-acceptance-failure` and phases 2 through 4 in
`provider-funding-failure-at-first-draw` are required parser inputs but are not
reached.

The fixture produces these timing facts by construction:

- `qualified-completion` completes at month `36`;
- `equipment-acceptance-failure` funds and spends phase 3, fails certification
  at month `28`, and receives recovery proceeds at month `40`;
- `cost-to-complete-stop-after-development` completes phase 1, then fails the
  phase-2 aggregate/share-constrained screen at month `6`. Its phase-2 provider
  entitlement is `24`, required sponsor construction contribution is `46`, and
  that current sponsor requirement fits within the remaining `91`. The remaining
  cost estimate is `160 - 70 = 90`; future sponsor sources are `91 - 46 = 45`;
  and future provider sources are `min(54 - 24, 0.40 * 90) = 30`. Aggregate
  future sources of `75` leave a `15` cost-to-complete gap. Recovery is at month
  `12`; and
- `provider-funding-failure-at-first-draw` stops at month `0` before any
  construction contribution or use, but the month-0 upfront fee and protected
  reserve postings have already occurred. Its configured completion value of
  `210` is unreachable and ignored.

All four actual cases enter the project-risk summaries with their stated
weights. All four paired provider-performance replays enter the fee sensitivity
with those same weights, which already sum to `1.0`. In the fourth replay,
holding provider performance true changes the outcome from provider funding
failure at month `0` to completion at month `36`; its configured completion
value of `210` is therefore used in the fee replay.

## Checked accounting and interpretation invariants

The implementation enforces or reports:

1. Strict synthetic-only configuration and exact model version.
2. Unique phase and case identifiers.
3. Positive provider and sponsor commitments and protected reserve.
4. Provider commitment equal to the sum of positive phase caps.
5. Strictly positive case weights with a raw sum within `1e-12` of one;
   normalized aggregate weights sum to one in both the actual evaluation and
   the paired all-provider-performance fee replay, while paths retain declared
   weights and the raw sum remains visible.
6. Linear phase progression and no later phase after a terminal outcome.
7. Provider draw entitlement bounded by cost share, stage cap, and remaining
   commitment.
8. Current sponsor construction capacity tested first, followed by the
   aggregate/share-constrained remaining cost-to-complete screen and then
   provider performance, all before current-phase funding.
9. Unused current-phase provider cap excluded from future sources in the
   cost-to-complete screen; the screen is not a phase-by-phase estimate
   allocation.
10. Final-phase cost-to-complete estimate equal to final-phase actual eligible
    cost within numerical tolerance.
11. Aggregate phase sources exactly equal aggregate eligible construction use.
12. Immediate cancellation and no fee accrual on unused current-phase provider
    cap, with no rollover to later phases.
13. Capped monthly PIK and no commitment fees after termination.
14. Provider terminal payment bounded by both claim and completion proceeds or,
    on failure, proceeds remaining after protected safety obligations.
15. Provider principal loss distinguished from total claim writeoff.
16. Protected reserve excluded from construction, with any remaining declared
    safe-workout shortfall paid from failure recovery proceeds before the
    provider claim.
17. Every five-account cash posting balanced within numerical tolerance.
18. Commitment and funded-claim memo rollforwards balanced within tolerance.
19. Closing ProjectCo cash, reserve cash, undrawn commitment, and funded claim
    equal zero after settlement.
20. Expected provider NPV after charged upfront fee equals the fee-adequacy gap.

## Explicitly unmodeled matters

Version 0.1 does **not** model:

- cure periods, waivers, amendments, re-baselining, milestone disputes, notice,
  replacement providers, later funding remedies, or restart after failure;
- invoices, monthly construction spending, procurement, eligibility audit,
  change orders, retainage, fraud detection, or cost-estimate validation;
- certification dates, milestone long stops, early certification, partial
  certification, or the technical content and independence of a certificate;
- external committed construction sources, grants, insurance recoveries,
  contractor payments, or provider administration-cost inputs;
- legal enforceability of future draws, security, priority, subordination,
  insolvency, damages, setoff, clawback, forfeiture, netting, specific
  performance, or intercreditor terms;
- stochastic provider credit, liquidity, capital, collateral, recovery,
  wrong-way risk, replacement funding, or legal consequences of provider
  nonperformance;
- operating revenue, working capital, maintenance, taxes, debt service,
  post-completion reserves, refinancing, cash sweep, or continuing repayment;
- a complete exported month-by-month cash-flow statement;
- Q-measure or risk-neutral pricing, replication, hedge value, market
  calibration, valuation adjustments, capital charges, expenses, or fair value;
- empirical calibration of case weights, costs, certifications, recovery,
  terminal value, discount rates, or market terms; or
- financing additionality, qualified-output adoption, animal-product
  displacement, animals spared, or any animal-welfare impact.

These omissions are controlling limitations. A result may be described only as
a deterministic case result or a weighted synthetic sensitivity under the
declared inputs.

## Honest interpretation

A phase certificate is an input, not evidence created by the model. Drawing at
the start of a phase places both sponsor and provider cash at risk before the
phase's end-of-period decision. Cancelling unused capacity limits later exposure
but can also make a later cost-to-complete test fail. PIK changes the contractual
claim; it does not create project cash. Completion and recovery values are
external inputs, not valuations.

The physical-P break-even fee can be highly sensitive to the paired provider-
performance convention, declared case weights, terminal proceeds, recovery
delay, PIK cap, and hurdle rate. Its precision is computational, not
evidentiary.
Completion in this module is not validated performance, safe operation,
financeability, bankability, market adoption, or animal-welfare impact.
