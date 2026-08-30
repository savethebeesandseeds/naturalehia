# Portfolio Scenario Format v0.1 and v0.2

Status: strict file-format specification, 2026-08-30.

## Purpose and boundary

These formats represent a finite, explicit joint distribution for the C++
participation-pool engine. They are human-readable contract-mechanics input.
Portfolio `0.1.0` is the closed legacy schema. Portfolio `0.2.0` adds an
explicit contractual-principal ledger without changing the meaning of a
legacy file. Neither version is a forecast, probability calibration, fair
value, market quote, rating, offer, or investment recommendation.

Every weight is an analyst-declared physical-measure scenario weight. The file
does not provide risk-neutral probabilities or a pricing kernel. The declared
physical hurdle discounts dated investor cash only; it does not turn the result
into fair value. Both versions accept `portfolio.synthetic_inputs=true` only.

The controlling synthetic example is
[`two-project-participation-pool-synthetic.cfg`](../scenarios/two-project-participation-pool-synthetic.cfg).
The accounting rationale and reconciliations introduced in v0.2 are specified
in
[`PORTFOLIO_EXPLICIT_PRINCIPAL_LEDGER_V0_2.md`](PORTFOLIO_EXPLICIT_PRINCIPAL_LEDGER_V0_2.md).

## Lexical rules

- The file is UTF-8 text with one `key=value` assignment per line.
- Blank lines and lines whose first non-space character is `#` are ignored.
- Inline comments are not supported; a `#` after `=` is part of the value.
- Keys and values are trimmed at their outer whitespace boundary.
- Keys are case-sensitive. Unknown, duplicate, and missing keys are errors.
- Every index is one-based, contiguous, and no greater than its controlling count.
- A count is always required, including when it is zero.
- Booleans are exactly `true` or `false`.
- Months and counts are base-10 unsigned integers.
- Monetary amounts, rates, and weights are finite base-10 numbers. `NaN` and
  infinity are invalid.
- Identifiers begin with an ASCII letter or digit and then use only ASCII
  letters, digits, `-`, `_`, or `.`.

Normalized output must contain every required key, including explicit zero
counts, and must be reloadable without changing the represented configuration.

## Exact enum spellings

`project.i.stage` accepts exactly:

```text
research
pilot
demonstration
first-industrial
repeat-production
```

`scenario.s.project.p.resolution` accepts exactly:

```text
resolved
continuing
```

In a v0.2 file, `project.i.principal_accounting_mode` accepts exactly:

```text
draw-equals-principal-legacy
explicit-contractual-ledger
```

In a v0.2 file, `scenario.s.project.p.investor_outlay.o.purpose` accepts
exactly:

```text
primary-project-funding
claim-purchase-price
buyer-direct-cost
```

In a v0.2 file, `scenario.s.project.p.principal_movement.m.kind` accepts
exactly:

```text
funded-principal-addition
capitalized-fee-addition
capitalized-interest-addition
conversion-extinguishment
writeoff
```

`scenario.s.cash_source.c.kind` accepts exactly:

```text
commercial
licensing-royalty
exit-sale
recovery
refinancing
explicit-support
sponsor-fee
financing-fee
```

Refinancing is liquidity supplied by a later financing claim, not new value
created by the project. `explicit-support` and `sponsor-fee` remain visible as
outside or financing-party transfers rather than operating cash.
`financing-fee` is fee cash paid to the claim investor when sponsor identity
is not established; the label does not infer a payer.

## Complete key schema

The letters `i`, `s`, `f`, `k`, `c`, `p`, `d`, `o`, `r`, and `m` below are
one-based integer indices. Literal keys use values from one through the
relevant count. `portfolio.model_version` selects an exact, closed key set:
v0.1 rejects every v0.2-only key, while v0.2 requires every v0.2 key even when
its controlling count or amount is zero.

### Portfolio metadata

Every file begins with:

