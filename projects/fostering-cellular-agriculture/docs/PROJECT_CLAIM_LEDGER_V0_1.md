# Project Claim Ledger v0.1

## Status and purpose

This document defines the minimum financial record for one project and one
investor claim before that claim can enter a return, pricing, or portfolio
model. It is an upstream normalization standard. It asks four narrow questions:

1. What cash did the buyer pay and what cash did the borrower receive?
2. What principal and accrued-interest balances did those cash movements and
   non-cash terms create?
3. What cash or non-cash settlement can the investor receive in each state?
4. Which facts were available at the transaction decision time, and which are
   later backtest evidence?

The ledger is not a valuation, market quote, credit opinion, legal
interpretation, probability forecast, or management-diligence file. A valid
incomplete ledger is useful: it identifies the exact financial facts that are
missing without replacing them with zeroes or assumptions.

## Position in the standard

```text
source records
    -> one-project / one-claim ledger
    -> complete state-contingent investor cash
    -> project and portfolio adapters
    -> loss, expected return, price, support, and hurdle tests
```

The ledger is narrower than the
[Project Financial Interface](PROJECT_FINANCIAL_INTERFACE_V0_1.md). It does not
model the project's capital need, external operating cash, technical states, or
common-factor dependence. It normalizes the financing claim that may later
attach to those objects.

Version 0.1 uses one native currency and one monetary basis. Currency or
inflation conversion must occur outside the ledger under an explicit rule. A
claim with several obligors, facilities, currencies, or legally distinct note
series must be split or accompanied by an evidenced aggregation rule. A public
announcement of a round is not automatically one fungible claim.

## 1. Typed values and the missing-data rule

Every numerical amount, rate, probability, provider term, face amount, and
opening balance has one status:

| Status | Meaning |
|---|---|
| `known` | Exact lower and upper values are equal and supported by the cited record |
| `bounded` | Both finite endpoints are supported, but no exact value is admitted |
| `unknown` | The value required for the calculation is not established |
| `not-applicable` | Evidence establishes that the field cannot apply to this claim |

The plain-file token for an absent value is `UNKNOWN`. It is not blank, zero,
`false`, `none`, or `not-applicable`. A new issue must explicitly evidence an
opening balance of zero; the software must never supply that zero by default.
`not-applicable` also needs evidence. Textual public terms can be retained even
when their numerical cash consequence remains `unknown`.

Every row also carries an `input_status`: `observed`, `contractual`, `derived`,
`estimated`, `stress`, `backtest`, or `unknown`. This describes how the input
is being used; it does not replace the source manifest's evidence class or
provenance tag. A contractual term can still come only from an issuer claim,
and a mathematically derived value is not an observed cash movement.

Each value has a `known_at_period`. That is the earliest period at which the
record proves the information was available to the relevant decision maker,
not the later date on which an analyst retrieved it. An entry is ex ante only
when:

```text
known_at_period <= decision_period
```

An entry known later is backtest evidence. An entry whose `known_at_period` or
the transaction `decision_period` is `UNKNOWN` cannot enter an ex-ante expected
return. A later disclosure of an old contract term does not by itself prove
that exact term belonged to the investor's frozen information set.

All monetary ledger values are non-negative millions of the declared currency;
the entry kind determines direction. Conversion units use their separately
declared unit label and are never added to money. Dates use ISO `YYYY-MM-DD`.
Periods are non-negative integers from the evidenced ledger origin. An unknown
date does not authorize an invented period zero.

## 2. Canonical package

The canonical directory contains these bound files:

| File | Financial role |
|---|---|
| `claim.cfg` | Identity, cluster boundary, typed dates, face and opening balances, typed discount input, and SHA-256 bindings |
| `terms.tsv` | Public contractual terms that are not themselves cash or balance movements |
| `common_entries.tsv` | Cash and non-cash entries that occur in every admitted scenario |
| `scenarios.tsv` | Physical scenario probabilities and their ex-ante timing |
| `scenario_entries.tsv` | State-contingent cash and non-cash entries |
| `provider_claims.tsv` | Separate guarantee or support-provider claim terms |
| `covenant_events.tsv` | Observed covenant states without inferred cash consequences |
| `conversion_context.tsv` | Potential conversion evidence not yet admitted to a claim path |

