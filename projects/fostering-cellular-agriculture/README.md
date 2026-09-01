# Fostering Cellular Agriculture

Fostering Cellular Agriculture is a financial-research and engineering project
building an open standard and a focused family of instruments that can fund
cellular-agriculture projects from research through mass production. The
standard translates capital needs, milestone states, success cash flows,
failure recovery, and shared risks into comparable financial terms.

Start with the [technical white paper](whitepaper/fostering-cellular-agriculture-white-paper.pdf),
the publication-level statement of the proposed instrument, its cash rights,
risk allocation, valuation equations, synthetic ten-claim results, and evidence
boundary. [The Financial Instrument in Brief](docs/FINANCIAL_INSTRUMENT_IN_BRIEF.md)
is the short explanation of what investors own, where returns come from, and how
loss and diversification work. The browser-facing
[Investor View](investor-view.html) makes the validated ten-claim synthetic
case explorable through cash sources, NPV, tail loss, shared factors, joint
states, protection variants, and the remaining investability gap. The technical
[Project Financial Interface v0.1](docs/PROJECT_FINANCIAL_INTERFACE_V0_1.md)
defines how initiatives connect to the instrument and portfolio models. The
[Project Claim Ledger v0.1](docs/PROJECT_CLAIM_LEDGER_V0_1.md) is the upstream
transaction normalizer: it keeps buyer price, borrower proceeds, claim balance,
cash settlement, conversion, guarantees, and later backtests separate before a
claim enters those models. The
[Financial Instrument Family v0.1](docs/INSTRUMENT_FAMILY_V0_1.md) then defines
which claim fits each stage, where its cash return comes from, who bears loss,
and what would falsify its investment thesis.

The current construction decision is the
[Multi-Project Milestone Participation candidate term sheet](docs/MULTI_PROJECT_MILESTONE_PARTICIPATION_TERM_SHEET_V1.md):
one untranched core and only two alternatives, funded first loss or a
failure-contingent partial-credit guarantee. The
[ten-claim analysis](docs/TEN_CLAIM_INSTRUMENT_ANALYSIS_V1.md) applies the same
fixed project paths and probability set to all three.

Naturalehia retains no protocol fee, carried interest, instrument royalty, or
share of financed-company receipts for publication or adoption of the open
standard. Investors still require enforceable repayment and success cash from
the underlying project claims.

The consolidated C++ comparison is `naturalehia-instrument-family`; its seven
reproducible inputs and invocation are documented with the
[ten-claim synthetic fixture](scenarios/ten-claim-instrument-v1-synthetic/README.md).
The complete current warnings-as-errors C++20 suite passes 75 of 75 tests in
the pinned Emscripten 6.0.5 Release build when every executable and CLI wrapper
runs through Node. The record includes the retained version-1 construction,
Capital Stack v0.2 bridge and direct Claim Ledger integration, the v0.2
capital-mobilization frontier, strict controlled-cohort loader, and the
issue-price browser target.

> **Current status:** this repository implements an open financial standard that
> keeps cash sources, contractual loss, dependence, and evidence quality
> explicit. The Project Claim Ledger
> v0.1 normalizes one claim's settlement, balances, cash rights, conversion,
> guarantees, and decision/backtest boundary. Its verified v0.2 adapter is the
> standard project hook into Portfolio: it reloads and hash-checks the package,
> admits only complete synthetic mechanics or an expected-return-admissible
> controlled candidate, consumes the frozen decision cut, separates investor
> cash from contractual principal, and preserves source, provider,
> economic-cluster, calendar, and output lineage. Its verified joint assembler
> combines two or more such claims only through a complete declared coupling,
> reproduces every project marginal exactly, and rejects duplicate clusters or
> cross-asset cash-source aliases.
>
> The portfolio and risk engines evaluate declared
> joint scenarios rather than inferred independence. They conserve external
> cash budgets and principal, report return, loss, liquidity, dependence, and
> tail risk, and re-project bounded physical-probability sets through
> participation and protection analyses, plus versioned fully funded capital-
> stack and market-claim analyses. Capital Stack v0.2 funds the sum of project-
> level maximum uses, preserves asset writeoff `L` and continuing principal
> `O`, and separately layers only issued-principal cash shortfall `Q`. These are
> transparent mechanics and ambiguity tests, not calibrated forecasts or prices.
>
> The selected core is now the **Multi-Project Milestone Participation**. Its
> exactly ten-claim fixture has nine explicit dependent states, milestone stop,
> continuing exposure, recoveries, success cash, a two-class funded waterfall,
> and a separately cost-recovery-tested and credit-stressed 30% guarantee. The central
> unsupported NPV is `0.661828`, but the adverse expected NPV is
> `-18.717674`; neither variant repairs the candidate across the whole
> synthetic probability set. That rejection is a financial result, not a
> management conclusion.
>
> The earlier
> [Failure-Contingent Public Partial-Credit Guarantee v0.1](docs/FAILURE_CONTINGENT_PUBLIC_PARTIAL_CREDIT_GUARANTEE_V0_1.md)
> remains an unchanged two-claim control: zero investor premium capacity and a
> `0.800000` claim-only catalytic gap before provider expenses. Retained public
> packages remain incomplete and supply no empirical calibration; no result is
> fair value, a rating, a recommendation, or proof of financeability.

## Mission and theory of change

The moral purpose is to help build a durable transition away from systems that
confine, harm, and slaughter animals. Moral urgency makes careful work more
important; it does not turn an assumption into evidence or negative unit
economics into a financeable asset.

An animal-welfare claim requires two separate counterfactual links:

```text
financial mechanism
    -> incremental funding, better terms, or earlier credible capacity
    -> incremental qualified output that is accepted and sold
    -> substitution for animal-derived production
    -> reduced animal use or suffering relative to the market counterfactual
```

The first link is a financing-additionality question. The second is a market,
composition, and displacement question. Cultivated share in blended products,
animal-derived process inputs, buyer substitution, price effects, rebound,
leakage, geography, timing, and uncertainty all matter. If either link is not
established, this project reports capacity or output—not “animals saved.”

Food safety, worker safety, scientific integrity, and animal welfare are
constraints on every design. No payment formula may reward off-specification
release, hidden contamination, deferred maintenance, selected reporting, or
avoidable animal use.

## What is built

- A concise public financial architecture and a common project-level interface
  for capital, states, external cash, instrument payoffs, recovery, shared
  factors, standalone metrics, and pooling.
- A deterministic C++20 one-project/one-claim cashflow and state ledger with a
  strict SHA-256-bound package format. It separates primary buyer cash,
  borrower proceeds, fees, principal, accrual, amounts due, actual borrower,
  recovery and provider cash, non-cash conversion, writeoff, covenant states,
  and ex-ante versus later backtest records. Incomplete packages remain valid
  records but cannot produce expected return or a rate preimage.
- Portfolio v0.2 explicit-contractual-principal accounting, with strict v0.1
  reload compatibility. Investor outlays and receipts remain separate from
  principal additions, repayment, conversion, and writeoff, so cash paid above
  or below a contractual-principal limit cannot be relabeled as principal loss.
- A verified ClaimLedgerPackage-to-Portfolio v0.2 adapter that consumes only
  the frozen decision cut, requires reconciled external source budgets, and
  preserves package, entry, provider, cluster, and generated-output lineage.
- A verified Claim-Ledger joint-portfolio assembler that reloads every package,
  requires one calendar and information cut, rejects duplicate economic rights
  and source IDs, accepts no inferred independence, and reconciles every
  evaluated joint marginal plus each project's expected cash, loss, and NPV.
