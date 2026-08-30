# Underwriting Data Standard

**Project:** Fostering Cellular Agriculture  
**Document status:** project specification, version 0.1  
**Effective date:** 27 August 2026

## Status and objective

This is an internal, open project specification for assembling evidence suitable
for research and preliminary underwriting of a cellular-agriculture facility.
It is not an accounting, audit, food-safety, engineering, prudential, or legal
standard and must not be represented as one.

The objective is to connect physical production, construction, regulation,
contracts, and cash flow without concealing uncertainty. The dataset should let
an independent reviewer answer:

1. What was directly observed?
2. What was derived, modeled, or extrapolated?
3. Which entity, facility, product, process, period, and jurisdiction does it
   describe?
4. Can it be reconciled to source records?
5. What is missing or contradicted?
6. How do uncertainty and failure propagate into liquidity, debt service,
   default, recovery, and instrument settlement?

## Non-negotiable controls

- No quantitative field without a unit, denominator, time basis, boundary,
  source, observation date, and evidence tag.
- Preserve raw records immutably. Corrections append a new version with reason,
  approver, timestamp, and link to the superseded value.
- Include all runs in the requested period. Never delete failed, aborted,
  contaminated, off-specification, research, cleaning-validation, or
  maintenance-affected runs; classify them.
- Keep source observations, derived fields, forecast assumptions, and scenario
  overrides in separate tables.
- Record missing values as missing with a reason. Do not silently use zero,
  averages, last observation carried forward, or management targets.
- Reconcile physical records to quality release, inventory, sales, invoices,
  cash, and the general ledger at defined control points.
- Label sponsor and vendor claims as claims until source records and verification
  support a stronger classification.
- Peer review does not make a modeled value observed. Regulatory publication of
  a sponsor submission does not make the sponsor's operational statements
  audited.
- Confidentiality may restrict access but may not change a provenance tag.
- Analytical code and data releases must be versioned. Reported results identify
  the exact data snapshot, code commit or release hash, compiler, build options,
  random seed, and scenario set.

Use the provenance and transformation tags defined in the
[evidence register](EVIDENCE_REGISTER.md).

## Required common metadata

Every table must carry or inherit the following fields:

| Field | Requirement |
|---|---|
| `record_id` | Stable unique identifier |
| `legal_entity_id` | Entity that owns, incurs, measures, or contracts the item |
| `facility_id` | Physical facility; do not merge pilot and commercial sites |
| `process_version` | Controlled process and equipment configuration identifier |
| `product_spec_id` | Exact specification and cultivated fraction where relevant |
| `jurisdiction` | Country and subnational authority where material |
| `period_start`, `period_end` | ISO 8601 timestamps with time zone or reporting dates |
| `value`, `unit`, `denominator` | SI units preferred; original unit retained; conversion formula recorded |
| `measurement_method` | Instrument, sampling plan, accounting rule, or calculation |
| `source_record_id` | Link to raw record, document, meter, invoice, contract, or ledger entry |
| `evidence_provenance` | `AUD`, `REG`, `CTR`, `MGT`, `PR-E`, `PR-M`, `STD`, `EXT`, `CLM`, or `HYP` |
| `transformation` | `OBS`, `DER`, `MOD`, `EXTPL`, or `SYN` |
| `verification_status` | `V0` through `V3`, with reviewer and review date |
| `uncertainty` | Distribution or interval and basis; measurement error kept separate from scenario uncertainty |
| `confidentiality` | Public, controlled, restricted, or privileged |
| `version` | Immutable source version and current record version |
| `exception_flag` | Missing, disputed, estimated, outlier, control failure, or none |

The data dictionary must define allowed values, sign conventions, currency,
price basis, rounding, and treatment of daylight-saving and calendar periods.

## 1. Reference-case and entity data

### Required fields

- legal owner, operator, borrower, issuer, parent, sponsors, and special-purpose
  entities;
- ownership, related-party relationships, guarantees, and structural
  subordination;
- site address, property rights, permits, utility interconnections, and material
  shared services;
- product form, species, cell line, cultivated share in any blended product,
  intended use, buyer specification, and shelf-life basis;
- process boundary from cell bank through released product, including outsourced
  steps;
- design train, reactor type and mode, working volume, seed train, downstream
  operations, packaging, storage, and logistics; and
- reporting currency, real or nominal basis, price base date, fiscal calendar,
  tax jurisdiction, and model horizon.

Changes in product, cell line, media formulation, process version, equipment,
facility, or regulatory scope require a new reference-case version. Historical
data may not be pooled across versions without an explicit comparability study.