Every TSV is bound in `claim.cfg` by relative path and lowercase SHA-256. A
binding proves which bytes were reviewed; it does not prove the economic or
legal assertion. Every admitted `source_record_id` must resolve to the parent
transaction package's source manifest. `NO_PUBLIC_SOURCE` is a typed absence
marker and never an evidence record.

The entry tables preserve the implemented core fields:

```text
entry_id, economic_fact_id, event_group_id, kind, period, known_at_period,
value_status, lower_value, upper_value,
input_status, source_record_id, provider_claim_id
```

Scenario entries add `scenario_id`. Scenario probabilities and the discount
rate use the same typed-value discipline. Scenario probability and cash-path
completeness have separate input classifications, known-at periods, and source
IDs; evidence for one is not evidence for the other. Context tables may
preserve a public fact that is not admissible to the core. Such a row must say
why it is not linked; it cannot silently become investor cash.

`entry_id` identifies one retained row. `economic_fact_id` identifies the
economic fact that row states. At any information cut, the evaluator selects
the latest version of each economic fact whose `known_at_period` is at or
before the cut. A later version replaces rather than adds to the earlier
version. Equal-time competing versions are invalid, and versions of one fact
cannot change its accounting kind, event group, or provider identity. This
prevents a later correction from being double-counted or silently rewriting
the frozen decision-time ledger.

An `unknown` entry row is a required-field placeholder, not an assertion that
cash or a balance movement occurred. Until its value and period are admitted,
the loader retains it as a blocker and does not construct a core cash event.

The TSV schemas are closed and column order is part of the file binding:

```text
terms.tsv
term_id, value_status, value, input_status, known_at_period,
source_record_id, note

common_entries.tsv
entry_id, economic_fact_id, event_group_id, kind, period, known_at_period,
value_status, lower_value, upper_value, input_status, source_record_id,
provider_claim_id

scenarios.tsv
scenario_id, probability_status, probability_lower, probability_upper,
probability_known_at_period, probability_input_status,
probability_source_record_id, cash_path_status,
cash_path_status_known_at_period, cash_path_input_status,
cash_path_status_source_record_id

scenario_entries.tsv
scenario_id, entry_id, economic_fact_id, event_group_id, kind, period,
known_at_period, value_status, lower_value, upper_value, input_status,
source_record_id, provider_claim_id

provider_claims.tsv
provider_claim_id, provider_id, known_at_period,
shortfall_allocation_status, shortfall_allocation_lower,
shortfall_allocation_upper, coverage_status, coverage_lower, coverage_upper,
deductible_status, deductible_lower_million, deductible_upper_million,
maximum_cash_status, maximum_cash_lower_million, maximum_cash_upper_million,
settlement_lag_status, settlement_lag_lower_periods,
settlement_lag_upper_periods, covers_principal_due, covers_interest_due,
payment_right_evidenced, provider_identity_evidenced,
coverage_and_priority_evidenced, obligation_priority, input_status,
source_record_id

covenant_events.tsv
scope, scenario_id, event_id, covenant_id, period, event_date_status,
event_date, known_at_period, state, input_status, source_record_id, note

conversion_context.tsv
context_id, observed_date, value_status, value, input_status,
known_at_period, source_record_id, admission_status, note

parent source_manifest.tsv
source_id, record_date, access_date, evidence_class, provenance_tag,
distribution_channel, originating_record, source_uri, retention_status,
retained_copy, retained_sha256, bytes, claim_scope, limitations
```

The actual files are tab-separated. Commas above are for readability only.
The non-guarantee `provider_claim_id` token is `NONE`. A covenant event common
to all paths uses `scope=common` and `scenario_id=NOT_APPLICABLE`. A retained
conversion observation that has not been linked to this claim uses
`admission_status=context-only`.