- A candidate-only Partial-Credit Claim-Loss Cohort kernel that re-verifies
  claim roots, keeps every caller-declared resolved, unresolved, immature,
  zero-loss, and excluded row visible, distinguishes contractual face from
  actual principal created, and reports exact resolved points, fixed-denominator
  positive-outcome frequency ranges, and finite provider lifetime-cap outer
  envelopes where defensible. Other open amount totals remain `Unknown` with
  blockers. Contractual face is never misused as a cumulative open-loss cap,
  and unknown provider amounts widen frequency bounds rather than becoming
  zero. Resolved rows are reloaded through Claim Ledger's one-scenario,
  selected-latest full-path evidence snapshot, which preserves input status,
  source and source date, common-versus-scenario scope, cash-path status,
  provider terms, and applicable covenants; an explicitly complete-resolved
  status is required. It emits no probability, Portfolio, price, empirical, or
  calibration authority. A strict five-file loader now binds the cohort,
  methods, dossier and evidence manifest by exact SHA-256; confines and reloads
  every Claim Ledger package; and runs Evidence Gate's claim-population
  profile. Census truth, classification/method admission, term comparability,
  exclusion timing, and a positive empirical as-of policy remain deferred.
- A deterministic C++20 multi-project participation-pool kernel. It requires
  complete weighted joint scenarios, traces receipts to named cash sources,
  measures project and pool loss/NPV/liquidity tails, calculates pairwise loss
  correlation and ES95/ES99 diversification, attributes projects' pool-tail
  contributions, and reconciles optional contiguous loss layers back to the
  same aggregate loss.
- A strict, canonically printable participation-pool scenario format and a
  dedicated CLI that reports project marginals, external return sources,
  outstanding exposure, realized loss, liquidity, pairwise dependence,
  ES95/ES99 diversification, joint-state ledgers, and reconciliation controls.
- A strict staged-capital adapter that preserves each actual facility path,
  dated provider cash, funded-principal loss, physical weight, and explicit
  completion-cash source allocation without inventing factor exposure.
- A deterministic C++20 physical-probability envelope over fixed pool paths.
  It reports exact componentwise ranges for expected cash, exposure, loss,
  impairment, NPV, liquidity, cash-source receipts, and loss/NPV-shortfall
  ES95/ES99, with a feasible probability witness for every endpoint.
- A separate strict, canonically printable probability-envelope format and CLI,
  plus a calibration standard covering lifecycle evidence, sparse data,
  recovery, timing, dependence, physical-versus-pricing measures, validation,
  and prohibited cross-sector parameter transfers.
- A strict C++20 event-probability-polytope v0.2 layer over the same fixed joint
  cash paths. It accepts explicit overlapping event subsets, requires the
  declared central measure to satisfy every scenario and event bound, and
  publishes audited floating-point linear pool and project ranges,
  event-constrained ES95/ES99, common-tail project attribution, full endpoint
  measures, and primal, objective, and reduced-cost residuals. It also routes
  the fixed candidate paths through the fully funded capital stack and
  re-projects tranche cash risk, return, shortfall tails, robust target results,
  and common-measure principal WAL under those same event constraints.
- A candidate-only C++20 joint-cohort bridge over a SHA-256-bound raw row
  ledger. It retains not-yet-matured and unresolved observations as compatible
  with every fixed joint state, constructs a conservative simultaneous
  Hoeffding outer set under an explicit IID complete-joint-unit assumption,
  keeps Goodman score intervals as a large-sample challenger only, and projects
  exact project dollar ranges, common-witness pool-tail contributions, and
  project, pairwise, and aggregate impairment without changing any cash path or
  central hypothesis.
- An implemented C++20 Calibration Binder v0.1 and CLI for one-project
  candidates. It confines and raw-file-hash-binds the scenario, probability
  envelope, evidence dossier, evidence manifest, and lineage; requires exact
  one-to-one coverage of material normalized inputs; checks source-derived
  citations through the controlled evidence gate; and always leaves
  `calibrated_execution_authorized=false`.
- A deterministic robust success-participation term solver, strict companion
  input, and CLI. It scales only selected non-principal cash already granted in
  the declared paths, recomputes the adverse probability witness jointly at
  each candidate term, publishes a certified threshold bracket when feasible,
  and rejects a term when full participation remains below target.
- A deterministic pooled principal-loss protection solver, strict companion
  input, and CLI. It solves an exact horizon-settled proportional loss share,
  leaves underlying project loss unchanged, reports provider payout tail risk,
  and rejects any supposed bilateral price when investor premium headroom and
  the provider's claim-only robust floor do not overlap.
- A deterministic provider price-ladder sensitivity, strict companion input,
  and CLI. It uses contractual exposure to disclose collateral and allocated-
  capital bases, charges only their independently asserted incremental costs,
  adds fixed and claim-variable expenses and target profit, preserves complete
  ambiguity witnesses, and reports any investor/provider gap as explicit
  required catalytic support. It does not expense collateral principal or
  allocated capital stock or claim fair value, a provider balance sheet, or
  capital adequacy.
- A deterministic fully funded capital-stack term, strict companion input,
  and CLI. Legacy v0.1 retains its at-par commitment model. Version 0.2 accepts
  the Claim Ledger's explicit contractual-principal portfolio and keeps four
  quantities separate: issued tranche principal, acquisition and primary-
  funding uses, contractual asset principal, and direct-cost calls. Its issued
  reserve `R` is the sum of project-level maximum uses. Asset writeoff `L` and
  continuing asset principal `O` remain Portfolio facts; horizon issued-
  principal cash shortfall `Q` is a separate liability fact, and only
  `layer(Q)` is mapped to tranches. Principal-base cash above issued principal
  enters the non-principal waterfall with source-preserving, equal-seniority
  pro-rata memos. The principal-limit-minus-use result is a capacity diagnostic,
  not a valuation. The exact Claim Ledger joint portfolio enters without
  re-encoding and retains its probability lineage. Both versions report actual
  cash, version-specific principal risk, NPV, shortfall tails, and common-
  witness principal WAL without claiming price, spread, rating, fair value, or
  calibrated execution.
- A consolidated C++20 ten-claim instrument-family reporter and strict golden
  test. It requires exactly ten synthetic claims, the five declared adverse
  risk categories, a two-class funded variant, and the exact 30% protection
  point; then reports the unsupported core, funded waterfall, full-performance
  guarantee, provider-credit stress, common-witness ranges, tails,
  concentration, dependence, premium feasibility, and catalytic gaps without
  creating another solver.
- A deterministic C++20 robust capital-mobilization frontier over a declared
  finite `q`-by-`A` grid. Legacy v0.1 generates its two-claim at-par structure
  and retains principal-loss layering. Additive v0.2 takes a separately
  validated Capital Stack v0.2 template, changes only `q` and junior issued
  principal `A`, and routes market principal risk exclusively through
  `E[Q]/M`, Q-ES95/M, Q-ES99/M, and `Pr[Q>0]`. Both versions re-project the
  whole structure through the event probability set, report contributions,
  principal and total distributions, complete NPV ranges, shortfall tails,
  negative-NPV probability, common-measure WAL, junior concession, and every
  endpoint's own witness. They report feasible, nondominated, least-tested-
  junior, and minimum-tested-participation results without claiming price,
  rating, continuous optimality, investor demand, or actual capital
  mobilization. Candidate count and combined
  probability-projection, individual portfolio-record, project-path, and
  two-claim monthly waterfall work are all covered by one fail-closed resource
  bound.
- A deterministic C++20 robust market non-principal priority-cap sensitivity,
  strict fifth input, and CLI. It keeps `q`, `A`, `K`, `M`, cash paths,
  hurdles, probability bounds, principal risk, and WAL fixed; changes only the
  market claim's lifetime cap `B` on actual non-principal cash; and publishes
  market adequacy, junior concession, every candidate and witness, selection
  brackets, and dated transfer invariants. It selects only the smallest tested
  adequate or balanced cap and makes no price, coupon, yield, investor-demand,
  or continuous-optimum claim.
