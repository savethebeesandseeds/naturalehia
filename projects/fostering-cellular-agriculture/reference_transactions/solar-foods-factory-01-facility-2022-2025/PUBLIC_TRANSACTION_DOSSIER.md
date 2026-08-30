# Solar Foods Factory 01 Facility — Public Transaction Dossier

**Package ID:** `solar-foods-factory-01-facility-public-2026-08-30`

**Observation cut-off:** 30 August 2026

**Transaction focus:** May 2022 Danske Bank export-credit facility

**Decision:** `RETAINED_PUBLIC_PACKAGE / INELIGIBLE_FOR_HURDLE_SET`

**Engine input:** deliberately not generated

## Executive result

This package reconstructs a real amortizing facility used to build Solar
Foods' Factory 01 production process. The public record says Solar Foods signed
a EUR 15 million export-credit loan with Danske Bank in May 2022, drew it in
full, capitalized a EUR 1.3 million Danske commission into principal, and
names Finnvera and Denmark's EKF as guarantors. It also reports a
floating interest formula, quarterly principal instalments, financial
covenants, one historic advance consent not to exercise the stated termination
right, and later covenant compliance.

This is stronger evidence than a financing announcement. Solar Foods' audited
cash-flow statements report aggregate long-term-loan repayments that equal the
disclosed instalment amount exactly: two instalments in 2024 and four in 2025.
That is a useful performance control, but it is not a lender-level settlement
ledger because Solar Foods also reports other loans.

The package still does **not** identify an expected-cash hurdle. The executed
loan, complete draw and fee ledger, final maturity, full interest mechanics,
executed guarantee rights and claims, security and priority, lender costs,
default and prepayment rights, and state-contingent recovery cash are
unavailable. The
manual protocol-admission result for this candidate is:

```text
candidate_admission_status = ineligible
candidate_cluster_boundary = defined
expected_cash_rate_preimage = not_calculated
hurdle_evidence_config_generated = false
reason = no_complete_expected_cash_rate_preimage
```

No engine evidence set was run. No quoted margin, policy rate, or assumed
guarantee value is promoted to a market hurdle.

### Post-package public-source update — 30 August 2026

