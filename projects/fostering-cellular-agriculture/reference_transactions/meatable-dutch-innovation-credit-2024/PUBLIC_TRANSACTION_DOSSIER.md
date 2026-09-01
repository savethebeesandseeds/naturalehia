# Meatable Dutch Innovation Credit Public Transaction Dossier

**Package ID:** `meatable-dutch-innovation-credit-public-2026-09-01`

**Observation cut-off:** 1 September 2026

**Transaction focus:** issuer-reported EUR 7.6 million Innovation Credit award
published on 24 September 2024; a regulated announcement on 19 December 2025
that Meatable's board and shareholders had resolved to dissolve and wind down
the business; individualized credit decision, advances and creditor outcome
unresolved

**Decision:** `RETAINED_PUBLIC_PACKAGE / INELIGIBLE_FOR_HURDLE_SET`

**Engine input:** canonical public-incomplete Claim Ledger only; no hurdle-evidence
configuration generated

## Executive result

This package tests a third public-capital architecture against the project's
[Market Observation and Hurdle Evidence Set v0.1](../../docs/MARKET_OBSERVATION_AND_HURDLE_EVIDENCE_V0_1.md).
Meatable stated on 24 September 2024 that it had been awarded EUR 7.6 million
under the Netherlands Enterprise Agency's Innovation Credit programme and
would use the credit to improve productivity and reduce costs before
commercialization. That is a relevant issuer-reported award observation.

The cut-off also contains a known adverse corporate outcome. Agronomics' RNS
2963M, published on 19 December 2025, reports that Meatable's board and
shareholders had resolved to dissolve Meatable B.V. and related group
companies, terminate operating activities and conduct an orderly wind-down
under the applicable statutory liquidation procedures after continued funding
could not be obtained. It does not state that legal dissolution completed on
19 December 2025. Agronomics separately reported that its GBP 11.9 million
equity carrying value would be written down to zero.

Neither observation is a credit cash observation. The public package does not identify the exact
recipient legal entity, individualized award decision, award-decision date,
approved project plan, eligible-cost base, project classification, advance
schedule, bank settlement, accrued balance, repayment schedule, collateral
schedule, remission decision or realized recovery.

The later dissolution resolution and the equity write-off do not fill those
gaps. They do not establish an RVO default or acceleration, an outstanding
advance balance, a proof of claim, a remission request or decision, security
release, liquidation priority, creditor recovery or settlement cash. The
corporate outcome is therefore retained as an adverse backtest while the RVO
claim outcome remains `UNKNOWN`. The 2024 award source names the Meatable
brand, while RNS 2963M names Meatable B.V. and related group companies; the
exact legal linkage between the RVO recipient and those entities also remains
unresolved.

The official 2024 opening notice and RVO's July 2024 obligations document add
useful programme mechanics. They do not fill those award-specific gaps. The
opening notice records a 3% annual base rate and fixed premiums of 15% for
technical development projects and 25% for clinical development projects.
Nothing in the allowed award-specific source establishes Meatable's formal
classification or that any one of those generic premiums appears in its
individual decision.

The July document is a standard obligations-and-conditions package with blank
fields and optional clauses. It describes binding project plans, milestone and
financial reporting, approval rights over material changes, repayment of
credit plus interest and a fixed premium, and a first-ranking pledge template
over specified project assets and derived intellectual property. Those are
generic programme mechanics, not proof that a completed Meatable agreement
contains every template provision or that a particular asset was pledged.

Most importantly, remission is not an automatic failure payoff. Current RVO
guidance says a recipient may ask for partial remission when a project fails
for technical reasons or loses market prospects, and RVO may grant temporary
or definitive remission depending on the circumstances. The guidance is a
current generic explanation, not a Meatable-specific 2024 decision or outcome.
The Claim Ledger therefore contains no automatic principal forgiveness, no
provider cash, and no assumed loss severity.

The manual admission result is:

```text
candidate_admission_status = ineligible
candidate_cluster_boundary = defined_as_one_public_award_observation
award_amount_status = observed_issuer_claim_not_cash
individualized_claim_status = unknown
post_award_corporate_outcome = resolution_to_dissolve_and_wind_down_announced
corporate_outcome_announcement_date = 2025-12-19
legal_dissolution_completion_date = unknown
rvo_claim_outcome_after_corporate_resolution = unknown
remission_status = discretionary_generic_mechanic_only
expected_cash_rate_preimage = not_calculated
hurdle_evidence_config_generated = false
reason = known_corporate_outcome_but_no_complete_credit_claim_price_settlement_or_recovery_cash
```

No amount, date, cash path or probability has been supplied by assumption.

## Evidence layers that must not be merged

### 1. Award-specific issuer observation

The Meatable release is the only permitted source that names the candidate
award. As an issuer award announcement it is classed `D` / `CLM`:

- publication on 24 September 2024;
- an award described as EUR 7.6 million under RVO's Innovation Credit
  programme;
- an intended use of improving productivity and reducing costs before
  commercialization; and
- contextual mention of Meatable's 2023 EUR 30 million Series B.

The Series B is not combined with the Innovation Credit, and the release date
is not used as the decision, execution, funding or settlement date. The
release names the Meatable brand rather than an exact legal obligor and does
not evidence a cash receipt.

### 2. Official 2024 programme opening

Staatscourant 2023 number 35780 was published on 27 December 2023 and opened
the 2024 Innovation Credit programme from 1 January through 1 December 2024.
It set separate EUR 30 million programme ceilings for clinical and technical
development projects and stated that the rates remained 3% annually as a base
rate plus a 15% fixed premium for technical projects or 25% for clinical
projects.

Those are programme-level legal facts (`A` / `REG`). The notice does not name
Meatable and does not establish its application date, decision date, formal
project class, individualized rate, approved amount, advance or cash balance.
The opening window is therefore only a consistency boundary, never a
transaction date.

### 3. July 2024 standard obligations and conditions

The official RVO document is also programme-level evidence (`A` / `REG`). It
contains standard obligations and templates, including placeholders for the
applicant, project, decision reference, related parties, assets and any surety.
The package supports the following generic state design:

| Generic programme mechanic | What the document supports | What remains Meatable-specific and unknown |
|---|---|---|
| Project and milestone control | Approved project plan is binding; progress reports are due after defined reporting periods and report milestone status | Approved plan, milestones, periods, achievement and advance history |
| Change control | Prior approval is required for material changes including budget, schedule, financing, commercial prospects, control, financial condition and project IP | Any Meatable request, consent, suspension, amendment or breach |
| Financial restrictions | Standard condition restricts shareholder withdrawals while the credit is not fully repaid | Exact liable group, exceptions, compliance and enforcement |
| Finalization | Final report and subsidy-finalization request follow project end; an accountant declaration is required above the stated cost threshold | Project end, eligible costs, accepted costs and final amount |
| Repayment | Standard framework refers to repayment of credit plus accrued interest and fixed premium | Meatable principal, accrual, payment dates, maturity, cash and amendments |
| Security | Pledge template describes a first-ranking pledge over identified and future project assets and derived IP | Executed pledge, pledgor, asset list, perfection, other liens, value and recovery |
| Surety | Annex language is conditional and contains placeholders | Whether any Meatable surety exists and, if so, its obligor and scope |
| Default realization | Template permits realization of pledged assets after default and application of proceeds to obligations | Default, enforcement, proceeds, costs, priority and creditor recovery |

No blank or optional template clause is promoted into an award-specific fact.

### 4. Current generic remission guidance

RVO's current post-application page explains that a recipient may request
partial remission if technical failure occurs or market prospects fall away,
and that RVO may grant temporary or definitive relief depending on the
circumstances. It also says RVO determines an individualized repayment
schedule from forecasts and that interest and the pledge continue during the
repayment period. Separately, the page describes next-period advances following
approval of a progress report and the ability to pause advances for major
changes until amended plans are approved.

