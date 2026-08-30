# Evidence Register

**Project:** Fostering Cellular Agriculture  
**Register status:** seeded public-evidence register, version 0.1  
**Evidence cut-off:** 27 August 2026

## Purpose

This register records what each source can establish, what it cannot establish,
and how it may be used in underwriting. It is deliberately not a bibliography
of favorable material. Adverse results, contradictory estimates, failed runs,
rejected applications, terminated contracts, and missing evidence belong here
as soon as they are identified.

The register is a control document, not proof by itself. The source file,
relevant extract, reviewer, access date, and any independent verification must
be retained in the controlled research archive.

## Evidence taxonomy

A single confidence grade would be misleading. Each item receives separate
tags for **provenance**, **transformation**, **verification**, and
**applicability**.

### Provenance tags

| Tag | Provenance | Permitted interpretation |
|---|---|---|
| `AUD` | Audited or independently assured primary records | Observed records within the exact scope, period, materiality, and exceptions of the signed report; “audited” is prohibited without such a report |
| `REG` | Law, regulator decision, inspection record, official guidance, or filing | Authoritative for the stated jurisdiction, product, date, and legal scope; not proof of commercial performance |
| `CTR` | Executed contract or direct counterparty confirmation | Evidence of the actual obligations and conditions in the reviewed document; enforceability and counterparty credit remain separate questions |
| `MGT` | Controlled internal primary record | Potentially direct operational or financial evidence, but unaudited and subject to control, completeness, and reconciliation testing |
| `PR-E` | Peer-reviewed empirical research | Evidence from the reported experiment or observations within its design and scale; peer review is not a plant audit |
| `PR-M` | Peer-reviewed model, techno-economic analysis, or review | A transparent analytical or synthesis source; outputs inherit assumptions and data limitations |
| `STD` | Standard-setter, supervisory, or intergovernmental framework | Authoritative framework or method within its stated remit; not project-specific validation |
| `EXT` | Transparent third-party engineering estimate or analysis | Usable as an external estimate if methods, boundaries, and inputs are inspectable; still not observed facility performance |
| `CLM` | Company, sponsor, investor, vendor, trade-group, or press claim | Attributable claim only until underlying records are obtained and verified |
| `HYP` | Project assumption or research hypothesis | Scenario input only; never described as evidence |

A document may require multiple rows. For example, an FDA page is `REG`, while
operating descriptions supplied by a sponsor within a consultation file may be
`CLM` or `MGT` even though the regulator publishes the file.

### Transformation tags

| Tag | Treatment |
|---|---|
| `OBS` | Directly observed or recorded for the stated unit and period |
| `DER` | Arithmetically derived from identified observations; formula and lineage required |
| `MOD` | Output of a model; model version, inputs, and sensitivity required |
| `EXTPL` | Extrapolated across scale, time, process, product, or geography; transfer basis required |
| `SYN` | Synthesis or review of other evidence; underlying sources control the strength of specific claims |

### Verification status

- `V3 — independently verified`: source and relevant assertion checked by a
  competent independent party under a documented scope.
- `V2 — primary source checked`: reviewer obtained the original source and
  checked the relevant passage or record.
- `V1 — secondary copy checked`: only an abstract, database record, extract, or
  reliable secondary copy has been checked.
- `V0 — not checked`: lead or assertion awaiting review; not permitted in a
  decision model.

### Applicability status

- `Exact`: same facility, process, product, jurisdiction, period, and decision.
- `Near`: material similarities are documented; differences are modeled.
- `Adjacent`: informs a mechanism or range but cannot be transferred directly.
- `Illustrative`: useful for questions or precedent only.

Peer review, regulatory authority, and audit answer different questions. A
peer-reviewed model may be methodologically valuable and still be `Adjacent`
to a proposed facility. An invoice tested within a documented assurance scope
may be `Exact` for historical cost and irrelevant to biological performance.

## Usage rules

1. Every material model input must cite one or more register IDs.
2. No `CLM` or `HYP` input enters a base underwriting case without an explicit
   model flag and conservative treatment.