```text
portfolio.model_version=<0.1.0 or 0.2.0>
portfolio.label=<non-empty single-line text>
portfolio.source_note=<non-empty single-line text>
portfolio.currency_label=<non-empty single-line text>
portfolio.monetary_basis=<non-empty single-line text>
portfolio.synthetic_inputs=true
portfolio.horizon_months=<positive integer>
portfolio.annual_physical_hurdle_rate=<non-negative decimal>
```

All monetary values share `currency_label` and `monetary_basis`. Currency or
price-basis conversion occurs before loading the file. The model version is
semantic, not decorative: a normalized file prints the selected version's
complete key set and reloads under that same version.

### Projects

```text
project.count=<positive integer>
project.i.id=<unique project identifier>
project.i.stage=<project-stage enum>
project.i.commitment_million=<positive decimal>
```

Every v0.2 project additionally requires:

```text
project.i.principal_accounting_mode=<principal-accounting-mode enum>
project.i.principal_limit_million=<non-negative decimal>
project.i.opening_principal_million=<non-negative decimal>
```

A v0.1 file contains none of those three keys and always uses the legacy
draw-equals-principal convention. A v0.2 legacy project declares
`draw-equals-principal-legacy` and sets both explicit principal amounts to
zero. A v0.2 explicit project declares `explicit-contractual-ledger`, has a
positive principal limit, and has opening principal no greater than that
limit.

In legacy mode, `commitment_million` is both the maximum cumulative cash draw
and the reference principal. In explicit mode, it is only the maximum
cumulative investor cash outlay; `principal_limit_million` is the maximum
contractual balance and reference principal. `opening_principal_million` is
contractual principal already outstanding at the analysis origin. The model
does not infer cash paid at origin from opening principal.

Each scenario must contain the same `project.count` project paths. Path index
`p` is only a storage position: `project_id`, not position, identifies the
project. Input order must not change results.

### Joint scenarios

```text
scenario.count=<positive integer>
scenario.s.id=<unique scenario identifier>
scenario.s.weight=<non-negative decimal>
```

Weights must sum to one within `1e-12`. The engine reports the raw accepted sum
and normalizes it for all aggregates. A joint scenario simultaneously states
the outcome of every project. The loader never multiplies marginal cases,
resamples paths, or assumes independence.

At least one weight is therefore positive. A zero central weight keeps a
declared path in the exhaustive scenario taxonomy without inventing a
pseudocount. It contributes no mass to central expected-value or tail
calculations, but a companion probability envelope may give that same state a
positive admissible upper weight. Zero observed occurrences do not establish
zero physical probability.

### Factor tags

```text
scenario.s.factor_tag.count=<non-negative integer>
scenario.s.factor_tag.f=<unique identifier within the scenario>
```

Tags disclose common conditions affecting the state. They are labels, not
calibrated factor loadings and not a substitute for explicit joint paths.

### Pool costs

```text
scenario.s.pool_cost.count=<non-negative integer>
scenario.s.pool_cost.k.month=<month within the horizon>
scenario.s.pool_cost.k.amount_million=<non-negative decimal>
```

Pool costs are investor cash outflows. They reduce pool NPV and increase gross
liquidity need, but do not change project principal loss.

### Shared scenario cash sources

```text
scenario.s.cash_source.count=<non-negative integer>
scenario.s.cash_source.c.id=<unique source-budget identifier in the scenario>
scenario.s.cash_source.c.kind=<cash-source enum>
scenario.s.cash_source.c.cash.count=<non-negative integer>
scenario.s.cash_source.c.cash.k.month=<month within the horizon>
scenario.s.cash_source.c.cash.k.amount_million=<non-negative decimal>
```

A cash source is a finite scenario-level budget shared by every project path
that references its ID. For each source and every month:

```text
cumulative receipts referencing source through month t
    <= cumulative cash available from source through month t.
```

The same guarantee, recovery account, refinancing facility, or commercial cash
budget therefore cannot be consumed independently by two projects. Distinct
sources require distinct IDs. Unused source cash is not an investor receipt.
The source declaration identifies an input budget; it does not prove that the
payer exists, is creditworthy, or is legally obligated. Source kind classifies
cash origin; `principal_component_million` separately classifies the amount
applied to principal. Neither classification determines the other. Nominal
receipts reconcile to the sum of nominal source totals, and the reported
present-value source totals use those same receipt dates.