`claim.cfg` uses package status `retained-public-incomplete`,
`synthetic-complete`, or `controlled-candidate`, and economic-cluster boundary
status `defined` or `unresolved`. Execution, funding, settlement, observation,
decision, maturity, and horizon dates each have status, value, and source
fields. Decision and horizon periods are independently typed. Face amount,
opening principal, opening accrued interest, and discount rate each have value
status, lower and upper endpoints, known-at-period status and value, and a
source. The package cannot declare its own readiness.

For expected-return admission of a controlled candidate, every used source
resolves to a confined, non-empty retained file with a matching SHA-256 and byte count. Source URI,
channel, originating record, scope, retention status, and limitations must be
explicit. File size, row count, line length, path-selection work, retained
source bytes, and reported blocker text are bounded before expensive work.
These are integrity and resource controls, not source authentication.

## 3. Settlement and proceeds

For funding event group `g`, the exact primary-issue identities are:

```text
BuyerPrice_g = BorrowerGrossProceeds_g

FundedPrincipal_g
    = BuyerPrice_g
      + OriginalIssueDiscount_g
      - OriginalIssuePremium_g

BorrowerGrossProceeds_g
    = BorrowerNetProceeds_g
      + CashFee_g
      + BorrowerThirdPartyCost_g
```

The identities are tested event by event, not only on lifetime totals.

| Entry kind | Meaning |
|---|---|
| `buyer-price` | Cash paid by the claim buyer for the primary claim; an investor outflow |
| `buyer-direct-cost` | Additional cash paid by the buyer to a third party; an investor outflow but not borrower funding |
| `borrower-gross-proceeds` | Primary cash credited before closing deductions; a financing-side control, not investor return |
| `borrower-net-proceeds` | Cash available to the borrower after the two declared closing deductions |
| `cash-fee` | Cash paid or withheld for the claim investor; investor receipt, not principal |
| `borrower-third-party-cost` | Borrower closing cash paid to another recipient; never investor return |
| `funded-principal` | Principal created by primary funding; a balance movement, not additional cash |
| `original-issue-discount` | Excess of funded principal over buyer price; a non-cash issue-price bridge |
| `original-issue-premium` | Excess of buyer price over funded principal; a non-cash issue-price bridge |

Gross issue price, buyer all-in cost, borrower gross proceeds, borrower net
proceeds, and funded principal are therefore different measures even when some
of them happen to be equal. Buyer all-in cash outflow is buyer price plus buyer
direct cost. A round amount, commitment, subscription announcement, or ending
debt balance cannot substitute for a settled event group.

One funding event cannot carry both a positive original-issue discount and a
positive original-issue premium. The full equality system must have one common
value even for bounded inputs; pairwise-overlapping ranges with no joint value
are rejected.

`cash-fee` is restricted to cash belonging to this claim's investor. Legal,
arranger, guarantee, diligence, tax, or other third-party closing costs use
`borrower-third-party-cost`. This prevents an issuer expense from being counted
as investor yield. Other project uses remain outside the claim ledger.

## 4. Face amount, opening balances, and fees

Three typed scalars remain separate:

| Scalar | Meaning |
|---|---|
| `contractual_face_amount_million` | Contractual notional or cap stated by the instrument |
| `opening_principal_million` | Principal legally outstanding immediately before the first ledger entry |
| `opening_accrued_interest_million` | Earned but unpaid interest immediately before the first ledger entry |

Face amount is not buyer price, cash proceeds, commitment, or opening
principal. A claim may issue at a discount, draw in stages, add a fee to
principal, or begin with accrued interest. None of those differences may be
collapsed into one number.

Fee treatment is recorded through separate entries:

- `cash-fee` records actual investor cash;
- `borrower-third-party-cost` records actual closing cash to another recipient;
- `capitalized-fee` increases principal without itself proving any cash paid;
- a fee both paid and financed needs the relevant cash entry and a separate
  capitalized-fee entry in the same evidenced event group.

Original-issue discount and premium bridge issue cash to principal created;
they are not fees, receipts, or borrower uses. They therefore remain separate
from both `cash-fee` and `capitalized-fee`.

A statement that a fee was “capitalized” does not reveal payer, recipient,
value date, withholding, or borrower net proceeds. The amount can be retained
while settlement readiness remains blocked.