3. Extrapolation is recorded at the input level, not hidden in narrative.
4. Conflicting sources remain side by side; the reviewer documents the
   reconciliation or uses a range.
5. Absence of public data is recorded as a gap, not converted into zero risk.
6. Approval or consultation for one product or process is not transferred to
   another.
7. Superseded legal and regulatory material remains archived with effective
   dates but is not treated as current.
8. Company-sponsored peer-reviewed work retains `PR-E` or `PR-M` provenance and
   also records sponsor, author affiliations, data access, and conflicts.
9. Every source receives an owner and next-review date. High-change regulatory,
   contract, price, and construction items require event-driven updates.

## Register fields for future entries

| Field | Required content |
|---|---|
| ID | Stable identifier; never reused |
| Assertion | One bounded proposition, not a general summary |
| Decision use | Underwriting decision or variable informed |
| Source | Title, issuer/authors, publication or effective date, direct link or archive path |
| Provenance / transformation | Tags defined above |
| Verification / applicability | Status defined above |
| Boundary | Facility, scale, process, product, geography, and period |
| Quantitative detail | Value, unit, denominator, uncertainty, sample size, and method where applicable |
| Limitations | Missing data, conflicts, sponsor interest, model dependence, and transfer limits |
| Model treatment | Base, downside, sensitivity only, question-forming only, or excluded |
| Owner / review | Named reviewer, review date, next-review date, and supersession status |

## Seeded public evidence

These entries establish the initial research perimeter. `V1` is used where the
initial review reached only an abstract, indexed record, or extract; `V2` is
used where the relevant official record was checked. Neither status means that
project-specific claims were independently assured.

**Control status:** all seeded entries remain provisional and are excluded from
decision-model calibration until a controlled archive record contains the
source file, retained extract, reviewer, access date, owner, next-review date,
and conflict check required by this register. The displayed `V1` or `V2` tag
describes the depth of the initial source check only; it does not waive missing
archive metadata.

### Financial mechanism and underwriting precedents