- A deterministic C++20 robust issue-price support engine, strict sixth input,
  and CLI. It holds the selected claim and physical paths fixed, shifts only
  buyer price and discounting, and reports robust price ceilings, issuer floors,
  conditional windows, support gaps, NPV downside, principal risk, and each
  endpoint witness. It distinguishes model arithmetic, support documentation,
  funded capacity, settled sources, and evidenced uses; rejects circular
  hurdle validation and ineligible secondary-price bases; and claims no fair
  value, yield, demand, support performance, or completed financing.
- A deterministic C++20 robust hurdle-evidence set engine, strict standalone
  input, and CLI. It classifies direct settled identical claims, settled
  comparables, and executable two-sided quotes without pooling tiers; maps each
  eligible price through an independently declared expected-cash reconstruction
  and jointly feasible log-return adjustment; and constructs exact closed
  `S_0` through `S_k` unions by coverage count. Empty mappings and disagreement
  gaps remain visible, higher-quality but insufficient evidence cannot fall
  through to a weaker tier, and every selected component and excluded cell is
  audited. Its output is a transaction- and model-conditioned candidate set,
  not a buyer-belief estimate, expected holding return, fair value, benchmark,
  demand finding, or authenticated empirical release.
- A dependency-free C++20 library and command-line applications.
- A strict, versioned `key=value` configuration format that rejects unknown,
  duplicate, missing, non-finite, and incoherent inputs.
- A common-path Monte Carlo engine for one facility, so unsupported and
  structured cases receive identical physical and market draws.
- Synthetic construction, ramp, utilization, biological yield, contamination,
  output-price, variable-cost, fixed-cost, debt-service, default, and recovery
  mechanics.
- Fixed-price physical offtake, one-way or two-way output-price support, capped
  completion-delay cover, and an upfront fee.
- Per-leg transfer attribution, project and sponsor cash-flow PV, shortfall
  VaR/expected shortfall, signed DSCR, horizon payment-default probability,
  unconditional expected lender loss, conditional default severity, terminal
  debt disclosure, conditional default timing, and paired counts for
  within-horizon defaults avoided or introduced and for defaults delayed,
  accelerated, or unchanged in timing.
- Two explicitly synthetic fixtures: an illustrative package and a deliberately
  adverse stress.
- Automated model, parser, and CLI tests compiled under strict warnings.
- A second dependency-free C++20 CLI that strictly parses a reference-project
  dossier and 29-field evidence manifest, verifies confined retained-copy
  SHA-256 values, preserves resolvable adverse history, and applies 57 atomic
  requirements across four separate fail-closed decision gates.
- A public dossier for the Believer Meats Wilson facility, including current
  FDA/FSIS, court-filed, parcel-index, docket, and sale-process leads; a
  machine-readable manifest; gap register; and phased controlled data request.
  Public capacity, investment, legal, and operating statements remain
  source-tagged and excluded from model calibration.
- A separate deterministic monthly C++20 engine for a milestone-gated,
  delayed-draw committed capital facility. It posts a five-account cash ledger,
  reconciles undrawn-commitment and funded-claim memo accounts pathwise,
  enforces sponsor and cost-to-complete funding tests, distinguishes provider
  entitlement from cash performance, protects a workout reserve from creditor
  recovery, and applies a capped PIK claim and terminal recovery waterfall.
- An explicit weighted synthetic case set and provider cash-flow analysis with
  draw and loss distributions, stranded spend, funding and safety shortfalls,
  and a signed physical-P zero-NPV upfront-fee sensitivity. Actual provider-
  failure stresses remain in project-risk results; fee adequacy replays every
  same-relative-weight physical case with provider cash performance held true,
  so the
  provider cannot benefit from its own modeled nonperformance.
- A research agenda, evidence register, underwriting data standard, financing
  failure map, instrument taxonomy, responsible-finance charter, four candidate
  research term sheets, and roadmap for a later monthly model.

## Annual engine v0.1: exact boundary

The current engine uses annual periods and millions of the scenario's declared
currency label. Each file must declare its monetary basis as `real`, `nominal`,
or `unspecified-synthetic` and remain internally consistent. The two software
fixtures use `DEMO` and `unspecified-synthetic`.

| Current convention | What it means |
|---|---|
| All qualified output is sold | There is no demand, product-acceptance, inventory, or working-capital model. |
| Offtake is incremental fixed-price repricing | It is not take-or-pay, capacity reservation, buyer credit, or a full sales agreement. It can remove upside in high-spot paths. |
| Completion-delay cover is parametric cash | It is not proof of a guarantee or insurance policy. It is treated as unrestricted cash and lacks fault, claims, exclusions, recourse, and counterparty risk. |
| Debt default is mechanical | It is the first uncured annual scheduled-payment shortfall within the analysis horizon; there is no reserve, cure, waiver, restructuring, or receiver waterfall. |
| Surviving terminal debt is sponsor-paid | The balance is an explicit modeled balloon, reported separately—not an assertion about real legal recourse. |
| Sponsor PV uses the project discount rate | It is a sponsor equity cash-flow diagnostic, not an equity valuation at a calibrated hurdle rate. |
| Physical paths are fixed across structures | The engine measures transfer and financing effects at fixed terms. It cannot show incremental debt capacity, lower spread, better tenor, earlier commissioning, or adoption. |
| Counterparties always perform | Buyer, hedge-provider, guarantor, collateral, close-out, and wrong-way credit risk are absent. |
| Project NPV continues after financing default | It is an unlevered asset view; post-default cash is not allocated through an insolvency estate. Stakeholder cash flows therefore do not consolidate after default. |

The full equations, sign conventions, default definition, incentive warnings,
and output definitions are in
[Annual Reference Engine v0.1](docs/ANNUAL_ENGINE_V0_1.md). The substantially
broader [monthly v0.2 target](docs/MODEL_SPECIFICATION.md) is an implementation
roadmap, not current behavior.

## Milestone-gated capital module v0.1: exact boundary

The staged-capital module is a finite-case contract-mechanics engine, not an
extension of the annual Monte Carlo model. Each case explicitly supplies phase
costs, cost-to-complete estimates, end-of-phase certification decisions,
provider-performance flags, completion cash proceeds or recovery, recovery delay, and
protected-workout need. It does not infer a certificate from the evidence gate,
elapsed time, capex progress, or public announcements.

At each phase start, sponsor and provider cash must fully fund that phase and
share-constrained deployable remaining sources must cover the declared
cost-to-complete estimate. Failure is atomic: the phase is not partly funded or
spent. During a funded phase, PIK accrues monthly subject to a claim cap and the
sponsor pays a monthly fee on modeled committed undrawn availability. Because
v0.1 permits one draw per phase, unused current-phase capacity cancels at that
draw date before fee accrual; final failed certification stops future draws.
On a failed path, the protected reserve funds the initial safe-workout need and
delayed recovery first cures any remaining workout shortfall. Only then does
recovery pay the modeled provider claim before sponsor residual. On completion,
the completion-proceeds waterfall pays the provider claim before sponsor residual
and releases the unused protected reserve separately.

The solved upfront fee is the negative weighted provider NPV before that fee,
using a paired all-provider-performs replay of every configured physical case
at its normalized relative weight and the declared provider hurdle. Declared
weights may differ from a sum of one only within `1e-12`; path records retain
them, while every summary, distribution, and fee calculation normalizes their
raw sum and reports it. The result is a physical-measure fee-adequacy
sensitivity—not fair
value, risk-neutral pricing, a market quote, or evidence of financeability. See
[Milestone-Gated Committed Capital Module v0.1](docs/MILESTONE_GATED_CAPITAL_V0_1.md).

## Build, test, and run

The canonical Linux environment is the persistent container managed by
`container.sh`. It is named and hostnamed
`naturalehia-fostering-cellular-agriculture`, uses the multi-architecture
`emscripten/emsdk:6.0.5` image pinned by digest, publishes no ports, requests
no GPU or other device, and has restart policy `no`. It bind-mounts only this
project at `/workspace/fostering-cellular-agriculture`; the developer home is
the guarded named volume
`naturalehia-fostering-cellular-agriculture-home-v1`.

