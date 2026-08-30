# Multi-Project Milestone Participation — Candidate Term Sheet

**Economic terms version 1.0 — 30 August 2026**

> Research design only. No issuer, provider, legal form, market price, rating,
> regulatory treatment, tax result, or offer is established.

## Core instrument

| Term | Candidate economic term |
|---|---|
| Name | Multi-Project Milestone Participation |
| Purpose | Carry a disclosed set of cellular-agriculture claims from research and pilot work into repeatable industrial production without pretending that packaging creates project cash. |
| Investor property | A pro-rata, limited-recourse contractual claim on a fixed schedule of project claims that pass the stated admission conditions, and on their identified receipts and recoveries. |
| What is not owned | Unassigned company equity, company net asset value, facilities, speculative intellectual-property value, unused source capacity, or a manager's promise. |
| Pool | Static and disclosed at issue. The synthetic fixture is mechanically included, not evidence-admitted. Additions, substitutions, removals, and concentration changes require a new identified schedule and re-analysis. |
| Currency and basis | One stated currency and monetary basis per issuance. |
| Commitment | Sum of scheduled claim limits. Investor cash paid and contractual principal remain separate fields; the synthetic example uses at-par primary advances, so each milestone draw creates equal principal by stated term. |
| Funding | Project cash is released only at declared milestones. A stopped path cancels later availability. The core uses callable commitments; peak liquidity and cumulative outlay are reported. |
| Repayment | Assigned principal cash from named external sources. |
| Success participation | A capped share of commercial, licensing, royalty, capacity, offtake, or exit cash that is already granted to the claim and actually paid. No residual enterprise value is inferred. |
| Failure recovery | Identified cash recovery only. Replacement cost, unevidenced intellectual property, payment in kind, and continuing exposure are not cash. |
| Loss | Final resolved contractual principal writeoff after in-horizon principal recovery. Continuing principal remains exposure, not realized loss. |
| Costs | Explicit investor cash outflows outside project principal unless a real contract states otherwise. |
| Return test | Dated physical-measure cash and NPV at a separately stated hurdle, with loss, liquidity, duration, correlation, and adverse probability witnesses. This is not fair value. |
| Legal form | Open. A limited-recourse participation certificate, note, or fund interest may implement the economics only after legal, tax, securities, insolvency, custody, and perfection analysis. |

A live issuance should disclose stage, currency, horizon, technology, supplier,
jurisdiction, buyer, and source concentrations. The ten-claim fixture spans
stages to test the common interface; its claims have not passed an empirical
Evidence Gate, and it is not evidence that a mixed-stage
issuance is homogeneous or financeable.

## Cash and loss waterfall

```text
project cash paid by named sources
  + cash recoveries
  + external support, shown separately
  - explicit pool costs
  = cash available to the investor claim

opening principal + principal created
  = principal cash returned
  + principal converted or otherwise extinguished under an evidenced term
  + principal still outstanding
  + resolved principal loss

continuing principal != cash and != realized loss
```

Principal recovery cash is included in principal cash returned. The untranched
core distributes actual cash pro rata and bears resolved loss pro
rata. Its maximum contractual reference principal is not the same as expected
cash drawn.

## Variant 1 — Funded First-Loss and Priority Claims

The same unchanged project pool is fully subscribed at par and modeled in a
separately accounted zero-yield synthetic reserve. The fixture establishes no
legal segregation or custody arrangement. The example uses only two classes:

- funded first-loss participation: attachment 0, detachment 20; and
- market priority claim: attachment 20, detachment 100, with a lifetime
  non-principal cash-allocation cap of 24.

Both classes fund their full notionals. Pool costs are additional pro-rata
calls. Unused reserve is returned at the horizon and is not profit.

```text
resolved loss L:
  first-loss allocation = min(L, 20)
  priority allocation   = min(max(L - 20, 0), 80)

principal cash and unused reserve: priority first
assigned non-principal cash: priority up to its lifetime cap
remaining non-principal cash: first-loss residual
```

This changes who bears loss and when cash is paid. It does not change project
cash, gross loss, or diversification. No intermediate class is introduced.

## Variant 2 — Failure-Contingent Partial-Credit Guarantee

This overlay attaches only to the untranched core in version 1. Combining it
with funded subordination requires a new no-double-recovery waterfall.

```text
g = stated covered share
C = provider's legal cash cap
K = aggregate contractual reference principal
g <= C / K

after recoveries and final resolved writeoff L:
  provider payment = g × L
  investor retained loss = (1 - g) × L
```

The ten-claim sensitivity uses `g=30%`, `C=30`, and settlement at month 60.
Continuing exposure is not covered. Provider cash is an external transfer, not
project revenue, principal repayment, recovery, or diversification. Premium,
provider expenses, collateral carry, capital charge, counterparty default, and
any public subsidy remain separately visible.

The base protection calculation assumes full provider performance without
claiming a funded reserve. A separate credit sensitivity replaces that
assumption with declared pledged collateral and unsecured provider exposure.
These are alternative modeling layers, not evidence of capacity. A live term
must say whether the commitment is prefunded, collateralized, or contingent and
must establish the provider's authority, funding, collateral control, and
payment enforceability. None is evidenced by the synthetic fixture.

## Required disclosure and admission test

Each project must connect through the Project Claim Ledger and disclose:

1. milestone cash schedule and stop rights;
2. principal creation, repayment, conversion, writeoff, and continuing balance;
3. every cash source, amount, date, capacity, and counterparty;
4. recovery priority and timing;
5. biological/process, scale-up/commissioning, supplier/media,
   regulatory/qualification, and buyer/product-acceptance dependencies; and
6. whether each input is contractual, observed, estimated, synthetic, or
   unknown.

Admission does not validate a project. Unknown is not zero. A pool must be
rejected or re-termed if the cash right is absent, source budgets overlap,
common shocks erase the claimed diversification, a milestone does not stop
funding, the robust NPV remains inadequate, or support lacks an authorized and
creditworthy payer.

## Current economic conclusion

The synthetic ten-claim fixture is a falsification example, not a financing
claim. At full declared success participation, central callable-core NPV is
slightly positive, but the worst permitted expected NPV remains negative. The
funded stack has prefunding drag, and neither its first-loss nor priority class
meets its stated hurdle at the central mix. The 30% synthetic partial-credit
overlay also fails to restore robust investor NPV, and the provider has no
investor-funded premium interval.

The candidate partial-credit guarantee therefore does not establish public loss
sharing. The structure is defined, measurable, and rejectable; it is not yet
shown attractive or financeable. That is the honest starting point for
evidence, term redesign, and independently supported hurdle research.
