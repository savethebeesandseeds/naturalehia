# Controlled Diligence Data Request

**Reference project:** Believer Meats Wilson facility  
**Request version:** 0.2  
**Issued:** 27 August 2026  
**Case status:** U.S. receivership and foreign-insolvency research case; current
asset disposition unresolved  
**Purpose:** reconstruct the financing failure, preserve a responsible workout
boundary, and determine whether any narrow future mechanism merits controlled
comparison

## Status and safeguards

This is a research diligence request, not a financing commitment, credit
approval, valuation, audit, regulatory opinion, or request to solicit an
investment. Receipt of information does not imply that the project or any
instrument is financeable.

This request is not addressed to former company management by default. Before
any transfer or outreach, establish which party is legally authorized to
release each record. Possible record holders include the U.S. receiver or
other controller, the foreign representative or trustee, secured creditors,
lessors, contractors, the IP or cell-bank owner, insurers, utilities,
regulators, and former officers acting only within confirmed authority. The
project has prepared this request but has not sent it or represented that
capital, a bid, or a transaction is available.

Before transfer, the parties should agree in writing on:

- confidentiality, permitted users, permitted purpose, retention, deletion,
  incident response, and return/destruction obligations;
- privilege and regulatory-sensitivity protocols;
- trade-secret and export-control handling;
- personal-data minimization and lawful processing;
- independent-reviewer access and reliance limits;
- a secure data-room provider, multifactor authentication, logging, watermarking,
  download policy, and named administrators; and
- the right to preserve adverse findings and report honest aggregate
  conclusions without disclosing protected details.

Do not transmit documents by ordinary email. Do not include unnecessary
personal, medical, authentication, bank-account, or employee-level data.
Redaction and coded identifiers are acceptable when they preserve the ability
to reconcile totals and test the relevant risk.

## Submission rules

1. Provide a current data-room index with stable request and document IDs.
2. Supply native machine-readable data in addition to signed or issued PDF
   records. Spreadsheets must retain formulas; do not replace them with values.
3. Mark each record `draft`, `issued`, `executed`, `superseded`, or `cancelled`.
4. State legal entity, facility, process version, product specification,
   reporting period, units, currency, price basis, owner, approver, and source
   system.
5. Include all records in the requested population. If the response is sampled,
   describe the population, sampling method, exclusions, and selection party.
6. Preserve failed, aborted, contaminated, off-specification, disputed, late,
   rejected, written-off, and cancelled items.
7. Identify missing records, the reason, responsible owner, expected delivery
   date, and any substitute evidence. Do not use zero for missing.
8. Provide source-system export definitions, field dictionaries, time zones,
   sign conventions, unit conversions, and change history.
9. For every management forecast, provide the observation/assumption split,
   source, formula, approval date, and prior forecast-versus-actual history.
10. Provide file hashes or a data-room immutable version identifier with every
    formal release.

Unless otherwise specified, the requested operating period is the later of:

- all runs since the process version proposed for Wilson entered controlled
  operation; or
- the most recent 24 months plus the full commissioning and ramp period.

## Phase 0 — establish authority, estates, and preservation

No later phase should begin until legal authority and record preservation are
clear.

### 001. U.S. receivership and foreign proceeding

Provide certified or court-filed copies of:

- the North Carolina limited and general receivership petitions and orders,
  later amendments, receiver bond, powers, reporting duties, automatic stay,
  litigation authority, sale authority, and current status;
- the Chapter 15 petition, foreign orders, sworn declarations, signed
  recognition order, protocol with the U.S. receiver, and current foreign
  proceeding status;
- formation, ownership, good-standing, assumed-name, board, shareholder, and
  beneficial-ownership records for Future Meat Technologies Ltd., Future Meat
  Technologies Inc., and every material affiliate; and
- a signed responsibility matrix naming who may certify, disclose, license,
  sell, abandon, or finance each facility, asset, contract, permit, process,
  dataset, cell bank, and IP right.