This source was last changed in January 2026, after the candidate award. It is
retained only to establish a current administrative interpretation of
discretion. It does not prove the wording applicable on Meatable's award date,
an application for remission, a decision, a release of security, or a cash
outcome.

### 5. Regulated 19 December 2025 corporate-outcome announcement

Agronomics' RNS 2963M is a regulated issuer-investor announcement (`C` /
`CLM`). It establishes the following post-award public facts:

- publication on 19 December 2025;
- a reported board-and-shareholder resolution to dissolve Meatable B.V. and
  related group companies and terminate operating activities;
- an intended orderly wind-down under applicable statutory liquidation
  procedures after continued funding was unavailable; and
- Agronomics' separate report that GBP 7.9 million had been invested in its
  equity position and that a GBP 11.9 million carrying value would be written
  down to zero.

The publication date is not encoded as the resolution date, cessation date or
legal dissolution-completion date. The announcement is not a liquidation
resolution filing, liquidator report, creditor schedule or distribution
account. Most importantly, Agronomics' equity is a different asset from the
RVO credit. A zero equity carrying value does not establish the RVO claim's
principal, default status, remission, collateral, ranking, recovery or loss
severity.

The direct London South East distribution page returned HTTP 403 to automated
retrieval. Its terms also restrict copying and redistribution. The package
therefore records the URL, RNS number, access date and normalized facts as
`NOT_RETAINED`; it does not commit distributor HTML. A permitted historical
RNS delivery or written redistribution permission is required before exact
bytes can be retained publicly.

### 6. Current issuer-homepage corroboration

The current Meatable homepage carries a statement of intended orderly
dissolution and liquidation after insufficient additional funding. Direct
retrieval timed out at the cut-off and no publication date is displayed in the
indexed representation, so it remains `D` / `CLM`, `NOT_RETAINED` and
corroborative only. It is not used to determine any legal effective date,
operating-cessation date or RVO claim outcome.

## Financial normalization

| Required field | Public result | Consequence |
|---|---|---|
| Economic cluster | One 24 September 2024 issuer award announcement | Candidate observation is defined; underlying legal claim remains incomplete |
| Recipient | Meatable brand | Exact legal obligor and related liable or pledging parties unknown |
| Provider | RVO named by issuer; standard pledge template names the State of the Netherlands | Individualized contracting and payment parties not established |
| Amount | EUR 7.6 million described as awarded | Award amount is not contractual face, advanced principal, buyer price, borrower gross proceeds or cash received |
| Decision and settlement | No individualized decision or bank evidence | Execution, funding, value and settlement dates remain `UNKNOWN` |
| Project | Productivity improvement and cost reduction before commercialization | Approved scope, budget, eligibility percentage, milestones and completion tests absent |
| Rate and premium | Generic 2024 rates observed | Meatable class and individualized accrual are unknown; programme rates are not a market hurdle |
| Repayment | Generic programme framework observed | Exact schedule, cash rights, prepayment, acceleration and realized cash unknown |
| Advance control | Current guidance describes progress-report approval before the next-period advance and possible pause for major changes | No Meatable advance, pause, restart or milestone state is observed |
| Failure and remission | Current guidance describes a request and discretionary RVO decision | No automatic forgiveness, probability, remission amount, timing or Meatable outcome may be modeled |
| Security | Standard first-ranking pledge template observed | Executed collateral, priority, perfection, value, enforcement and recovery unknown |
| Post-award corporate outcome | 19 December 2025 announcement of a resolution to dissolve, terminate operations and conduct an orderly statutory wind-down | Legal completion date, creditor schedule and liquidation distributions absent |
| Agronomics equity outcome | GBP 11.9 million carrying value reported written down to zero | Separate equity asset; not an observation of the RVO claim balance, recovery or loss |
| RVO claim after corporate resolution | No claim-specific creditor record | Default, acceleration, proof of claim, remission, security release, priority, recovery and settlement remain `UNKNOWN` |
| Price and costs | No investor or borrower settlement ledger | All-in investor outflow and borrower net proceeds cannot be reconciled |
| Physical expected cash | No independent ex-ante probability or recovery set | Expected-cash rate preimage cannot be solved |
| Comparable bridge | No complete eight-axis adjustment package | Candidate cannot supply a target hurdle |

