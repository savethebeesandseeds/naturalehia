# Financial-Engineering Precedents v0.1

Status: research map for instrument design, 2026-08-29.

## Question

What can cellular agriculture legitimately borrow from financial engineering
for rare-disease research, global-health development, blended finance, and
innovation lending?

The useful answer is not “copy a biomedical megafund.” It is a set of tested
distinctions:

- which development stage can support which claim;
- whether repayment comes from project cash, sale or licensing of rights, a
  creditworthy external receivable, or explicit subsidy;
- whether first-loss support is funded capital or an unfunded guarantee;
- whether projects genuinely diversify after common factors;
- whether leverage is introduced only after risk becomes measurable; and
- whether simulated performance is clearly separated from issued-market
  evidence.

Each note below separates source evidence from our design inference.

## 1. Research-backed obligations: the intellectual starting point

Fernandez, Stein, and Lo propose a bankruptcy-remote vehicle that pools
biomedical-development assets and issues senior debt, junior debt, and equity.
The modeled vehicle receives value mainly through sale or licensing of
partially developed assets. Senior-first waterfalls, overcollateralization,
reserves, coverage triggers, cash sweeps, guarantees, and staged asset sales
allocate risk
([Nature Biotechnology](https://www.nature.com/articles/nbt.2374),
[author PDF](https://www.rogermstein.com/wp-content/uploads/FernandezSteinLo_NBT_2012.pdf)).

**Evidence boundary.** Favorable default and return figures in that work are
simulations. They depend on assumed development probabilities, dependence,
asset-sale values, deployment speed, financing terms, and the depth of the
market for partially developed rights. A 2025 PLOS ONE article states that
securitizing drug-development pools into tranches had not been used to finance
drug development as of its publication
([PLOS ONE](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0325826)).

**Inference for this project.** “Research-backed obligation” is honest only
where a limited-recourse vehicle controls transferable, enforceable licensing,
royalty, exit, procurement, or other cash rights. A mission fund holding
uncertain company valuations but no attachable cash is not an RBO. We use the
RBO as an analytical precedent, not proof of investor demand or rating.

## 2. Correlation, not project count, controls pooling

Hull, Lo, and Stein analyze portfolios of low-probability “long shots.”
Independent risks can become more stable as the portfolio grows, but even
small common-factor correlation can sharply worsen senior and mezzanine
default performance. Their work also states the central conservation rule:
tranching cannot turn a negative-NPV pool into a positive-NPV pool; it
redistributes available cash and risk
([Journal of Investment Management issue](https://joim.com/issue/2019q4/),
[author PDF](https://www.rogermstein.com/wp-content/uploads/LongShots_12.pdf)).

Lo and Siah similarly show that common phase-transition factors weaken modeled
drug-development portfolio performance and that asset-sale proceeds dominate
cash flow
([author PDF](https://qlsadvisors.com/wp-content/uploads/2021_Megafund_JSF.pdf)).

**Inference for this project.** Report effective independent bets and explicit
joint scenarios, not raw facility count. Cellular-agriculture common factors
include shared cell platforms, media and growth-factor inputs, bioreactor
scale-up, contamination modes, equipment suppliers, energy, regulation,
consumer acceptance, buyers, and the same follow-on funding market. A senior
tranche whose safety disappears under these states is not diversified senior
risk.

## 3. Dynamic leverage is more credible than day-one research debt

Montazerhodjat, Frishkopf, and Lo propose financing early drug discovery mainly
with equity and introducing debt as assets mature and default risk becomes
measurable
([PubMed](https://pubmed.ncbi.nlm.nih.gov/26708982/),
[journal](https://www.sciencedirect.com/science/article/pii/S1359644615004560)).

**Inference for this project.** A stage ladder is stronger than one universal
bond:

1. grants finance shared science, standards, cell lines, public evidence, and
   other public goods without artificial repayment claims;
2. mission, sponsor, and private equity or funded first-loss capital finance
   early company and scale-up risk;
3. participating or contingent venture debt follows technical milestones,
   professional-equity validation, or early commercialization; and
4. senior debt follows enforceable licensing, royalty, procurement, offtake,
   or other receivables capable of servicing it.

The project standard should let exposures migrate between these instruments as
evidence changes without renaming early risk as seasoned debt.

## 4. Guarantees consume risk-bearing capacity; they are not free diversification

Fagnan, Fernandez, Lo, and Stein simulate a public guarantee for a biomedical
megafund. In some calibrations the expected guarantee cost is small relative
to notional, while adverse-percentile draws remain hundreds of millions
([American Economic Review](https://www.aeaweb.org/articles?id=10.1257/aer.103.3.406),
[unabridged paper](https://papers.ssrn.com/sol3/papers.cfm?abstract_id=2203203)).

Sida distinguishes loan-portfolio, project-finance, balance-sheet, and
fund-structure guarantees. A fund-structure guarantee covers terminal fund net
loss rather than each loan. Its reported practice prices guarantees from
expected-loss assessment and reserves the expected-loss amount
([official portfolio report](https://cdn.sida.se/app/uploads/2022/09/13120328/10206362_Sida_Guarantee_Portfolio_2021_web.pdf)).

Public-credit cost, price, provision, and capital are different quantities:

| Institutional precedent | Quantity and boundary | Use here |
|---|---|---|
| [US OMB Circular A-11, section 185 (2025)](https://www.whitehouse.gov/wp-content/uploads/2025/08/s185.pdf) | Federal credit subsidy cost is the present value of government cash outflows less inflows, adjusted for expected departures from contract and discounted on a maturity-matched Treasury basis. Administrative expenses are separate, and estimates are periodically revised. | A modeled expected claim is not automatically a fiscal subsidy estimate or a complete provider budget. |
| [EU Financial Regulation 2024/2509](https://eur-lex.europa.eu/eli/reg/2024/2509/oj/eng), Articles 214 and 216 | Global provisioning is based on net expected loss plus an adequate safety buffer; common provisioning considers correlation. | Pooling can leave mean quota-share claims unchanged while changing liquidity and capital needed for adverse joint states. |
| [EU Guarantee Notice](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX%3A52008XC0620%2802%29) | A market-compatible scheme premium addresses normal risk, administration, and remuneration of adequate capital; an insufficient premium can contain aid. | Expected claim PV is only one component of a market premium. The notice's numerical examples are historical legal precedent, not calibration. |
| [Basel CRE35](https://www.bis.org/basel_framework/chapter/CRE/35.htm?inforce=20230101&published=20200327&tldate=20241130) | For covered bank exposures, expected loss, eligible provisions, and regulatory-capital treatment are separate concepts. | The research model must not call a claim-cost estimate accounting provision or regulatory capital. Applicability requires institution-specific analysis. |
| [EIF InnovFin SME Guarantee brochure (2014)](https://www.eif.org/news_centre/publications/eif_flyer_innovfin_sme_guarantee_en.pdf) | The historical programme described 50% principal-and-interest loss sharing, pro-rata recoveries, and annual fees of 0.50% for SMEs and 0.80% for small mid-caps. | This is an architecture precedent only—not current availability, a market quote, or a cellular-agriculture calibration. |

For transparent instrument research, the project's declared provider cost
ladder is:

```text
net provider claim-cash-flow present value
+ administration
+ liquidity and unexpected-loss capital cost
+ required provider return
= declared provider funding requirement before explicit catalytic support
```

The first line is gross provider claim cash paid less evidenced subrogation or
provider recovery cash under the stated timing convention. Guarantee fees are
excluded from that line and shown, with catalytic capital, as separate funding
sources for the requirement. This is a research decomposition, not a universal
pricing identity, fair-value rule, fiscal subsidy method, accounting provision,
or regulatory-capital formula. Fees do not reduce the underlying claim cash or
create diversification.

**Inference for this project.** A guarantee must disclose provider, reference
loss, attachment, exhaustion, cap, term, claims rules, timing, fee,
counterparty performance, expected loss, and tail loss. A funded first-loss
tranche and an unfunded contingent guarantee are different instruments. Any
public or philanthropic loss absorption creates contingent exposure and, when
paid, expenditure. It must be reported as catalytic support, not diversification
created inside the pool. An unfunded guarantee consumes risk-bearing capacity
before any claim cash is paid.

## 5. A useful realized life-sciences pool analogue: GHIF

The Global Health Investment Fund pooled public, philanthropic, and private
capital for late-stage global-health products. The 108-million-dollar fund
used equity and mezzanine structures, milestone and royalty repayment, and a
partial Gates Foundation/Sida guarantee. The guarantee absorbed the first 20%
of losses and then 50% of remaining losses; private investors retained loss
exposure
([IFC disclosure](https://disclosures.ifc.org/project-detail/SII/33006/global-health-investment-fund),
[World Bank/IFC case study](https://documents1.worldbank.org/curated/en/959631487668386111/pdf/112812-WP-GHIF-PUBLIC.pdf),
[manager](https://ghicfunds.org/global-health-investment-fund/)).

**Inference for this project.** GHIF supports a late-stage pooled innovation
fund with capped partial loss protection, not rated senior debt on basic
research. Relevant features are real first-loss capacity, private downside
retention, milestone and royalty instruments, and mission covenants. It does
not establish that cultivated-meat technical assets have comparable sale
markets, development probabilities, or guarantee appetite.

## 6. Blended-finance discipline

IFC's blended-finance principles require a development rationale and
additionality, minimum concessionality, crowding-in, commercial
sustainability, market reinforcement, and high standards. IFC discloses the
amount of concessionality and why it is needed
([IFC](https://www.ifc.org/en/what-we-do/sector-expertise/blended-finance/how-blended-finance-works)).

**Inference for this project.** Public or philanthropic first-loss capital
should be the minimum amount required to mobilize otherwise unavailable
capital, transparently valued, time-bounded, and separately tested for market
distortion. Mission impact and investor return are separate outputs. A
subsidized tranche may be valuable for animals and society while not being a
commercial return source; the standard should say both.

## 7. Reliable external receivables can support senior debt

The International Finance Facility for Immunisation issues bonds against
long-dated sovereign donor pledges
([Gavi/IFFIm](https://www.gavi.org/investing-gavi/innovative-financing/iffim)).
Gavi's pneumococcal advance market commitment adds post-success price support
for qualifying products subject to specifications and supply commitments
([Gavi AMC](https://www.gavi.org/investing-gavi/innovative-financing/pneumococcal-advance-market-commitment-amc/how-it-works)).

**Inference for this project.** Investment-grade cellular-agriculture debt is
more credible against enforceable sovereign, donor, procurement, capacity, or
offtake receivables than against an optimistic technology valuation. An AMC is
a pull mechanism supporting revenue after qualified success. It does not
insure scientific failure or make an unqualified product financeable.

## 8. Observed cellular-agriculture financing signals

The 2023 InvestEU/Invest-NL Meatable operation is historical evidence of a
cultivated-meat production proposal financed at company level through direct
equity with private co-investors and public guarantee support. It is not
current operating-performance evidence
([InvestEU](https://investeu.europa.eu/meatable_en),
[Investment Committee conclusions](https://investeu.europa.eu/document/download/2e0f8bb8-21cc-4cba-852f-f1d48571e742_en?filename=INVEU-ICR-0069-2023---IC-20-RIDW-%28InvestNL%29---Meatable_Conclusions_draft-1.pdf)).

The EIB's InvestEU-backed 35-million-euro venture-debt transaction with Formo
followed a 61-million-dollar Series B and commercial supermarket presence.
EIB venture debt can use bullet repayment, subordination, and equity-linked or
participating remuneration. Formo is precision-fermentation-based rather than
cultivated meat
([EIB product](https://www.eib.org/en/products/equity/venture-debt/index.htm),
[transaction](https://www.eib.org/en/press/all/2025-008-eib-provides-eur35-million-to-formo-to-expand-production-of-cheese-alternatives-free-from-animal-products)).

Agronomics' audited FY2025 results report that 97% of total assets by value
were held in investments for which no quoted market price was available. The
carrying amount of invested assets fell 16% to GBP 121.0 million and NAV per
share fell 20%. Disclosed write-downs included GBP 11.9 million for full
impairment of Meatable following its announced voluntary liquidation, GBP 3.9
million for Solar Foods, GBP 2.3 million for Liberation Bioindustries, and GBP
5.2 million for Blue Nalu. These are issuer portfolio disclosures, not
performance data for a financed pool
([LSE/RNS](https://www.londonstockexchange.com/news-article/ANIC/final-results-and-notice-of-agm/17392489)).

Foundational cellular-agriculture work remains grant-financed, including the
USDA NIFA Tufts program and Horizon Europe's FEASTS project
([USDA NIFA](https://training-portal.nifa.usda.gov/web/crisprojectpages/1027620-integrated-approaches-to-enhance-sustainability-resiliency-and-robustness-in-us-agri-food-systems.html),
[CORDIS](https://cordis.europa.eu/project/id/101136749)).

**Inference for this project.** The observed sector ladder is grants plus
equity at the foundation, flexible venture-like claims after institutional
validation, and very limited evidence for senior project debt. Unquoted NAV is
not debt-service cash. Public guarantees can crowd in professional capital but
do not change the technical stage of the asset.

## Design standard adopted here

The project therefore uses this terminology precisely:

- **funded first-loss capital:** cash already subscribed and available to
  absorb defined pool loss;
- **guarantee:** a separately modeled contingent obligation of a named payer;
- **senior, intermediate, first-loss, residual:** legal/economic priority in a
  complete cash and loss waterfall, not marketing risk labels;
- **attachment and detachment:** absolute loss coordinates within a funded
  stack;
- **overcollateralization and reserve:** real assets or subscribed cash, with
  ownership, yield, loss, and return rules stated;
- **limited recourse:** investors rely only on identified vehicle assets and
  contracts;
- **offtake-, pledge-, royalty-, licensing-, or procurement-backed:** the
  actual repayment source is named;
- **common-factor dependence and effective independent bets:** pool size is
  never used as a proxy for diversification; and
- **concessionality and additionality:** subsidy is quantified and justified
  separately from commercial value.

The first implemented capital stack is consequently conservative: fully
funded at par, zero reserve yield, no price claim, separate principal and
non-principal cash, fixed physical scenarios, explicit probability ambiguity,
and complete conservation controls. Its specification is
[Fully Funded Capital Stack v0.1](CAPITAL_STACK_TERM_V0_1.md).