## 2. Run-level operating data

The atomic production record is a run or continuous-campaign segment. Aggregate
monthly data alone are insufficient for calibration of failure frequency,
severity, downtime, and dependence.

### Run identity and configuration

- `run_id`, reactor/train ID, campaign ID, purpose, and commercial/research
  classification;
- cell line and bank lot, passage or population-doubling information where
  applicable, inoculum source, process version, media formulation and lots;
- reactor nominal and working volume, mode, control strategy, sensor set,
  scaffold/microcarrier system, and downstream route;
- inoculation, production, harvest, cleaning, sterilization, hold, release, and
  availability timestamps; and
- operator shift, material deviations, equipment deviations, and approved
  process changes.

### Inputs

- media and each material component by lot, mass/volume, concentration, and
  actual landed cost;
- growth factors, gases, antifoam, cleaning and sterilization inputs, scaffolds,
  microcarriers, enzymes, processing aids, packaging, and consumables;
- electricity, thermal energy, cooling, water by grade, wastewater, and waste
  treatment from calibrated meters or documented allocations;
- direct labour hours and role, quality-control testing, maintenance, and
  outsourced services; and
- animal-derived inputs, animal biopsies, tests involving animals, and their
  stated purpose.

### Outputs and quality

- gross harvested wet biomass and dry matter, with sampling and moisture method;
- **qualified cultivated output**: cultivated cell-derived mass released against
  the stated specification, excluding carrier, scaffold, plant ingredients,
  added water, packaging, rejected product, and unqualified inventory unless
  expressly defined otherwise;
- finished-product mass, cultivated fraction, yield through each downstream
  step, rework, scrap, waste, and by-product disposition;
- viable cell density, viability, growth-rate basis, metabolites, and other
  process-specific critical quality attributes;
- test results, sampling plans, limits, laboratory, out-of-specification events,
  holds, release decision, release date, and shelf-life outcome; and
- actual sale, internal use, destruction, donation, or inventory disposition.

### Run outcome taxonomy

Each run must end in exactly one primary outcome:

- released to specification;
- released under documented concession or alternate specification;
- reworked and released;
- partially rejected;
- fully rejected/off specification;
- contamination or suspected contamination;
- aborted for process, utility, equipment, material, personnel, or external
  cause;
- research/non-saleable by design; or
- open/in progress.

Record root cause, discovery time, affected lots, lost output, incremental cost,
downtime, remediation, recurrence, insurer recovery, customer consequence, and
regulatory notification where applicable. A financial model must not collapse
all failures into a harmless average yield.

### Minimum derived operating metrics

Derived metrics must be recalculable from atomic records:

- qualified output per reactor working-volume day;
- released-run rate and released-output rate;
- contamination event frequency and contaminated-output loss rate;
- gross-to-qualified and qualified-to-sold mass bridges;
- productive, occupied, cleaning, scheduled-down, unscheduled-down, and
  available reactor hours;
- capacity utilization using qualified output and a separately disclosed design
  basis;
- input consumption and cash cost per kilogram of qualified cultivated output;
- downstream recovery, rework rate, scrap rate, and inventory expiry;
- mean, median, dispersion, tail quantiles, confidence interval, sample size,
  and censored observations; and
- performance by scale, process version, cell line, operator period, and campaign.

Do not report nameplate output as actual capacity. Do not mix wet biomass,
finished blended product, and qualified cultivated output in one “kilograms”
series.

## 3. Construction and commissioning data

### Scope and cost

- process design basis, drawings, equipment list, battery limits, capacity,
  redundancy, and design assumptions;
- engineering maturity, open decisions, constructability review, and change log;
- vendor and contractor quote, scope, quantity, price, currency, quote date,
  expiry, escalation, tax, freight, duties, installation, commissioning, spares,
  exclusions, and payment schedule;
- land, buildings, clean areas, utilities, laboratory, warehousing, information
  systems, validation, permits, insurance, pre-operating expense, owner cost,
  financing cost, working capital, and reserves;
- committed, incurred, paid, forecast-to-complete, and disputed amount by work
  package; and
- contingency amount, method, allocation, draw criteria, and remaining risk.

All cost estimates must distinguish quotations, executed commitments, incurred
cost, invoices, paid cash, management estimates, and model escalation.

### Schedule and completion

- baseline and current critical-path schedules with logic and float;
- land, permit, design, procurement, manufacture, delivery, construction,
  utility, installation, commissioning, food-safety, validation, inspection,
  label, and commercial-operation milestones;