The row-level result is recorded in
[`normalization_decision.tsv`](normalization_decision.tsv).

## Why the programme rates are not an investor hurdle

The 3% base rate and fixed premium are policy terms in a public support
programme. They do not observe a price negotiated by independent investors for
a transferable claim, and the word “fixed” does not make the premium a
cash-on-cash return. The public records do not reveal when advances occur, how
interest accrues on each advance, when principal and premium become due, how a
schedule changes, or what is paid after failure, remission or enforcement.

Investor expected return requires a paid price and the probability-weighted
dated cash received in success, delay, partial failure, complete failure and
recovery states. Here, each of those quantities is missing even though a later
adverse corporate outcome is known. Treating 3% plus 15% as a discount rate
would mix a generic policy formula with an unknown project classification and
would ignore the discretion embedded in remission. Treating EUR 7.6 million
as both price and principal would manufacture a funding event that has not
been observed. Treating the equity write-off as RVO loss would manufacture a
creditor claim and recovery result that has not been observed.

The transaction is therefore useful architecture evidence but is not eligible
for hurdle calibration, portfolio weighting, expected loss, expected return or
valuation.

## What this transaction changes in the instrument design

This public programme and its later corporate backtest provide four useful
design precedents without
validating a market price:

1. Development capital can be released against an approved budget and
   reporting periods rather than fully exposed at announcement.
2. Technical failure can enter a decision process that changes repayment, but
   the state transition must include the application, authority decision,
   effective date, amount and residual secured claim. “Failure” alone cannot
   trigger automatic forgiveness.
3. Project assets and derived IP can support a recovery interface, but the
   exact collateral ledger, priority, enforcement cost and proceeds still have
   to be exposed before several claims can be pooled honestly.
4. A corporate failure state is not itself a credit-loss amount. The standard
   must bridge from resolution, liquidation and cessation facts to the exact
   creditor claim, remission decision, collateral waterfall and dated recovery
   before admitting a severity observation.

That is valuable for the cellular-agriculture financing standard: stage risk,
public risk absorption and project security can be explicit modules. It does
not justify copying a concessional programme rate into a privately funded
instrument.

## Control and claims boundary

Three official programme records are retained with exact byte counts and
lowercase SHA-256 hashes in
[`source_manifest.tsv`](source_manifest.tsv). The Meatable award page and
homepage remained technically unavailable. The London South East RNS page
returned HTTP 403, and its redistribution restrictions preclude substituting a
downloaded distribution copy in this public repository. All three live URLs,
access dates and `NOT_RETAINED` statuses remain explicit. A hash binds only
the downloaded bytes; it does not authenticate the source, prove legal
completeness or approve a model use.

`NO_PUBLIC_SOURCE` means the permitted public records are silent on the field.
`DERIVED_DECISION` / `DER` identifies a protocol conclusion rather than a
transaction fact. Generic programme rows retain `A` / `REG`, while the named
award remains `D` / `CLM`; neither is promoted by being adjacent to the other.

The canonical [`claim_ledger_v0_1`](claim_ledger_v0_1/claim.cfg) is deliberately
public-incomplete. It contains no scenario cash, provider claim, covenant event
or conversion event because the public sources do not observe those items for
Meatable. It does include the later corporate resolution and equity write-off
as context terms, while the RVO claim outcome remains a separate `UNKNOWN`.
Missing fields remain `UNKNOWN`, never zero. The focused request needed to
change that result is in
[`CONTROLLED_DATA_REQUEST.md`](CONTROLLED_DATA_REQUEST.md). No outreach,
financing offer, valuation or representation of available capital has been
made.