The host needs Bash, a local Docker installation using the Linux engine, and
network access the first time the pinned image or missing white-paper packages
are installed. From Git Bash, WSL with Docker integration, or another Bash
host, create or resume the environment with:

```sh
bash container.sh up
bash container.sh shell
```

`setup.sh` is called inside the container by `container.sh`; it only installs
missing tool and white-paper packages and configures the developer identity and
home. It is not a lifecycle, build, test, run, or status interface. Existing
same-name objects with missing or mismatched ownership labels are rejected, as
are preserved legacy disposable recipe containers. `recreate` transactionally
replaces only the exact managed container and retains the named home volume.

Project operations remain ordinary CMake/CMakePresets and executable commands.
The development preset requires CMake 3.24 or newer and a C++20 compiler. Run
it through the host interface from this directory:

```sh
bash container.sh exec emcmake cmake --preset dev
bash container.sh exec cmake --build --preset dev
bash container.sh exec ctest --preset dev
```

Run the illustrative scenario on Windows with a multi-configuration generator:

```powershell
.\build\dev\Debug\naturalehia-cellular-finance.exe `
  .\scenarios\illustrative.cfg
```

On a single-configuration Linux or macOS build:

```sh
./build/dev/naturalehia-cellular-finance scenarios/illustrative.cfg
```

Evaluate the public reference dossier on Windows:

```powershell
.\build\dev\Debug\naturalehia-evidence-gate.exe `
  .\reference_projects\believer-wilson\dossier.cfg `
  .\reference_projects\believer-wilson\evidence_manifest.tsv
```

The expected exit code is `3`: the dossier is valid, but its evidence does not
pass the gates. Use `--report-only` only when a zero process status is needed
for an expected-failure regression run; it does not change the printed result.

Run the staged-capital synthetic financial-engineering case set on Windows:

```powershell
.\build\dev\Debug\naturalehia-staged-capital.exe `
  .\scenarios\milestone-gated-capital-synthetic.cfg
```

Add `--print-normalized` to emit the complete strict staged-capital input after
the report.

Run the explicit two-project participation-pool case set on Windows:

```powershell
.\build\dev\Debug\naturalehia-participation-pool.exe `
  .\scenarios\two-project-participation-pool-synthetic.cfg
```

Add `--print-normalized` to append the complete reloadable pool input after
the report.

Apply the synthetic physical-probability envelope to those fixed paths:

```powershell
.\build\dev\Debug\naturalehia-probability-envelope.exe `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-probability-envelope-synthetic.cfg
```

Add `--print-normalized` to append both complete reloadable inputs.

Intersect those component bounds with explicit marginal and shared-event
probability bounds:

```powershell
.\build\dev\Debug\naturalehia-probability-envelope.exe `
  --event-polytope `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-event-polytope-synthetic.cfg
```

This v0.2 mode reports audited floating-point fixed-path ranges, pool loss and
NPV-shortfall ES95/ES99, common-tail project contributions, and the full and
tail probability witnesses. Add `--print-normalized` to append both reloadable
semantic inputs.

Solve the synthetic commercial success-participation term against that
probability envelope:

```powershell
.\build\dev\Debug\naturalehia-success-participation.exe `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-probability-envelope-synthetic.cfg `
  .\scenarios\two-project-success-participation-synthetic.cfg
```

Add `--print-normalized` to append all three complete reloadable inputs. The
fixture intentionally fails the robust zero-NPV target at `q=1`; that is a valid
economic result, not a process failure.

Test a capped external share of final resolved pool loss and both sides of its
upfront premium:

```powershell
.\build\dev\Debug\naturalehia-pooled-loss-protection.exe `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-probability-envelope-synthetic.cfg `
  .\scenarios\two-project-success-participation-synthetic.cfg `
  .\scenarios\two-project-pooled-loss-protection-synthetic.cfg
```

Add `--print-normalized` to append all four reloadable inputs. The synthetic
result reaches investor break-even before premium at roughly one-sixth loss
coverage, but rejects a bilateral price because the provider's `0.80` claim-
only floor exceeds investor premium headroom by `0.80` DEMO million.

Add disclosed provider costs and target profit to that exact protection point:

```powershell
.\build\dev\Debug\naturalehia-provider-price-ladder.exe `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-probability-envelope-synthetic.cfg `
  .\scenarios\two-project-success-participation-synthetic.cfg `
  .\scenarios\two-project-pooled-loss-protection-synthetic.cfg `
  .\scenarios\two-project-provider-price-ladder-synthetic.cfg
```

Add `--print-normalized` to append all five reloadable inputs. The synthetic
robust provider floor rises from `0.80` claim-only to `1.278667` cost recovery
and `1.378667` including target profit. With a zero investor premium ceiling,
the entire `1.378667` is reported as provider premium support required; it is
not hidden in project probabilities or called diversification.

Stress collection when the protection provider can fail in the same states as
the financed projects:

```powershell
.\build\dev\Debug\naturalehia-provider-credit-stress.exe `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-probability-envelope-synthetic.cfg `
  .\scenarios\two-project-success-participation-synthetic.cfg `
  .\scenarios\two-project-pooled-loss-protection-synthetic.cfg `
  .\scenarios\two-project-provider-price-ladder-synthetic.cfg `
  .\scenarios\two-project-provider-credit-stress-synthetic.cfg
```

Add `--print-normalized` to append all six reloadable inputs. Fixed conditional
provider states make default more likely in high-claim project scenarios. The
stress preserves the contractual claim and the provider's full-performance
price, then separates direct payment, explicitly pledged collateral, unsecured
recovery, unpaid claim, and additional support. It is physical-scenario credit
analysis, not CVA, fair value, legal proof, or a rating.

Allocate the same fixed participation pool into fully funded at-par tranches:

```powershell
.\build\dev\Debug\naturalehia-capital-stack.exe `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-probability-envelope-synthetic.cfg `
  .\scenarios\two-project-success-participation-synthetic.cfg `
  .\scenarios\two-project-capital-stack-synthetic.cfg
```

Add `--print-normalized` to append all four reloadable inputs. The report keeps
the fixed underlying cash and loss, then shows how first-loss, intermediate,
and senior priority redistributes exposure and return without creating value.

Apply the explicit event candidate set to that same fixed-`q` waterfall:

```powershell
.\build\dev\Debug\naturalehia-capital-stack.exe `
  --event-polytope `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-event-polytope-synthetic.cfg `
  .\scenarios\two-project-success-participation-synthetic.cfg `
  .\scenarios\two-project-capital-stack-synthetic.cfg
```

This report re-projects every pool and tranche expectation, four loss or
shortfall tails, the robust NPV target, and principal weighted-average life
through one event-constrained probability set. Ratio endpoints retain one
common probability measure; they are not assembled from unrelated numerator
and denominator bounds.

Test a finite grid of success-cash participation and funded junior first loss
against one declared market-claim mandate:

```powershell
.\build\dev\Debug\naturalehia-capital-mobilization-frontier.exe `
  .\scenarios\capital-mobilization-frontier-v0.1-synthetic\portfolio.cfg `
  .\scenarios\capital-mobilization-frontier-v0.1-synthetic\event-polytope.cfg `
  .\scenarios\capital-mobilization-frontier-v0.1-synthetic\success-participation.cfg `
  .\scenarios\capital-mobilization-frontier-v0.1-synthetic\frontier.cfg
```

The synthetic report shows the analysis basis, all 20 tested pairs, total
market cash contributions and distributions, NPV and principal-risk metrics,
all twelve mandate decisions, feasible and nondominated sets, metric-specific
probability witnesses, and numerical controls. Add `--print-normalized` to
append all four reloadable inputs. No feasible point is a valid result with
exit code zero; it is not evidence of investor demand or capital mobilization.

Run the additive v0.2 frontier on the same ten-claim pool with an explicit
Capital Stack v0.2 template:

```sh
bash container.sh exec node \
  build-wasm-v02/naturalehia-capital-mobilization-frontier.js \
  scenarios/ten-claim-instrument-v1-synthetic/portfolio.cfg \
  scenarios/ten-claim-instrument-v1-synthetic/event-polytope-v0.2.cfg \
  scenarios/ten-claim-instrument-v1-synthetic/success-participation.cfg \
  scenarios/ten-claim-instrument-v1-synthetic/capital-stack-v0.2.cfg \
  scenarios/ten-claim-instrument-v1-synthetic/capital-mobilization-frontier-v0.2.cfg
