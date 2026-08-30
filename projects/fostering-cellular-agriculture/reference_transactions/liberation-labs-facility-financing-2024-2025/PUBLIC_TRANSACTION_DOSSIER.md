# Liberation Labs Facility-Financing Public Transaction Dossier

**Package ID:** `liberation-labs-facility-financing-public-2026-08-30`

**Observation cut-off:** 30 August 2026

**Transaction focus:** October 2024 secured convertible promissory-note add-on;
relationship to the April 2024 note round unresolved

**Decision:** `RETAINED_PUBLIC_PACKAGE / INELIGIBLE_FOR_HURDLE_SET`

**Engine input:** deliberately not generated

## Executive result

This is the first real transaction package tested against the project's
[Market Observation and Hurdle Evidence Set v0.1](../../docs/MARKET_OBSERVATION_AND_HURDLE_EVIDENCE_V0_1.md).
It is directly relevant: the issuer reported that the proceeds financed
continued construction of Liberation Labs' Richmond, Indiana
precision-fermentation facility. Public records disclose a
USD 3.5 million round, 10% annual interest, a 10 October 2027 maturity, security
in name, and conversion into a future qualified financing.

The October announcement describes “further” financing and incorporates an
April 2024 announcement of a USD 12.5 million secured convertible
promissory-note round for the same facility and qualified financing. The public
record does not establish whether April and October are separate economic
clusters, add-on issuances under one note series, or part of the USD 19 million
of 2024 insider bridge notes later reported in aggregate. The October claim is
therefore not counted as a de-duplicated observation.

The package does **not** identify an investor hurdle. The exact note contract,
settlement ledger, buyer costs, payment convention, conversion economics,
collateral and priority, default rights, and recovery cash flows are not
public. The investors also sit inside an existing ownership and related-party
network. A contractual 10% interest term therefore cannot be relabelled as an
arm's-length expected return on a comparable claim.

The manual protocol-admission result for this candidate is:

```text
candidate_admission_status = ineligible
candidate_cluster_boundary = unresolved
expected_cash_rate_preimage = not_calculated
hurdle_evidence_config_generated = false
reason = no_complete_expected_cash_rate_preimage
```

No engine evidence set was run, and no zero-rate placeholder or invented
interval is stored.

## What the public record actually observes

The 5 April 2024 RNS reports a USD 12.5 million secured convertible
promissory-note round: Agronomics invested USD 10 million and existing investor
Siddhi Capital participated. It financed continued construction and would
convert into the same instrument and terms as a future qualified financing.

The corrected 15 October 2024 RNS then says Liberation Labs raised a **further**
USD 3.5 million through a Secured Promissory Note. Agronomics subscribed USD 2
million from its own cash; New Agrarian, Galloway, and Jim Mellon each
subscribed USD 0.5 million on identical terms. It states 10% interest per year
and maturity on 10 October 2027. The note financed continued construction and
refers back to the qualified financing set out in April. The correction removed
an unrelated Department of Defense award from the earlier announcement; the
corrected version is the controlling retained October copy.

The 30 January 2025 RNS reports a separate USD 31.5 million convertible-note
close, combined with USD 19 million of insider bridge notes raised during 2024
for a USD 50.5 million total. It says the current round would convert at the
next equity financing and that proceeds would help complete the 600,000-litre
facility. It does not prove that every amount shares the October note's rights.

The February 2025 Form D reports USD 51,982,971 of debt securities sold to six
investors, with first sale on 28 January 2025. That amount and legal filing
scope do not exactly equal the RNS headline. The records are not forced into
one transaction cluster without a closing ledger and instrument schedule.

