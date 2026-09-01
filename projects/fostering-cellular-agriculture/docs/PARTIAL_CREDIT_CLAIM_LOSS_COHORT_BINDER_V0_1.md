# Partial-Credit Claim-Loss Cohort Binder v0.1

Status: implemented five-file candidate loader plus mechanical-evaluation kernel

Purpose: target empirical loss identification; current authority remains mechanical only

Execution authority: none (`candidate_only=true`, `calibrated_execution_authorized=false`)

The current C++ kernel re-verifies supplied Claim Ledger roots, preserves every
row in the caller-declared denominator, extracts and reconciles synthetic
resolved-path mechanics, and forms coarse mechanical outer envelopes for open
cases. The five-file loader now confines and hash-binds the declared files,
parses their closed v0.1 schemas, batch-runs Evidence Gate's compiled
`FIN-CLAIM-POPULATION-FRAME` profile, and reuses the Claim Ledger loader for
every included row. It does not prove that the declared rows are a complete
population or that classifications, methods, or realized cash are true. Claim
Ledger's one-scenario selected-full-path
evidence snapshot is implemented and consumed for resolved rows: it retains the
selected latest entries and their common/scenario scope, input status, source,
source date, retained-copy state, requested cash-path status, provider terms,
and applicable covenants. That provenance seam is necessary but does not by
itself admit a row as empirical.
Accordingly, the kernel always reports
`empirical_realized_cash_admissible=false` and never authorizes calibration or
Portfolio export. This is an explicit implementation boundary, not a temporary
assumption that caller metadata is trustworthy.

Concretely, callers may construct an in-memory cohort or load the exact
five-file package through
`load_partial_credit_claim_loss_cohort_package(root)`. The loader rejects
structural contradictions, returns the parsed method and evidence records,
and exposes separate five-file-integrity, population-frame-gate, and candidate
validity flags. It deliberately has no positive empirical status/date/source
admission policy: `empirical_realized_cash_admissible` remains false even when
the structural candidate and population-frame conjunction pass. The result
records selected-path verification but does not serialize the full provenance
snapshot or method/evidence lineage.

## 1. Purpose and boundary

The completed target binder is intended to turn a complete, controlled population of partial-credit claim histories into an auditable empirical loss cohort. It would be the bridge between verified single-claim Claim Ledger packages and later probability, dependence, pricing, or instrument work.

The target binder answers a limited financial question: **what loss, claim, payout, and residual-writeoff experience is supported by an evidence-admitted cohort, and what remains unidentified because cases are open or immature?**

It does not estimate transferable default probabilities, loss-given-default, correlation, diversification, tail loss, fair premium, fair value, rating, capital, subsidy need, or investor return. Separate claim histories are not synchronized joint-pool outcomes. The binder therefore cannot authorize portfolio calibration or instrument replay.

The three economic quantities below MUST remain separate throughout:

1. pre-support principal shortfall after borrower cash and asset recovery;
2. provider claim generated, payable, paid, and still unpaid;
3. final contractual principal writeoff after support and recoveries.

The Claim Ledger remains the cash authority. This binder MUST NOT create a second event or cash ledger.

## 2. Target loader/evaluator admission contract

Sections 2 through 5 specify the intended empirical-admission boundary. They
are not a claim that caller-constructed in-memory input performs these steps.
The current file loader distinguishes structural validity from empirical
admission: schema, confinement, hash, identity, and mechanical contradictions
fail closed; a population or citation gate failure may return a structurally
inspectable package with `candidate_package_valid=false` and explicit blockers.
Mechanical diagnostics may still be evaluated in that state, but they are not
empirical cohort outputs and cannot authorize calibration, pricing, or
Portfolio use. Admission under the completed empirical contract requires every
rule below to pass.

