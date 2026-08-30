# Candidate Structures

## Status, purpose, and boundary

This document contains four **provisional research term-sheet outlines** for a
defined cellular-agriculture facility. They are synthetic design templates, not
transaction terms, valuations, recommendations, offers, solicitations, or
claims that an instrument is lawful, financeable, or suitable in any
jurisdiction. No amount, price, probability, rating, impact, or counterparty is
represented as observed or approved.

There is no preferred instrument. Each candidate addresses a different
financing failure and should be considered only after the failure has been
evidenced for a named reference project. The candidates are not presumed to be
combined. Complexity, fees, collateral, concessionary value, and failure modes
must be assessed before a structure can be compared with ordinary equity,
debt, procurement, grants, or operating improvements.

Any payment made by a buyer, guarantor, public body, philanthropic funder, or
other provider is a transfer from that provider to the project or its capital
providers. Where the provider does not receive market-equivalent value, the
concessionary component is a subsidy and must be identified, valued, capped,
authorized, and reported as such. Financial engineering does not make the
economic cost disappear.

The project's purpose includes enabling a durable transition away from animal
confinement, harm, and slaughter. These outlines make **no animal-welfare impact
claim**. Financing, installed capacity, reactor volume, gross biomass, and even
qualified output are not evidence of animal-product displacement. Any later
impact claim requires a separately verified counterfactual, additionality
analysis, and displacement method.

Read these outlines with the [Financing-Failure Map](./FINANCING_FAILURE_MAP.md),
[Instrument Taxonomy](./INSTRUMENT_TAXONOMY.md),
[Responsible Finance Charter](./RESPONSIBLE_FINANCE_CHARTER.md),
[Underwriting Data Standard](./UNDERWRITING_DATA_STANDARD.md), and
[Evidence Register](./EVIDENCE_REGISTER.md). Model terms should be reconciled to
the [First Facility Risk Model](./MODEL_SPECIFICATION.md) and, where applicable,
the implemented [Annual Reference Engine v0.1](./ANNUAL_ENGINE_V0_1.md).

### Implementation-status matrix

The outlines are broader than the software. A term appearing in this document
does not mean that its cash flow, legal form, waterfall, or risk has been
implemented.

| Candidate | Implemented annual engine v0.1 | Separate module / monthly-model status |
|---|---|---|
| Milestone-gated development and construction capital | Not modeled in the annual engine. Its completion-delay payout remains a separate simplified support leg. | **Separate deterministic module v0.1 implemented.** It accepts explicit weighted synthetic cases; posts sponsor, ProjectCo, provider, protected-reserve, and external cash entries; reconciles commitment and funded-claim memo accounts; applies phase and aggregate-source tests, provider-performance stress, monthly commitment fee, capped PIK, draw stop, terminal waterfall, and physical-P zero-NPV upfront-fee sensitivity. It does not validate certificates, calibrate probabilities, model operations, or establish legal enforceability or financeability. See [Milestone-Gated Committed Capital Module v0.1](./MILESTONE_GATED_CAPITAL_V0_1.md). The broader integrated monthly project model remains a target. |
| Qualified-output offtake or capacity reservation | Only fixed-price repricing of a configured share of modeled qualified output relative to modeled spot sales. “Qualified” is an input label: v0.1 has no food-safety, release-testing, acceptance, or quality-system model and does not establish safe release. There is no take-or-pay minimum, capacity reservation, acceptance failure, buyer default, or physical-delivery operation. | Take-or-pay cash flows and buyer default are specified as future targets; no implementation or validation is claimed. |
| Capped one-way floor or two-way contract for difference | A simplified eligible-output share, strike, annual cap, and lifetime absolute-settlement cap. Both payment directions consume the same lifetime capacity. There is no observed benchmark, basis model, collateral, margining, counterparty credit, hedge accounting, or legal characterization. | More detailed monthly formulas are specified as a future target; no implementation or validation is claimed. |
| Capped completion/overrun or partial credit support | Only a capped parametric completion-delay payout treated as unrestricted project cash. There is no cost-overrun reimbursement, guarantee claim, lender payment, provider default, recovery waterfall, or double-recovery control. A separate constant debt-recovery fraction is not a guarantee. | **A narrower pooled principal-loss overlay is implemented.** It cash-settles an exact share of terminal resolved untranched pool loss, preserves gross loss, limits percentage by cap over aggregate commitment, reports provider payout tails, and tests investor premium capacity against a claim-only provider floor. It is not completion/overrun support, a debt-service guarantee, fair value, or proof of provider capacity; provider performance, enforceability, collateral, funding cost, capital, expenses, exclusions, subrogation and payment delay remain unmodeled. See [Pooled Principal-Loss Protection Term v0.1](./POOLED_LOSS_PROTECTION_TERM_V0_1.md). |

This matrix must be updated whenever code or the target specification changes.
Only tested implemented behavior may be presented as a model result.

## Common drafting conventions

Before any outline is populated, the transaction record should identify:

- the legal project company, sponsor, technology provider, site, jurisdiction,
  product, intended use, and regulatory pathway;
- the facility boundary, design maturity, construction and commissioning plan,
  sources and uses, working-capital need, and decommissioning obligation;
- the definition of **qualified output**, including product grade, release
  specification, test method, measurement point, unit, time interval, title,
  traceability, and treatment of rejected or recalled product;
- the unsupported base case and deterministic downside cases, before any
  support payment or probabilistic simulation;
- the diagnosed financing failure, rejected or unavailable conventional terms,
  and the causal reason the candidate could change financing availability,
  tenor, cost, or resilience;
- each party controlling, observing, bearing, and verifying each material risk;
- the expected value, maximum exposure, liquidity need, collateral need, and
  accounting treatment of every payment leg; and