An [EKF-issued announcement dated 24 May 2022](https://via.ritzau.dk/pressemeddelelse/13651980/ny-fabrik-fremstiller-protein-ud-af-den-bla-luft?publisherId=13560667)
publicly states an EUR 8.1 million buyer-credit guarantee, describes the risk
allocation as 45% EKF, 45% Finnvera, and 10% Danske Bank, and says the
financing included co-financing of the EKF guarantee premium. This corrects
the earlier statement that no high-level coverage allocation was public.

The source has been reviewed but is not retained or hash-bound in the existing
package, so the claim ledger and machine admission result have not been
changed. Executed coverage base, beneficiary rights, premium cash, exclusions,
recourse, claim timing, calls, losses, settlements, and recoveries remain
unknown. Dividing EUR 8.1 million by 45% produces an implied EUR 18 million
denominator that does not reconcile automatically to the reported EUR 15
million loan or EUR 16.3 million principal; it is not admitted to the ledger
without the executed loan, guarantee, and fee bridge.

## Publicly observed terms

Solar Foods' 5 September 2024 First North company description, PDF page 84
(numbered A-77), reports the following:

- a EUR 15 million export-credit loan agreement signed with Danske Bank in May
  2022 and fully drawn to finance construction of Factory 01's production
  process;
- EUR 16.3 million of principal, comprising the original EUR 15 million plus a
  capitalized EUR 1.3 million Danske commission;
- interest of 2.27% plus three-month EURIBOR through 31 December 2025 and 4.03%
  plus three-month EURIBOR thereafter;
- EUR 679,166.66 principal instalments every three months beginning 30
  September 2024;
- Finnvera Oyj and Denmark's export-credit agency EKF as guarantors; and
- minimum liquidity of EUR 4 million, maximum gearing of 1.5, a negative
  pledge, and environmental and social undertakings.

The same description says the covenants were not met at 30 June and 30
September 2023, but Danske gave advance consent not to exercise its termination
right. The covenants were met at 31 March and 30 June 2024. The audited 2024
and 2025 financial statements say the covenants were met during each year.

## Principal-cash reconciliation

| Period | Disclosed schedule implied by dates | Audited company-wide long-term-loan repayments | Result |
|---|---:|---:|---|
| 2024 | 2 × EUR 679,166.66 = EUR 1,358,333.32 | EUR 1,358,333.32 | Exact arithmetic match |
| 2025 | 4 × EUR 679,166.66 = EUR 2,716,666.64 | EUR 2,716,666.64 | Exact arithmetic match |

The match is evidence consistent with scheduled performance. It does not prove
which creditor received each euro: the audited balance sheet includes loans
from credit institutions alongside publicly described Business Finland
development loans, and the cash-flow line is not facility-specific.

The audited statements also report EUR 1,555,395.45 of interest and other
finance costs paid in 2024 and EUR 1,127,826.16 in 2025. Those are aggregate
company figures and are not attributed to the Danske claim. Likewise, security
and off-balance-sheet tables contain several unlabeled financial-loan and
guarantee amounts. This package does not infer facility collateral or guarantee
coverage from those totals.

The covenant and payment records from 2023–2025 are ex-post performance
evidence. They may test a model frozen at the May 2022 transaction date, but
they cannot enter the ex-ante physical probability set used to reconstruct the
original lender price.

## Financial normalization

| Required field | Public result | Consequence |
|---|---|---|
| Claim and funding | EUR 15 million agreement, full draw, and EUR 16.3 million including capitalized commission are reported | Economic price remains partial without the draw, deduction, and fee cash ledger |
| Principal cash | Instalment amount and start date are reported; audited aggregate repayments match six scheduled instalments through 2025 | Strong reconciliation signal, not claim-level settlement evidence |
| Interest cash | Index and margins are reported | Reset dates, fixing source, floor, day count, payment dates, default rate, and actual facility interest are missing |
| Maturity and optionality | No final maturity, complete prepayment, acceleration, or amendment schedule is public | Full promised cash cannot be reconstructed |
| Guarantee | EKF publicly announced an EUR 8.1 million buyer-credit guarantee and a 45% EKF / 45% Finnvera / 10% Danske risk allocation; financing reportedly included co-financing of the EKF premium | The new source is not yet retained or hash-bound. Executed coverage base, beneficiary rights, premium cash, exclusions, claim timing, recourse, calls, losses, settlement, and recovery remain unknown |
| Security and recovery | Negative pledge and lender termination or amendment rights are reported | Collateral, priority, enforcement, workout cost, and dated recovery cash are unknown |
| Covenants | Thresholds, two 2023 noncompliance dates covered by one advance non-exercise consent, and later compliance are reported | Useful state variables, but not a default or recovery distribution |
| Lender costs and formation | No complete cost ledger or competitive execution record | All-in price and orderly arm's-length formation are unresolved |
| Physical expected cash | No independent probability and recovery set for this claim | Expected-cash rate preimage cannot be solved |
| Comparable bridge | No complete eight-axis bridge to a 2026 cellular-agriculture market claim | The observation cannot be transferred to the target claim |

The complete field ledger is in
[`normalization_decision.tsv`](normalization_decision.tsv).

## Why the floating margin is not the expected return

The stated margins describe promised interest on an indexed principal. They do
not reveal the lender's all-in purchase price or the cash the lender expects
after prepayment, covenant breach, borrower default, guarantee claims,
enforcement, and recovery. The capitalized commission may change both borrower
principal and lender economics, but its funding and payment timing are not
shown. The public risk split is economically important, but it does not value
the guarantee without the executed coverage base, premium cash, claim rules,
and recovery mechanics.

The project's issue-price engine discounts cash that has already been reduced
for physical loss, delay, and recovery. Treating the contractual margin as the
discount rate would ignore the guarantee and fee economics and could count
credit-loss compensation twice. Until the complete state-contingent cash map
exists, there is no defensible `r`, comparable interval `H_j`, or market hurdle
`h`.

## What this transaction adds to the standard

This case contributes four concrete design rules without supplying a price:

1. A standard project claim needs a dated amortization schedule and a
   claim-level payment ledger, not only an ending debt balance.
2. A capitalized fee must be decomposed into borrower proceeds, claim
   principal, recipient cash, payment date, and payer; “principal” is not
   automatically the investor's price.
3. A public guarantee is a separate contingent claim with its own provider,
   cap, exclusions, timing, recourse, and realized settlement. It cannot be
   represented by the word “guaranteed.”
4. Covenant tests, lender consents, cures, and acceleration rights belong in the
   common project-state interface because they alter when cash can be called,
   delayed, or recovered.

Solar Foods is a gas-fermentation protein facility rather than a cultivated-
meat tissue bioreactor. It is therefore a useful adjacent construction-credit
case, not an identical asset. Technology, currency, maturity, scale,
liquidity, claim rights, and the 2022-to-2026 market regime all require an
evidenced comparison bridge.

## Control and claims boundary

Three official PDF records are retained with byte counts and lowercase SHA-256
hashes in [`source_manifest.tsv`](source_manifest.tsv). The manifest keeps
public-fact class (`A`–`E`), provenance tag, and distribution channel separate:
the company description is issuer disclosure (`C` / `CLM`), while exact facts
inside the signed audited statements are `A` / `AUD` within their stated scope.
Relevant pages were rendered and visually checked against the extracted
figures. A hash binds the retained bytes; it does not authenticate every
underlying statement, provide the omitted legal documents, or authorize use in
an investment decision.

In [`normalization_decision.tsv`](normalization_decision.tsv),
`NO_PUBLIC_SOURCE` means that every retained public record is silent on the
field; `DERIVED_DECISION` / `DER` marks a protocol conclusion rather than a
public fact. These are typed markers, not source identifiers or zeros. Public
facts retain their own evidence class and provenance on each normalized row.

The focused request required to change the result is in
[`CONTROLLED_DATA_REQUEST.md`](CONTROLLED_DATA_REQUEST.md). No outreach,
financing offer, valuation, guarantee representation, or claim of available
capital has been made.