1. **Candidate status.** `candidate_only` MUST equal `true`. `calibrated_execution_authorized` is derived as `false` and MUST NOT be supplied by the caller.
2. **Frozen population frame.** The package binds an as-of date, eligibility window, population definition, sampling unit, economic-cluster rule, protection-term stratum, outcome horizon, loss sequence, resolution rule, censoring rule, denominator rule, currency, and monetary basis.
3. **Enumerated census.** Every member of the controlled population frame appears exactly once in `observations.tsv`, including zero-loss, denied or disputed, cancelled or zero-draw if eligible, immature, unresolved, and excluded members. `population_frame_count` MUST equal the row count.
4. **One economic risk unit per row.** Amendments, draws, lenders, filings, transfers, or repeated measurements MUST NOT create independent observations. Repeated members of one `economic_cluster_id` require the bound consolidation method. Without a valid consolidation, only cash totals may be reported and frequency or severity outputs are blocked.
5. **Outcome-blind exclusions.** Every exclusion uses a named rule frozen no later than the member's eligibility date. The row remains in the frame, cites that rule, and does not disappear from reporting.
6. **Comparable claims.** One binder contains one currency and monetary basis and one materially comparable legal protection term/loss sequence. Heterogeneous terms require separate binders or separately validated strata; silent pooling is prohibited.
7. **Verified claim roots.** Every non-excluded row binds a Claim Ledger package by path and SHA-256 root. The loader MUST re-run Claim Ledger verification and MUST ignore caller-supplied summaries.
8. **Identity match.** Observation, economic-cluster, provider-claim, currency/basis, and selected scenario identities MUST agree with the verified package and bound methods.
9. **One realized path.** A resolved row selects exactly one full/backtest scenario. Its terminal balances MUST close, its provider path MUST be computable, and all facts used as realized outcomes MUST be observed, contractual, or derived by the as-of date. Stress, hypothesis, estimate, or synthetic cash MUST NOT enter a real cohort metric.
10. **No imported model authority.** Claim Ledger probabilities, discount rates, expected returns, and any expected-return admission flags MUST be ignored.
11. **Open cases remain unknown.** Not-yet-matured and unresolved members remain in count denominators and compatible amount ranges. A missing terminal amount MUST NOT be converted to zero.
12. **Evidence admission.** Hashes prove byte identity, not truth or completeness. Population-frame evidence, classification evidence, and method evidence MUST pass the Evidence Gate bound by this package.
13. **No downstream authorization.** v0.1 MUST NOT emit a portfolio configuration, ambiguity configuration, price, rating input, or calibration authorization. Use in pricing requires a separately approved transfer model, dependence evidence, and instrument replay boundary.

## 3. Minimal package

The package contains exactly five binder-level files plus the referenced Claim Ledger packages:

```text
cohort.cfg
observations.tsv
methods.cfg
dossier.cfg
evidence_manifest.tsv
```

All files MUST use UTF-8, LF line endings, closed schemas, case-sensitive identifiers, and no duplicate keys. Dates use `YYYY-MM-DD`. Money uses finite decimal strings in the bound common currency and basis; binary floating-point is not an admissible source representation. Paths are confined, package-relative paths. SHA-256 values are lowercase 64-character hexadecimal strings.

### 3.1 `cohort.cfg`

Required scalar keys:

```text
schema_version=partial-credit-claim-loss-cohort-binder-v0.1
cohort_id=<stable-id>
as_of_date=<YYYY-MM-DD>
frame_start_date=<YYYY-MM-DD>
frame_end_date=<YYYY-MM-DD>
population_definition=<method-id>
source_note=<bounded plain-text scope note>
sampling_unit_definition=<method-id>
economic_cluster_definition=<method-id>
protection_term_stratum_definition=<method-id>
outcome_horizon_definition=<method-id>
loss_definition=<method-id>
resolution_definition=<method-id>
censoring_definition=<method-id>
denominator_definition=<method-id>
currency_label=<ISO-4217-or-explicit-unit>
monetary_basis=<exact label matched to every Claim Ledger package>
monetary_basis_definition=<method-id>
population_frame_count=<non-negative-integer>
candidate_only=true
observations_path=observations.tsv
observations_sha256=<sha256>
methods_path=methods.cfg
methods_sha256=<sha256>
dossier_path=dossier.cfg
dossier_sha256=<sha256>
evidence_manifest_path=evidence_manifest.tsv
evidence_manifest_sha256=<sha256>
```