- actual versus baseline dates and cause of variance;
- contractor and vendor dependencies, interfaces, credit quality, and
  replacement lead times;
- mechanical-completion, sterile-commissioning, performance-test, reliability-
  run, regulatory, and commercial-acceptance criteria; and
- warranties, bonds, parent guarantees, liquidated damages, caps, exclusions,
  cure rights, and claim status.

“Complete” must be tied to an objective acceptance test. Mechanical completion,
regulatory permission, qualified production, and commercial operation are
separate milestones.

## 4. Operating-cost and cash records

Record actual quantity, price, supplier, invoice, payment, accounting period,
and cost center for:

- media components, growth factors, gases, scaffolds, enzymes, consumables, and
  packaging;
- utilities, water, wastewater, waste disposal, cleaning, sterilization, and
  environmental controls;
- direct and indirect labour, quality, laboratory, maintenance, facilities,
  software, licences, insurance, and contracted services;
- logistics, cold chain, storage, sales allowances, returns, disposal, and
  product liability;
- recurring regulatory, inspection, testing, certification, and compliance
  costs;
- sustaining capital expenditure and major maintenance; and
- related-party charges, allocations, subsidies, grants, tax credits, and
  non-cash items.

Reconcile at least monthly:

1. physical consumption to purchase, inventory, and waste records;
2. released output to inventory movement and sales;
3. invoice records to the accounts payable/receivable ledgers and cash; and
4. modeled operating cost to the general ledger, with an explained residual.

Report gross cost, grant- or subsidy-supported cost, and net cash cost
separately. Do not assume future supplier quotations remain available beyond
their validity or at volumes outside their stated scope.

## 5. Revenue, demand, and offtake data

### Evidence ladder

| Level | Evidence | Permitted model use |
|---|---|---|
| D0 | Survey, stated preference, market-size estimate, press interest | Market context only |
| D1 | Non-binding letter, memorandum, reservation without meaningful at-risk payment | Scenario support only; zero contracted revenue |
| D2 | Paid sample, trial, or purchase order with completed delivery and acceptance | Historical realized demand for that quantity/specification only |
| D3 | Executed conditional offtake or capacity-reservation agreement | Contracted case only after every condition, termination right, and credit limit is modeled |
| D4 | Effective take-or-pay, minimum-purchase, advance-purchase, or equivalent obligation from a creditworthy party | Contracted revenue to the enforceable amount, subject to performance, defenses, caps, set-off, and counterparty default |

### Required contract fields

- parties, affiliates, guarantors, governing law, execution and effective dates;
- product specification, testing, acceptance, rejection, substitution, recall,
  and warranty;
- annual/monthly minimum, maximum, ramp, tolerance, nomination, delivery point,
  title, and risk transfer;
- price, currency, index, floor, cap, reset, rebate, credit, tax, freight, and
  most-favored terms;
- take-or-pay or minimum-purchase mechanics, prepayment, deposit, capacity fee,
  security, and refund rights;
- conditions precedent, including facility, product, regulatory, label,
  certification, price, quality, and launch conditions;
- tenor, renewal, termination for convenience/cause, cure, force majeure,
  change in law, hardship, volume suspension, and liability caps;
- buyer financial information, external rating if any, payment history,
  concentration, wrong-way risk, and collateral or guarantees; and
- amendments, side letters, waivers, disputes, credit notes, claims, and direct
  counterparty confirmation.

Revenue data must bridge ordered, delivered, accepted, invoiced, collected,
returned, credited, and written-off amounts. Gross list price is not realized
net price. A public announcement is `CLM` until the agreement or direct
confirmation establishes its terms.

## 6. Regulatory and food-safety data

Maintain one record for each required regulatory action:

- jurisdiction, authority, legal basis, product, species, cell line, process,
  facility, intended use, and label within scope;
- action type: consultation, application, safety assessment, authorization,
  licence, registration, inspection, grant of inspection, label approval,
  permit, certification, or notification;
- submission identifier, dates, published file, confidential file, questions,
  responses, deficiencies, conditions, commitments, expiry, renewal, and change
  control;
- responsible person, counsel/adviser, expected and actual timeline, fees, and
  dependency on construction or operating milestones;
- HACCP or applicable food-safety plans, preventive controls, sanitation,
  traceability, recall, environmental monitoring, and testing records;
- inspection findings, observations, corrective actions, warning or enforcement
  actions, adverse events, recalls, and closure evidence; and
- exact words and scope of any authorization or regulator conclusion.