```

The checked five-by-five grid evaluates 25 `(q,A)` terms. None passes every
declared synthetic mandate: feasible and nondominated feasible indices are
`none`, and minimum tested feasible `q` is `none`. The fixture separately
reports project-outlay limit 100, contractual asset-principal limit 100, and
funded reserve/issued principal `K=100`; their equality is fixture-specific.
`A` layers liability shortfall `Q`, not asset writeoff `L` or continuing asset
principal `O`. This is a finite-grid rejection, not a universal impossibility
claim. The same-pool scenario README gives the downstream `B=24` priority-cap
selection and conditional issue-price/support windows. That downstream record
uses a separate, relaxed sensitivity mandate; it does not reverse the strict
frontier's rejection of the same `q=1`, `A=20`, `M=80` point, and price or
support cannot cure its fixed `Q`-risk failure. No reported window is backed
by funded or escrowed support.

Hold one tested `q`-by-`A` structure fixed and test only the market claim's
lifetime non-principal priority cap:

```powershell
.\build\dev\Debug\naturalehia-market-priority-cap.exe `
  .\scenarios\market-priority-cap-v0.1-synthetic\portfolio.cfg `
  .\scenarios\market-priority-cap-v0.1-synthetic\event-polytope.cfg `
  .\scenarios\market-priority-cap-v0.1-synthetic\success-participation.cfg `
  .\scenarios\market-priority-cap-v0.1-synthetic\capital-stack.cfg `
  .\scenarios\market-priority-cap-v0.1-synthetic\priority-cap.cfg
```

The retained hand fixture reports `B=8/15` as the first tested market-adequate
and balanced cap; the immediately lower `B=0.50` leaves robust market NPV at
`-0.03`. Increasing `B` transfers dated non-principal cash from the junior
claim to the market claim without changing aggregate cash or principal risk.
Add `--print-normalized` to append all five reloadable inputs. A completed grid
with no adequate cap still exits zero; cross-input financial invalidity exits
three. Every result remains synthetic and unauthorized for execution.

Hold that selected claim fixed and test primary issue price against separately
supplied investor hurdles and explicit no-rights support:

```powershell
.\build\dev\Debug\naturalehia-issue-price-support.exe `
  .\scenarios\market-priority-cap-v0.1-synthetic\portfolio.cfg `
  .\scenarios\market-priority-cap-v0.1-synthetic\event-polytope.cfg `
  .\scenarios\market-priority-cap-v0.1-synthetic\success-participation.cfg `
  .\scenarios\market-priority-cap-v0.1-synthetic\capital-stack.cfg `
  .\scenarios\market-priority-cap-v0.1-synthetic\priority-cap.cfg `
  .\scenarios\market-priority-cap-v0.1-synthetic\issue-price.cfg
```

With market notional `M=8`, declared support capacity `G=1.5`, and reference
price `P=6.5`, the hand fixture has modeled conditional windows at supplied
0%, 5%, and 10% hurdles. At 10%, the robust buyer-price ceiling is
`6.663802` and minimum support capacity for any overlap is `1.336198`. At 15%,
the ceiling falls to `6.120378`, below the `6.5` issuer floor, leaving a
`0.379622` gap. These are synthetic physical-NPV boundaries, not prices or
market returns. The synthetic support record is neither committed nor funded,
so no modeled window is called capital-ready. Add `--print-normalized` for all
six reloadable inputs. Economic no-window results exit zero; invalid terms or
failed output exit three.

Check the synthetic one-project calibration package and print every bound
input and lineage row:

```powershell
.\build\dev\Debug\naturalehia-calibration-binder.exe `
  .\scenarios\calibration-binder-v0.1-synthetic\binder.cfg `
  --print-normalized
```

Exit code zero means the candidate package is structurally coherent. It never
means that its probabilities, expected returns, recovery, or price are
calibrated.

Construct a candidate physical-probability outer set directly from the bound
raw joint-outcome cohort, then project it through the fixed cash paths:

```powershell
.\build\dev\Debug\naturalehia-joint-cohort-envelope.exe `
  .\scenarios\joint-cohort-v0.1-synthetic\cohort.cfg `
  --print-normalized
```

Exit code `0` means the synthetic candidate envelope and its financial ranges
were computed coherently; it is never calibration or execution authorization.
Exit code `3` means a readable package failed the statistical or financial
export boundary, such as duplicated included cluster IDs or a declared central
probability outside the finite-sample outer set. `--print-normalized` also emits
the generated probability-envelope configuration. Its cohort, portfolio, and
ledger sections are semantic audit renderings, not a new hash-consistent
package: recompute the portfolio and ledger hashes before rebinding those
rendered files. The generated envelope is independently reloadable.

Apply that candidate set directly to the fixed-`q`, fully funded waterfall:

```powershell
.\build\dev\Debug\naturalehia-capital-stack.exe --joint-cohort `
  .\scenarios\joint-cohort-v0.1-synthetic\cohort.cfg `
  .\scenarios\joint-cohort-v0.1-synthetic\success-participation.cfg `
  .\scenarios\joint-cohort-v0.1-synthetic\capital-stack.cfg `
  --print-normalized
```

The report exposes the cohort and hash boundary, project risk at the selected
participation fraction, common-witness tail attribution, the underlying target
gap, and tranche results. A statistical or reference block exits `3` without a
stack. The term files are strict inputs outside the cohort hash binding, and
every path states `calibrated_execution_authorized=false`.

Annual Monte Carlo executable options (not accepted by the term CLIs):

```text
--trials N      override the declared Monte Carlo trial count
--seed N        override the declared seed
--print-config  emit the complete normalized input set before results
--help          show usage and the model boundary
```

Every distributed result should retain the model version, original scenario,
normalized configuration, compiler/build record, trial count, and seed.

## How to read the output

The unsupported and structured cases share every random path. The paired
project-NPV change therefore reconciles to signed modeled contract transfers.
The report separates:

- physical-offtake repricing relative to simulated spot sales;
- net price-support settlement;
- completion-delay cover payout;
- upfront fee;
- gross positive support payout, excluding offtake repricing; and
- signed net instrument transfers after a prior-period default, plus effects on
  sponsor cash flow, default, and lender loss.

A positive mean transfer is not newly created social value. Expected payout is
not a market price; real pricing would also require provider funding cost,
capital, expenses, liquidity, collateral, legal characterization, counterparty
credit, model uncertainty, and return.

A structure can raise mean project NPV while moving more marginal paths below
zero: the fee applies in every path, fixed offtake removes high-price upside,
and support may concentrate in projects that remain deeply uneconomic. It can
also reduce lender loss while lowering sponsor PV because senior debt receives
cash first and avoiding default may preserve later sponsor obligations. Raw
paired transition counts are reported so a handful of paths cannot hide behind
a rounded probability.

The zero-factor, probability-weighted steady-state checkpoint is intentionally
simple. If base CFADS remains negative before debt service, financial
engineering has not repaired the operating business; the scenario needs
technical redesign, lower cost, higher realized revenue, or explicit
concessionary support.

## Research and evidence gate

Before a real calibration, the project needs at least:

- a named product, process, facility, operator, buyer, and jurisdiction;
- complete run-level records, including aborted, contaminated,
  off-specification, and maintenance-affected runs;
- reconciled mass, energy, input, labor, waste, quality, and financial records;
- an engineering scope, vendor quotations, schedule, interfaces, contingency,
  commissioning plan, and independent review;