**Gate mapping:** `REF-LEGAL-OPERATOR`,
`REF-FINANCING-OBLIGOR-AUTHORITY`, `REF-GOVERNING-LAW`.

### 002. Claims, liens, sale, and proceeds

Provide:

- complaints, answers, judgments, confessed judgments, mechanics' liens,
  deeds of trust, UCC records, landlord and lessor claims, tax claims, employee
  claims, intercompany claims, objections, settlements, and releases;
- the receiver's claims register with asserted, allowed, disputed, secured,
  priority, and unsecured amounts;
- asset inventories, appraisals, marketing materials, bidder access rules,
  bids, stalking-horse documents, termination records, auction/hearing records,
  receiver reports, proposed and approved sale terms, and excluded assets;
- professional and preservation expenses, cash receipts/disbursements,
  insurance proceeds, distributions, and remaining reserves; and
- the contractual and statutory priority, subrogation, set-off, netting, lien,
  cure, enforcement, and no-double-recovery analysis.

**Gate mapping:** `REF-ASSET-PERIMETER`, `FIN-CLAIMS-RECOVERY`,
`INS-COLLATERAL-CLOSEOUT`.

### 003. IP, cell bank, data, and licence continuity

Provide:

- patent, application, know-how, trade-secret, process-data, software,
  automation, recipe, cell-bank, biological-material, trademark, and domain
  schedules with legal and beneficial owner;
- licences, royalties, restrictions, sublicensing, field and territory,
  termination, insolvency, assignment, escrow, source-code, material-transfer,
  confidentiality, cure, and lender or purchaser step-in rights;
- cell-bank genealogy, location, backup, access control, viability, testing,
  replacement rights, transport, export/import, and chain of custody; and
- independent chain-of-title, freedom-to-operate, and successor-operation
  opinions for the exact proposed use.

**Gate mapping:** `REF-IP-RIGHTS-CONTINUITY`.

## Phase A — close the reference boundary

This phase is the minimum required before a confidential calibration effort can
be scoped.

### A01. Legal entities and authority

Provide:

- legal-entity and ownership chart through ultimate beneficial ownership;
- formation, registered-name, assumed-name, good-standing, and authorization
  records for site owner, equipment owner, operator, borrower, issuer, parent,
  sponsors, guarantors, and material related parties;
- board-approved Wilson project scope, authorized budget, and financing need;
- existing guarantees, intercompany agreements, management/service agreements,
  IP licences, and related-party balances; and
- authorized representatives for technical, commercial, financial, regulatory,
  and data certifications.

**Gate mapping:** `REF-LEGAL-OPERATOR`,
`REF-FINANCING-OBLIGOR-AUTHORITY`, `REF-GOVERNING-LAW`,
`REF-USE-OF-FUNDS`.

### A02. Site, property, and asset perimeter

Provide:

- legal address, survey, title or lease, easements, zoning, building and
  occupancy records, environmental matters, utility rights, and property tax;
- fixed-asset register with owner, serial number, location, in-service date,
  gross/net book value, liens, lease status, and disposition;
- shared assets, off-site services, tolling, warehousing, laboratories, and
  steps performed outside Wilson; and
- current insurance schedule and material claims.

**Gate mapping:** `REF-FACILITY-SITE`, `REF-ASSET-PERIMETER`,
`REF-JURISDICTION`.

### A03. Product and process definition

Provide:

- controlled cell-material, ingredient, intermediate, and finished-product
  specifications, including cultivated fraction and other ingredients;
- product applications, package, shelf life, storage, distribution, and buyer
  specifications;
- cell-line and bank genealogy, current bank lots, process versions, change
  controls, and comparability assessments;
- process-flow diagrams from source cells through released saleable product;
- equipment/train mapping, nominal and working volumes, mode, seed train,
  harvest, washing, downstream formulation, packaging, and quality release; and
- a cross-reference showing which Wilson process/equipment/product versions are
  covered by each regulatory submission and amendment.

**Gate mapping:** `REF-PRODUCT-SPEC`, `REF-PROCESS-BOUNDARY`,
`REF-REGULATORY-FACILITY-SCOPE`.