### Project paths

There is no per-scenario project-path count. For every scenario, `p` ranges
from one through `project.count`:

```text
scenario.s.project.p.project_id=<configured project identifier>
scenario.s.project.p.resolution=<path-resolution enum>

scenario.s.project.p.draw.count=<non-negative integer>
scenario.s.project.p.draw.d.month=<month within the horizon>
scenario.s.project.p.draw.d.amount_million=<non-negative decimal>

scenario.s.project.p.receipt.count=<non-negative integer>
scenario.s.project.p.receipt.r.month=<month within the horizon>
scenario.s.project.p.receipt.r.cash_source_id=<declared scenario source ID>
scenario.s.project.p.receipt.r.amount_million=<non-negative decimal>
scenario.s.project.p.receipt.r.principal_component_million=<non-negative decimal>
```

Every v0.2 path additionally requires both count keys and, when a count is
positive, its indexed rows:

```text
scenario.s.project.p.investor_outlay.count=<non-negative integer>
scenario.s.project.p.investor_outlay.o.month=<month within the horizon>
scenario.s.project.p.investor_outlay.o.purpose=<investor-outlay-purpose enum>
scenario.s.project.p.investor_outlay.o.amount_million=<non-negative decimal>

scenario.s.project.p.principal_movement.count=<non-negative integer>
scenario.s.project.p.principal_movement.m.month=<month within the horizon>
scenario.s.project.p.principal_movement.m.kind=<principal-movement-kind enum>
scenario.s.project.p.principal_movement.m.amount_million=<non-negative decimal>
```

Every configured project ID appears exactly once in every scenario. A receipt
always references an identified scenario cash-source budget. Its principal
component cannot exceed its cash amount; the component is a classification of
that cash, not additional cash. Payment in kind, a principal memo movement,
and an accounting claim are not investor receipts.

For `draw-equals-principal-legacy`, cumulative draws cannot exceed project
commitment and cumulative principal cash returned cannot exceed cumulative
draws at any month. `investor_outlay.count` and
`principal_movement.count` must both be zero in a v0.2 legacy path.

For `explicit-contractual-ledger`, `draw.count` must be zero. Cash paid by the
investor is recorded only in classified investor-outlay rows, whose cumulative
amount cannot exceed project commitment. Contractual principal is recorded
independently through opening principal and principal-movement rows. Investor
outlay neither creates principal nor proves a loss; principal movement neither
creates cash nor proves a receipt.

Within each month, an explicit contractual balance is processed as category
totals in this order:

1. funded-principal, capitalized-fee, and capitalized-interest additions;
2. cash receipts classified as principal return;
3. conversion extinguishment; and
4. writeoff.

The balance cannot be negative after any reduction or exceed the project's
principal limit after additions. The path must satisfy:

```text
opening principal + principal additions
  = principal cash returned + principal converted
  + principal written off + closing principal.
```

Cash remains a separate reconciliation:

```text
project net cash = investor receipts - investor cash outlays.
```

Outlays and pool costs are assumed funded before receipts released in the same
month for the gross liquidity measure. Same-month contractual-principal
ordering does not imply same-day legal settlement netting.

`resolved` means the instrument has terminated by the analysis horizon. In
legacy mode, unreturned funded principal is realized principal loss. In
explicit mode, closing principal must be zero and only an explicit `writeoff`
movement is realized principal loss. A conversion extinguishes principal
without creating cash or loss.

`continuing` means the claim remains legally or economically outstanding at
the horizon. Legacy unreturned principal, or the explicit closing contractual
balance, is reported as outstanding exposure rather than additional loss. An
explicit continuing path may also contain a prior writeoff; that writeoff is
loss and the remaining closing balance is exposure. A continuing label may be
valid with zero closing principal when other legal or economic rights remain.
The physical-hurdle NPV includes only supplied dated cash and assigns no
implicit terminal value to outstanding exposure or conversion units. Analysts
must not use `continuing` to hide an actual impairment or `resolved` to label a
performing balloon as a loss.