- product- and facility-specific regulatory records and open conditions;
- executed buyer obligations, acceptance history, and counterparty credit—not
  surveys, announcements, or non-binding interest treated as revenue;
- proposed capital terms, sources and uses, reserves, covenants, security,
  recovery, tax, accounting, and legal analysis; and
- a conservative, independently reviewable financing-additionality and
  animal-displacement method.

Seeded public sources are provisional research records, not calibrated model
inputs. Gate use requires a controlled retained copy and matching SHA-256,
document version, exact extract, current review, named verifier and approver,
stated procedures, cleared or managed conflicts, and the compiled source and
applicability minimum.

That rule is now executable. The
[Reference-Project Evidence Gate](docs/REFERENCE_PROJECT_GATE.md) separately
controls: (1) reference boundary, (2) model calibration, (3) cross-structure
diligence checklist, and (4) animal-impact claims. There is no blended readiness
score. A supporting record must be exact-project, current, retained, reviewed
to the compiled minimum, and authorized for gate use; an unresolved contrary
record blocks the affected requirement.

The [Public Calibration Evidence Snapshot v0.1](docs/PUBLIC_CALIBRATION_EVIDENCE_V0_1.md)
now maps current regulator, audited-account, official-financing, issuer, and
technical evidence to candidate-state and data-acquisition questions—and to
the inferences it cannot support. It can define real candidate events, but it
does not supply a pooled probability, recovery distribution, expected return,
or value. The
[Calibration Binder v0.1](docs/CALIBRATION_BINDER_V0_1.md) is the implemented
machine boundary: hash-bind the exact scenario files and map every
material input to its declared source status, method label, limitation, and
retirement fact while keeping execution explicitly candidate-only.

## Candidate mechanisms by financing failure

| Financing failure | Mechanisms worth testing | Central questions |
|---|---|---|
| Technical evidence is too early for infrastructure capital | Research grants, milestone finance, strategic equity | What evidence is purchased at each milestone, and who verifies it? |
| Construction cost or completion risk exceeds lender capacity | Sponsor contingency, EPC/vendor support, overrun facility, completion support | Who controls the risk, what is the deductible/cap, and what conduct is excluded? |
| Credible output lacks bankable demand | Binding offtake, advance purchase, capacity reservation, public procurement | Is the buyer obligated, creditworthy, and receiving real commercial value? |
| Realized price is too uncertain | Fixed-price offtake, one- or two-way price support | What output is eligible, who bears upside/downside, and how are basis and caps handled? |
| Input exposure can be hedged in established markets | Energy or commodity hedges, indexed supply, vendor finance | Does the hedge match quantity, timing, location, quality, credit, and liquidity? |
| A narrow tail risk blocks otherwise sound debt | Reserve, liquidity facility, partial guarantee, subordinated capital | Does it cure a defined bottleneck without concealing subsidy or weakening discipline? |
| Replication requires more capital after evidence is mature | Project bonds, warehouse facilities, pooled vehicles | Are assets truly comparable and diversified after common technology, supplier, buyer, and regulatory shocks? |

These are economic descriptions, not legal labels. A real arrangement could be
a loan, security, derivative, commodity interest, insurance contract, supply
agreement, public liability, or another regulated product. Qualified legal,
regulatory, tax, accounting, prudential, and insolvency review is required in
each relevant jurisdiction before solicitation or execution.

## Documentation map