### A04. Regulatory and food-safety status

Provide:

- complete FDA and USDA submissions, supplements, correspondence, meetings,
  commitments, questions, responses, and open-action logs;
- complete FSIS P2816 grant, label, inactivation, withdrawal or suspension,
  effective-date, reason, condition, correspondence, reactivation, formula,
  claim, and inspection records; the public directory currently says Inactive;
- HACCP plan, hazard analysis, SSOPs, preventive controls, recall plan, food
  defence, traceability, and product-release authority;
- inspection, observation, nonconformance, warning, recall, complaint, and
  corrective-action records;
- environmental, wastewater, pressure-vessel, fire, building, occupancy,
  worker-safety, and other material permits; and
- a regulatory matrix stating exact product, process, facility, jurisdiction,
  approval/inspection type, effective date, conditions, expiry, owner, and open
  actions.

**Gate mapping:** `REF-REGULATORY-FACILITY-SCOPE`, `REG-PRODUCT-SCOPE`,
`REG-OPEN-CONDITIONS`.

## Phase B — establish model-calibration evidence

Phase B must use complete source populations. Selected success runs or summary
slides are insufficient.

### B01. Run and campaign master

For every run or continuous campaign segment, provide native tables containing:

- run, campaign, cell bank/lot, process version, product spec, equipment/train,
  purpose, research/commercial class, and operator shift IDs;
- inoculation, expansion, production, harvest, wash, formulation, release,
  cleaning, sterilization, maintenance, and availability timestamps;
- nominal/working volume, viable cell density, viability, doubling/growth,
  harvest biomass, dry matter, recovery, quality result, released cultivated
  material, finished-product cultivated fraction, and saleable quantity;
- all deviations, alarms, holds, contamination, aborts, rejects, rework,
  destruction, investigation, root cause, corrective/preventive action, and
  downtime; and
- change-control links and reasons a run is excluded from any internal KPI.

Reconcile the run master to batch records, quality release, inventory, waste,
shipment, invoice, and general-ledger control totals.

**Gate mapping:** `TEC-COMPLETE-RUN-HISTORY`, `TEC-FAILED-RUNS`.

### B02. Physical inputs, outputs, and mass balance

By run and lot, provide:

- cell-culture media and every component, growth factor, gas, antifoam,
  cleaning/sterilization material, enzyme, scaffold/microcarrier, processing
  aid, ingredient, package, and consumable;
- input lot, quantity, concentration, unit, supplier, landed cost, expiry,
  release status, loss, and substitute;
- material remaining in recycle, waste, work in process, released cell biomass,
  other ingredients, finished product, inventory, shipment, and unexplained
  variance; and
- animal-derived origin, species, purpose, stage, quantity, replacement
  frequency, and animal-testing use for every applicable material.

Supply reconciliation formulas and control limits. Quantify rather than omit
unexplained variance.

**Gate mapping:** `TEC-MASS-BALANCE`, `IMP-ANIMAL-INPUT-BASELINE`.

### B03. Utilities, labour, maintenance, quality, and cost

Provide interval or batch-allocated:

- electricity, thermal energy, gases, cooling, water by grade, wastewater,
  waste treatment, and demand charges from calibrated meters;
- labour hours, role, shift, payroll allocation, contractor, overtime, and
  training burden using coded staff IDs;
- quality sampling, assays, external laboratory, release, environmental
  monitoring, deviation, and investigation cost;
- planned/unplanned maintenance, spare parts, service contracts, calibration,
  cleaning, sterilization, and changeover; and
- purchase order, receipt, invoice, inventory, accrual, payment, and general
  ledger account links.

Provide a monthly bridge from source transactions to reported COGS and from
run quantities to cost per released kilogram. Separate cash cost, accrual,
depreciation, research cost, commissioning cost, abnormal loss, and allocation.

**Gate mapping:** `TEC-UTILITY-COST`, `FIN-OPERATING-LEDGER`.

### B04. Design basis and independent technical review

Provide issued versions of:

- basis of design, PFDs, P&IDs, equipment list, line list, control narrative,
  facility layout, utility balance, mass balance, process safety and hazard
  studies, hygienic zoning, contamination-control strategy, and redundancy;
- capacity model from working volume and cycle time through availability,
  yield, release, formulation, and saleable output;
- vendor design guarantees, acceptance protocols, operating envelopes,
  warranties, exclusions, and performance history; and
- every independent engineer, commissioning, technology-readiness, validation,
  capacity, reliability, and lender technical-adviser report, including scope,
  reliance, exceptions, management responses, and unresolved actions.

**Gate mapping:** `CON-DESIGN-BASIS`, `TEC-INDEPENDENT-REVIEW`.

### B05. Project cost, commitments, and schedule

Provide:

- original and current control budgets at work-breakdown-structure level;
- vendor bids, purchase orders, contracts, invoices, payment applications,
  change orders, claims, credits, accruals, paid-to-date, committed cost,
  uncommitted forecast, escalation, tax, freight, owner cost, and interest;
- original and current sources and uses, contingency, reserves, public
  incentives, reimbursable costs, compliance, draws, and clawback terms;
- baseline and current resource-loaded schedules, monthly updates, logic,
  critical/near-critical path, float, delays, change records, and recovery plan;
- mechanical completion, energization, commissioning, validation, reliability
  run, product qualification, regulatory, first-sale, and ramp milestones; and
- quantitative cost and schedule risk analyses with assumptions, correlations,
  simulations, confidence levels, contingency draw policy, and independent
  review.

**Gate mapping:** `CON-CAPEX-QUOTES`, `CON-SCHEDULE`, `CON-CONTINGENCY`,
`FIN-SOURCES-USES`.

### B06. Supply, utilities, resilience, insurance, and closure

Provide:

- critical media, growth-factor, gas, ingredient, consumable, service, and
  spare-part contracts; concentration, lead times, quality rights, price,
  substitutes, inventory, arrears, assignment, and termination;
- executed electric, gas, water, wastewater, interconnection, capacity,
  tariff, demand, curtailment, upgrade, security-deposit, transfer, and arrears
  records;
- O&M organization, staffing and training plan, technology-provider duties,
  maintenance strategy, key-person dependencies, OT cybersecurity, backups,
  incident history, data integrity, and business continuity;
- property, contamination, recall, product-liability, business-interruption,
  cyber, D&O and other policies, limits, deductibles, exclusions, notices,
  claims, proceeds and insurer reservations; and
- safe-shutdown, biological-material, chemical, waste, equipment, remediation,
  permit, decommissioning, abandonment, cost, owner, funding and financial-
  assurance plans.

**Gate mapping:** `TEC-CRITICAL-SUPPLY`, `CON-UTILITY-CAPACITY`,
`TEC-OM-RESILIENCE`, `FIN-INSURANCE`, `CON-DECOMMISSIONING`.

## Phase C — complete the common commercial and financing diligence checklist

Phase C completes shared inputs for later structure-specific research. It does
not approve a comparison, create an executable term sheet, or assess execution
readiness.

### C01. Buyer obligations and acceptance

For every proposed contracted buyer, provide:

- executed agreement, amendments, side letters, purchase orders, and current
  confirmation directly from the counterparty;
- annual/monthly minimum and maximum volume, ramp, take-or-pay or reservation
  amount, price, indexation, floor/cap, rebate, currency, delivery, taxes, and
  term;
- exact qualified-output definition, sampling, testing, release, rejection,
  substitution, rework, credit, title/risk transfer, and dispute mechanics;
- conditions precedent, regulatory/label conditions, force majeure, change in
  law, termination, liquidated damages, cure, step-in, assignment, and renewal;
  and
- counterparty legal entity, audited financials, ratings where applicable,
  parent guarantee, collateral, letter of credit, limits, existing exposures,
  and concentration.

Expressions of interest, forecast volumes, tastings, development arrangements,
MOUs, and non-binding term sheets must be identified separately.