- the governing-law, regulatory, tax, accounting, prudential-capital,
  insolvency, procurement, state-aid or subsidy-control, and enforceability
  reviews still required.

All references to a benchmark, price, cost, date, test, or certificate require
a source hierarchy, correction policy, publication calendar, fallback method,
calculation agent, dispute process, and record-retention period. A beneficiary
must not have unilateral control over a trigger on which its payment depends.
Food safety, truthful reporting, worker protection, animal-care obligations,
environmental compliance, and lawful regulatory action always take precedence
over production, payment, or covenant targets.

## Candidate-to-failure map

| Candidate | Financing failure principally tested | Economic function | Principal caution |
| --- | --- | --- | --- |
| Milestone-gated development and construction capital | Technical information asymmetry; design and completion uncertainty; inability to commit full capital before evidence exists | Staged capital formation with conditional draw availability | A gate cannot validate what it does not independently measure; late-stage cancellation can itself strand the asset |
| Qualified-output offtake or capacity reservation | Buyer-producer coordination failure; insufficient contracted revenue for underwriting | Physical demand support or availability-style payment | A buyer obligation is valuable only if its qualification terms and credit remain effective when the project is stressed |
| Capped one-way floor or two-way contract for difference | Defined output-price exposure that prevents otherwise viable cash flows from supporting capital | Contingent price transfer; potentially an explicit subsidy | No robust cultivated-product benchmark may exist; basis, manipulation, and collateral risk can exceed apparent hedge value |
| Capped completion/overrun or partial credit support | Concentrated construction or debt loss that an otherwise willing capital provider cannot absorb | Guarantee, contingent capital, or credit enhancement | The provider can inherit correlated, difficult-to-control losses; support can weaken sponsor and contractor discipline |

The mapping is diagnostic, not exclusive. If the unsupported project remains
uneconomic after conservative technical and commercial assumptions, the answer
is redesign, research finance, or a transparent policy subsidy—not a more
complicated label.

## 1. Milestone-gated development and construction capital

### Provisional term-sheet outline

