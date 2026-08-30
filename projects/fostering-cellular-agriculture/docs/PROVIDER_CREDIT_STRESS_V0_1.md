# Provider Counterparty-Credit Stress v0.1

**Working physical-measure instrument analysis — synthetic until calibrated**

## Purpose

The pooled loss-protection term promises cash when portfolio projects lose
principal. That promise is least useful if its provider also fails in the same
states. This module tests that wrong-way risk at settlement while preserving
three separate quantities:

```text
contractual protection claim
provider price conditional on full performance
cash the investor actually collects after provider default
```

Provider failure never reduces the claim, the projects' gross loss, or the
provider's disclosed full-performance price floor. It only creates a second
loss: counterparty credit loss on the promised protection.

## Instrument chain

The calculation uses the same six versioned inputs throughout:

```text
project cash paths
physical-probability envelope
success-participation term
pooled principal-loss protection term
provider full-performance price ladder
provider counterparty-credit stress
```

The credit file is an overlay. It cannot edit a project path, reprice a claim,
or replace the joint project scenarios. Its provider identifier must exactly
match the named provider in the protection input; a mismatched credit file is
rejected. For each existing project scenario it
declares a fixed conditional set of provider outcomes, such as performance and
default. Their weights are real-world stress assumptions, not risk-neutral
default probabilities or independently optimized variables.

If `p_s` is the project-scenario probability and `theta_sj` is the fixed
conditional weight of provider outcome `j`, the expanded central probability
is:

```text
joint weight(s,j) = p_s theta_sj
```

Allowing the conditional default weight to be larger in high-claim scenarios
makes wrong-way dependence explicit. The probability envelope may reweight
complete project scenarios, but it never chooses the provider-state weights
inside a scenario independently. Every robust objective first collapses the
complete conditional provider outcomes for a scenario and then projects that
one scalar through the existing envelope.

## Settlement waterfall

Let:

```text
X_s       unchanged contractual protection claim in project scenario s
T         contractual claim-settlement month
A0        price-ladder collateral sizing base
y         declared annual collateral yield
rho_sj    fraction of grown collateral recognized in a default outcome
r_sj      fraction recovered on the remaining unsecured claim
delta_sj  unsecured-recovery delay in months
D_I(t)    investor physical-hurdle accumulation factor to month t
```

Using the price-ladder collateral as investor credit support requires explicit
analyst assertions that it is pledged to the investor and that its yield stays
in that pledged account. Those assertions are modeling inputs, not evidence of
posting, segregation, perfection, priority, or enforceability.

For a performing provider, the investor receives `X_s` at `T`. For a default:

```text
grown collateral       = A0 (1 + y)^(T/12)
recognized collateral  = rho_sj x grown collateral
collateral applied     = min(X_s, recognized collateral)
unsecured EAD           = X_s - collateral applied
delayed recovery        = r_sj x unsecured EAD
ultimate nominal loss   = (1 - r_sj) x unsecured EAD
```

Collateral is applied at `T`; unsecured recovery arrives at
`T + delta_sj`. Excess collateral is not an investor windfall. The present
values are:

```text
full-claim PV = X_s / D_I(T)

received-claim PV
  = direct provider payment / D_I(T)
  + collateral applied / D_I(T)
  + delayed recovery / D_I(T + delta_sj)

counterparty credit-loss PV = full-claim PV - received-claim PV
```

This difference includes both ultimate nonpayment and the time cost of delayed
recovery. At a zero investor hurdle, delay has no separate PV effect, but the
unpaid amount remains a loss.

## Price and support convention

The provider all-in floor remains the price ladder's full-performance floor:

```text
provider floor after credit stress = provider full-performance floor
```

The engine does not multiply that floor by survival, replace claim cost with
stressed receipts, give the provider a default rebate, or report a provider-own
default benefit. A future operating allowance for credit support would have to
be disclosed as a separate nonnegative cost.

The investor side is different. First measure the economics before premium or
new restoration support:

```text
pre-support investor NPV = underlying project NPV + received-claim PV

funded investor NPV
  = pre-support investor NPV
  - investor-funded upfront premium
  + separately identified target-restoration cash
```

The robust minimum pre-support NPV sizes investor premium capacity or target
restoration; it does not assume either cash leg already exists. If the target
does not hold, the engine reports additional counterparty-credit support
required. This is added to, and never netted against, the price ladder's
provider-premium support and pre-existing target restoration. The calculation
says how much unsupported economics remain; it does not assert that a grant,
guarantee, collateral source, or other payer exists.

## Required risk disclosures

The report keeps every conditional provider outcome visible and publishes:

- provider-default probability and default-with-positive-claim probability;
- contractual claim, direct payment, collateral, unsecured exposure, recovery,
  unpaid amount, and received and lost present value;
- expected and tail counterparty loss, including ES95 and ES99;
- the credit-adjusted investor NPV range and its feasible endpoint witnesses;
- claim/default correlation and claim-at-default concentration;
- central and robust minimum claim-PV delivery ratios;
- the unchanged provider floor, base support need, additional credit support,
  and total support if separately funded; and
- reconciliation residuals for probabilities, waterfall cash, present value,
  investor NPV, pricing invariance, and support decomposition.

The robust delivery ratio is not formed by dividing unrelated ambiguity
endpoints. It minimizes the complete ratio of expected received-claim PV to
expected full-claim PV over feasible probability measures.

## Synthetic hand case

The repository fixture retains the two-project price-ladder case near
one-sixth loss coverage. Conditional provider default probabilities are `1%`
in common success, `10%` in either single-project loss, and `50%` in common
loss. A default realizes `75%` of grown pledged collateral, recovers `25%` of
the unsecured claim after six months, and pays no direct provider cash.

At the idealized exact coverage `g = 1/6`, zero investor hurdle, and the
declared price inputs:

```text
full-performance provider floor                 1.378667
central provider-default probability             0.052200
central default-with-positive-claim probability  0.046000
central counterparty credit-loss PV              0.011133
robust counterparty credit-loss PV range     0.005837–0.052216
central claim-PV delivery ratio                   0.979126
robust minimum claim-PV delivery ratio            0.903018
additional credit support required                0.052216
```

Amounts are DEMO million. The provider floor is unchanged. Because the
full-performance investor premium ceiling was already zero, the additional
credit loss restores neither bilateral pricing nor project value; it widens
the disclosed support need. This is the intended result of an adverse test,
not a model failure.

## Calibration needed for real use

A real analysis needs a named provider and legal entity, claim trigger and
settlement mechanics, collateral agreement and custodian, eligible assets,
haircuts, segregation and insolvency priority, current exposure and
creditworthiness, recovery evidence, default timing, exclusions and disputes,
and evidence for dependence between provider failure and cellular-agriculture
losses. Conditional weights, collateral realization, and recovery must be
source-tagged and independently reviewed.

Version 0.1 is a finite physical-scenario settlement stress. It is not CVA,
fair value, a hazard-rate model, a rating, a legal opinion, a capital model, or
an investable quote. It excludes pre-settlement default, rating migration,
replacement cost, dynamic margin, close-out and netting, multiple providers,
contagion, tax, currency, accounting, regulation, and legal enforcement. Those
boundaries stay visible so a transparent stress result cannot be mistaken for
market valuation or contractual certainty.