**Gate mapping:** `COM-EXECUTED-OFFTAKE`, `COM-ACCEPTANCE`,
`COM-COUNTERPARTY-CREDIT`.

### C02. Capital structure, liquidity, and recovery

Provide:

- a dated financing chronology; amounts sought, offered, committed, funded, or
  withdrawn; terms, conditions, decision records, and the diagnosed binding
  constraint;
- direct capital-provider records for ordinary debt, equity, strategic,
  customer, grant, and asset-sale alternatives, including declined structures
  and reasons, plus an independent review of the diagnosis;
- three years of audited statements if available, current management accounts,
  cash, runway, forecast, capitalization, shareholder rights, debt, leases,
  liens, covenants, defaults/waivers, and contingent liabilities;
- proposed financing term sheets, sources, fees, reserves, draw conditions,
  amortization, maturity, cash sweep, covenants, security, guarantees,
  intercreditor terms, events of default, cures, enforcement, and recovery;
- a reconciled claims register and independently reviewed inventory,
  receivable, fixed-asset, IP, insurance, and enterprise-recovery values under
  base and downside cases, including administrative costs and distributions;
  and
- litigation, investigations, claims, related-party exposures, public support,
  grants, incentive compliance, and clawbacks.

**Gate mapping:** `FIN-FINANCING-FAILURE-DIAGNOSIS`,
`FIN-CAPITAL-ALTERNATIVES`, `FIN-SOURCES-USES`, `FIN-TERM-SHEET`,
`FIN-OPERATING-LEDGER`, `FIN-CLAIMS-RECOVERY`,
`FIN-RECOVERY-VALUATION`, `FIN-SPONSOR-EQUITY`.

### C03. Completion definition and support

Provide the proposed contractual test for:

- mechanical completion and lien release;
- utilities, safety, food-safety systems, and regulatory readiness;
- commissioning, process qualification, consecutive reliability runs, minimum
  capacity, yield, quality release, and unit cost;
- independent certifier, test data, retest, cure, waiver, dispute, longstop,
  termination, and step-in;
- sponsor/vendor overrun and delay support, deductible, cap, expiry, exclusions,
  recourse, collateral, and payment timing; and
- interaction with debt draws, reserves, covenants, offtake, and default.

**Gate mapping:** `CON-COMPLETION-TEST`.

### C04. Professional characterization

For each candidate economic arrangement, obtain scoped advice addressing:

- enforceability, capacity and authority, regulatory licences, insolvency,
  security, set-off, netting, close-out, and dispute forum;
- securities, commodity/derivative, insurance, banking, consumer, procurement,
  sanctions, antitrust, and food-regulatory classification;
- tax, withholding, transfer pricing, grant/public-support interaction, and
  cross-border matters;
- accounting recognition, revenue, leases, guarantees, derivatives, fair value,
  hedge accounting, consolidation, impairment, and disclosure; and
- prudential capital, large exposure, collateral, liquidity, and risk-retention
  implications for relevant counterparties.

**Gate mapping:** `FIN-TAX-ANALYSIS`, `FIN-ACCOUNTING-ANALYSIS`,
`FIN-LEGAL-ANALYSIS`, `REF-GOVERNING-LAW`.

### C05. Structure-specific controlled terms

Provide separate controlled term sets; do not combine unlike mechanisms into a
single “support” case.

For **milestone-gated capital**, provide eligible costs, objective milestones,
tolerances, independent verifier, staged commitment, draw controls, stop, cure,
re-baselining, provider capacity, and safe workout.

For a **qualified-output offtake or capacity reservation**, provide direct
buyer mandate, product and acceptance, availability, metering, delivery,
quantity, price, payment, buyer credit, security, regulatory conditions,
termination, substitution, and lender step-in.

For a **floor or contract for difference**, provide evidence that price is the
binding constraint, benchmark governance and history, basis, eligible physical
notional, manipulation controls, settlement, cap, fallback, disruption,
collateral, liquidity, close-out, provider authority, and severe-case capacity.