| Document | Role |
|---|---|
| [Technical White Paper](whitepaper/fostering-cellular-agriculture-white-paper.pdf) | Article-format statement of the instrument, financial mechanics, risk and return results, investability gap, limitations, and research path |
| [The Financial Instrument in Brief](docs/FINANCIAL_INSTRUMENT_IN_BRIEF.md) | Short front-door explanation of the core asset, two alternatives, ten-claim result, and honest rejection boundary |
| [Multi-Project Milestone Participation Candidate Term Sheet](docs/MULTI_PROJECT_MILESTONE_PARTICIPATION_TERM_SHEET_V1.md) | Institution-readable property right, funding, cash, loss, waterfall, disclosure, and falsification terms |
| [Ten-Claim Instrument Analysis](docs/TEN_CLAIM_INSTRUMENT_ANALYSIS_V1.md) | Exactly ten claims, nine dependent states, unsupported/first-loss/guarantee comparison, provider economics, and adverse witnesses |
| [Financial Architecture research note](docs/FINANCIAL_ARCHITECTURE.md) | Maintained design note behind the public white paper: aim, common interface, instrument family, value thesis, and honest boundary |
| [Project Financial Interface v0.1](docs/PROJECT_FINANCIAL_INTERFACE_V0_1.md) | Common capital, state, payoff, recovery, factor, metric, and pooling conventions |
| [Portfolio Explicit Contractual-Principal Ledger v0.2](docs/PORTFOLIO_EXPLICIT_PRINCIPAL_LEDGER_V0_2.md) | Separate investor-cash and contractual-principal accounting, above-par behavior, strict format, and v0.1 compatibility |
| [Project Claim Ledger v0.1](docs/PROJECT_CLAIM_LEDGER_V0_1.md) | Upstream one-claim settlement, balance, cash, conversion, guarantee, covenant, and ex-ante/backtest normalization standard |
| [Project Claim Ledger v0.1 Verification](docs/PROJECT_CLAIM_LEDGER_VERIFICATION_V0_1.md) | Hand reconstruction, strict Debug/Release record, accounting and parser controls, canonical fixture results, and residual limitations |
| [Claim-Ledger Portfolio Adapter v0.2](docs/CLAIM_LEDGER_PORTFOLIO_ADAPTER_V0_2.md) | Verified package admission, decision-cut conversion, external source budgets, independent cash/principal reconciliation, and output lineage |
| [Claim-Ledger Joint-Portfolio Assembler v0.1](docs/CLAIM_LEDGER_JOINT_PORTFOLIO_ASSEMBLER_V0_1.md) | Verified multi-claim assembly, explicit dependence, common-calendar and unique-right gates, marginal conservation, and expanded package/cash lineage |
| [Claim-Ledger Joint-Portfolio Assembler v0.1 Verification](docs/CLAIM_LEDGER_JOINT_PORTFOLIO_VERIFICATION_V0_1.md) | Debug/Release record, hand-reconciled dependence example, rare-state and calendar controls, deep conservation checks, resource guards, review corrections, and residual evidence boundary |
| [Partial-Credit Claim-Loss Cohort Binder v0.1](docs/PARTIAL_CREDIT_CLAIM_LOSS_COHORT_BINDER_V0_1.md) | Strict five-file evidence-census package loader plus a mechanical evaluator that consumes Claim Ledger's authoritative selected-path provenance while preserving a hard no-empirical/no-calibration boundary |
| [Partial-Credit Claim-Loss Cohort Binder v0.1 Verification](docs/PARTIAL_CREDIT_CLAIM_LOSS_COHORT_VERIFICATION_V0_1.md) | Exact file binding, population-frame Evidence Gate, selected-path provenance, hand-reconstructed frame, face-versus-principal oracle, open-case ranges, adversarial controls, and residual empirical-admission boundary |
| [Financial Instrument Family v0.1](docs/INSTRUMENT_FAMILY_V0_1.md) | Stage-specific claims, draw and payoff rules, real return sources, loss allocation, metrics, and falsification tests |
| [Participation Pool Engine v0.1](docs/PARTICIPATION_POOL_ENGINE_V0_1.md) | Implemented explicit-joint-scenario kernel, cash-source invariants, loss and NPV outputs, diversification tests, layers, and current boundary |
| [Portfolio Scenario Format v0.1](docs/PORTFOLIO_SCENARIO_FORMAT_V0_1.md) | Implemented strict reloadable input schema, source-budget semantics, and hand-calculated two-project fixture |
| [Participation-Pool v0.1 Verification](docs/PARTICIPATION_POOL_VERIFICATION_V0_1.md) | Debug/Release environment, parser and kernel controls, hand-checked CLI results, correction history, and residual boundary |
| [Physical-Probability Envelope Engine v0.1](docs/PROBABILITY_ENVELOPE_ENGINE_V0_1.md) | Exact bounded-probability optimization, strict companion format, endpoint witnesses, hand-calculated fixture, and interpretation boundary |
| [Physical-Probability Envelope v0.1 Verification](docs/PROBABILITY_ENVELOPE_VERIFICATION_V0_1.md) | Debug/Release environment, core/parser/CLI controls, hand calculations, correction history, and residual limitations |
| [Event-Probability Polytope v0.2](docs/EVENT_PROBABILITY_POLYTOPE_V0_2.md) | Explicit marginal and shared-event constraints, audited floating-point expectation and ES projection, common-tail attribution, strict candidate-set format, and capital-stack routing |
| [Event-Probability Polytope v0.2 Verification](docs/EVENT_PROBABILITY_POLYTOPE_VERIFICATION_V0_2.md) | Strict build record, independent expectation and ES checks, hand-reconciled pool and tranche results, parser and CLI controls, and residual limitations |
| [Robust Success-Participation Term v0.1](docs/SUCCESS_PARTICIPATION_TERM_V0_1.md) | Cash-conserving payoff equation, robust threshold solver, strict term format, infeasible synthetic result, and research implications |
| [Robust Success-Participation v0.1 Verification](docs/SUCCESS_PARTICIPATION_VERIFICATION_V0_1.md) | Debug/Release record, hand reconciliation, parser and solver controls, witness-switch test, correction history, and residual limitations |
| [Pooled Principal-Loss Protection Term v0.1](docs/POOLED_LOSS_PROTECTION_TERM_V0_1.md) | External proportional loss-share equation, legal cap, robust coverage solver, provider exposure, and two-sided premium feasibility test |
| [Failure-Contingent Public Partial-Credit Guarantee v0.1](docs/FAILURE_CONTINGENT_PUBLIC_PARTIAL_CREDIT_GUARANTEE_V0_1.md) | First concrete synthetic instrument: one-sixth public principal-loss share, explicit subsidy result, provider exposure, and real-contract evidence boundary |
| [Pooled Principal-Loss Protection v0.1 Verification](docs/POOLED_LOSS_PROTECTION_VERIFICATION_V0_1.md) | Debug/Release record, hand reconciliation, parser, solver, tail-risk and CLI controls, and residual provider-model boundary |
| [Provider Price-Ladder Sensitivity v0.1](docs/PROVIDER_PRICE_LADDER_V0_1.md) | Implemented claim, collateral-carry, economic-capital-charge, expense, profit, investor-ceiling, and catalytic-gap equations and accounting boundary |
| [Provider Price-Ladder v0.1 Verification](docs/PROVIDER_PRICE_LADDER_VERIFICATION_V0_1.md) | Debug/Release record, hand reconciliation, strict parser and CLI controls, independent math audit, and correction history |
| [Provider Counterparty-Credit Stress v0.1](docs/PROVIDER_CREDIT_STRESS_V0_1.md) | Implemented settlement-default waterfall, fixed conditional provider states, wrong-way dependence, credit-loss tails, claim delivery, and unchanged-price boundary |
| [Provider Counterparty-Credit Stress v0.1 Verification](docs/PROVIDER_CREDIT_STRESS_VERIFICATION_V0_1.md) | Debug/Release record, hand reconciliation, strict sixth input and CLI controls, independent audit, and residual boundary |
| [Fully Funded Capital Stack v0.1](docs/CAPITAL_STACK_TERM_V0_1.md) | Implemented at-par reserve, principal and non-principal waterfalls, loss/exposure allocation, robust tranche risk/return, common-witness WAL, and strict term format |
| [Fully Funded Capital Stack v0.1 Verification](docs/CAPITAL_STACK_VERIFICATION_V0_1.md) | Debug/Release record, hand table, staged-reserve and continuing-exposure tests, strict parser/CLI controls, and residual boundary |
| [Capital Stack Asset-to-Liability Bridge v0.2](docs/CAPITAL_STACK_ASSET_LIABILITY_BRIDGE_V0_2.md) | Per-project-maximum reserve funding, exact asset `L/O` and liability `Q` separation, `layer(Q)` tranche shortfalls, simultaneous-source pro-rata memos, and the direct Claim Ledger acceptance contract |
| [Capital Stack Asset-to-Liability Bridge v0.2 Verification](docs/CAPITAL_STACK_ASSET_LIABILITY_BRIDGE_VERIFICATION_V0_2.md) | Exact 8/10 and 12/10 `L/O/Q` hand tables, staggered-use and simultaneous-surplus checks, high-scale minimum-tranche boundary, event-polytope coverage, and focused WebAssembly results |
| [Callable-Capital and Warehouse Funding Integrity Bridge v0.1](docs/CALLABLE_CAPITAL_WAREHOUSE_FUNDING_BRIDGE_V0_1.md) | Executable synthetic test of delayed permanent capital and temporary warehouse debt, with disjoint cash buckets, funded-protection custody, provider-performance states, source-specific failures, maturity EAD, and concentration/NPV boundaries |
| [Robust Capital-Mobilization Frontier v0.1](docs/ROBUST_CAPITAL_MOBILIZATION_FRONTIER_V0_1.md) | Implemented finite `q`-by-`A` market-claim feasibility frontier, two funded claims, twelve optional mandates, endpoint witnesses, Pareto reporting, and institutional boundaries |
| [Robust Capital-Mobilization Frontier v0.1 Verification](docs/ROBUST_CAPITAL_MOBILIZATION_FRONTIER_VERIFICATION_V0_1.md) | Hand reconciliation, strict Debug/Release record, parser and CLI controls, independent 228-candidate oracle, exposure correction, resource guard, and residual limitations |
| [Robust Capital-Mobilization Frontier v0.2](docs/ROBUST_CAPITAL_MOBILIZATION_FRONTIER_V0_2.md) | Additive Capital Stack v0.2 five-input frontier, strict `Q` risk family, separate outlay/asset-principal/reserve ledgers, 25-candidate same-pool rejection, and conditional issue-price/support handoff |
| [Robust Capital-Mobilization Frontier v0.2 Verification](docs/ROBUST_CAPITAL_MOBILIZATION_FRONTIER_VERIFICATION_V0_2.md) | `L`/`Q` divergence oracles, version and parser closure, Emscripten Release/Node CLI regression, ten-claim empty feasible set, and explicit non-claims |
| [Robust Market Non-Principal Priority-Cap Term v0.1](docs/ROBUST_MARKET_PRIORITY_CAP_TERM_V0_1.md) | Implemented fixed-`q`, fixed-`A` finite lifetime-cap adequacy grid, junior-transfer account, exact `8/15` hand boundary, resource guard, and pricing limitations |
| [Robust Market Non-Principal Priority-Cap v0.1 Verification](docs/ROBUST_MARKET_PRIORITY_CAP_VERIFICATION_V0_1.md) | Hand reconciliation, strict Debug/Release record, parser and CLI controls, cash-transfer audit correction, frontier cross-check, resource guard, and residual limitations |
| [Robust Issue-Price Support Term v0.1](docs/ROBUST_ISSUE_PRICE_SUPPORT_TERM_V0_1.md) | Implemented fixed-claim buyer-price ceiling, issuer floor, no-rights support gap, non-circular hurdle provenance, evidence hierarchy, exact hand fixture, and resource boundary |
| [Robust Issue-Price Support v0.1 Verification](docs/ROBUST_ISSUE_PRICE_SUPPORT_VERIFICATION_V0_1.md) | Hand and downside oracles, strict Debug/Release core/parser/CLI record, normalized six-file replay, independent math audit, evidence-boundary corrections, and residual limitations |
| [Market Observation and Hurdle Evidence Set v0.1](docs/MARKET_OBSERVATION_AND_HURDLE_EVIDENCE_V0_1.md) | Expected-cash price reconstruction, bounded comparable bridges, non-circular sparse-observation set identification, preserved disagreement gaps, and hard market-evidence boundary |
| [Market Observation and Hurdle Evidence Set v0.1 Verification](docs/MARKET_OBSERVATION_AND_HURDLE_EVIDENCE_VERIFICATION_V0_1.md) | Exact disjoint-set oracle, strict Debug/Release core/parser/CLI record, canonical replay, independent hierarchy and set-math audits, correction history, and residual empirical boundary |
| [Real-Transaction Acquisition Ladder v0.1](reference_transactions/README.md) | First three controlled-data targets: a private facility converter, an amortizing guaranteed facility, and a failure-contingent public credit |
| [Liberation Labs Public Transaction Dossier](reference_transactions/liberation-labs-facility-financing-2024-2025/PUBLIC_TRANSACTION_DOSSIER.md) | First retained real-note package, April–October cluster ambiguity, exact missing-rights decision, source hashes, and refusal to treat promised 10% interest as an expected-cash hurdle |
| [Solar Foods Factory 01 Public Transaction Dossier](reference_transactions/solar-foods-factory-01-facility-2022-2025/PUBLIC_TRANSACTION_DOSSIER.md) | Retained amortizing-facility reconstruction, audited principal-cash reconciliation, named guarantors and unresolved guarantee/fee cash, and refusal to treat floating margins as expected return |
| [Meatable Dutch Innovation Credit Public Transaction Dossier](reference_transactions/meatable-dutch-innovation-credit-2024/PUBLIC_TRANSACTION_DOSSIER.md) | Public-incomplete award package separating the issuer-reported EUR 7.6 million amount from generic programme rates, templates and discretionary remission; no award amount is treated as cash or a market hurdle |
| [Financial-Engineering Precedents v0.1](docs/FINANCIAL_ENGINEERING_PRECEDENTS_V0_1.md) | Evidence/inference map from biomedical RBOs, global health, blended finance, guarantees, dynamic leverage, and current cellular-agriculture financing |
| [Portfolio Calibration and Probability-Uncertainty Standard v0.1](docs/PORTFOLIO_CALIBRATION_STANDARD_V0_1.md) | Evidence requirements for state, marginal, payoff, recovery, timing, dependence, physical probabilities, sparse data, and validation |
| [Joint-Cohort Probability Envelope v0.1](docs/JOINT_COHORT_PROBABILITY_ENVELOPE_V0_1.md) | Candidate-only raw cohort ledger, conservative nonasymptotic simultaneous bounds, declared-reference checks, fixed-path investor-risk projection, and the direct-joint-state scaling boundary |
| [Joint-Cohort Probability Envelope v0.1 Verification](docs/JOINT_COHORT_PROBABILITY_ENVELOPE_VERIFICATION_V0_1.md) | Strict Debug/Release record, hand-reconciled fixture, exact-byte/parser/CLI controls, audit corrections, and residual limitations |
| [Public Calibration Evidence Snapshot v0.1](docs/PUBLIC_CALIBRATION_EVIDENCE_V0_1.md) | Current primary-source technical and financial facts, candidate-state and acquisition uses, prohibited inferences, and remaining empirical gaps |
| [Calibration Binder v0.1](docs/CALIBRATION_BINDER_V0_1.md) | Implemented one-project, candidate-only evidence-to-input lineage, artifact hashes, exact coverage rules, and hard non-synthetic boundary |
| [Calibration Binder v0.1 Verification](docs/CALIBRATION_BINDER_VERIFICATION_V0_1.md) | Debug/Release record, identity and monetary-scope controls, adversarial lineage/citation tests, audit corrections, and residual limitations |
| [Staged-Capital Portfolio Adapter v0.1](docs/STAGED_CAPITAL_PORTFOLIO_ADAPTER_V0_1.md) | Implemented reconciling bridge from actual milestone-facility provider paths into the common pool interface |
| [Multi-Project Risk Engine Integration Target](docs/MULTI_PROJECT_RISK_ENGINE_V0_1.md) | Extended adapter, calibration, reporting, and verification roadmap beyond the current API |
| [Responsible Finance Charter](docs/RESPONSIBLE_FINANCE_CHARTER.md) | Mission, additionality, subsidy, safety, evidence, conflicts, and go/no-go commitments |
| [Research Agenda](docs/RESEARCH_AGENDA.md) | Decisions, workstreams, interview sequence, and staged evidence acquisition |
| [Evidence Register](docs/EVIDENCE_REGISTER.md) | Source taxonomy, admitted public evidence, limitations, and open gaps |
| [Underwriting Data Standard](docs/UNDERWRITING_DATA_STANDARD.md) | Required technical, construction, commercial, financial, regulatory, impact, and model data |
| [Financing-Failure Map](docs/FINANCING_FAILURE_MAP.md) | Facility lifecycle, risk allocation, cash-flow tests, and go/no-go diagnosis |
| [Instrument Taxonomy](docs/INSTRUMENT_TAXONOMY.md) | Institutional classification and least-complex-effective-instrument test |
| [Candidate Structures](docs/CANDIDATE_STRUCTURES.md) | Four provisional research term sheets tied to diagnosed financing failures |
| [Reference-Project Selection](docs/REFERENCE_PROJECT_SELECTION.md) | Candidate screen, selection rationale, source boundaries, and comparator roles |
| [Reference-Project Evidence Gate](docs/REFERENCE_PROJECT_GATE.md) | Executable reference-project schema 0.2, 57 project requirements, pass/fail logic, and interpretation boundary |
| [Evidence Gate Verification](docs/EVIDENCE_GATE_VERIFICATION_V0_2.md) | Reference-project gates plus the isolated claim-population schema 0.3/profile, immutable-byte parsing, adversarial regressions, and residual limitations |
| [Believer Wilson Public Dossier](reference_projects/believer-wilson/PUBLIC_EVIDENCE_DOSSIER.md) | Current public facts, claims, conflicts, and machine-gate result |
| [Believer Wilson Gap Register](reference_projects/believer-wilson/GAP_REGISTER.md) | Prioritized missing evidence and decisions blocked |
| [Believer Wilson Controlled Data Request](reference_projects/believer-wilson/CONTROLLED_DATA_REQUEST.md) | Phased data-room request mapped to every evidence gate |
| [Liberation Labs Note Controlled Data Request](reference_transactions/liberation-labs-facility-financing-2024-2025/CONTROLLED_DATA_REQUEST.md) | Focused request for the exact claim, settlement, conversion, security, recovery, and expected-cash records needed for hurdle eligibility |
| [Solar Foods Factory 01 Controlled Data Request](reference_transactions/solar-foods-factory-01-facility-2022-2025/CONTROLLED_DATA_REQUEST.md) | Focused request for the executed loan, draw and fee cash, guarantees, claim-level settlement, recovery, and expected-cash records needed for hurdle eligibility |
| [Annual Engine v0.1](docs/ANNUAL_ENGINE_V0_1.md) | Implemented equations, conventions, outputs, and limitations |
| [Internal Verification Record](docs/VERIFICATION_REPORT_V0_1.md) | Tested environment, invariants, scenario checks, and unresolved validation gaps |
| [Milestone-Gated Capital v0.1](docs/MILESTONE_GATED_CAPITAL_V0_1.md) | Implemented delayed-draw state machine, ledgers, waterfall, fee sensitivity, tests, and exclusions |
| [Staged Capital v0.1 Verification](docs/STAGED_CAPITAL_VERIFICATION_V0_1.md) | Debug/Release environment, mechanic and adversarial controls, checked fixture results, audits, and residual limitations |
| [Monthly Model Target](docs/MODEL_SPECIFICATION.md) | Provisional v0.2 equations, waterfall, validation invariants, and implementation roadmap |

## License

This project is covered by the repository's [MIT License](../../LICENSE).