## 5. Principal and accrued-interest ledgers

The core recognizes these additional entry kinds:

| Entry kind | Ledger effect |
|---|---|
| `interest-accrual` | Increases accrued interest; no cash |
| `capitalized-interest` | Transfers accrued interest into principal; no cash |
| `principal-due` | Declares principal contractually due; no cash by itself |
| `interest-due` | Declares interest contractually due; no cash by itself |
| `principal-cash` | Borrower cash allocated to principal |
| `interest-cash` | Borrower cash allocated to interest |
| `recovery-principal-cash` | Workout or asset-recovery cash allocated to principal |
| `recovery-interest-cash` | Workout or asset-recovery cash allocated to interest |
| `principal-writeoff` | Non-cash principal extinguishment through loss |
| `accrued-interest-writeoff` | Non-cash accrued-interest extinguishment through loss |

For each period and scenario, the principal and accrued-interest accounts roll
forward exactly:

```text
closing principal
  = opening principal
    + funded principal
    + capitalized fee
    + capitalized interest
    - principal cash
    - recovery principal cash
    - guarantee principal cash
    - conversion principal extinguishment
    - principal writeoff

closing accrued interest
  = opening accrued interest
    + interest accrual
    - capitalized interest
    - interest cash
    - recovery interest cash
    - guarantee interest cash
    - conversion interest extinguishment
    - accrued-interest writeoff
```

No balance may become negative. Due amounts and cash receipts remain separate:
a scheduled instalment is not evidence of payment, and an aggregate borrower
cash-flow line is not a claim-level receipt without allocation evidence.
Payment reduction follows borrower cash, recovery cash, provider cash,
conversion, and writeoff in that order. Any different contractual ordering
requires a later schema version rather than an informal adjustment.

## 6. Investor cash and non-cash conversion

Investor cash is exactly:

```text
InvestorCash
  = - buyer price
    - buyer direct cost
    + cash fee
    + principal cash
    + interest cash
    + recovery principal cash
    + recovery interest cash
    + guarantee principal cash
    + guarantee interest cash
```

Borrower gross and net proceeds, funded principal, accrued interest,
capitalization, due amounts, conversion, and writeoff are not investor cash.

Conversion uses three separate non-cash entries:

| Entry kind | Meaning |
|---|---|
| `conversion-principal-extinguishment` | Principal surrendered or discharged on conversion |
| `conversion-interest-extinguishment` | Accrued interest surrendered or discharged on conversion |
| `conversion-units` | Shares or other units actually received |

Units are not assigned a cash value inside this ledger. A later sale can enter
as separately evidenced external cash in a downstream project or instrument
model. A financing announcement that says notes “will convert” is a contractual
term, not settlement. A later issuer-family conversion is context only until
the note register, legal-entity continuity, price, units, and holder settlement
link it to the normalized claim.

## 7. Guarantee shortfall and provider cash

A guarantee is a second claim on a named provider, not a credit adjective. Each
provider record contains:

```text
provider_claim_id, provider_id, known_at_period,
shortfall_allocation_fraction, coverage_fraction, deductible_million,
maximum_cash_million, settlement_lag_periods,
covers_principal_due, covers_interest_due,
payment_right_evidenced, provider_identity_evidenced,
coverage_and_priority_evidenced, obligation_priority, source_record_id
```

All fractions, caps, deductibles, and lags are typed values. Provider cash is
calculated from the remaining covered due-amount shortfall after borrower and
recovery cash, subject to the evidenced allocation, deductible, coverage, cap,
lag, and priority. Multiple providers must have non-overlapping
shortfall-allocation fractions whose supported minimums sum to no more than
one within each covered obligation category. A principal-only allocation and
an interest-only allocation do not overlap merely because each is 100%. A provider's
`obligation_priority` allocates that provider's payable amount between
principal and interest; it is not an inter-provider waterfall. Provider names
alone cannot be combined into one synthetic guarantee.