Exclusion rules are repeated closed records:

```text
exclusion_rule.<id>.frozen_date=<YYYY-MM-DD>
exclusion_rule.<id>.outcome_blind_asserted=true
exclusion_rule.<id>.statement=<plain-language rule>
exclusion_rule.<id>.evidence_record_ids=<comma-separated ids>
```

`frame_start_date <= frame_end_date <= as_of_date`. All referenced method and evidence IDs MUST exist. Unknown keys are rejected.

### 3.2 `observations.tsv`

The exact header is:

```text
observation_id\teconomic_cluster_id\teligible_date\thorizon_end_date\tdisposition\ttrigger_status\ttrigger_date\tclassification_date\tresolution_date\texclusion_rule_id\tclaim_cfg_path\tclaim_config_sha256\trealized_scenario_id\tprovider_claim_id\tpopulation_evidence_record_ids\tpopulation_requirement_ids\tclassification_evidence_record_ids\tclassification_requirement_ids
```

Field rules:

- `observation_id` is unique and identifies one economically distinct protected claim/risk unit.
- `economic_cluster_id` is never empty. Cluster repetition invokes the bound consolidation method.
- `eligible_date` lies inside the eligibility window. `horizon_end_date` is derived under the bound horizon method and cannot precede eligibility.
- `disposition` is exactly one of `resolved`, `not-yet-matured`, `unresolved`, or `excluded`.
- `trigger_status` is exactly one of `triggered`, `not-triggered`, `unknown`, or `not-applicable`.
- Unknown or inapplicable dates and IDs use the literal `NONE`; empty fields are rejected.
- `trigger_date`, when present, cannot follow `as_of_date`. `classification_date` and `resolution_date`, when present, cannot follow `as_of_date`.
- `excluded` requires an `exclusion_rule_id` and uses `NONE` for claim/scenario/provider fields unless the exclusion method expressly requires a verification package. Non-excluded rows use `NONE` for `exclusion_rule_id` and MUST bind a claim package.
- `resolved` requires `classification_date`, `resolution_date`, and `realized_scenario_id`. Other dispositions use `NONE` for `resolution_date` and `realized_scenario_id`.
- `not-yet-matured` requires `as_of_date < horizon_end_date`. `unresolved` requires `as_of_date >= horizon_end_date` or a triggered/open case under the bound resolution method.
- `provider_claim_id` is `NONE` only when the verified path establishes that no provider claim exists or applies.
- Evidence and requirement fields are non-empty comma-separated, sorted, unique ID lists. Every listed evidence record and requirement MUST exist and pass its applicable gate.

### 3.3 `methods.cfg`

`methods.cfg` binds definitions, not labels. It contains one closed record for every method ID referenced by `cohort.cfg` and the evidence requirements. Each record has:

```text
method.<id>.purpose=<population|sampling-unit|cluster|term-stratum|horizon|loss|resolution|censoring|denominator|monetary-basis|amount-bound|metric>
method.<id>.version=<stable-version>
method.<id>.implementation_id=<compiled-algorithm-id>
method.<id>.effective_date=<YYYY-MM-DD>
method.<id>.definition=<complete deterministic rule>
method.<id>.inputs=<sorted comma-separated field names>
method.<id>.output=<field or metric name>
method.<id>.evidence_record_ids=<sorted comma-separated ids>
method.<id>.evidence_requirement_ids=<sorted comma-separated ids>
```

At minimum, deterministic methods MUST exist for:

- population enumeration and sampling unit;
- economic-cluster consolidation;
- comparable protection-term stratum and monetary basis;
- outcome horizon and resolution;
- pre-support shortfall, provider claim/payable/payout/unpaid amount, and final writeoff;
- censoring compatibility and row amount lower/upper bounds;
- every reported numerator, denominator, severity, delay, and concentration measure.