| Field | Research outline |
| --- | --- |
| **Diagnosed failure and purpose** | Test whether capital is unavailable because technical, design, permitting, or cost-to-complete evidence arrives sequentially and providers will not make an irrevocable full commitment at financial close. The structure would make later draws conditional on independently verified progress while preserving a credible path to full funding. It is not a device for financing an undefined process or shifting normal sponsor development risk. |
| **Possible legal/economic form** | A delayed-draw senior or subordinated facility, tranched preferred equity commitment, reimbursable development facility, or subscription agreement with conditions precedent. The form is unresolved and must follow economic substance, investor mandate, insolvency treatment, and applicable law. |
| **Parties** | Project company or development company as borrower/issuer; sponsor and technology owner; one or more capital providers; account bank and security trustee where applicable; independent engineer; independent technical, quality, regulatory, and cost reviewers; major engineering, procurement, construction, equipment, and utility counterparties. A public or philanthropic provider, if any, must be identified separately from market-rate capital. |
| **Eligibility** | A defined reference project; documented use of proceeds; sponsor equity committed; site and material intellectual-property rights; declared regulatory pathway; design maturity appropriate to the requested phase; baseline schedule and cost estimate; complete pilot or demonstration evidence including failed runs; approved quality and commissioning plans; no unresolved material integrity, safety, legal, or sanctions issue. Eligibility is phase-specific and does not imply later-stage eligibility. |
| **Commitment and cash flows** | Capital provider commits up to a fixed maximum for stated eligible costs. Project company draws only after satisfying initial and milestone conditions. Draws may fund verified invoices, defined development work, contingency, interest during construction, or required reserves. Commitment fee applies only to legally committed undrawn capital; interest or preferred return applies only to funded amounts unless lawfully and transparently agreed otherwise. Repayment or redemption begins only under a stated schedule and cash waterfall. Sponsor funds the agreed first-loss equity and cost share before or alongside each draw. |
| **Milestones and settlement** | Illustrative gates may include process-design freeze, site control, permit submission or receipt, completion of specified engineering packages, executable major contracts, delivery and factory-acceptance testing of critical equipment, mechanical completion, utilities-ready status, commissioning protocol completion, repeated qualification runs, and a defined operating acceptance test. Each gate must state objective evidence, tolerance, responsible verifier, cure rights, and whether failure suspends, reduces, reprices, or terminates only future availability. A certificate releases capital; it must not deem technical success beyond the evidence reviewed. |
| **Cap, deductible, and retained risk** | Aggregate commitment is capped. Eligible cost categories and change-order allowances are capped. Sponsor contributes a stated minimum amount and retains overruns, delays, and losses outside agreed eligibility unless another explicitly priced facility applies. A funded contingency may be drawn only after a defined sponsor deductible or co-funding ratio. No milestone should eliminate sponsor or contractor responsibility for misrepresentation, negligence, avoidable delay, or defective work. |
| **Fee and pricing unknowns** | Base rate or return, credit and technology spread, commitment fee, original-issue discount, exit fee, equity participation, amendment fee, independent-review costs, hedging costs, reserve carry, and prepayment terms remain unknown. Research must compare the all-in cost with ordinary staged equity, vendor finance, leasing, construction debt, and a smaller demonstration. Pricing cannot be inferred from the synthetic risk model alone. |
| **Collateral and credit support** | Potential security includes project assets, accounts, material contracts, insurance proceeds, receivables, and shares in the project company, subject to law and intercreditor terms. Sponsor completion equity, a funded reserve, contractor bonds, vendor warranties, or limited guarantees may supplement—but not duplicate—the same exposure. Intellectual-property security must not impair safe continuity, lawful access, or an orderly technology license on enforcement. Each provider's ability to fund every committed draw under stress must be assessed. |
| **Covenants and controls** | Sources-and-uses balance; minimum sponsor contribution; permitted-cost schedule; budget and schedule variance thresholds; change-control procedure; no distributions before agreed acceptance and reserve tests; reporting of all failed and interrupted runs; access to records and site; related-party controls; insurance; key-contract maintenance; liquidity and cost-to-complete tests; no additional debt or liens without consent; preservation of safety, quality, worker, animal-care, environmental, and decommissioning obligations. Financial tests must not compel unsafe acceleration or concealment. |
| **Exclusions** | Research without reproducible evidence; undefined product or facility scope; routine operating losses after the agreed ramp period; ineligible related-party charges; costs arising from fraud, willful misconduct, undisclosed design changes, unlawful conduct, sanctions, or unremedied material safety breaches; output or work that does not meet defined acceptance standards. Exclusions must be narrow enough that the committed capital remains credible, not illusory. |
| **Verification** | Independent engineer validates design maturity, progress, schedule, incurred cost, remaining cost, contingency, and milestone completion. Independent technical and quality reviewers validate evidence within their competence. Account bank reconciles invoices and draw use. The verifier owes a defined duty, discloses conflicts, has access to negative evidence, and cannot be replaced solely to obtain a favorable certificate. Review is documented; it is not a warranty of success. |
| **Suspension, termination, and workout** | Define draw-stop events, information failures, milestone long-stop dates, cure periods, re-baselining rules, provider vote thresholds, and consequences of a permanent stop. Workout should prioritize safe preservation, payroll and legally required care, security and environmental controls, records, essential maintenance, and orderly disposition or decommissioning. State whether unused commitment terminates, whether funded claims accelerate, how substitution of sponsor or contractor works, and how intellectual property remains available to a competent substitute operator. |
| **Legal-characterization questions** | Loan or security; equity or collective investment; securities-offering requirements; enforceability of future funding commitments; licensing and banking rules; financial-assistance and capital-maintenance limits; security perfection; insolvency and executory-contract treatment; intercreditor priority; public-procurement and subsidy-control rules; accounting consolidation; tax treatment of fees and contingent returns. Specialist opinions are prerequisites to execution, not outputs of this outline. |
| **Residual risks** | Milestone evidence may not predict commercial operation; design changes and inflation may emerge after a gate; regulatory action, customer qualification, supply constraints, contamination, low yield, and weak prices remain; the capital provider may fail to fund; a draw stop may strand a partially completed facility; security recovery may be low for specialized assets; multiple reviewers may create delay without reducing common-mode risk. |
| **Incentive and animal-welfare safeguards** | Gates use qualified, safely released output and complete failure reporting—not gross biomass or schedule alone. No gate can waive lawful testing, maintenance, cleaning, worker protection, biosafety, animal-care, or environmental duties. Sponsor and technology owner retain meaningful risk. Any animal-derived inputs, testing, or care obligations are disclosed. Financing additionality and animal-welfare impact are measured separately; closing or construction progress is not animal impact. |
| **Evidence gate before modeling or negotiation** | Complete sources and uses; independent design, schedule, and cost review; cost-class and contingency rationale; executed or substantially agreed major-contract terms; full run history and scale-transfer plan; site, utility, intellectual-property, permitting, and regulatory evidence; sponsor financial capacity; proposed milestone measurement protocol; written evidence that unconditional capital was unavailable or materially more costly for the diagnosed reason. |
| **Go / no-go criteria** | **Go to limited diligence** only if the full funding plan remains credible at each gate, milestones are objectively verifiable, the provider can fund, the sponsor retains controllable risks, and downside abandonment is safer and less costly than an unconditional commitment. **No-go** if the structure merely postpones an acknowledged funding gap, gates depend on beneficiary discretion or selected runs, the project is uneconomic after support is removed, later funding is not actually committed, or a draw stop would predictably leave unfunded safety, care, remediation, or decommissioning obligations. |

### Questions to test

1. Does staged information reduce required return enough to offset commitment
   fees, verification cost, execution delay, and stranded-asset risk?
2. Which milestone is both causally relevant to loss and observable before the
   next irreversible capital decision?
3. How much sponsor equity and contingency must remain at risk after each gate?
4. Would a smaller demonstration, equipment lease, vendor warranty, or ordinary
   staged equity solve the failure at lower total cost?

## 2. Qualified-output offtake or capacity reservation

### Provisional term-sheet outline

