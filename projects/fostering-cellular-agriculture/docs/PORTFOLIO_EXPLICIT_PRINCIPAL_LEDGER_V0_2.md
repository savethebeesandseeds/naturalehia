# Portfolio Explicit Principal Ledger v0.2

Status: implemented accounting and file-format boundary, 2026-08-30.

## Purpose

Portfolio model `0.2.0` separates two amounts that can coincide at par but are
not the same financial quantity:

- **investor cash outlay** is cash paid to fund or acquire the asset, including
  a purchase price and buyer-direct costs; and
- **contractual principal** is the legal balance exposed to repayment,
  conversion, writeoff, and continuing credit risk.

The separation is necessary for a claim issued or acquired above or below par,
for a buyer that incurs direct transaction costs, and for principal increased
without contemporaneous investor cash through a capitalized fee or capitalized
interest. None of those differences may be converted mechanically into a
recovery or principal loss.

This is a deterministic accounting interface over an explicitly supplied joint
scenario distribution. It does not establish fair value, risk-neutral
probabilities, legal enforceability, source creditworthiness, or an investable
capital structure. The current implementation still accepts synthetic
portfolio inputs only.

## The two accounting modes

Each project declares one `principal_accounting_mode`.

### `draw-equals-principal-legacy`

This is the Portfolio `0.1.0` convention. Each legacy capital draw is both an
investor cash outflow and principal created. The project commitment is both the
maximum cash draw and the principal reference notional. Principal not returned
by the horizon is:

- realized principal loss when the path is `resolved`; or
- outstanding principal exposure when the path is `continuing`.

Legacy mode remains available in `0.2.0` so existing synthetic portfolio and
staged-capital cases retain their original economics. It cannot carry explicit
principal movements or classified investor outlays.

### `explicit-contractual-ledger`

This mode maintains independent cash and principal ledgers. The project fields
have distinct meanings:

| Field | Meaning |
|---|---|
| `commitment_million` | Maximum cumulative investor outlay on any supplied path; it is a cash limit, not principal |
| `principal_limit_million` | Maximum contractual principal balance and the project's loss-reference notional |
| `opening_principal_million` | Contractual principal already outstanding at the analysis origin |

`principal_limit_million` must be positive. Opening principal must be
non-negative and no greater than that limit. An explicit path leaves legacy
`capital_draws` empty and uses classified `investor_outlays`. Cash outlays and
principal additions need not be equal in amount or date.

## Exact enum meanings

### Investor-outlay purpose

- `primary-project-funding`: Cash paid by the investor as project funding. It
  is an outlay; in explicit mode it does not create principal without a
  separate principal-addition movement.

- `claim-purchase-price`: Cash paid to issue or acquire the claim. Paying above
  or below contractual principal changes investor return, not principal loss.

- `buyer-direct-cost`: Additional investor cash paid as a transaction or
  acquisition cost. It affects cash return, NPV, and liquidity, but never
  creates principal by itself.

All outlay amounts are non-negative and fall within the portfolio horizon.
Cumulative project outlays cannot exceed `commitment_million`.

### Principal-movement kind

- `funded-principal-addition`: Contractual principal created through funding.
  It is a balance movement, not a second cash outlay.

- `capitalized-fee-addition`: A fee added to contractual principal without
  itself proving investor cash.

- `capitalized-interest-addition`: Accrued interest transferred into
  contractual principal. Once capitalized, it is part of the principal balance
  and can later be returned, converted, or written off as principal.

- `conversion-extinguishment`: Contractual principal extinguished through
  non-cash conversion. It is neither investor cash nor realized principal loss.
  No value is assigned to conversion units unless a separate, identified cash
  receipt is supplied.

- `writeoff`: Contractual principal extinguished through realized loss. In
  explicit mode, and only in explicit mode, this movement is the direct source
  of reported `principal_loss_million`.