An amount upper-bound method MUST use an exact legal principal, guarantee, or contractual cap from verified evidence. If no finite defensible cap exists for an open member, amount-range and ratio exports are blocked rather than truncated by assumption. Prose cannot change a calculation: each computational method references a supported compiled `implementation_id`; unsupported IDs fail closed.

### 3.4 `dossier.cfg` and `evidence_manifest.tsv`

Evidence Gate now implements a distinct claim-population dossier schema
`0.3.0` and the supplemental compiled requirement
`FIN-CLAIM-POPULATION-FRAME`. That requirement requires an exact, controlled
primary register source from a capital provider, government disclosure, or
regulator plus a separate independent report. It is assessed by the same batch
engine but is excluded from the four reference-project gates and grants no
project, calibration, pricing, or investment authority.

A cohort-bound dossier intended for empirical use MUST cover population-register authority,
completeness, term
comparability, classification authority, exclusion-rule timing, and each method
definition. `observations.tsv` cites the exact evidence records and requirements
used for population membership and classification.

The implemented population-frame requirement covers only the declared register
source conjunction. It does not establish row classification, method validity,
term comparability, exclusion timing, or truth. Those remain separately bound
requirements for any future empirical-admission profile. A generic evidence pass cannot
be relabeled as proof of census completeness.

Individual Claim Ledger packages retain their own source manifests. Binder evidence supplements rather than replaces them.

Provider expenses, subrogation, and post-payment recoveries are outside the minimal v0.1 schema. Until a future hash-bound `provider_costs.tsv` is specified and verified, the binder reports gross provider cash and MUST NOT label it net provider economic loss.

## 4. Denominators, censoring, and identification

### 4.1 Mandatory counts

Let each enumerated row belong to exactly one disposition. The evaluator reports:

```text
N_frame = N_resolved + N_not_yet_matured + N_unresolved + N_excluded
N_included = N_resolved + N_not_yet_matured + N_unresolved
```

It also reports, without substituting them for the primary denominators:

- `N_trigger_known`, `N_triggered`, and `N_trigger_unknown`;
- resolved-path `N_provider_claim_generated` and `N_provider_claim_paid` until
  observed-to-date provenance is implemented for open rows;
- known-positive and possible-positive provider unpaid-claim counts as distinct
  endpoints; and
- `N_censored` (`N_not_yet_matured + N_unresolved`);
- counts by exclusion rule, cluster, provider, eligibility vintage, and term stratum.

Every ratio prints its numerator definition, denominator definition, row set, and count. Frequency estimates MUST NOT silently replace `N_included` with paid, triggered, mature, or resolved cases.

### 4.2 Open and censored cases

- An unresolved or not-yet-matured member remains compatible with both zero and positive ultimate outcome unless irreversible evidence narrows that set.
- A triggered but open member remains in the triggered count and in severity uncertainty.
- Pending, denied, disputed, and unpaid claims remain visible. Paid-only samples cannot identify payout probability or payment delay.
- A row is `resolved` only when underlying principal/due and provider-payable balances close, or a final legal resolution establishes the terminal amount.
- Extensions and novations do not reset cohort age or create a new observation unless the sampling-unit method proves a new economically distinct risk unit.
- Left-truncated claims require a separate prevalent-book stratum with exact opening exposure. Otherwise the class must be excluded under a rule frozen before outcomes are observed.

The target loader/evaluator derives, for open included row `i`, a compatible
amount interval `[L_i, U_i]`, where `L_i` is the known irreversible amount and
`U_i` is the verified contractual cap. Missing terminal amounts are never
imputed as zero. Cohort amount bounds are:

```text
aggregate_lower = sum_i L_i
aggregate_upper = sum_i U_i
```

Positive-outcome frequency bounds use the same fixed included row set: rows already irreversibly positive enter both endpoints; unresolved compatible rows enter only the upper endpoint. These are **arithmetic identification ranges**, not sampling confidence intervals.

