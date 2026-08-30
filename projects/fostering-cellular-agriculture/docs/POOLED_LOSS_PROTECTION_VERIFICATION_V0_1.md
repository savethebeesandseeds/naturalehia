# Pooled Principal-Loss Protection v0.1 Verification

## Verification conclusion

The v0.1 pooled principal-loss protection module passed the complete project
test matrix in Debug and Release with strict MSVC warnings treated as errors.
The synthetic CLI result reconciles to the hand calculation, preserves gross
project loss, and rejects a bilateral premium at the investor-target coverage.

This verifies implemented mechanics against synthetic inputs. It does not
validate a protection provider, legal obligation, empirical probability,
market price, fair value, collateral plan, capital charge, or real project.

## Environment and build policy

Verification date: 2026-08-29.

```text
Operating system: Microsoft Windows NT 10.0.26200.0
CMake: 4.3.3
Generator: Visual Studio 17 2022
Compiler: MSVC 19.44.35227.0
Windows SDK selected by CMake: 10.0.26100.0
C++ language level: C++20
Diagnostics: /W4 /WX /permissive-
```

An isolated build directory was configured with:

```powershell
cmake -S . -B codex-loss-protection-integration-build `
  -DBUILD_TESTING=ON `
  -DNATURALEHIA_CELLULAR_FINANCE_BUILD_CLI=ON `
  -DNATURALEHIA_CELLULAR_FINANCE_WARNINGS_AS_ERRORS=ON
```

Complete Debug and Release builds succeeded. The complete suite then passed:

```text
Debug:   25/25 tests passed
Release: 25/25 tests passed
```

The isolated build path was resolved inside the project, removed after
verification, and checked absent.

The suite includes all pre-existing annual, evidence-gate, staged-capital,
portfolio, adapter, probability-envelope and success-participation tests plus
the new protection core, strict parser, and CLI regressions.

## Hand reconciliation

The fixture fixes the underlying success-participation fraction at `q=1` and
uses zero investor and provider hurdles. Its four terminal scenarios are:

| Scenario | Underlying NPV | Gross resolved principal loss |
|---|---:|---:|
| common loss | -16.20 | 16.00 |
| common success | 5.80 | 0.00 |
| culture loss / scale-up success | -5.20 | 8.00 |
| culture success / scale-up loss | -5.20 | 8.00 |

The probability envelope implies:

```text
underlying investor expected NPV = -0.80 / 1.40 / 2.39
gross expected loss              =  2.48 / 3.20 / 4.80
gross impairment probability     = 30% / 38% / 50%
```

For exact proportional coverage `g`, terminal claim is `g L`. Therefore:

```text
investor NPV before premium
  minimum = -0.80 + 4.80g
  central =  1.40 + 3.20g
  maximum =  2.39 + 2.48g