For the United States, use primary FDA and USDA FSIS records; for the European
Union, use EUR-Lex, European Commission, EFSA, and competent-authority records;
for Singapore, use Singapore Food Agency records. The
[research agenda](RESEARCH_AGENDA.md) lists initial authoritative anchors.

Never encode “regulatory approved = yes” as a single project-wide flag.
Consultation completion, scientific opinion, marketing authorization, facility
inspection, grant of inspection, label approval, and export eligibility are
different variables.

## 7. Financing and counterparty data

### Capital structure and cash waterfall

- instrument, provider, commitment, drawn amount, currency, seniority, security,
  maturity, amortization, interest or return, fees, and prepayment;
- equity, sponsor support, grants, guarantees, insurance, reserves, and
  contingent capital;
- conditions precedent, representations, covenants, events of default, cure,
  cross-default, acceleration, enforcement, and intercreditor terms;
- cash waterfall, restricted payments, distribution lock-up, debt-service
  reserve, maintenance reserve, working-capital facility, and permitted hedges;
- collateral description, perfection status, valuation, useful life, alternate
  use, removal cost, and recovery timing; and
- tax, accounting, regulatory-capital, insolvency, netting, and set-off treatment
  supported by qualified advice.

### Counterparty data

For sponsors, borrowers, buyers, suppliers, engineering contractors, vendors,
guarantors, insurers, banks, hedge providers, calculation agents, and data
verifiers, record:

- legal identity and group structure;
- audited financial statements, interim statements, external rating if any,
  debt, liquidity, contingent liabilities, and relevant exposure;
- contractual obligation, limits, termination rights, credit support, and
  collateral;
- probability of default basis, loss given default, recovery period, and
  dependence on the same risk that triggers payment; and
- concentration and common exposures across facilities.

Do not assign value to a guarantee, warranty, insurance policy, or offtake
without modeling the provider's ability and obligation to perform in the same
stress that causes the claim.

## 8. Mission and impact data

Mission evidence must remain separate from credit evidence unless a contract
legally links payment to a verified outcome.

Required fields include:

- product mass and cultivated animal-cell-derived mass;
- conventional product and functional unit being compared;
- sales channel, geography, price, customer type, and substitution evidence;
- animal species and production system in the counterfactual;
- animal-derived media, scaffold, processing, testing, biopsy, and other inputs;
- attribution among the facility, buyer, subsidy, and financial instrument;
- displacement, leakage, rebound, additionality, and time horizon;
- measured energy, water, materials, waste, emissions factors, allocation, and
  uncertainty for any environmental claim; and
- verifier, method, conflicts, exceptions, and correction process.

Do not use facility output one-for-one as “meat displaced” or “animals saved.”
Those are modeled outcomes requiring an explicit behavioral and supply-response
counterfactual.

## 9. Forecast and risk-model dataset

### Input separation

Maintain distinct files or schemas for:

1. immutable observations;
2. validated derived measures;
3. contractual terms;
4. base forecast assumptions;
5. stochastic distributions and correlations;
6. named stress scenarios;
7. instrument terms; and
8. model outputs.

Every assumption must state its register IDs, fitting method, sample period,
exclusions, uncertainty, and override authority. When data are insufficient for
estimation, use transparent scenario ranges and label them `HYP`; do not fit a
spurious distribution.

### Required dependencies

Model joint behavior where evidence supports it, including:

- scale and yield;
- yield, media consumption, downstream loss, and quality release;
- contamination, downtime, and lost output;
- construction delay, capital overrun, interest during construction, and
  regulatory timing;
- price, demand, utilization, and buyer credit;
- input price, availability, supplier default, and substitution lead time;
- multiple facilities sharing a cell line, process, vendor, regulator, buyer,
  or data provider; and
- trigger events and hedge-counterparty credit or collateral calls.

Independence is never a default assumption merely because assets have different
facility IDs.

### Required scenarios

The scenario library must include, at minimum:

- construction delay and cost overrun;
- delayed or conditional regulatory outcome;
- slower ramp, lower density/yield, higher media consumption, and lower
  downstream recovery;
- single and repeated contamination or batch rejection;
- utility interruption and critical-equipment failure;
- supplier failure, input-price shock, and substitution delay;
- offtaker delay/default, price compression, volume shortfall, and product
  rejection;
- correlated failure across facilities or technologies;
- interest-rate, foreign-exchange, refinancing, and collateral stress where
  relevant;
- hedge provider or guarantor default at the trigger event; and
- reverse stress identifying combinations that exhaust liquidity, breach
  covenant, or make the instrument insolvent.