| Field | Research outline |
| --- | --- |
| **Diagnosed failure and purpose** | Test whether a credible buyer wants future qualified product or capacity but cannot obtain it without a facility, while the facility cannot secure capital without binding demand. The structure would convert a limited portion of future demand into underwritable contracted cash flow. It must not force a buyer to accept unsafe, unlawful, unqualified, or commercially unusable product. |
| **Possible legal/economic form** | A physical fixed- or formula-price offtake; take-or-pay contract with carefully defined delivery and buyer-relief conditions; tolling or processing agreement; or capacity reservation/availability agreement that pays for verified readiness to supply. Advance purchase or prepayment is a separate variant with distinct credit and insolvency risk. No form is presumed superior. |
| **Parties** | Project company or seller; one or more creditworthy buyers with a genuine procurement need; buyer parent guarantor or letter-of-credit bank if required; logistics, warehousing, and testing parties where material; independent quality and quantity verifier; account bank and lenders with assignment or direct-agreement rights. A public procurer must document authority, procurement process, budget, and policy purpose. |
| **Eligibility** | Defined product and intended use; lawful regulatory status for delivery; buyer-approved specification and qualification protocol; traceable qualified output; credible capacity, ramp, logistics, and input-supply plan; buyer demand and substitution analysis; agreed delivery point and title/risk transfer; concentration limits; counterparty credit approval; no unresolved conflict between lender rights and buyer termination rights. |
| **Contracted cash flows** | For physical offtake, buyer pays the agreed fixed or formula price for accepted qualified output up to contracted volume. A take-or-pay shortfall payment may apply only when seller has made conforming volume available under defined conditions. For capacity reservation, buyer pays a fixed reservation or availability fee for independently verified reserved capacity and pays a separate product price on delivery. Make-up volume, carry-forward, resale, and refund rights must be stated and valued. Any prepayment is recorded as buyer credit exposure and as a financing inflow, not revenue before performance. |
| **Trigger and settlement** | Delivery payment follows metered quantity, quality release, acceptance, and delivery under the agreed Incoterm or equivalent. A buyer shortfall payment follows verified seller availability and buyer non-take, net of permitted relief. A seller shortfall remedy may be liquidated damages, fee reduction, make-up rights, or termination—not deemed delivery. Availability requires a testable definition covering qualified capacity, staffing, utilities, inputs, maintenance, and regulatory ability to produce. Disputes cannot be resolved solely by either commercial party. |
| **Cap, deductible, and retained risk** | Contract volume, tenor, annual payment, aggregate shortfall liability, make-up bank, and termination amount are capped. The seller retains uncontracted volume and performance risk; the buyer retains demand risk only within the documented commitment. A ramp allowance, delivery tolerance, or seller deductible can prevent immaterial deviations from causing default, but it may not relabel unqualified output. No payment applies above actual documented exposure. |
| **Fee and pricing unknowns** | Contract price, indexation, escalation, reservation fee, prepayment discount, credit support cost, logistics adjustment, quality differentials, minimum volume, tolerance, make-up value, termination payment, buyer option value, and lender step-in value remain unknown. Research must distinguish the market value of product and capacity from any strategic, public-benefit, or concessionary premium. A public or philanthropic over-market payment is explicit support, not evidence of the unsupported market price. |
| **Collateral and credit support** | Buyer support may include parent guarantee, standby letter of credit, cash collateral, escrow, or termination payment security. Seller support may include performance security, reserve, parent support, or capped liquidated damages. Requirements should be symmetric with actual exposure and avoid collateral calls that make commissioning failure self-fulfilling. Lender direct agreements may permit notice, cure, and competent substitute operation without forcing the buyer to accept altered product. Wrong-way risk is tested where buyer, sponsor, input supplier, and guarantor depend on the same market. |
| **Covenants and controls** | Product and process change notification; quality-system and audit compliance; forecast and nomination procedures; capacity allocation; inventory and traceability records; maintenance and business-continuity plans; no double sale or double reservation of capacity; buyer credit reporting; sanctions and anti-corruption compliance; confidentiality that preserves necessary lender and verifier access; prompt reporting of failed batches, contamination, recalls, and regulatory action. Volume incentives cannot override safe release. |
| **Exclusions and relief** | No deemed delivery of rejected, recalled, contaminated, mislabeled, unlawfully produced, late beyond material tolerance, or specification-failing output. Define force majeure narrowly and allocate ordinary equipment, contamination, input, and operating failures rather than hiding them. Buyer relief may include regulatory prohibition, seller default, quality failure, or agreed change-in-law conditions. Market-price decline alone is not relief from a fixed commitment unless expressly priced. Seller relief from buyer nonpayment is also defined. |
| **Verification** | Accredited or otherwise competent testing confirms release specification; calibrated metering and auditable batch genealogy establish quantity; independent records establish availability and tender; published or contract-defined data establish indexation; periodic capacity tests and buyer credit reviews occur at stated intervals. Sampling, retest, laboratory conflict, data retention, audit, and expert determination procedures are specified before first delivery. |
| **Suspension, termination, and workout** | Define recall and safety suspension, repeated quality failure, persistent seller or buyer default, insolvency, change in law, loss of approval, long-stop date, cure, replacement buyer or seller, lender step-in, and termination compensation. Product may not be delivered during a safety hold to preserve revenue. Workout protects traceability, storage, recall capability, lawful disposal, employee and environmental obligations, and orderly release of reserved capacity. Termination amounts must not exceed defensible exposure or operate as an undisclosed guarantee of project debt. |
| **Legal-characterization questions** | Sale of goods or services; lease or capacity right; embedded derivative; financing or deposit-taking implications of prepayment; security interest in goods, receivables, or capacity; revenue recognition and consolidation; commodity, competition, consumer, food, labeling, and product-liability law; assignment and direct agreements; insolvency treatment of prepayments and executory contracts; public-procurement and subsidy-control requirements; tax and transfer pricing. |
| **Residual risks** | Buyer qualification may take longer than contracted; the buyer may default or find legal termination rights when the project is stressed; product can meet specification yet lack consumer demand; uncontracted output remains exposed; input prices and operating costs remain; fixed prices can become uneconomic for either party; reservation may block superior uses of capacity; contract concentration can reduce rather than improve resilience. |
| **Incentive and animal-welfare safeguards** | Payment applies only to safely qualified output or genuine verified availability. No minimum-volume remedy may pressure release of unsafe product or obscure failed batches. Changes that increase animal-derived inputs or animal testing are disclosed and cannot be used to meet metrics without review. Buyer and seller may not market contracted volume as animal displacement without a counterfactual. Any impact-linked covenant uses a separately governed metric and does not condition ordinary food-safety decisions. |
| **Evidence gate before modeling or negotiation** | Executed product trials or equivalent buyer qualification evidence; complete specification and acceptance protocol; binding and non-binding demand separated; buyer procurement authorization and credit analysis; proposed volume and tenor tied to a documented need; credible seller ramp and cost curves; logistics and shelf-life evidence; term-level buyer feedback; lender analysis showing how contracted cash flow changes sizing or terms; independent test and metering plan. |
| **Go / no-go criteria** | **Go to limited diligence** only if the buyer has a real use, the obligation is enforceable and creditworthy, the seller can plausibly deliver qualified product, the payment definition is auditable, and financing additionality survives all-in cost and concentration analysis. **No-go** if interest is non-binding but modeled as revenue, qualification is controlled solely by the buyer without objective standards, the buyer cannot perform under project stress, payment rewards unsafe or unqualified output, or the contract price conceals unsupported concessionary value. |

