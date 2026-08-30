# Public Calibration Evidence Snapshot v0.1

**Observation date:** 30 August 2026  
**Status:** source-backed research boundary; no real project or probability is
calibrated by this document.

## Purpose

The instrument engines already calculate cash, loss, exposure, liquidity,
tail risk, and return from explicit project paths. This snapshot records what
current public evidence can honestly contribute to those paths.

The result is deliberately asymmetric. Public records can define real event
dates, financing forms, announced commitments, observed accounting amounts,
regulatory states, demonstrated experiments, and failed-project stresses. They
do **not** presently supply a complete multi-project population from which to
estimate default probabilities, loss distributions, recoveries, dependence,
or risk-adjusted expected returns.

This is not a market survey, valuation, rating, recommendation, or assertion
that the named companies are comparable pool assets.

## Evidence classes

Each public fact retained for research must keep one of these classes. A lower
class may define a candidate state, diligence question, or sensitivity, but it
cannot be silently promoted into a controlled model input.

| Class | Meaning | Permitted use |
|---|---|---|
| A | regulator record, executed public decision, audited account, court record, or official filing | exact dated fact within its legal and entity boundary |
| B | peer-reviewed empirical measurement | technical observation within the tested process, vessel, product, and run boundary |
| C | issuer operating disclosure | candidate observation with issuer, scope, and independent-verification limitations visible |
| D | nameplate, target, plan, award announcement, forecast, LOI, or MOU | scenario definition and diligence question only unless converted by an executed or observed record |
| E | techno-economic model or other extrapolation | sensitivity or stress only; never realized operating performance |

The same source can contain several classes. Audited financial statements, for
example, can contain audited cash balances alongside unaudited management
capacity targets.

## Current technical and regulatory observations