Scenario magnitudes must come from evidence or be labeled policy stresses. No
single “worst case” can establish bounded loss when the structure contains
uncapped or path-dependent obligations.

### Required outputs

- annual and monthly cash flow during construction, ramp, and stabilization;
- CFADS, DSCR, LLCR, reserve draw/replenishment, covenant breach, and liquidity
  shortfall;
- probability and timing of default, expected loss, loss given default,
  recovery, tail quantiles, and expected shortfall;
- equity cash flows, dilution, NPV, and IRR with timing conventions;
- instrument premium/fee, settlement, collateral/margin, counterparty exposure,
  and basis risk;
- sensitivity to each material unobservable input and important interactions;
- decomposition of loss by construction, biological, operational, demand,
  regulatory, market, and counterparty cause; and
- assisted versus unassisted financing and mission counterfactuals.

Report distributions and sample uncertainty, not only means. Present nominal
and real figures consistently and disclose discount-rate, inflation, tax, and
terminal-value assumptions.

## 10. Assurance and data-room requirements

### Data-room index

The controlled data room must contain:

- source-data inventory and data dictionary;
- raw exports with system, query, preparer, extraction date, and cryptographic
  hash;
- contracts, amendments, side letters, source invoices, bank and ledger
  records, quality records, regulatory correspondence, engineering packages,
  and meter/calibration records;
- transformation scripts, model source, build instructions, tests, and release
  artifacts;
- management representations, reviewer queries, responses, exceptions,
  conflicts, and unresolved items; and
- audit, assurance, agreed-upon-procedures, engineering, legal, accounting, and
  regulatory reports with complete scope and limitations.

### Independent review procedures

At minimum, agree procedures to:

1. sample run records back to raw control-system and laboratory records;
2. test completeness against reactor schedules, batch numbers, quality logs,
   inventory, and waste records;
3. recompute qualified output and key performance metrics;
4. reconcile major inputs and output to invoices, inventory, ledger, and cash;
5. confirm material offtake and support contracts directly with counterparties;
6. inspect construction progress and source cost/schedule evidence;
7. test regulatory status against primary authority records;
8. reproduce model builds and outputs from a clean environment; and
9. report exceptions without management filtering.

The scope determines the label. An agreed-upon-procedures report is not an
audit opinion; an engineering review is not a financial-statement audit; a food-
safety consultation is not assurance of forecast cash flows.

## 11. Readiness levels

| Level | Evidence available | Permitted conclusion |
|---|---|---|
| U0 — concept | Public literature, hypotheses, company claims | Frame questions and broad scenarios only |
| U1 — diligence | Complete internal records and draft contracts supplied, controls assessed | Preliminary facility model with explicit management-data dependence |
| U2 — verified pilot | Relevant-scale run history and key costs independently tested; regulatory path and buyer terms documented | Calibrated downside analysis for the reference configuration; still no transfer beyond stated scale |
| U3 — construction-ready | Engineering, permits/path, contracts, capital structure, contingencies, and independent reviews substantially complete | Investment/credit committee consideration subject to listed conditions |
| U4 — commissioned | Objective completion and regulatory/quality milestones passed; live reporting operating | Performance-based funding or risk transfer may begin under contract |
| U5 — stabilized | Sufficient consecutive operating history across relevant seasons and conditions, with failures captured and covenants tested | Recalibration, potential refinancing, and cautious replication analysis |

There is no automatic promotion based on elapsed time or management designation.
Unresolved material exceptions can prevent or reverse a readiness level.

## 12. Model-release checklist

A model release is not approved unless:

- the reference case and as-of date are explicit;
- every material input maps to evidence-register IDs;
- observations, extrapolations, peer-reviewed models, company claims, and
  hypotheses are visibly distinguished;
- all runs and material adverse events in scope are represented;
- physical and financial reconciliations pass or exceptions are quantified;
- contracts are modeled according to enforceable terms, conditions, caps, and
  counterparty credit;
- regulatory milestones have precise meanings and sources;
- dependencies and correlated stresses are considered;
- base, downside, severe, and reverse-stress outputs are reproducible;
- validation is performed by someone other than the principal model author;
- source data, code version, compiler, build options, random seed, and output
  hashes are archived; and
- limitations and conflicts appear with the results, not in a detached appendix.

Honesty in this project means that a wide range, an unfinanceable case, or a
decision to wait for evidence is a valid result. A financial instrument is
useful only if the underlying risk is real, measurable enough to govern, placed
with a capable counterparty, and connected to responsible productive capacity.