Liberation Labs and Ameris Bank also [announced on 30 November 2023](https://www.globenewswire.com/news-release/2023/11/30/2788554/0/en/USDA-Awards-25-Million-Loan-Guarantee-to-Ameris-Bank-for-Liberation-Labs-Biomanufacturing-Facility-in-Indiana.html)
that USDA had awarded Ameris a nominal USD 25 million Business and Industry
loan guarantee for the Richmond facility. This is an issuer/counterparty
announcement, not the executed conditional commitment or loan-note guarantee.
USDA publishes guarantee percentages and fees by fiscal year; those programme
terms are conditional architecture precedent, not proof of the terms attached
to this loan. In particular, multiplying USD 25 million by a programme
percentage would manufacture transaction coverage without the obligation
date, guaranteed-loan principal, closing certification, fee records, and issued
guarantee document.

On 19 December 2025, Agronomics reported that all of its convertible loan notes
in Liberation Bioindustries, Inc. were converted, with capital plus accrued
interest, into Series A1 shares. This is issuer-family contextual evidence of a
later aggregate non-cash state. It does **not** establish legal-entity
continuity with Liberation Labs Holdings Inc., link a registered October note,
or prove that claim's settlement, shares, price, or exact accrued interest.

## Financial normalization

| Required field | Public result | Consequence |
|---|---|---|
| Economic cluster | April USD 12.5 million and October “further” USD 3.5 million notes share a facility and qualified-financing context; January later reports USD 19 million of 2024 insider bridge notes | Cluster boundary and de-duplication are unresolved |
| Claim and gross price | USD 3.5 million round and investor subscriptions are reported; face amount, issue price, net funding, and per-note register are absent | Partial observation only |
| Settlement | “Raised” and “subscribed from own cash” are reported; bank wire, closing statement, and dated ledger are absent | Not settled-source evidence under v0.1 |
| Contractual cash | 10% per year and a maturity date are reported | Payment dates, day count, cash/PIK, compounding, prepayment, acceleration, and maturity redemption are unresolved |
| Conversion | April and October announcements describe conversion into a qualified financing; a later issuer-family aggregate conversion is reported | Legal-entity continuity, trigger, deadline, discount, cap, security received, dilution, and note-specific settlement are missing |
| Security and recovery | The label “secured” is reported | Collateral, lien rank, intercreditor rights, enforcement costs, timing, and recovery are unknown |
| External guarantee | Liberation Labs and Ameris announced a nominal USD 25 million USDA B&I guarantee for the Richmond facility | The conditional commitment, obligation date, guaranteed-loan principal, loan-note guarantee, coverage percentage, fees, exclusions, claims, and settlements are not retained; no transaction coverage amount is calculated |
| Independence | The same RNS identifies existing ownership and investor relationships | Arm's-length price formation is not established |
| Buyer costs | None disclosed | All-in price and investor return cannot be reconstructed |
| Physical expected cash | No independent state probabilities or recovery paths | Expected-cash rate preimage cannot be solved |
| Comparable bridge | No complete eight-axis adjustment package | The note cannot be transferred to the target claim |

The complete field ledger is in
[`normalization_decision.tsv`](normalization_decision.tsv).

## Why 10% is not the required return

The 10% figure describes promised contractual accrual. Investor value depends
on what is actually received across at least three paths: cash repayment,
equity conversion, and default or restructuring. Conversion can be valuable or
worthless; security can materially change recovery; delay and buyer costs
change the annual return. None can be inferred from the promised interest rate.

The project's issue-price engine discounts cash that has already been reduced
for physical loss, delay, and recovery. Reusing a risky promised rate as its
discount rate would usually count loss compensation twice while ignoring the
conversion option. Until the full state-contingent cash map exists, there is no
honest `r`, no comparable interval `H_j`, and no market hurdle `h`.

## What this transaction changes in the instrument design

It supports three design priorities without validating a price:

1. Debt cash rights and success-linked conversion must be shown and valued as
   separate components, even when legally packaged in one note.
2. “Secured” is not a recovery model. Pool eligibility requires collateral,
   priority, enforcement, and dated recovery cash.
3. A common project interface is necessary before diversification. Several
   facilities cannot reduce risk merely by placing several opaque convertibles
   in one vehicle.

This is useful negative evidence. It shows exactly what a cellular-agriculture
project must publish or provide under control before its financing can become a
standardized, comparable asset.

The 2025 financing and conversion records are ex-post context only. They may
test a model frozen at the transaction date, but they cannot enter the ex-ante
physical probability set used to reconstruct an October 2024 price.

## Control and claims boundary

Four retrieved HTML records are retained with byte counts and SHA-256 hashes
in [`source_manifest.tsv`](source_manifest.tsv). The manifest keeps public-fact
class (`A`–`E`), provenance tag, and distribution channel separate: RNS
statements remain issuer claims (`C` / `CLM`) even when regulator-hosted. The
hashes bind the retained bytes; they do not authenticate the originating
parties, prove legal completeness, or approve a decision use. The SEC filing
is classed `A` / `REG` only for its exact filing facts, but was not retained
because the public endpoint refused the automated download; it is not promoted
to controlled evidence.

In [`normalization_decision.tsv`](normalization_decision.tsv),
`NO_PUBLIC_SOURCE` means that every retained public record is silent on the
field; `DERIVED_DECISION` / `DER` marks a protocol conclusion rather than a
public fact. These are typed markers, not source identifiers or zeros. Public
facts retain their own evidence class and provenance on each normalized row.

The focused request needed to change the result is in
[`CONTROLLED_DATA_REQUEST.md`](CONTROLLED_DATA_REQUEST.md). No outreach,
financing offer, valuation, or representation of available capital has been
made.