Version 0.1 aggregates each provider's allocated due-date shortfall over the
life of the path. It applies one evidenced lifetime deductible, then coverage,
then one lifetime cap; it does not assume a per-occurrence deductible. A claim
generated from that cumulative shortfall remains a distinct provider payable
through its settlement lag. If later borrower cash, recovery, conversion,
writeoff, capitalization, cancellation, or subrogation would change an earlier
allocated shortfall, the path is marked uncomputable rather than silently
netting or reallocating it. Contracts that specify those interactions require a
later schema version.

The evaluator retains principal and interest shortfall, claim generation,
payability, actual guarantee cash, and unpaid payable claim separately. A
principal cash entry cannot consume an interest claim, or vice versa, even
when total provider cash is below the combined cap.

`guarantee-principal-cash` and `guarantee-interest-cash` are investor receipts
from the named provider. They are not borrower cash, project revenue, collateral
recovery, or proof that the provider performed. A claim that contains guarantee
cash is provider-ready only when the matching provider identity, payment right,
coverage and priority, cap, allocation, and timing are exact and ex ante.
Provider creditworthiness, wrong-way risk, enforceability, appropriation, and
default recovery remain separate analyses.

## 8. Covenant states

Covenant events are kept outside the cash ledger with:

```text
event_id, covenant_id, period, known_at_period, state, source_record_id
```

Allowed states are `pass`, `breach`, `breach-with-cure`,
`breach-with-waiver`, `breach-with-non-exercise-consent`, `default`, and
`acceleration`. These terms are not interchangeable. In particular, advance
consent not to exercise a termination right is not silently called a waiver.

A covenant event does not generate default, acceleration, recovery, or cash.
Those consequences require their own contractual and cash entries. Common
events apply in every scenario; scenario events are path-specific. A later
compliance certificate is backtest evidence unless it belonged to the frozen
decision information set.

## 9. Exact readiness gates

The loader and evaluator derive separate readiness states instead of trusting a
package-supplied “complete” flag. No `readiness.*` input is permitted.

**Core-config-ready** means only that a structurally valid frozen
decision-period configuration can be constructed. Facts first known later are
removed from that snapshot: later scalar values and probabilities become
`UNKNOWN`, a later completeness attestation becomes incomplete, and later-only
conversion metadata becomes not applicable. They remain in the package for a
separate full/backtest evaluation and cannot erase or rewrite the decision
view.

**Mechanical expected-cash-ready** requires exact opening balances, face
treatment, issue-price identities, rollforwards, provider mechanics, a complete
resolved cash-path attestation separate from the probability evidence, exact
ex-ante physical probabilities summing to one, and zero terminal claim and due
balances. A promised rate or amortization table is not enough.

At and after the calendar month containing contractual maturity, all
pre-resolution principal and accrued interest must already be declared due.
After cash, recovery, guarantee, conversion, or writeoff in each such period,
the remaining balance must equal the remaining outstanding due amount. Positive
funding after maturity is not admitted. This stops a same-period cash receipt
or writeoff from concealing an omitted due obligation.

**Mechanical rate-preimage-ready** additionally requires standard-sign
expected cash and a root inside the fixed annual-effective numerical guardrail
`[-0.999999, 10]`. The result is an expected-cash discount-equivalent rate
under the declared physical probabilities. The guardrail is a solver boundary,
not market evidence, a hurdle, fair value, spread, or investor target.

**Expected-return-admissible** is a package-level evidence label, not a synonym
for arithmetic success. It requires a `controlled-candidate`, mechanically
ready expected cash and rate preimage, exact decision/funding/settlement/
maturity/horizon timing, non-stress decision inputs, retained evidence dated no
later than the decision date, maturity consistency, and—for every scenario—the
earliest selected buyer-price period containing both funding and settlement.

**NPV-admissible** is separate. It adds a mechanically available NPV and an
exact, ex-ante, retained-source discount input to an already admissible expected
return. A diagnostic mechanical NPV can therefore exist without being called
an admissible valuation.

