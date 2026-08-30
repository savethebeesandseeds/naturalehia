# Liberation Labs Note — Controlled Data Request

**Purpose:** obtain only the records needed to bound the April–October 2024
note cluster, reconstruct the October claim's all-in investor cash at its own
observation date, and decide whether it can enter a market-hurdle evidence set.
This is not a general management diligence request.

## 1. Exact claim and closing

- Fully executed April and October notes, purchase or subscription agreements,
  every schedule, side letter, waiver, amendment, accession, and
  qualified-financing definition.
- Final note register across every 2024 bridge issuance, showing holder, note
  series, face amount, issue price, currency, execution date, funding date, and
  settlement status; reconcile April USD 12.5 million, October USD 3.5 million,
  and the later reported USD 19 million aggregate.
- Redacted bank wire confirmations, closing statement, cash ledger, and
  sources-and-uses reconciliation linking proceeds to the Richmond facility.
- Investor, arranger, legal, diligence, custody, hedging, and other direct
  transaction costs, with payer and payment date.

## 2. Every dated cash right

- Interest payment dates, day count, compounding, cash-versus-PIK treatment,
  default rate, capitalization, withholding, and tax gross-up.
- Principal redemption, voluntary and mandatory prepayment, make-whole,
  acceleration, default, change-of-control, and transfer provisions.
- Complete conversion trigger, deadline, price, discount, valuation cap,
  accrued-interest treatment, dilution definition, security class, fractional
  treatment, and investor election rights.

## 3. Security, priority, and recovery

- Collateral schedule, UCC and other lien evidence, perfection status, lien
  rank, guarantors, excluded assets, intercreditor terms, permitted debt, and
  covenant package.
- Enforcement waterfall, standstill and control rights, workout costs,
  collateral valuations, recovery timing, and any guarantee or insurance.
- Historical covenant tests, waivers, defaults, restructurings, and adverse
  records through conversion.

## 4. Conversion settlement

- Series A1 financing documents and capitalization table immediately before
  and after conversion.
- Holder-by-holder calculation of principal, accrued interest, conversion
  price, shares or other claims issued, and settlement date.
- Share register or transfer-agent evidence and the fair-value method for any
  non-cash consideration; valuation is kept separate from observed cash.

## 5. Expected-cash reconstruction

- Frozen state definitions for repayment, conversion, delay, default,
  restructuring, and recovery, including all dated cash calls and receipts.
- Exact execution, funding, settlement, and observation timestamp used for the
  rate calculation. Freeze the physical probability set to information
  available no later than that evidenced transaction timestamp and in no case
  later than the 15 October 2024 public announcement; exclude all later
  financing and conversion outcomes from the ex-ante set.
- Independently reviewed physical probability set and recovery method that are
  independent of the observed price, promised rate, target price, and any
  reverse-engineered risk-neutral probability; retain source lineage, solver
  method, roots or root failure, and price-reconstruction residual.

## 6. Universe, de-duplication, and comparison bridge

- Frozen search universe, lookback, inclusion rule, and de-duplication manifest
  covering April, October, the January 2025 aggregate, related issuances, and
  known declined or failed financings. State the exact economic-cluster rule.
- For each of the eight comparison axes—contractual rights; priority and
  residual recovery risk; systematic, covariance, concentration, and model
  risk; maturity; currency and hedge cash; liquidity; size; and observation
  date or regime—provide separately sourced signed lower and upper adjustments.
- Provide the joint feasible adjustment set and prove that the displayed total
  interval is its exact connected projection; a componentwise box or hull is
  not eligible.
- Reconcile expected loss and recovery placed in state-contingent cash against
  residual risk-premium adjustments so no loss, guarantee, or recovery effect
  is counted in both cash and rate.

## Acceptance rule

Provide an indexed release with immutable file identifiers, document versions,
lowercase SHA-256 hashes, named reviewer, review date, procedures, conflicts,
and open exceptions. Missing fields stay missing; blank adjustments are not
zero. Later realized conversion data must remain a separately labelled
backtest. The project will generate a `hurdle-evidence.cfg` only if a frozen,
de-duplicated package produces an eligible, independently reconstructed
expected-cash rate preimage and an exact comparison bridge. A promised interest
rate or issuer valuation cannot substitute for that result.