### Questions to test

1. Does the contract support debt or only signal demand to equity providers?
2. Is physical output, verified capacity, or a combination the exposure that the
   buyer is actually willing and able to purchase?
3. How much contract value survives buyer default, termination rights,
   qualification delay, and basis between contracted and produced grades?
4. Would shorter procurement commitments across several buyers reduce
   concentration more effectively than one long take-or-pay agreement?

## 3. Capped one-way floor or two-way contract for difference

### Provisional term-sheet outline

| Field | Research outline |
| --- | --- |
| **Diagnosed failure and purpose** | Test whether a defined and measurable output-price exposure—not negative underlying unit economics, insufficient demand, or technical failure—prevents an otherwise credible project from supporting capital. A one-way floor would transfer downside price risk to a support provider. A two-way contract for difference (CfD) would exchange deviations around a strike, transferring downside and returning upside. These are distinct contracts and must be evaluated separately. |
| **Possible legal/economic form** | Bilateral cash-settled price-support contract; physical contract with embedded floor; public or philanthropic CfD; or privately negotiated derivative with eligible counterparties. A one-way public or concessionary floor may contain a material subsidy. A two-way CfD can also contain subsidy depending on strike, tenor, fees, credit, caps, and expected settlements. The label does not determine the legal character. |
| **Parties** | Project company or documented hedging entity; price-support provider or swap counterparty; public authority or philanthropic funder if applicable; calculation agent; independent benchmark administrator or data trustee; collateral agent and account bank; lenders with assignment and close-out rights; independent model, legal, and market-integrity reviewers. Broad speculative participation is outside this outline. |
| **Eligibility** | A defined reference project with regulatory path and credible qualified-output plan; documented economic exposure; eligible product grades and delivery geography; objective volume and price records; hedge notional no greater than eligible exposure; stated tenor; capability and authority of both counterparties; benchmark or reference-price method with sufficient representativeness, independence, continuity, and auditability; no unresolved manipulation or material basis-risk finding. |
| **Cash flows** | For eligible qualified output volume \(Q_t\), reference price \(P_t\), and strike \(K_t\): a one-way floor provisionally pays the project \(Q_t\max(K_t-P_t,0)\); a two-way CfD provisionally settles \(Q_t(K_t-P_t)\), positive to the project below strike and negative from the project above strike. Actual drafting must define currency, unit conversion, taxes, timing, netting, rounding, fees, indexation, and whether eligible volume is actual qualified output, a conservative scheduled amount, or the lesser of the two. No settlement is assumed for gross, rejected, recalled, or fictitious volume. |
| **Trigger and settlement** | Settlement occurs on scheduled dates after eligible volume and reference price are independently determined. The contract specifies publication time, averaging window, grade and location adjustment, data correction, fallback waterfall, disruption event, manifest error, and dispute process. A regulatory or production failure is not automatically a price event. Where no transaction occurs, a pre-agreed fallback may use a transparent basket or expert determination, but discretionary valuation by the payment beneficiary is prohibited. |
| **Cap, deductible, and retained risk** | State annual, aggregate, and per-unit payment caps; eligible-volume cap; tenor; floor attachment point or price deductible; and any co-payment. A one-way floor can include a corridor below which only a stated proportion is covered. A two-way CfD can cap both directions symmetrically or explain asymmetry. Unqualified output, volume shortfall, operating costs, basis, counterparty credit, and amounts above caps remain with the project unless separately and explicitly allocated. Notional must not exceed documented commercial exposure. |
| **Fee and strike unknowns** | Strike, premium or fixed fee, bid process, valuation method, implied distribution, expected settlement, maximum exposure, collateral funding cost, credit charge, benchmark cost, termination value, and administrative burden remain unknown. The strike may be set by auction, competitive procurement, cost benchmark, policy formula, or negotiation only after governance review. A cost-based strike is not a market price. Concessionary value equals the difference from market-equivalent terms and must be separately reported. |
| **Collateral and counterparty credit** | Define initial and variation margin, thresholds, minimum transfer amount, eligible collateral, haircuts, custody, interest, valuation, dispute, and liquidity buffers, or state why a funded reserve, guarantee, budget appropriation, or other support replaces collateral. Model wrong-way risk and close-out exposure. A provider must demonstrate authority and loss-bearing capacity through the severe-but-plausible case. Collateral calls must not create an avoidable project failure that exceeds hedge benefit. |
| **Covenants and controls** | Exposure and notional reporting; no duplicate hedge or sale of the same volume; maintenance of production and price records; benchmark conduct and conflicts rules; position limits; related-party disclosure; collateral and liquidity reporting; prompt disclosure of quality failure, recall, regulatory action, production interruption, and benchmark disruption; audit rights; no discretionary production or inventory action principally intended to manufacture settlement. Safety and maintenance cannot be deferred to preserve eligibility. |
| **Exclusions** | Volume without qualified release; production outside the defined facility or grade; wash or affiliated transactions used to influence price; prices from non-arm's-length sales unless expressly adjusted; fraud, manipulation, material misreporting, sanctions, or unlawful conduct; economic loss caused by volume, cost, quality, regulatory, or operational failure rather than the defined price variable. Exclusions do not remove amounts already earned by an innocent party unless the contract and law permit. |
| **Verification and benchmark governance** | An independent administrator applies a published methodology and conflicts policy. The data set records transactions, volumes, grades, locations, exclusions, revisions, and representativeness. Qualified-output measurement follows the underwriting data standard. The calculation agent produces a reproducible statement for each settlement. Independent review tests hedge effectiveness, basis, data concentration, sensitivity to individual reporters, and fallback performance. If no benchmark meets minimum governance, the structure does not proceed as a price hedge. |
| **Suspension, termination, and workout** | Define benchmark disruption, illegality, tax event, failure to pay, collateral default, insolvency, loss of project eligibility, prolonged no-production period, change in law, force majeure, cure, suspension, early termination, close-out valuation, set-off, transfer, and replacement. A public-support contract also states budget withdrawal and legislative-change treatment without implying an unqualified sovereign guarantee. Workout preserves records and permits independent recalculation. Close-out value is not allowed to become uncapped support. |
| **Legal-characterization questions** | Derivative, commodity interest, swap, option, insurance, guarantee, contract for sale, security, gaming or wagering restriction, or public price-support program; eligible contract participant and licensing requirements; clearing, margin, reporting, position limits, benchmark regulation, market-abuse law, netting and collateral enforceability; public appropriation, procurement, subsidy-control, and state-aid rules; tax, accounting hedge designation, fair value, and prudential capital. Each relevant jurisdiction requires specialist review. |
| **Residual risks** | Benchmark basis and discontinuity; illiquidity; strategic reporting; model error; counterparty and sovereign or budget credit; collateral liquidity; production-volume shortfall; operating-cost inflation; technology, food-safety, regulation, and demand; fixed strike becoming either inadequate or excessively generous; political and reputational risk; settlement correlation with broader stress. A floor does not guarantee project completion or solvency. |
| **Incentive and animal-welfare safeguards** | Eligible volume is actual safely qualified output and is capped at documented exposure. The project cannot increase settlement by overproducing for disposal, suppressing sales, changing grade, weakening quality, manipulating transfer prices, or increasing avoidable animal-derived inputs. Independent records preserve failed batches and recalls. Price support is not described as animal-welfare impact. Any impact objective, counterfactual, and resulting concessionary value are measured and governed separately. |
| **Evidence gate before modeling or negotiation** | Historical or defensible reference-price data with governance review; realized project or comparable transaction-price evidence; grade/location basis analysis; documented exposed volume; credible operating cost and demand cases; lender or investor evidence that price volatility is the binding constraint; provider mandate and loss capacity; proposed collateral mechanics; deterministic settlement examples including caps and fallbacks; legal perimeter memorandum. Uncalibrated price paths are stress tests, not forecasts. |
| **Go / no-go criteria** | **Go to limited diligence** only if price is the evidenced financing failure, eligible exposure and settlement are independently measurable, basis is tolerable, both parties can perform and fund collateral, maximum support is capped, and fees plus subsidy are transparent. **No-go** if no robust reference or fallback exists, the beneficiary can influence settlement, notional exceeds exposure, collateral risk dominates benefit, the project relies on the floor to conceal negative expected unit economics, or legal characterization and public authority remain unresolved. |