For **completion, overrun, or partial credit support**, provide the covered
loss, trigger, causation, exclusions, claim calculation, verifier, provider
capacity, reimbursement, subrogation, recovery, moral-hazard controls, cure,
and no-double-recovery rules.

**Gate mapping:** `INS-MILESTONE-MECHANICS`,
`INS-PRICE-INDEX-GOVERNANCE`, `INS-PRICE-SETTLEMENT-MECHANICS`,
`INS-SUPPORT-PROVIDER-CREDIT`, `INS-SUPPORT-CLAIMS-PROTOCOL`,
`INS-COLLATERAL-CLOSEOUT`, `COM-ACCEPTANCE`,
`COM-COUNTERPARTY-CREDIT`.

## Phase D — establish mission measurement separately

### D01. Financing additionality protocol

Before the instrument is known to outcome evaluators, pre-specify:

- the no-instrument funding, timing, capacity, and cost-of-capital counterfactual;
- decision records showing why ordinary capital is unavailable or mismatched;
- the exact mechanism expected to change funding probability, amount, tenor,
  spread, commissioning date, or qualified capacity;
- comparison group or other identification method, confounders, sensitivity,
  uncertainty, governance, conflicts, and independent review; and
- rules for partial attribution when grants, sponsor equity, strategic support,
  or other changes also affect the outcome.

**Gate mapping:** `IMP-FINANCING-ADDITIONALITY`.

### D02. Displacement, animal-input, and welfare protocol

Pre-specify and then report:

- released and sold cultivated quantity, finished-product cultivated fraction,
  geography, channel, price, buyer, inventory, waste, return, and substitution;
- conventional chicken purchasing/production displaced by the buyer, market
  response, leakage, rebound, imports/exports, time horizon, and uncertainty;
- all source animals, biopsies, fertilized eggs, animal-derived media/reagents,
  scaffolds, testing, bank replacement, validation, and supplier-chain inputs;
- welfare boundary and safeguards for workers, food safety, consumers, and
  animals; product and bird yield, production-system boundary, coproduct
  allocation, severity and duration, and uncertainty; and
- independent reviewer, data access, methods, exceptions, correction policy,
  and publication boundary.

Until this method is accepted and observed data exist, report only source-tagged,
independently reconciled achieved output—or explicitly identified design
capacity—with no impact interpretation. Do not report “animals saved.”

**Gate mapping:** `IMP-DISPLACEMENT-METHOD`,
`IMP-ANIMAL-INPUT-BASELINE`, `IMP-WELFARE-CONVERSION`.

### D03. Observed outcome release

After the method is frozen, provide the observed financing decision and terms;
capital funded; commissioning and output chronology; released, sold and paid
quantity; cultivated fraction; inventory, returns and waste; buyer conventional
purchases; substitution; animal inputs; leakage; rebound; attribution;
uncertainty; exceptions; corrections; and independent assurance.

**Gate mapping:** `IMP-OBSERVED-FINANCING`,
`IMP-OBSERVED-OUTPUT-SALES`, `IMP-BUYER-SUBSTITUTION`,
`IMP-ATTRIBUTION-RESULT`, `IMP-OUTCOME-ASSURANCE`, and all other Phase D
requirements.

## Acceptance and release protocol

Each phase should end with a frozen release containing:

1. data-room index, immutable release identifier, document versions, and an
   externally anchored signed manifest root or equivalent release control;
2. files, machine-readable exports, and lowercase SHA-256 for each retained
   file;
3. data dictionary and entity/facility/process/product crosswalk;
4. management completeness certification with disclosed exceptions;
5. named reviewers and approvers; competence; procedures; review dates;
   conflicts and management; findings; reconciliations; and open issues;
6. evidence-manifest records with retained-copy, hash, document version,
   extract, resolution chain, decision-use, approval, and conflict fields; and
7. an evidence-gate report stating the highest allowed use.

No phase passes through elapsed time or document volume. It passes only when
every compiled requirement for that gate has qualifying evidence and no
unresolved contrary record. The research team may stop after any phase if the
underlying economics, evidence quality, legal feasibility, safety, or mission
case does not support further work.