The current programmatic kernel has an authoritative selected-full-path
provenance seam for resolved rows, but no observed-to-date path evaluation for
open rows and no empirical admission policy over the retained status/source/date
fields. It therefore does not yet identify observed-to-date irreversible cash
for open cases. Contractual face
is only a peak-principal cap; it is not used as a cumulative shortfall or
writeoff cap for an open revolving claim. Except for an exact zero-face claim,
open shortfall and writeoff amounts therefore remain `Unknown` until a separate
verified lifetime cap exists. The same rows remain positive-compatible in the
fixed-denominator frequency ranges.

The kernel admits an open provider envelope only from one exact, decision-time,
structurally evidenced principal-only term. An exact zero face, zero shortfall
allocation, or zero coverage gives a zero provider cap; otherwise the only safe
finite outer cap is the exact lifetime `maximum_cash` term:

```text
provider_cash_upper = maximum_cash
```

Coverage and deductible cannot reduce that upper endpoint without a separately
verified lifetime shortfall cap. This is a legal-term outer envelope, not an
estimate of claim generation, payability, payment, or loss. An unknown term
amount blocks that provider amount export but leaves a positive payout in the
compatible frequency upper endpoint. Each metric's interval is computed
separately; provider-cash, unpaid-claim, and writeoff endpoints are not asserted
to occur together in one feasible state.

Aggregate ratios are computed from aggregate numerators and aggregate denominators. The evaluator MUST NOT average row ratios. It MUST NOT construct a ratio interval by independently optimizing mutually incompatible numerator and denominator endpoints.

## 5. Permitted and prohibited outputs

### 5.1 Permitted target-loader outputs

The evaluator may emit only:

- all frame, disposition, trigger, claim, payment, unpaid-claim, and censoring
  counts; dispute counts require a separately modeled dispute field;
- exact observed-to-date and resolved totals for contractual face, opening and
  funded/capitalized principal, principal roll-forward basis, peak EAD, borrower
  principal cash, recovery principal cash, pre-support principal shortfall,
  provider claim generated, provider payable claim, gross provider cash, unpaid
  payable claim, conversion, and final writeoff;
- compatible arithmetic ranges for positive-loss frequency, payout frequency, aggregate ultimate pre-support shortfall, provider payout, unpaid claim, and residual writeoff, retaining all open included rows;
- commitment- or cap-based aggregate ratios when the exact denominator and row set are printed;
- resolved-case descriptive severity and delay, explicitly labeled conditional on resolution and accompanied by `N_censored`;
- provider, obligor/economic-cluster, vintage, and term-stratum concentration; and
- complete source, method, observation, scenario, claim, and evidence lineage.

If every included member is resolved, the evaluator may report empirical frame frequencies and means. They remain descriptions of that frame, not transferable probabilities.

### 5.2 Prohibited

The binder MUST NOT emit or imply:

- calibrated PD, LGD, expected loss, fair or actuarial premium, fair value, rating, capital charge, subsidy need, or investor expected return;
- joint probabilities, correlation, independence, diversification benefit, or tail loss inferred from separate claim histories;
- a Portfolio configuration, probability polytope, ambiguity set, instrument price, or replay authorization;
- equivalence among coverage percentage, claim submitted, claim allowed, claim paid, recovery, writeoff, commitment, exposure at default, or legal cap;
- paid-only, recovered-only, or resolved-only frequency claims presented as cohort frequencies;
- annualized rates without exact exposure time, pooled currencies or incompatible terms, name-based independence, or outcome-dependent exclusions; or
- application of a proposed guarantee fraction or capital structure to historical observations.

## 6. C++ implementation boundary

The target v0.1 package adds two pure boundaries and no pricing path:

```cpp
PartialCreditClaimLossCohortPackage
load_partial_credit_claim_loss_cohort_package(const std::filesystem::path& root);

PartialCreditClaimLossCohortEvaluation
evaluate_partial_credit_claim_loss_cohort(
    const PartialCreditClaimLossCohortPackage& package);
```