**Observation-admissible** is narrower again. It requires one common primary
closing group in the funding/settlement month: buyer price and buyer direct
cost must be observed; the other eight closing bridge facts must be observed or
derived; every source must be transaction-capable and dated exactly at
settlement; no scenario-specific funding may occur in or before the anchor
period; and later retained versions or newly discovered closing facts must not
change the price, period, identity, or classification. The observation date
must equal settlement. A source `record_date` check is chronology evidence, not
proof of same-day availability; controlled review must verify actual ordering.
The economic-cluster boundary is required for this market-observation label so
the same financing is not counted repeatedly. It is not required merely to
reconstruct one identified claim's mechanically complete expected cash or its
controlled expected return.

**Full-path-evaluation-available** means every retained later row is
structurally representable and the separate full configuration validates. It
does not retroactively alter the decision expected return, and it is not an
attestation that a forecast became true. If later facts conflict, the frozen
decision calculation can remain available while full-path evaluation is
explicitly unavailable.

**Portfolio-adapter-ready** remains a downstream requirement: investor cash,
principal return, principal loss, continuing exposure, source IDs, and
resolution time must map without reclassification. Conversion units without an
external cash realization cannot be inserted as portfolio receipts.

Any unresolved cluster boundary, timing, settlement identity, fee treatment,
opening balance, cash-right schedule, recovery, conversion linkage, provider
allocation, source, or ex-ante probability set blocks only the relevant gate.
The correct output is an explicit blocker with retained `UNKNOWN` values.

## 10. Adapter boundaries

The claim ledger supplies facts; it does not reproduce downstream engines.

- The participation-pool adapter may consume dated investor cash and principal
  classification. It must not treat borrower proceeds, capitalization, or
  conversion units as external project receipts.
- The capital-stack engines may consume reconciled principal and claim cash
  only after the project-level cash source, loss, and joint-scenario interface
  is complete. The ledger does not create diversification or priority.
- The robust issue-price-support term may use a settled `buyer-price` and
  `buyer-direct-cost` only after the exact claim quantity, primary settlement,
  side rights, issuer costs, and use-side mapping are reconciled. A borrower
  proceeds announcement is not a settled market price.
- The market-observation and hurdle-evidence process may use a complete ledger
  to reconstruct an expected-cash rate preimage. A coupon, margin, or issuer
  valuation cannot substitute for the physical cash paths and probabilities.
- A provider claim here is an underlying transaction guarantee. It is not the
  separately priced pooled-loss-protection overlay. Mapping between them must
  preserve provider identity, trigger, cash, cap, priority, and timing.
- The milestone-gated facility adapter may export its actual provider cash and
  principal ledgers into this schema. This standard does not infer milestones
  or approve draws.

## 11. Canonical incomplete public fixtures

The two public transaction packages contain deliberately incomplete
`claim_ledger_v0_1` directories.

The Liberation Labs fixture retains the announced October amount, 10% promised
interest, maturity date, security and qualified-financing language only in
their proper term or context fields. The April and October cluster boundary,
exact execution and settlement date, face amount, buyer price, proceeds, cash
mechanics, security priority, and ex-ante probability set remain `UNKNOWN`.
The later Series A1 report is context only because legal-entity and note-level
continuity are not established.

The Solar Foods fixture retains the reported EUR 15 million full draw, EUR 1.3
million capitalized commission, EUR 16.3 million reported principal, floating
interest formula, first instalment, named guarantors, and covenant history. It
does not convert the aggregate audited repayment lines into Danske claim cash.
Fee settlement, exact opening date, final maturity, complete cash schedule,
guarantee allocations, claims, provider timing, and recovery remain `UNKNOWN`.
The 2023–2025 performance records are backtest controls, not May 2022 ex-ante
probabilities.

Both fixtures must validate as honest incomplete packages and remain blocked
for expected return, rate preimage, hurdle admission, and portfolio use. Their
purpose is to make the next controlled evidence request machine-specific—not to
turn sparse public disclosures into an investable asset.

The hand reconstruction, strict Debug and Release results, adversarial
controls, canonical package outcomes, and remaining validation boundary are
recorded in the
[Project Claim Ledger v0.1 Verification Record](PROJECT_CLAIM_LEDGER_VERIFICATION_V0_1.md).