### Questions to test

1. Does a physical fixed-price offtake address the exposure with less basis,
   legal, collateral, and operational risk?
2. What portion of projected qualified output is actually exposed to the chosen
   reference price rather than to product grade, customer, or channel effects?
3. How do expected settlement, tail exposure, and financing benefit change under
   one-way, two-way, corridor, and capped formulations?
4. Can a support provider survive the same market state in which project
   payments peak, and what explicit subsidy does that capacity require?

## 4. Capped completion/overrun or partial credit support

### Provisional term-sheet outline

| Field | Research outline |
| --- | --- |
| **Diagnosed failure and purpose** | Test whether capital is unavailable because an otherwise willing lender or investor cannot absorb a concentrated, measurable completion overrun or defined portion of credit loss. Completion/overrun support would fund eligible excess cost or delay-related need. Partial credit support would pay an agreed share of defined debt loss or scheduled debt service. The two variants address different exposures and must not be conflated. |
| **Possible legal/economic form** | Limited completion guarantee; contingent subordinated facility; funded first-loss reserve; cost-overrun facility; performance bond; partial risk or partial credit guarantee; or capped debt-service guarantee. The provider may be a sponsor, contractor, vendor, insurer, public body, development institution, philanthropic entity, or specialist investor only if it controls, understands, prices, and can bear the stated risk. |
| **Parties** | Project company and sponsor; construction and term lenders; support provider; security trustee and facility agent; independent engineer and cost verifier; engineering, procurement, construction, equipment, technology, utility, and insurance counterparties; account bank; substitute operator where contemplated. The guaranteed creditor and payment recipient must be identified, as must reimbursement or subrogation rights against the project or sponsor. |
| **Eligibility** | Defined scope, budget, schedule, completion test, and long-stop date; complete sources and uses including funded contingency; appropriate design maturity; executed material contracts; sponsor first-loss capital; independent cost-to-complete and schedule review; credit-approved base financing; clear guarantee exposure; provider authority and capital capacity; no unresolved material technical, safety, legal, or integrity defect. The guarantee is not a substitute for a balanced base funding plan. |
| **Cash flows** | Completion variant: provider pays verified eligible cost above the agreed base budget and sponsor deductible, or a stated delay payment, up to a fixed aggregate cap; a contingent facility may instead advance subordinated funds with stated interest, repayment, and standstill. Partial credit variant: provider pays a stated percentage or amount of verified unpaid scheduled debt service or final realized lender loss after agreed recoveries and waiting period. Project or sponsor pays an upfront or periodic fee and may owe reimbursement or subrogated debt after payment. Every cash flow is shown in the consolidated waterfall to prevent double recovery. |
| **Trigger and settlement** | Completion trigger requires independent certification of eligible incurred cost, remaining cost, completion status, causation, available contingency, sponsor contribution, and absence of excluded conduct. Partial credit trigger requires a defined payment default or realized loss, notices, cure and standstill, acceleration rules, and recovery allocation. Timing must distinguish temporary liquidity from permanent loss. Payment cannot depend only on lender declaration or project-company estimate. A guarantee of debt is not automatically a guarantee that the facility will complete. |
| **Cap, deductible, co-pay, and retained risk** | State the fixed monetary cap, covered percentage, per-event and aggregate limits, tenor, reinstatement, waiting period, sponsor first-loss amount, contractor or vendor recoveries, and lender risk retention. Sponsor funds base equity and agreed contingency before support attaches. Lender retains a meaningful share unless a documented policy purpose justifies otherwise. Caps cannot reset through amendments without renewed underwriting and authority. Fraud, negligence, and ordinary controllable performance risk remain with responsible parties. |
| **Fee and pricing unknowns** | Upfront and periodic guarantee fee, commitment fee, contingent-facility spread, unused fee, reimbursement rate, risk participation, collateral cost, diligence and monitoring cost, recovery share, amendment fee, and termination price remain unknown. Pricing requires loss-frequency, severity, timing, correlation, recovery, expense, capital, liquidity, and provider mandate analysis. For public or philanthropic support, disclose market-equivalent fee, charged fee, expected subsidy, maximum exposure, and beneficiary. |
| **Collateral and provider credit** | Provider support may be funded in escrow, secured by eligible collateral, backed by a legally durable appropriation, or supported by a rated or otherwise evidenced balance sheet; the actual mechanism must match claim timing. Sponsor, contractor, or vendor collateral and bonds are applied before or alongside support according to a no-double-recovery waterfall. Provider wrong-way risk, downgrade, collateral replacement, commingling, and insolvency are tested. A promise from an entity exposed to the same technology, buyer, or funding source is not independent credit enhancement. |
| **Covenants and controls** | Budget, schedule, change-order, procurement, related-party, contingency, and cost-to-complete controls; sponsor funding and liquidity tests; no distribution while support is outstanding or completion tests unmet; maintenance of contracts, insurance, permits, quality systems, and records; monthly or milestone reporting; incident and adverse-evidence disclosure; lender consultation without lender control over unsafe operating decisions; restriction on debt amendments that increase provider exposure; provider consent and re-underwriting for material scope change. |
| **Exclusions** | Unapproved scope expansion, owner enhancements, pre-existing undisclosed defects, ordinary base-budget cost, amounts recoverable from contractors or insurance, duplicate claims, fines and penalties where coverage is unlawful, fraud, willful misconduct, sanctions, unlawful conduct, and losses caused by unauthorized abandonment. Safety-related expenditure should not be excluded merely because it is inconvenient; its allocation must be agreed in advance so needed remediation remains funded. Broad exclusions that defeat expected protection are unacceptable. |
| **Verification** | Independent engineer verifies baseline scope, progress, completion, cause and amount of overrun, remaining contingency, recoveries, and cost to finish. Agent and account bank verify debt schedule, payment, reserves, and recoveries. Technical, quality, and regulatory experts address matters beyond engineering competence. Claims files preserve invoices, change orders, incident records, rejected work, notices, and conflicts. Provider may audit but cannot suppress a valid adverse finding. |
| **Suspension, termination, and workout** | Define notice, cure, claim waiting period, funding mechanics, long-stop, provider default, lender amendments, project abandonment, substitution, acceleration, subrogation, recovery sharing, restructuring votes, and termination. Workout preserves funds for safe shutdown or continued operation, payroll, quality and traceability, lawful animal care where applicable, environmental protection, data, site security, and decommissioning. Step-in requires a competent lawful operator and continued intellectual-property access. Recoveries follow a transparent waterfall and cannot exceed loss. |
| **Legal-characterization questions** | Guarantee, surety, insurance, credit derivative, loan commitment, indemnity, security, or public contingent liability; authorization and licensing; insurable interest and claims handling; financial-guarantee insurance rules; bank capital and credit-risk mitigation recognition; public appropriation, debt-limit, procurement, state-aid or subsidy-control requirements; sovereign immunity; security, subrogation, intercreditor, netting, insolvency, tax, accounting, and consolidation. Provider mandate and enforceability must be confirmed before reliance. |
| **Residual risks** | Loss above cap or outside coverage; disputes over cause, eligibility, completion, acceleration, and recoveries; provider default or delayed appropriation; moral hazard; lender forbearance or acceleration incentives; contractor failure; uninsurable contamination, regulation, demand, price, and operating performance; specialized-asset recovery; correlation across supported projects; political withdrawal; refinancing and maturity risk after support expires. Credit enhancement changes allocation, not project fundamentals. |
| **Incentive and animal-welfare safeguards** | Sponsor, lenders, contractors, and vendors retain exposures they control. Support cannot cover fraud or reward avoidable delay, but it must preserve essential safety, quality, animal-care, worker, environmental, and decommissioning spending during distress. Completion tests require lawful, safely qualified capability, not physical construction alone. Providers cannot compel unsafe commissioning to avoid a claim. Guarantee utilization, financed capacity, and debt repayment are not animal-welfare impact; any displacement claim is separately evidenced. |
| **Evidence gate before modeling or negotiation** | Independent design and cost review; documented cost estimate class and contingency; schedule risk analysis; complete contract and insurance matrix; historic comparable overrun, delay, default, and recovery evidence where available; sponsor, contractor, lender, and provider financial capacity; defined completion and claim protocol; unsupported financing terms showing the exact credit constraint; recovery and collateral analysis; consolidated cash waterfall; severe-but-plausible provider liquidity and correlation stress. |
| **Go / no-go criteria** | **Go to limited diligence** only if the covered exposure is narrow and causal, base funding and contingency are sound, triggers and loss are independently verifiable, responsible parties retain risk, the provider can pay promptly in the peak-loss state, lender behavior remains disciplined, and maximum support plus subsidy is explicit. **No-go** if support is needed to conceal an incomplete budget or nonviable process, the provider shares the same failure driver, exclusions make payment unreliable, leverage rises solely because loss is shifted to an unaccountable party, the claim can be manufactured, or workout leaves essential obligations unfunded. |

