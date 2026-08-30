# Fostering Cellular Agriculture: The Financial Instruments in Brief

**Working economic description — version 0.9**

## The purpose

Cellular agriculture must turn research into reliable industrial production
before it can replace animal-derived supply. Ordinary debt fits poorly when
failure risk is high, cash arrives late, and facilities share technical risks.
This project is building an open financial standard and a focused instrument
family for that gap. It defines the asset investors may finance, not a system
for managing a fund.

## The common asset standard

Every project claim must state:

- investor cash needs and milestones for later funding;
- contractual principal, repayment, conversion, and writeoff;
- identified success cash, recoveries, and third-party payments; and
- shared risks that can make several projects fail together.

Investor cash and contractual principal are separate:

```text
investor return = dated cash received - dated cash paid
contractual principal loss = principal finally written off
```

Price changes investment return, not principal loss. A fully repaid above-par
purchase can lose money and still have no protection claim. Unknown is not
zero; payment in kind, conversion units, unused capacity, and refinancing are
not operating cash.

## The instrument family

| Instrument | Function |
|---|---|
| Milestone-gated claim | Funds later stages only after stated conditions; failure cancels future availability. |
| Success participation | Shares success cash already granted to the claim. |
| Participation pool | Holds several disclosed claims, their contractual cash rights, and realized cash when evidenced. |
| Partial-credit guarantee | Moves a fixed share of final principal loss to a public or catalytic provider. |
| First-loss and priority claims | Redistribute one pool's cash and loss among investors. |

Pooling reduces concentration only when projects do not fail together. The
model uses explicit joint outcomes, never assumed independence.

## The first concrete public-credit instrument

The first stated overlay is a **Failure-Contingent Public Partial-Credit
Guarantee** on an investor's pool claim. It puts no public cash into projects
upfront.

All figures in the following demonstration are synthetic `DEMO million`
amounts. They are proposed mechanics, not executed legal terms.

```text
aggregate contractual reference principal K = 20.000000
public loss share g                          = 1/6
attachment A                                = 0
modeled maximum public commitment g × K     = 3.333333
payment date                                = month 24, after recoveries

scenario public payment X = (1/6) × final principal writeoff
```

If both projects succeed, payment is zero. If either fails, gross loss is `8`
and payment is `1.333333`; if both fail, gross loss is `16` and payment is
`2.666667`. The investor retains five sixths. Gross loss stays visible, and
public payment is not project revenue, recovery, or returned principal.

Under the declared synthetic probability bounds:

| Measure | Minimum | Central | Maximum |
|---|---:|---:|---:|
| Expected gross principal loss | 2.480000 | 3.200000 | 4.800000 |
| Expected public payment | 0.413333 | 0.533333 | 0.800000 |
| Expected retained principal loss | 2.066667 | 2.666667 | 4.000000 |
| Expected investor NPV before guarantee | -0.800000 | 1.400000 | 2.390000 |
| Expected investor NPV after guarantee, before premium | 0.000000 | 1.933333 | 2.803333 |

NPV means net present value at the stated investor target return. Endpoints are
different feasible probability mixtures, not one forecast. A provider issuing
these terms would need authority and capacity for the full `3.333333`
commitment, not only the maximum expected payment.

## The honest price result

At a one-sixth loss share, the minimum **expected** investor NPV over admitted
probability mixtures reaches zero before premium. No failed scenario is made
whole: protected NPV remains `-3.866667` when one project fails and
`-13.533333` when both fail.

The investor can pay at most zero and preserve that threshold. Under this
synthetic probability set, the provider's largest expected gross claim is
`0.800000` before its other costs. No commercial premium satisfies both sides.

The public version therefore has a modeled `0.800000` gross-claim-only funding
gap, plus provider costs and buffers. It is an explicit catalytic subsidy in
this research design—not a market premium, legal or accounting classification,
private return, project value, recovery, or diversification.

## Where value can come from

Value requires real mechanisms: milestones stop future funding, enforceable
success rights offset failures, different outcomes reduce concentration, and a
public balance sheet deliberately bears disclosed risk. Packaging creates no
cash, priority reallocates cash, and a guarantee transfers loss.

The C++ implementation contains the Project Claim Ledger, a package-verified
portfolio adapter and multi-claim joint assembler, joint-scenario risk
analysis, success participation, and the public-credit term. It keeps cash,
principal, exposure, return, and loss separate.

These are synthetic mechanics. Real use requires enforceable rights, dated
cash, risk evidence, defensible joint probabilities, provider authority and
credit analysis, and independent professional review. Nothing here is a market
price, rating, offer, financing result, or proof of animal displacement.

See the detailed
[public guarantee terms](FAILURE_CONTINGENT_PUBLIC_PARTIAL_CREDIT_GUARANTEE_V0_1.md)
and the [Project Financial Interface](PROJECT_FINANCIAL_INTERFACE_V0_1.md).