provider claim-only robust premium floor = 4.80g
```

The mathematical zero-NPV threshold is `g=1/6`. The monetary cap input is
rounded slightly above `20/6`, so the floating-point solver publishes the
actual result honestly as a certified bracket:

```text
failing lower coverage:                 0.16666666666666655
investor-target-passing upper coverage: 0.16666666666666657
```

At the reported upper endpoint, six-decimal results are:

| Metric | Minimum | Central | Maximum |
|---|---:|---:|---:|
| Investor NPV before premium | 0.000000 | 1.933333 | 2.803333 |
| Expected provider claim | 0.413333 | 0.533333 | 0.800000 |
| Claim probability | 30.000000% | 38.000000% | 50.000000% |
| Nominal claim ES95 | 1.600000 | 1.866667 | 2.666667 |
| Nominal claim ES99 | 2.666667 | 2.666667 | 2.666667 |

Aggregate contractual reference principal is `20`. The reported contractual
maximum exposure is `g*20 = 3.333333`, while the largest claim in the finite
modeled scenario table is only `g*16 = 2.666667`. The engine keeps those
quantities separate so scenario incompleteness cannot understate legal
notional.

The investor's maximum non-negative premium at the threshold rounds to zero.
The provider's claim-only robust break-even floor is `0.80`. Thus:

```text
premium feasibility gap = 0.80
robust non-negative bilateral price interval = none
```

At the investor ceiling, the investor meets its target and provider robust NPV
is `-0.80`. At the provider floor, provider robust NPV is zero and investor
robust NPV is `-0.80`. This is the same missing value viewed from opposite sides
of the transfer, not a second loss and not value created by protection.

## Core and numerical controls

The adversarial core tests verify:

- exact proportional claims `gL`, never `min(gL,C)`;
- coverage domain based on monetary cap divided by aggregate contractual
  reference principal;
- separate legal cap, contractual maximum exposure, modeled maximum claim,
  contractual headroom, and uncommitted cap capacity;
- unchanged underlying project and pool gross principal loss;
- project claims summing to the pool claim and residual plus claim equaling
  gross loss;
- equal-and-opposite investor/provider settlement cash;
- continuing unreturned principal remaining outstanding and receiving no
  realized-loss claim;
- zero claims on full-recovery paths;
- terminal settlement only and rejection of existing loss layers;
- separate investor and provider terminal discount factors;
- full re-projection of combined scenario NPV at every coverage candidate;
- a two-scenario witness switch whose true robust threshold is `2/3`, where a
  frozen adverse witness would give the wrong answer;
- zero, interior, cap-boundary, full-coverage, no-loss, no-capacity and
  unattainable statuses, including adjacent floating-point boundary cases;
- provider expected payout, claim probability, ES95 and ES99 with endpoint
  witness reconstruction;
- reversed probability witnesses for provider NPV after premium;
- a conservatively representable investor premium ceiling, including a large-
  value/small-target cancellation case; and
- rejection of negative or non-finite premiums and incoherent in-memory terms.

The final synthetic report publishes zero, at six decimals, for every
computational control:

```text
underlying gross-loss change
project-to-pool claim error
two-party settlement cash error
support-cap violation
combined-NPV reconstruction error
witness reconstruction error
endpoint probability error
```

Zero controls establish arithmetic identities only. They do not establish that
the external support contract exists or will perform.

## Probability and tail primitive

The prepared probability-envelope projector was extended with a generic keyed
upper-expected-shortfall operation. It accepts an explicit tail probability and
returns minimum, central and maximum fractional-tail ES with full feasible
endpoint probability witnesses. It rejects missing, duplicate, unknown,
unsafe, non-finite or differently keyed scenario values and invalid tails.

Its tests cover fractional 5% and 10% atoms, deterministic ties, multilevel
tails, reordered inputs, the tail-one mean limit, and witness reconstruction.
This lets protection payout tails reuse the same exact bounded-simplex method
instead of maintaining a second risk implementation.

## Strict input and CLI controls

The companion protection parser has a closed 13-key schema. Tests verify:

- required, unique and known keys;
- strict booleans and finite numeric syntax;
- bounded safe text and provider identifier;
- all four explicit modeling assertions;
- `q` in `[0,1]`, non-negative cap, supported provider hurdle, and bounded
  settlement month;
- deterministic normalized output and parse/print/parse equality; and
- bounded file size, line length, and record count.

Portfolio-dependent validation separately requires settlement at the portfolio
horizon, cap no greater than aggregate contractual reference principal, a
valid underlying success-participation term, and an untranched portfolio.

The CLI regression verifies the hand numbers and interpretation warnings,
normalization of all four inputs, exit code `2` and usage for unknown options,
and exit code `2` for a missing protection file.

## Corrections made during verification

The review changed several points before this record was frozen:

1. Maximum supported coverage originally used the largest modeled scenario
   loss. It now uses aggregate contractual commitment, preventing the scenario
   table from understating legal exposure.
2. One “unused capacity” field originally mixed facility cap, contractual
   exposure and scenario claim. Those are now separate quantities.
3. The raw investor premium headroom could fail its own target after a second
   floating-point subtraction. The published maximum premium is now rounded
   conservatively and tested with the evaluator's exact arithmetic.
4. Explicit investor and provider cash legs were added at project, scenario and
   premium level and are reconciled as equal and opposite.
5. Provider present-value tail risk is projected and witness-reconciled directly
   rather than inferred from independently optimized endpoints.
6. The CLI now prints the certified coverage bracket at 17 digits; six-decimal
   output had made distinct adjacent endpoints look identical.

These corrections are part of the verification result, not post-hoc caveats.

## Residual boundary

Version 0.1 assumes the external provider is fully funded and performs in every
modeled scenario. It does not model or validate provider default, authority,
appropriation, collateral, collateral yield, funding cost, capital, expenses,
tax, legal enforceability, notice, cure, exclusions, disputes, payment delay,
recoveries after settlement, subrogation, wrong-way risk, close-out, fair value,
market liquidity, or regulatory classification.

It also settles only at the portfolio horizon because the underlying interface
does not yet contain a loss-determination date. The instrument may ultimately
be a guarantee, insurance contract, derivative, public contingent liability,
or another arrangement. That requires jurisdiction-specific specialist review.

The verified result is therefore narrow but useful: an assumed external loss
share can reshape investor tail exposure, yet this synthetic underlying still
has no mutually financeable claim-only premium under the common probability
envelope. The next research must locate a real provider mandate and balance
sheet, calibrate the now-separate all-in provider cost sensitivity, quantify
provider credit, and test whether the required catalytic support is justified by
measurable financing additionality and public or philanthropic purpose.