Principal movements are non-negative memo entries. They never enter investor
cash flow directly.

## Monthly contractual ordering

For every explicit project path, each month is processed in this order:

1. add funded principal, capitalized fees, and capitalized interest;
2. reduce principal by the principal component of actual investor receipts;
3. reduce principal by conversion extinguishment; and
4. reduce principal by writeoff.

The principal balance cannot become negative at any step. The balance after
additions cannot exceed `principal_limit_million`. A receipt's
`principal_component_million` cannot exceed its cash amount.

The path roll-forward is:

```text
opening principal
  + funded-principal additions
  + capitalized-fee additions
  + capitalized-interest additions
= principal cash returned
  + principal converted
  + principal written off
  + closing principal.
```

For a `resolved` explicit path, closing principal must be zero. For a
`continuing` path, closing principal is outstanding exposure, not loss, and the
cash-flow NPV assigns it no implicit terminal value. A continuing path can also
retain non-principal legal or economic rights; the resolution label is not
derived solely from whether principal happens to be zero.

## Cash accounting and liquidity

For one project and month:

```text
project net cash
  = investor receipts
  - investor outlays.
```

At pool level:

```text
pool net cash
  = project investor receipts
  - project investor outlays
  - pool costs.
```

NPV discounts those dated net cash amounts at the supplied physical hurdle.
It does not discount principal memo movements as if they were cash.

For output compatibility, v0.2 result fields that retain `draw` in their name
report the same investor-cash-outlay measure: for example,
`capital_draws_million`, `total_draws_million`, `expected_draws_million`, and
`peak_same_month_draw_million`. Where an `investor_outlays_million` field is
also present, the two values are equal. In an explicit path these legacy names
must not be read as funded-principal additions or exposure at default.

The liquidity convention is deliberately conservative within a month: investor
outlays and pool costs are funded before that month's receipts are released.
`funding_need_million` and peak cumulative net outlay therefore do not assume
same-day settlement netting. A legally netted closing requires finer settlement
evidence before this gross liquidity measure can be reinterpreted.

## Source-labelled receipts

Every investor receipt supplies a cash amount, a principal component, and one
`cash_source_id` referencing a declared scenario-level source budget. The
source kind records where cash is asserted to come from; the principal
component records how much of that same cash retires contractual principal.
Those are independent classifications. A recovery label does not automatically
make all cash principal, and a principal component does not change the source
of cash.

The exact source labels are `commercial`, `licensing-royalty`, `exit-sale`,
`recovery`, `refinancing`, `explicit-support`, `sponsor-fee`, and
`financing-fee`. Refinancing is later financing liquidity, not operating value.
`financing-fee` identifies fee cash without asserting sponsor identity. No
source label proves payer existence, creditworthiness, or legal enforceability.

At every month, cumulative receipts referencing one source cannot exceed that
source's cumulative available cash. The constraint applies across all projects
that share the source ID, preventing the same recovery, support, or financing
budget from being spent more than once. Unused availability is not an investor
receipt. Reported nominal receipts reconcile to the sum of the source-labelled
nominal amounts; present-value source totals use the same receipt dates.

## Principal loss and exposure

Explicit-ledger reporting uses these definitions:

```text
principal returned   = cash receipts classified as principal
principal converted  = non-cash conversion extinguishment
principal loss       = explicit writeoff
outstanding principal= closing principal on a continuing path.
```

Purchase premium, original-issue premium, buyer-direct cost, and negative cash
return are not principal loss. Original-issue discount and capitalized amounts
are not cash return. Conversion is not recovery cash. These classifications
remain separate even if two amounts are numerically equal.

Expected principal loss is the normalized physical-scenario-weighted average
of path principal loss. Expected outstanding principal is the corresponding
weighted average of continuing closing balances. They are credit-accounting
measures, distinct from expected NPV and the probability of negative NPV.

At project and scenario level, the engine reconciles:

```text
opening principal + principal added
= principal returned + principal converted
 + outstanding principal + principal loss.
```

Separately, cash receipts reconcile to identified return sources, and direct
project NPV less pool-cost NPV reconciles to the scenario's monthly cash NPV.
Loss layers, when present, reconcile back to the same aggregate realized
principal loss.

## Above-par worked example

Consider a claim with these month-zero facts, in millions:

```text
claim purchase price                    10.5
buyer-direct cost                        0.5
funded principal added                  10.0
principal limit                         10.0
opening principal                        0.0
```

The project's cash commitment is at least `11.0`, while its contractual
principal limit is `10.0`. At month 12 the investor receives `10.0`, entirely
classified as principal, and the path resolves.

Cash and principal produce different but consistent results:

```text
investor cash at a 0% hurdle = -10.5 - 0.5 + 10.0 = -1.0
principal roll-forward       = 0.0 + 10.0 - 10.0 = 0.0
principal loss               = 0.0
```

The investor has a one-million negative nominal cash return, but no contractual
principal was written off. Treating the eleven-million cash outlay as eleven
million of principal would manufacture a false one-million principal loss.

Likewise, if a fee is capitalized, it appears as
`capitalized-fee-addition`; it does not increase investor outlay unless an
actual, separately classified cash outlay also occurred.

## Loss layers and pooled-loss protection

Loss allocation uses **reference principal**, not investor cash:

```text
project reference principal = commitment_million        in legacy mode
project reference principal = principal_limit_million   in explicit mode
aggregate reference principal = sum across projects.
```

Portfolio loss layers must form a contiguous partition from zero through
aggregate reference principal. They allocate modeled principal writeoff; they
do not reimburse purchase premium or buyer-direct cost.

Pooled-loss protection uses the same aggregate reference principal to bound the
legal support cap and the supported coverage-fraction domain. Its scenario
claim remains the selected coverage fraction times gross realized principal
loss. In an explicit-ledger portfolio, the compatibility field
`aggregate_covered_commitment_million` is an alias for aggregate reference
principal; it is not aggregate investor cash commitment.

## Capital-stack boundary

Capital Stack v0.1 rejects every portfolio containing an
`explicit-contractual-ledger` project. That engine currently assumes:

- the aggregate commitment is subscribed fully at par in month zero;
- subscription cash becomes a zero-yield, lossless reserve;
- project draws consume that reserve; and
- unused commitment is returned from the reserve at the horizon.

Those mechanics require cash commitment, tranche subscription cash, reserve
cash, asset purchase price, and asset principal to be the same amount. They are
not valid for an above- or below-par acquired claim. Capital-stack support must
remain blocked until those quantities have separate contractual fields and
reconciliations.

## Version compatibility

Portfolio `0.1.0` retains its exact closed key set. It accepts only
`draw-equals-principal-legacy`; v0.2 project fields, investor-outlay rows, and
principal-movement rows are unknown keys and are rejected.

Portfolio `0.2.0` requires every new project field and both new per-path count
keys, including explicit zeros. A legacy project encoded in a v0.2 file uses:

```text
project.i.principal_accounting_mode=draw-equals-principal-legacy
project.i.principal_limit_million=0
project.i.opening_principal_million=0
scenario.s.project.p.investor_outlay.count=0
scenario.s.project.p.principal_movement.count=0
```

It continues to use legacy draw rows. An explicit project uses
`principal_accounting_mode=explicit-contractual-ledger`, a positive principal
limit, both required per-path count keys, and `draw.count=0`. Any investor cash
outlay or principal movement is represented only by its respective classified
row; either count can legitimately be zero when the supplied path has no such
event. Mixing the legacy and explicit representations on one project path is
invalid.

The complete normalized key schema is stated in
[`PORTFOLIO_SCENARIO_FORMAT_V0_1.md`](PORTFOLIO_SCENARIO_FORMAT_V0_1.md).