The loader MUST:

1. confine and normalize all paths beneath `root`;
2. read immutable byte snapshots and verify every declared SHA-256 digest;
3. reject unknown keys, columns, enum values, duplicates, missing fields, non-finite numbers, and inconsistent dates;
4. batch-run the Evidence Gate;
5. re-run `load_claim_ledger_package()` for each required claim root;
6. verify row/root/cluster/provider/currency/scenario identities; and
7. select exactly one verified full/backtest realized scenario for each resolved row.

Both boundaries are implemented in
[`partial_credit_claim_loss_cohort.hpp`](../include/naturalehia/cellular_finance/partial_credit_claim_loss_cohort.hpp)
and
[`partial_credit_claim_loss_cohort.cpp`](../src/partial_credit_claim_loss_cohort.cpp),
with the file loader declared in
[`partial_credit_claim_loss_cohort_config.hpp`](../include/naturalehia/cellular_finance/partial_credit_claim_loss_cohort_config.hpp)
and implemented in
[`partial_credit_claim_loss_cohort_config.cpp`](../src/partial_credit_claim_loss_cohort_config.cpp).
The evaluator accepts loaded packages but reloads the sealed five-file binder
and every Claim Ledger root, ignores mutable caller summaries and parsed-field
changes, blocks duplicate roots and claims, requires one unique
economic cluster for every declared frame row including exclusions, and keeps
all empirical admission flags false. It checks declared frame-count
reconciliation but does not establish census completeness. The loader uses
bounded immutable snapshots, verifies all four declared file hashes, confines
nested Claim Ledger roots, snapshots and rechecks every manifest-named retained
evidence copy, retains methods and Evidence Gate assessments, and then invokes
the same evaluator as a structural postcondition. Positive loader provenance is
held in a private seal; caller-constructed input receives no such provenance,
and evaluation of a sealed package independently reloads its canonical binder.
It does not
create a second cash parser: resolved rows reload
through Claim Ledger's authoritative one-scenario snapshot of selected latest
full-path entries, statuses, sources, dates, provider terms, and covenants.
Reparsing Claim Ledger TSV cash inside this binder remains prohibited because
it would create a second cash authority. The deferred work is a compiled
method, classification, term-comparability, exclusion-timing, and empirical
as-of admission profile—not five-file binding and not a replacement cash
engine.

The target evaluator MUST extract, from verified Claim Ledger results,
contractual face, opening principal, funded principal created, capitalized
principal, principal roll-forward basis, peak EAD, borrower principal cash,
recoveries, pre-support shortfall, provider claim
generated/payable/cash/unpaid and payable after horizon, conversion, and final
writeoff. Contractual face is a cap/reference denominator, not funded principal.
For a resolved path, principal conservation uses opening principal plus funded
and capitalized principal, never contractual face. `peak_ead_million` is labeled
peak EAD; it is not called EAD at default without a trigger event linked to a
verified Claim Ledger period. The evaluator MUST not read probabilities or
accept caller-computed cash summaries.

The target result contains a canonical observation ledger, denominator table,
exact totals, compatible arithmetic ranges, concentration disclosures, lineage,
and blockers. The current programmatic result contains the canonical observation
ledger, declared denominator and count fields, Claim-Ledger-exact synthetic
resolved totals, finite provider outer envelopes where mechanically available,
Unknown open shortfall/writeoff amounts, compatible positive-outcome frequency
ranges, claim/root IDs, blockers, and a flag that the resolved selected-path
provenance was verified during evaluation; it does not yet serialize the full
source/method/evidence lineage or concentration measures. Both
boundaries hard-code or derive:

```text
candidate_only=true
calibrated_execution_authorized=false
```

It has no conversion to `PortfolioConfig`, no ambiguity/calibration export, and no pricing, dependence, or expected-return API.