### Questions to test

1. Is the binding exposure completion cost, temporary liquidity, scheduled debt
   service, or ultimate lender loss? One contract should not blur all four.
2. Which party controls each cause of overrun, and which vendor, contractor,
   insurance, or sponsor remedy must pay before external support?
3. Does the credit benefit survive guarantee fees, cap, exclusions, provider
   correlation, claim delay, and imperfect prudential-capital recognition?
4. Would additional sponsor equity, a larger funded contingency, narrower
   project scope, or contractor support solve the failure more directly?

## Cross-structure evaluation record

Each candidate that survives its evidence gate should be evaluated against the
same unsupported project and the same deterministic paths. The record should
show at least:

| Test | Required disclosure |
| --- | --- |
| Financing additionality | Offered terms without and with the candidate; capital amount, tenor, pricing, covenant headroom, timing, and evidence that the change is causal |
| Project economics | Cash flow and value before support, after support, and after all fees, collateral funding, reserves, taxes, and transaction costs |
| Transfer account | Payer, recipient, expected transfer, severe-case transfer, maximum legal exposure, timing, and recoveries; concessionary component shown separately |
| Risk allocation | Exposure before and after; controlling party; proposed bearer; basis, counterparty, liquidity, legal, operational, and residual risk |
| Loss and resilience | Completion probability, liquidity deficit, default timing, expected loss, tail loss, recovery, and effect of claim or collateral delay |
| Incentives | Ability to influence a trigger, defer maintenance, alter output or price, hide failure, increase leverage, or weaken safety and welfare constraints |
| Counterparty capacity | Capital, liquidity, authority, collateral, correlation, concentration, and ability to perform in the same state that creates the claim |
| Operational burden | Verification, reporting, benchmark, collateral, legal, accounting, tax, governance, dispute, and replacement costs |
| Mission evidence | Stated theory of change and counterfactual; no animal-impact claim unless a separate conservative displacement analysis passes review |
| Exit and failure | Suspension, cure, substitution, restructuring, safe continuity or shutdown, recoveries, remediation, and decommissioning |