| Public fact | Class | Candidate-state or acquisition use | Prohibited inference |
|---|---|---|---|
| FDA's inventory, last updated 19 February 2026, lists five completed cultured-animal-cell consultations: UPSIDE chicken, GOOD Meat chicken, Mission Barns pork fat, Wildtype salmon, and Believer chicken. ([FDA inventory](https://www.hfpappexternal.fda.gov/scripts/fdcc/index.cfm?set=AnimalCellCultureFoods)) | A | Product-, species-, sponsor-, and date-specific regulatory event states | Five consultations are not a success-rate sample and consultation completion is not full US commercial clearance. |
| FSIS states that covered meat and poultry facilities also need a grant of inspection, applicable controls, and prior label approval; harvest and processing inspection occurs at least once per shift. ([FSIS framework](https://www.fsis.usda.gov/inspection/compliance-guidance/labeling/labeling-policies/human-food-made-cultured-animal-cells), [Directive 7800.1](https://www.fsis.usda.gov/policy/fsis-directives/7800.1)) | A | Separate FDA, establishment, inspection, and label milestones with distinct dates | An FDA consultation cannot be modeled as permission for any facility, label, process, or product to sell. |
| Pasitka and co-authors report continuous cultivated-chicken measurements up to 130 million cells/ml, 43% w/v yield, harvests over more than 20 days, and animal-component-free medium at USD 0.63/litre. Their 50,000-litre and USD 6.20/lb case is a theoretical extrapolation. ([Nature Food](https://www.nature.com/articles/s43016-024-01022-w)) | B; E | Bench-scale density, duration, yield, and medium-cost observations; a separately tagged scale-up sensitivity | The theoretical facility is not realized industrial output, cost, utilization, or qualified product. |
| UPSIDE reported in February 2024 that it had conducted dozens of 2,000-litre runs and produced enough cells for thousands of pounds over several months. ([issuer update](https://upsidefoods.com/blog/the-winding-road-from-first-sale-to-commercial-scale/)) | C | Issuer-claimed demonstrated vessel scale and a lower-bound run-count description | No failed-run denominator, per-run mass balance, saleable fraction, utilization, cost, or paid sales volume is disclosed. |
| Believer's FDA file says production-run evidence came from Rehovot, while the planned Wilson process used larger bioreactors, larger wash centrifuges, and more automation. FDA's review notes that further large-scale confirmation data were planned. ([FDA response material](https://www.fda.gov/media/188063/download), [scientific review memorandum](https://www.fda.gov/media/188065/download)) | A; C; D | A site-transfer and scale-transfer state with explicit validation-site and financed-site fields | Rehovot safety evidence is not Wilson commissioning, capacity, operating-cost, or commercial-output evidence. |
| Humbird's peer-reviewed engineering model uses 24 twenty-cubic-metre production reactors and reports modeled fed-batch and perfusion costs; its optimum is constrained by carbon-dioxide removal at larger reactor sizes. ([Biotechnology and Bioengineering](https://pmc.ncbi.nlm.nih.gov/articles/PMC8362201/)) | E | A conservative engineering challenger and factor list | Conceptual process economics are not vendor quotes, a forecast, or a realized facility cost. |

The public observations admitted to this snapshot show empirical performance
far below many announced industrial designs. That scale-transfer gap is itself
a material exposure; it is not a complete sector survey and is not a reason to
interpolate a success probability.

## Current financial and transaction observations

| Public fact | Class | Candidate-state or acquisition use | Prohibited inference |
|---|---|---|---|
| Solar Foods' audited FY2025 accounts report EUR 123,594.51 revenue, EUR 7,989,830.30 cash, EUR 10,400,346.16 operating loss, EUR 6,239,000.24 gross investment cash outflow, and EUR 5,593,079.67 investment-grant receipts. The board report separately describes a EUR 1.5 million order book, four LOIs/MOUs for 6.5–7.65 kt/year, and Factory 01 design capacity of 160 t/year. ([audited report](https://attachment.news.eu.nasdaq.com/a6d552ad8b8a9ae706e54eba7fa24be32)) | A; C; D | Dated entity cash, realized investment and grant cash, revenue, operating loss, order-book, non-binding pipeline, and nameplate fields kept separate | Entity cash is not facility cash without a ledger bridge; LOIs/MOUs are not contracted receipts; design capacity is not output or paid sales. |
| Solar Foods' May 2022 Danske facility was fully drawn for Factory 01, with EUR 16.3 million principal including a capitalized EUR 1.3 million commission, floating margins, quarterly EUR 679,166.66 principal, Finnvera and EKF guarantees, and covenants. EKF separately announced an EUR 8.1 million buyer-credit guarantee, a 45% EKF / 45% Finnvera / 10% Danske risk allocation, and co-financing of its guarantee premium. Audited aggregate long-term-loan repayments equal two disclosed instalments in 2024 and four in 2025. The [retained transaction dossier](../reference_transactions/solar-foods-factory-01-facility-2022-2025/PUBLIC_TRANSACTION_DOSSIER.md) binds its earlier source set and field-level rejection; the EKF page has been reviewed but is not yet retained or hash-bound. ([EKF announcement](https://via.ritzau.dk/pressemeddelelse/13651980/ny-fabrik-fremstiller-protein-ud-af-den-bla-luft?publisherId=13560667), [company description](https://investors.solarfoods.com/files/documents/Solar%20Foods%20Oyj%20Yhtioesite.pdf), [2024 audit](https://attachment.news.eu.nasdaq.com/a3e35f7378b94c29a94be57f1c7a89558), [2025 audit](https://attachment.news.eu.nasdaq.com/a6d552ad8b8a9ae706e54eba7fa24be32)) | A; C | A real amortizing construction-credit schedule, capitalized-fee principal composition, disclosed risk allocation, covenant states, and strong aggregate principal-cash reconciliation | Aggregate cash is not a facility ledger. The EUR 8.1 million / 45% implied denominator is not a ledger fact and does not automatically reconcile to reported loan or principal. Executed coverage base, premium cash, claim mechanics, lender price and costs, full interest and maturity mechanics, security, recovery, probabilities, and comparison bridge remain missing, so no expected-cash rate preimage is formed. |
| Business Finland announced EUR 77.8 million for construction and commissioning of Solar Foods' Lappeenranta plant, with first phase intended by end-2028 and stated capacity rising from 160 to 6,400 t/year. ([official announcement](https://www.businessfinland.fi/ajankohtaista/tiedotteet/2026/business-finlandilta-suuri-rahoituspaatos-solar-foodsin-kaasufermentaatiolaitoksen-rakentamiseksi-lappeenrantaan/)) | A; D | Conditional public-support and construction-milestone states | An award is not cash at announcement. Payment conditions, eligible costs, lag, audit rights, cancellation, and retention require the executed decision. |
| EIB signed EUR 35 million for Formo on 16 December 2024 against an approximate EUR 73 million project cost for 2025–2027 R&D and production-capacity expansion. EIB describes its venture debt generally as long-term quasi-equity linking return to company performance. ([project record](https://www.eib.org/en/projects/all/20240101), [EIB release](https://www.eib.org/en/press/all/2025-008-eib-provides-eur35-million-to-formo-to-expand-production-of-cheese-alternatives-free-from-animal-products)) | A; D | A real quasi-equity/venture-debt precedent, signed amount, approximate project cost, date, and use boundary | Public material omits the exact return formula, maturity, security, covenants, waterfall, and realized return. The amount ratio is transaction-specific. |
| Invest-NL invested EUR 10 million in Mosa Meat through a convertible loan in a EUR 40 million 2024 round; its announcement states that a 50% InvestEU guarantee backed Invest-NL's investment. A EUR 5 million follow-on was announced in December 2025 within about EUR 15 million additional financing. ([2024 announcement](https://www.invest-nl.nl/en/news/investment-in-mosa-meat-cultivated-meat-as-a-sustainable-alternative), [2025 follow-on](https://www.invest-nl.nl/en/news/invest-nl-makes-follow-up-investment-in-mosa-meat), [programme parameters](https://www.invest-nl.nl/en/impact/our-portfolio/investeu)) | A; D | Convertible-loan class, observed staging, provider-side guarantee coverage, and programme tenor/amount limits | Guarantee coverage is not borrower cash, automatic recovery, or project LGD. Coupon, conversion, priority, exclusions, and claim timing are undisclosed. |
| InvestEU approved a EUR 15 million equity operation for Meatable in November 2023. Agronomics' audited FY2025 result later recorded a GBP 11.9 million full impairment of its separate Meatable position following announced voluntary liquidation. ([InvestEU operation](https://investeu.europa.eu/meatable_en), [Agronomics result](https://www.londonstockexchange.com/news-article/ANIC/final-results-and-notice-of-agm/17392489)) | A | A named historical financing and a realized full-equity-impairment stress for one investor | The two positions are not the same claim. Neither record supplies creditor recovery, guarantee settlement, plant-sale proceeds, debt LGD, or a default frequency. |
| Official North Carolina docket order 43 confirms that Believer's Wilson entity entered limited receivership on 31 December 2025 and general receivership on 6 February 2026; it recites Gray's asserted balance above USD 36.4 million and Ameris's asserted balance above USD 13 million. Docket order 63 says the receiver was liquidating assets and records CHRP as unsecured under a USD 3.25 million settlement, with USD 2 million paid by Believer in November 2025 and USD 1.25 million due 14 August 2026. The orders have been reviewed but are not yet retained or approved in the controlled package. ([docket order 43](https://www.nccourts.gov/documents/orders-of-significance/gray-constr-inc-v-future-meat-techs-inc-2026-ncbc-order-43), [docket order 63](https://www.nccourts.gov/documents/orders-of-significance/gray-constr-inc-v-future-meat-techs-inc-2026-ncbc-order-63)) | A | An observed receivership, cessation/liquidation, asserted-creditor, unsecured-workout, maintenance, and resolution-state taxonomy for a cellular-agriculture scale-up | Asserted balances are not allowed claims or priorities. The pre-receivership payment to CHRP is not an estate distribution. No default frequency, LGD, collateral value, sale proceeds, recovery timing, final distribution, or probability may be inferred. |
| Liberation Labs' February 2025 Form D reports USD 51,982,971 of debt securities sold to six investors. ([SEC filing](https://www.sec.gov/Archives/edgar/data/1952448/000195244825000001/xslFormDX01/primary_doc.xml)) | A | An observed issuer financing event, instrument category, amount sold, investor count, and filing date | Form D does not establish facility allocation, cash availability, coupon, maturity, security, conversion, use of proceeds, or construction completion; the SEC warns that it has not verified the filing. |
| Liberation Labs' April 2024 RNS reports a USD 12.5 million secured convertible promissory-note round; the corrected October RNS reports a **further** USD 3.5 million note for the same Richmond facility and qualified-financing context, with 10% annual interest and 10 October 2027 maturity. The [retained transaction dossier](../reference_transactions/liberation-labs-facility-financing-2024-2025/PUBLIC_TRANSACTION_DOSSIER.md) hash-binds the public copies and field-level rejection. ([April RNS copy](https://www.investegate.co.uk/announcement/rns/agronomics-limited--anic/agronomics-invests-us-10m-in-liberation-labs/8122169), [October RNS copy](https://www.lse.co.uk/rns/ANIC/agronomics-invests-us-2m-in-liberation-labs-wy84h8i0s3uzhzr.html?mobile_view=mobile)) | C | A real private-note candidate with promised terms, construction use, conversion context, investor amounts, relationship concerns, and an explicit cluster-de-duplication question | April versus October note-series overlap is unresolved. Promised 10% interest is not expected return; missing contract, settlement, costs, conversion, collateral, priority, recovery, ex-ante probabilities, and exact adjustment bridge prevent a rate preimage. |
| Official UK records show Algal Omega Ingredients entered administration in May 2025 with outstanding charges, while a later market announcement said operating assets were acquired with access to one million litres of fermentation capacity but did not disclose consideration. ([Companies House](https://find-and-update.company-information.service.gov.uk/company/NI630830), [charges](https://find-and-update.company-information.service.gov.uk/company/NI630830/charges), [asset announcement](https://www.londonstockexchange.com/news-article/ANIC/clean-food-group-update/17254722)) | A; C | Recovery-state taxonomy: secured claims, administration, specialist-asset transfer, and possible reuse | Capacity and reuse are not sale proceeds, creditor distribution, recovery percentage, recovery lag, or value per litre. |

## Hard conversion rules

A calibration release must reject these substitutions:

- LOI or MOU → contracted revenue;
- grant award → cash received;
- issuer financing → facility financing without a sources-and-uses bridge;
- nameplate capacity → realized or qualified output;
- produced biomass → finished product without mass and composition records;
- regulatory consultation → facility permission or transferable approval;
- equity impairment or book value → creditor recovery;
- guarantee percentage → borrower funding or automatic investor recovery; and
- one delay, liquidation, or successful run → event probability.

## What the public record supports now

It supports named candidate states such as:

- regulatory consultation complete but establishment or label incomplete;
- announced support not yet reimbursed;
- construction delayed, deferred, or dependent on partner funding;
- demonstrated process at one scale with transfer to a larger site unresolved;
- qualified output below nameplate capacity;
- non-binding demand without collected cash;
- issuer financing not demonstrably allocated to the facility;
- full equity impairment; and
- specialist assets sold or reused with creditor recovery unknown.

These states can enter a **synthetic candidate** scenario set. Their weights
and cash values remain hypotheses, bounds, or stresses. A future controlled
calibration release would also need exact-project evidence, a fully disclosed
method, and independent challenge before treating them as calibrated inputs.

## Evidence still required for a real pool

Before estimating probability, recovery, or expected return, obtain:

1. complete project and run populations with denominators, dates, failed,
   aborted, contaminated, off-specification, delayed, and unresolved cases;
2. facility-level construction budgets, commitments, invoices, change orders,
   schedule history, cost-to-complete, contingency, and commissioning tests;
3. reconciled mass, quality, inventory, energy, water, media, waste, labour,
   maintenance, output, invoice, collection, and general-ledger records;
4. executed financing, grant, guarantee, offtake, acceptance, security,
   priority, and claims documents;
5. creditor-level insolvency proceeds, costs, timing, and distributions;
6. common technology, cell line, supplier, equipment, regulator, buyer,
   sponsor, utility, insurer, and financing exposures across candidate assets;
7. a frozen observation date, inclusion and censoring rules, transfer method,
   challenger model, uncertainty basis, and independent validation; and
8. separate evidence for financing additionality and animal displacement.

Until that evidence exists, the honest result is a useful mechanics-tested
research artifact whose empirical risk, expected return, and price remain
unresolved.