Current result rows are sorted by `observation_id`. Caller-supplied evidence-ID
lists must already be sorted and unique. Canonical target-package serialization
is not implemented.
The current Claim Ledger uses binary `double` amounts, so this iteration uses
known point values, `long double` accumulation, finite and overflow
checks, and explicit tolerances. “Claim-Ledger-exact” means exact within that
declared representation; it is not decimal-exact accounting. Principal,
provider-cash, and horizon-settlement reconciliation failures are hard errors,
not warnings.

## 7. Synthetic verification boundary

The implemented in-memory regression contains five caller-declared members:

1. resolved performing claim with zero shortfall and zero provider cash;
2. resolved loss with borrower recovery, provider payment, and final residual writeoff;
3. triggered unresolved protected claim with ultimate provider amounts still
   unidentified;
4. not-yet-matured protected claim;
5. excluded member under a caller-declared timely frozen outcome-blind rule with
   syntax-checked placeholder evidence IDs.

The current regression proves:

- frame and included denominators reconcile exactly;
- zero-loss members remain in denominators;
- mechanically exact synthetic cash and actual principal created conserve
  through the selected Claim Ledger paths without substituting contractual face;
- resolved rows require the raw scenario attestation to be explicitly
  `complete-resolved`, and their full selected-path provenance is obtained from
  the same immutable Claim Ledger package load used for evaluation;
- open members widen compatible positive-outcome frequency ranges rather than
  becoming zero; amounts remain `Unknown` where no valid lifetime cap exists,
  while finite provider-cap ranges are retained where available;
- result rows are sorted by observation ID; and
- no calibration, portfolio, price, or expected-return object is produced.

The implemented five-file regression proves all four binder-file hash bindings,
closed configuration and observation schemas, LF-only parsing, nested Claim
Ledger hash/path confinement, supported method identifiers, evidence citation
matching, and a positive and negative population-frame conjunction. It also
proves that caller-constructed or mutated objects cannot forge positive loader
provenance, retained-copy drift fails closed, and a structurally valid package
retains false empirical, calibration, and Portfolio authority. Symlink-race
simulation, a byte-for-byte cohort
serializer, a positive empirical source-status/date policy, concentration
outputs, and resolved severity or delay statistics are not implemented.

The completed target regression suite MUST reject or block, as applicable:

- duplicate observations or unconsolidated duplicate economic clusters;
- an exclusion rule frozen after eligibility or unsupported as outcome-blind;
- an open row with no defensible finite cap when amount ranges are requested;
- synthetic, estimated, hypothetical, stress, or post-as-of cash used as realized evidence;
- classification or resolution dated after the binder as-of date;
- any hash drift or path escape;
- paid-only or resolved-only denominator substitution;
- mismatched claim root, realized scenario, provider claim, currency, or monetary basis; and
- non-closing principal or provider-claim paths.

Passing the current suite validates parsing, binding, bookkeeping, mechanical
validation, and fail-closed authority only. It does not validate a real-world
loss model, census completeness, row classifications, or empirical
source-status/date admission.

## 8. Honest next evidence step

Any future evidence-acquisition milestone would require a real, controlled
provider or program register covering the complete issued or at-risk
partial-credit book, not a convenience sample. It is deliberately deferred from
the present financial-construction checkpoint. If separately authorized, every
frame member should bind executed protection terms, claim filings and decisions,
dated lender and provider cash, recoveries, final distributions, open and
disputed cases, and—if net provider loss will later be studied—expenses and
subrogation.

An independent challenger should test frame completeness, economic clustering, term comparability, classifications, exclusion timing, and source lineage. Only after that challenge should a separate transfer study ask whether this cohort is informative for new cellular-agriculture facilities. Dependence and common-factor evidence must then be added before marginal loss bounds can enter a probability polytope, pooled instrument replay, premium research, or investor-return analysis.

That sequence preserves the project's financial-engineering aim: build a valuable, investable risk instrument from losses that are measured honestly before they are priced.