Model comparisons should use common random paths where simulation is
appropriate and should report paired changes. An uncalibrated distribution is a
sensitivity or stress device, not an empirical probability. Expected project
benefit must reconcile to counterparty cost before fees, frictions, tax, and
externalities; a transfer cannot be counted as newly created value.

## Common evidence and approval gates

These outlines may advance only in the following order:

1. **Reference-project gate:** the asset, product, parties, facility boundary,
   schedule, sources and uses, and qualified-output definition are complete.
2. **Technical gate:** full run history, failures, scale transfer, design
   maturity, cost, schedule, quality, and regulatory evidence survive
   independent review.
3. **Unsupported-economics gate:** the project can be distinguished from a
   fundamentally uneconomic or technically immature proposal. Any justified
   public-benefit support is declared as support.
4. **Financing-failure gate:** observed financing evidence identifies the
   specific constraint and falsification condition.
5. **Instrument-evidence gate:** trigger, exposure, baseline, settlement,
   verification, cap, counterparty, and residual risk are measurable and
   legally reviewable.
6. **Incentive and mission gate:** no term weakens safety, quality, truthful
   reporting, worker protection, animal care, environmental duties, or
   remediation; impact claims remain no stronger than evidence.
7. **Provider-capacity gate:** the proposed bearer can understand, fund,
   collateralize where needed, and survive severe-but-plausible loss without
   relying on the same source of project resilience.
8. **Legal and institutional gate:** qualified advisers and competent decision
   makers approve the actual form, parties, jurisdiction, disclosures, and
   authority. Research language does not satisfy this gate.
9. **Limited-pilot gate:** notional and loss are capped; counterparties are
   suitable; settlement, claims, collateral, data, incident, complaint, and
   termination operations have been tested; no public solicitation is implied.

Failure at a gate produces a documented no-go or a return to research. It does
not justify substituting another instrument without diagnosing that
instrument's own financing failure and evidence needs.

## Research decision record template

For each candidate, preserve:

1. reference project and document version;
2. diagnosed failure and evidence relied on;
3. alternatives considered, including no transaction;
4. provisional form, parties, use of proceeds, and cash-flow diagram;
5. formulas, units, dates, caps, fees, collateral, and examples;
6. unsupported, supported, downside, and severe-case results;
7. transfer and subsidy account from every party's perspective;
8. data lineage, verifier, model version, seed, limitations, and open findings;
9. legal, regulatory, accounting, tax, prudential, procurement, and subsidy
   review status;
10. safety, worker, animal-welfare, environmental, and community review;
11. additionality conclusion and animal-impact claim status;
12. conflicts, dissent, conditions, decision owner, and next review date; and
13. go, conditional-go, redesign, research-only, or no-go decision with its
   falsification and reconsideration conditions.

The absence of a suitable instrument is an acceptable result. A clean no-go is
preferable to financing that disguises an economic shortfall, creates an
unfunded public liability, or weakens the conditions under which cellular
agriculture could responsibly reduce reliance on animals.