### Optional loss layers

```text
loss_layer.count=<non-negative integer>
loss_layer.i.id=<unique layer identifier>
loss_layer.i.attachment_million=<non-negative decimal>
loss_layer.i.detachment_million=<decimal greater than attachment>
```

If the count is positive, layers are listed in ascending order and form one
contiguous partition from zero through aggregate reference principal, where:

```text
legacy project reference principal   = commitment_million
explicit project reference principal = principal_limit_million.
```

Layers allocate the same modeled pool principal loss: residual principal loss
in resolved legacy paths and explicit writeoff in explicit-ledger paths. They
do not create cash, lower aggregate loss, cover purchase premium or buyer
direct cost, or assign value to conversion units. The initial synthetic fixture
uses `loss_layer.count=0` so that the untranched economics remain primary.

## Version compatibility and downstream boundary

Portfolio v0.1 and v0.2 are separate closed schemas:

| File version and mode | Required representation |
|---|---|
| v0.1 legacy | Original project, draw, receipt, and source keys only; every v0.2-only key is rejected |
| v0.2 legacy | All three v0.2 project fields are present; explicit principal fields are zero; every path prints zero investor-outlay and principal-movement counts and continues to use draw rows |
| v0.2 explicit | Positive principal limit and zero legacy draw count; required outlay and movement count keys, with any cash outlay or principal movement represented only by its classified rows |

An explicit project cannot be represented honestly by changing only the model
version or by putting purchase cash in a legacy draw. Conversion between modes
requires a complete economic mapping of cash and contractual principal.

Capital Stack v0.1 rejects every portfolio containing an
`explicit-contractual-ledger` project. Its current reserve and subscription
mechanics require commitment cash, subscription cash, asset purchase price,
and asset principal to coincide at par. Above- or below-par claims, buyer-direct
costs, opening principal, and non-cash principal additions violate that
identity. Downstream capital-stack evaluation remains blocked until those
amounts have separate contractual fields and reconciliations. Pooled-loss
protection, by contrast, references aggregate principal limit rather than
aggregate investor outlay, as detailed in the explicit-ledger specification.

## Legacy v0.1 synthetic fixture hand calculations

The fixture has two 10-million commitments. Both projects draw 10 at month
zero in every state, and the pool pays a 0.2 cost at month zero. A successful
project receives 13 at month 24, of which 10 returns principal. A failed
project receives 2 of recovery cash at month 12, all classified as principal
return. The physical hurdle is zero.

| Joint state | Weight | Receipts | Principal loss | Pool NPV |
|---|---:|---:|---:|---:|
| Common success | 0.62 | 26 | 0 | 5.8 |
| Culture platform loss only | 0.18 | 15 | 8 | -5.2 |
| Bioprocess scale-up loss only | 0.18 | 15 | 8 | -5.2 |
| Common loss | 0.02 | 4 | 16 | -16.2 |

Expected and tail results are:

```text
expected draws                         = 20.0
expected commercial receipts           = 20.8
expected recovery receipts             = 0.8
expected total receipts                = 21.6
expected pool cost                     = 0.2
expected pool NPV at 0% hurdle         = 1.4
probability of negative pool NPV       = 0.38
expected principal loss               = 3.2
probability of any principal impairment= 0.38
expected outstanding principal         = 0.0
pool loss p50 / p95 / p99              = 0 / 8 / 16
pool ES95 / ES99                       = 11.2 / 16
sum of standalone ES95 / ES99          = 16 / 16
diversification benefit ES95 / ES99    = 4.8 / 0
diversification ratio ES95 / ES99      = 0.30 / 0
pairwise principal-loss correlation    = -0.125
peak same-month draw                   = 20.0
peak same-month funding need           = 20.2
peak cumulative net outlay             = 20.2
```

Diversification is positive but incomplete at 95% because most impairment
states affect only one project. It disappears at 99% because the common-loss
state controls the most severe tail. This is a mechanical illustration of
dependence, not evidence that real cellular-agriculture projects have these
probabilities, recoveries, returns, or correlations.