| ID | Assertion and decision use | Source | Classification | Boundary and limitations | Model treatment |
|---|---|---|---|---|---|
| FIN-001 | Portfolio structure, diversification, debt and equity tranching, and simulation can be used to investigate financing of high-risk biomedical research. This is a method precedent for risk decomposition. | Fernández, Stein & Lo, [“Commercializing biomedical research through securitization techniques”](https://www.nature.com/articles/nbt.2374), *Nature Biotechnology* 30 (2012) | `PR-M`, `MOD`; `V1`; `Illustrative` | A published Perspective containing an oncology-development simulation, not an implemented transaction; not a cellular-agriculture dataset and not evidence that facility risks diversify | Question-forming and model-method precedent only |
| FIN-002 | An advance market commitment can combine eligibility criteria, manufacturer agreements, supply procurement, capped support, and independent governance to share demand risk. | World Bank, [2009 launch announcement](https://documents1.worldbank.org/curated/en/816001634659826556/pdf/Announcement-of-Funding-New-Vaccines-to-Save-Millions-of-Lives-on-April-3-2009.pdf); Gavi, [manufacturer process and legal agreements](https://www.gavi.org/investing-gavi/innovative-financing/pneumococcal-advance-market-commitment-amc/manufacturers) and [final outcomes evaluation](https://www.gavi.org/our-impact/evaluation-studies/pcv-amc-pilot-2nd-outcomes-impact) | `STD`, `OBS`/`SYN`; `V1`; `Illustrative` | Health-policy mechanism with donor backing, WHO prequalification, UNICEF procurement, and an independent committee; the final evaluation found no evidence that the pilot accelerated new-product R&D, while finding evidence consistent with accelerated uptake. Different mandate, product, beneficiaries, and market structure | Governance and contract-research precedent only; no copied calibration or unqualified additionality claim |
| FIN-003 | Prudent loan origination and monitoring require robust creditworthiness assessment, lifecycle monitoring, and adverse sensitivity analysis. | European Banking Authority, [Guidelines on loan origination and monitoring](https://www.eba.europa.eu/activities/single-rulebook/regulatory-activities/credit-risk/guidelines-loan-origination-and-monitoring) (in force) | `STD`; `V2`; `Adjacent` | EU supervisory framework; applicability depends on lender and transaction | Underwriting-method requirement where applicable |
| FIN-004 | Fair value is an exit-price measurement based on market-participant assumptions; significant unobservable inputs require appropriate treatment and disclosure. | IFRS Foundation, [IFRS 13 Fair Value Measurement](https://www.ifrs.org/issued-standards/list-of-standards/ifrs-13-fair-value-measurement/) | `STD`; `V2`; `Adjacent` | Applies when another IFRS Standard requires or permits fair value; it does not decide legal classification or mandate fair value for every candidate instrument | Valuation and disclosure reference where applicable |
| FIN-005 | A narrow contingent contract can be proposed to transfer a defined regulatory event risk from biomedical R&D investors to broader counterparties. | Jørring, Lo, Philipson, Singh, Thakor & Ellul, [“Sharing R&D Risk in Healthcare via FDA Hedges”](https://www.nber.org/papers/w23344), NBER Working Paper 23344 (2017), published in *Review of Corporate Finance Studies* 11 (2022) | `PR-M`, `MOD`; `V1`; `Illustrative` | Proposed FDA-approval-risk instrument using historical drug data. The reported low cross-class and market correlation is specific to that dataset and must not be transferred to first-of-a-kind facilities with common technology, supplier, buyer, or policy shocks | Direct event-risk design precedent; settlement, basis, counterparty, and market-structure questions only |
| FIN-006 | A rare-disease company can combine equity, senior debt, royalty-linked funding, licensing, upfront payments, and milestone cash rather than relying on one securitized portfolio structure. | BridgeBio Pharma, [2024 Form 10-K](https://www.sec.gov/Archives/edgar/data/1743881/000095017025024119/bbio-20241231.htm) | `REG`, `OBS`; `V2`; `Illustrative` | Company-specific public filing; instruments fund a biopharmaceutical enterprise and carry product, royalty, covenant, licensing, and corporate-credit terms unlike a cellular-agriculture project. It is not proof that an RBO securitization was executed or that the capital stack was optimal | Broad-financing implementation precedent; use filed terms, not promotional summaries |
| FIN-007 | Model risk arises from assumptions, input and data limitations, aggregate dependencies, and use outside an intended purpose; validation includes alternative assumptions, outcomes analysis, performance ranges, and recalibration. | Federal Reserve, [Supervisory Guidance on Model Risk Management](https://www.federalreserve.gov/frrs/guidance/supervisory-guidance-on-model-risk-management.htm) and [SR 26-2](https://www.federalreserve.gov/supervisionreg/srletters/SR2602.htm) (2026) | `STD`; `V2`; `Adjacent` | US banking supervisory guidance; it is not automatically applicable law for this research project or every future issuer or investor | Model-design and validation discipline; no regulatory-status claim |
| FIN-008 | Uncertainty in estimated default probability and asset correlation can materially increase measured portfolio tail risk, and parameter uncertainty is not removed merely by adding exposures. | Tarashev, [“Measuring portfolio credit risk correctly: why parameter uncertainty matters”](https://www.bis.org/publ/work280.htm), BIS Working Paper 280 (2009) | `PR-M`, `MOD`; `V1`; `Adjacent` | Credit-portfolio model study, not a cellular-agriculture dataset; its numerical results and model form are not transferred | Requires visible probability/dependence uncertainty and robust tail sensitivity |
| FIN-009 | PD and LGD estimation standards seek to reduce unjustified model variability while retaining risk sensitivity and clear parameter definitions. | European Banking Authority, [Guidelines on PD estimation, LGD estimation and treatment of defaulted exposures](https://eba.europa.eu/activities/single-rulebook/regulatory-activities/model-validation/guidelines-pd-estimation-lgd?version=2018) | `STD`; `V2`; `Adjacent` | EU prudential credit standard; a participation claim is not presumed to be an IRB credit exposure | Parameter-definition and estimation reference only where economically analogous |
| FIN-010 | Low-default portfolio validation cannot rely on sparse internal defaults alone; relevant external and pooled data, justified aggregation, conservative bounds, benchmarking, and independent validation may be needed. | Basel Committee on Banking Supervision, [CRI30 — IRB: supervisory assessment of banks' risk parameter estimations](https://www.bis.org/committees/bcbs/basel-consolidated-guidelines/module/cri/30) | `STD`; `V2`; `Adjacent` | Bank-capital standard; does not assign this project a risk weight or approve cross-industry pooling | Sparse-data calibration questions and controls; no copied PD or LGD |
| FIN-011 | Strictly proper probability scores reward an honest predictive distribution; discrimination alone does not establish calibration. | Gneiting & Raftery, [“Strictly Proper Scoring Rules, Prediction, and Estimation”](https://sites.stat.washington.edu/people/raftery/Research/PDF/Gneiting2007jasa.pdf), *Journal of the American Statistical Association* 102 (2007) | `PR-M`; `V1`; `Adjacent` | General forecast-evaluation method; requires frozen predictions and matured outcomes and does not solve target-population transfer | Calibration-curve and out-of-sample scoring requirement |
| FIN-012 | Technical-success dependence can materially change tail loss and investor returns in a pooled high-risk research structure; an assumed correlation remains model uncertainty rather than evidence. | Lo & Siah, [“Financing Correlated Drug Development Projects”](https://qlsadvisors.com/wp-content/uploads/2021_Megafund_JSF.pdf), *Journal of Structured Finance* (2021) | `PR-M`, `MOD`; `V1`; `Illustrative` | Drug-development one-factor model; the study states that its technical correlation was assumed in the absence of a historical estimate. Its marginal probabilities, copula, correlation, and results do not transfer to cellular agriculture | Dependence sensitivity and challenger-model precedent only |
| FIN-013 | US federal loan-guarantee subsidy cost is the discounted expected value of government cash outflows less inflows, adjusted for defaults, prepayments, and other departures from contract; administrative expenses are separate and estimates are revised over time. | US OMB, [Circular A-11, section 185 (2025)](https://www.whitehouse.gov/wp-content/uploads/2025/08/s185.pdf) | `STD`; `V2`; `Adjacent` | US federal budget method, not a market price, accounting ECL, or automatically applicable rule for another provider | Separates expected-claim PV, administration, discounting, and re-estimation; no copied parameter |
| FIN-014 | EU budgetary-guarantee provisioning covers net expected loss plus an adequate safety buffer, and common provisioning considers risk correlation. | European Union, [Regulation (EU, Euratom) 2024/2509](https://eur-lex.europa.eu/eli/reg/2024/2509/oj/eng), Articles 214 and 216 | `REG`, `STD`; `V2`; `Adjacent` | EU budget framework; legal applicability and provisioning method depend on the actual programme | Requires provider provisioning and correlation analysis to remain separate from mean claims |
| FIN-015 | A market-compatible guarantee-scheme premium addresses normal risk, administration, and remuneration of adequate capital; premium shortfall may constitute aid. | European Commission, [Guarantee Notice 2008/C 155/02](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX%3A52008XC0620%2802%29) | `REG`, `STD`; `V2`; `Adjacent` | Historical State-aid guidance with legal conditions and examples; not a current market quote or cellular-agriculture calibration | Premium-component and explicit-aid boundary only |
| FIN-016 | Under the Basel IRB framework, expected loss, eligible provisions, and resulting regulatory-capital treatment are distinct quantities. | Basel Committee on Banking Supervision, [CRE35](https://www.bis.org/basel_framework/chapter/CRE/35.htm?inforce=20230101&published=20200327&tldate=20241130) | `STD`; `V2`; `Adjacent` | Bank prudential standard; no claim that a proposed instrument is an IRB exposure or that the rule applies to every investor or provider | Prevents relabelling a claim estimate as accounting provision or regulatory capital |
| FIN-017 | A historical EIF programme used 50% principal-and-interest loss sharing, pro-rata recoveries, and 0.50%/0.80% annual guarantee fees. | European Investment Fund, [InnovFin SME Guarantee brochure (2014)](https://www.eif.org/news_centre/publications/eif_flyer_innovfin_sme_guarantee_en.pdf) | `OBS`, `STD`; `V1`; `Illustrative` | Historic programme architecture; not current availability, a market quote, or evidence for target PD, LGD, dependence, or price | Loss-share, recovery-sharing, and fee-structure precedent only |

### Technical and operating evidence

| ID | Assertion and decision use | Source | Classification | Boundary and limitations | Model treatment |
|---|---|---|---|---|---|
| TEC-001 | Animal-cell culture scale-up faces engineering and economic constraints including growth rate, metabolic efficiency, inhibition, shear, practical reactor scale, and attainable density. | Humbird, [“Scale-up economics for cultured meat”](https://pubmed.ncbi.nlm.nih.gov/34101164/), *Biotechnology and Bioengineering* 118 (2021), DOI 10.1002/bit.27848 | `PR-M`, `MOD`/`EXTPL`; `V1`; `Adjacent` | Techno-economic analysis, not observed commercial facility history; results depend on process and cost assumptions | Range construction, stress variables, and research questions; not a bank case |
| TEC-002 | Published cultivated-meat techno-economic analyses use differing assumptions and substantial transfer from adjacent bioprocessing fields, leaving material scale-up evidence gaps. | Goodwin, Aimutis & Shirwaiker, [“A scoping review of cultivated meat techno-economic analyses…”](https://www.nature.com/articles/s43016-024-01061-3), *Nature Food* 5 (2024), DOI 10.1038/s43016-024-01061-3 | `PR-M`, `SYN`; `V1`; `Adjacent` | Review quality depends on included studies; identifies gaps but supplies no commercial operating history. The work reports support from the Bezos Earth Fund and no author competing interests | Supports model-risk controls and evidence-gap register |
| TEC-003 | A serum-free formulation supported bovine satellite-cell differentiation under the reported experimental conditions. | Messmer et al., [“A serum-free media formulation for cultured meat production…”](https://www.nature.com/articles/s43016-021-00419-1), *Nature Food* 3 (2022) | `PR-E`, `OBS`; `V1`; `Adjacent` | Laboratory experimental boundary; does not establish commercial-scale media cost, supply, sterility, yield, uptime, or qualified output. The paper states that the study was funded by Mosa Meat; seven authors were employees, one was a co-founder and stakeholder, and related patents were pending | Scientific feasibility input and diligence questions only; funding and conflicts remain visible |
| TEC-004 | Selected spontaneously immortalized chicken fibroblast lines achieved the paper's reported high-density serum-free production results under its stated system. | Pasitka et al., [“Spontaneous immortalization of chicken fibroblasts…”](https://www.nature.com/articles/s43016-022-00658-w), *Nature Food* (2023) | `PR-E`, `OBS`/`DER`; `V1`; `Near` or `Adjacent` depending on reference process | Cell-line- and process-specific experiment. The paper reports funding from Believer Meats and the Sam and Rina Frankel Foundation; its corresponding author was a Believer officer/director/shareholder, multiple authors were employees, and one was a consultant. Sample definition, duration, downstream qualification, and facility transfer still require review | Candidate technical range only after full-paper and data review; not commercial performance proof |
| TEC-005 | Public peer-reviewed evidence found in the initial search does not provide the complete, independently assured run-level, cost, downtime, and qualified-output history required by this project standard. | Initial register review of TEC-001 through TEC-004 | `HYP`, `SYN`; `V1`; `Illustrative` | A provisional synthesis limited to the public materials admitted to this register, most checked only through abstracts or extracts; not a claim that no complete private, omitted, or future dataset exists | Critical evidence gap; triggers a broader controlled search and confidential data request |

### Food-safety and regulatory evidence

| ID | Assertion and decision use | Source | Classification | Boundary and limitations | Model treatment |
|---|---|---|---|---|---|
| REG-001 | FDA publishes completed pre-market consultation files, including sponsor submissions, FDA response letters, and scientific memoranda for specific cultured-animal-cell foods. | US FDA, [Inventory of Completed Pre-market Consultations](https://www.fda.gov/food/human-food-made-cultured-animal-cells/inventory-completed-pre-market-consultations-human-food-made-cultured-animal-cells) | FDA documents: `REG`; sponsor assertions: `CLM` or `MGT`; `OBS`; `V2`; scope is product-specific | Consultation completion is not an audit of commercial economics and does not transfer to a different cell line, process, product, or facility | Regulatory-path evidence; sponsor performance data require separate validation |
| REG-002 | For covered US livestock and poultry products, FDA oversees cell collection, growth, and differentiation, and jurisdiction transitions to FSIS at harvest; FSIS oversees further processing, labeling, and packaging. Establishments require applicable inspection and controls. | USDA FSIS, [Human Food Made with Cultured Animal Cells](https://www.fsis.usda.gov/inspection/compliance-guidance/labeling/labeling-policies/human-food-made-cultured-animal-cells) | `REG`, `SYN`; `V2`; jurisdiction/product-specific | General US framework; species and product determine jurisdiction; current counsel must test applicability | Regulatory schedule, conditions precedent, and operating-control requirements |
| REG-003 | FSIS Directive 7800.1 instructs inspection personnel on establishments harvesting or processing covered cell-cultured meat or poultry and notes that relevant labels require FSIS review rather than generic approval. | USDA FSIS, [Directive 7800.1](https://www.fsis.usda.gov/policy/fsis-directives/7800.1) (21 June 2023) | `REG`, `OBS`; `V2`; jurisdiction/product-specific | Directive may be revised or superseded; exact facility and label remain subject to agency process | Regulatory milestone and monitoring input; verify current status before use |
| REG-004 | EU novel-food law expressly includes food produced from animal cell or tissue culture in the novel-food definition. | European Union, [Regulation (EU) 2015/2283](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX%3A32015R2283), Article 3 | `REG`, `OBS`; `V2`; EU-specific | Legal classification may interact with other EU law and specific cell treatment; authorization remains product-specific | Regulatory-path anchor; legal opinion required for exact product |
| REG-005 | EFSA's current novel-food framework and guidance cover cell-culture-derived food applications; EFSA performs scientific risk assessment while authorization decisions belong to the European Commission and Member States. | EFSA, [Novel food topic and guidance page](https://www.efsa.europa.eu/en/topics/topic/novel-food) | `REG`, `SYN`; `V2`; EU-specific | Web content and application status can change; risk assessment is not itself market authorization | Process map and evidence request; refresh at each decision date |
| REG-006 | Singapore requires pre-market evidence addressing the safety of the novel food and production process, including cultivated-meat/seafood-specific submission materials. | Singapore Food Agency, [Guidelines on Applying for Pre-Market Approval for a Novel Food](https://www.sfa.gov.sg/regulatory-standards-frameworks-guidelines/novel-food-framework/guidelines-on-applying-for-pre-market-approval-for-a-novel-food) | `REG`, `OBS`; `V2`; Singapore-specific | Requirements and schedules may change; approval remains product- and process-specific | Regulatory-path and data-room requirement |
| REG-007 | FAO and WHO identify food-safety hazards and survey regulatory approaches for cell-based food. | FAO & WHO, [*Food safety aspects of cell-based food*](https://www.who.int/publications/i/item/9789240070943) (2023) | `STD`, `SYN`; `V2`; `Adjacent` | Hazard-identification and overview document; not a facility approval, safety assurance, or commercial-performance record | Hazard checklist and regulator-interview foundation |

### Demand, construction, and financeability evidence presently missing

The entries below are not assertions that evidence does not exist. They record
what has not yet been admitted to this register and therefore may not be assumed
in the underwriting base case.

| ID | Required evidence | Current classification | Why it matters | Acquisition and acceptance test |
|---|---|---|---|---|
| GAP-OPS-001 | Complete run-level history at the chosen reference scale, including aborted, contaminated, off-specification, and maintenance-affected runs | Missing | Calibrates yield, uptime, loss severity, correlation, and ramp risk | Obtain `MGT` source records; reconcile to quality, inventory, and ledger records; independent technical procedures for `AUD` status |
| GAP-OPS-002 | Reconciled mass, energy, water, labour, media, consumables, waste, and downstream-loss records | Missing | Determines variable cost and physical credibility | Meter/invoice/ledger bridge with defined boundaries and no unexplained residual above agreed tolerance |
| GAP-CAP-001 | Reference facility engineering package, vendor bids, schedule, contingency basis, interfaces, guarantees, and owner costs | Missing | Determines completion funding and delay exposure | Dated source documents, scope normalization, independent engineering review, and explicit exclusions |
| GAP-DEM-001 | Executed, binding buyer agreements for the reference product and independent confirmation of enforceable volume, price, conditions, and credit support | Missing | Separates forecast demand from contracted cash flow | Review full agreements and amendments; buyer confirmation; legal enforceability and counterparty credit review |
| GAP-DEM-002 | Paid repeat orders and product acceptance/rejection history | Missing | Tests specification fit, realized price, retention, and claims rates | Invoice-to-cash reconciliation, return/credit notes, and cohort history |
| GAP-REG-001 | Product- and facility-specific regulatory pathway, submissions, authority correspondence, approvals, inspections, and labels for the reference case | Missing until reference case is selected | Determines legal ability and timing to operate and sell | Primary records plus current local regulatory/legal opinion; record all open conditions |
| GAP-FIN-001 | Historical audited financial statements or agreed-upon procedures covering relevant facility costs and revenues | Missing | Reconciles technical model to actual cash economics | Signed practitioner report, scope, materiality, exceptions, and source-ledger bridge |
| GAP-FIN-002 | Proposed capital stack, term sheets, security package, waterfall, reserves, covenants, tax, accounting, and recovery analysis | Missing | Required for debt capacity, instrument interaction, and loss allocation | Dated executable or indicative terms clearly distinguished; counsel and accounting review |
| GAP-CP-001 | Credit evidence for offtakers, sponsors, vendors, guarantors, insurers, and hedge counterparties | Missing | Contract value depends on performance by counterparties | Audited accounts/ratings where available, exposure limits, guarantees, collateral, wrong-way risk, and default scenarios |
| GAP-IMP-001 | Facility-specific, independently reviewable animal-displacement and animal-derived-input method | Missing | Prevents mission claims from outrunning evidence | Define counterfactual, product cultivated share, channel substitution, attribution, leakage, time period, and assurance scope |
| GAP-ENV-001 | Facility-specific environmental inventory using measured inputs and a declared boundary | Missing | Avoids relying on scenario life-cycle claims as operating fact | Metered inventory, grid and supplier factors, allocation choices, uncertainty, comparative functional unit, and independent review |

## Evidence acquisition queue

### Priority 1 — required before a reference facility forecast

1. Select the exact reference product, process, scale, jurisdiction, operator,
   facility boundary, and buyer segment.
2. Issue the complete run-history and cost-ledger data request in the
   [underwriting data standard](UNDERWRITING_DATA_STANDARD.md).
3. Obtain current regulator correspondence and a product-specific pathway
   memorandum.
4. Obtain executed commercial agreements and buyer confirmations.
5. Obtain the engineering basis, vendor quotations, and schedule.

### Priority 2 — required before instrument term sheets

1. Complete independent technical and financial verification procedures.
2. Reconcile operating data to source records and the general ledger.
3. Establish counterparty credit and recovery assumptions.
4. Freeze a versioned model-input dataset and exceptions register.
5. Document whether common technology, supplier, regulator, and buyer exposures
   defeat presumed diversification.

### Priority 3 — required before a live pilot

1. Obtain legal classification and enforceability opinions.
2. Confirm accounting, tax, prudential, collateral, and reporting treatment.
3. Test trigger calculation, data outage, correction, dispute, and settlement
   procedures with historical or synthetic records.
4. Agree ongoing reporting, audit rights, incident notification, and data escrow.
5. Establish counterfactual financing and mission-outcome measurement.

## Review log template

| Review date | Register ID | Reviewer | Change | Reason | Supersedes | Approval |
|---|---|---|---|---|---|---|
| _YYYY-MM-DD_ | _ID_ | _Name/role_ | _Added, amended, challenged, retired_ | _New record or finding_ | _Prior ID/version_ | _Name/role_ |

The register must be reviewed before every model release, investment or credit
committee submission, term-sheet revision, and external claim using its data.
