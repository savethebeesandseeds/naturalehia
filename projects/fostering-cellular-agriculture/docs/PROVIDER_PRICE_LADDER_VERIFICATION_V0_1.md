# Provider Price-Ladder v0.1 Verification

## Verification result

The provider price-ladder sensitivity, strict parser, CLI, and exact-coverage
integration passed the complete project test suite in Debug and Release on
2026-08-29. Both configurations compiled as C++20 with MSVC `/W4`, `/WX`, and
`/permissive-`.

The verified boundary is narrow: physical-probability premium adequacy
conditional on full provider performance. No result is fair value, a market
quote, regulatory capital, collateral sufficiency, a credit assessment, or an
offer.

## Environment and commands

```text
Host             Windows
Generator        Visual Studio 17 2022, x64
C++ compiler     MSVC 19.44.35227.0
Windows SDK      10.0.26100.0
CMake            4.3.3
Language         C++20, extensions off
Diagnostics      /W4 /WX /permissive-
```

The existing `build/dev` directory was not modified because its Windows owner
does not permit this execution identity to write it. Verification used the
exact project-scoped directory `codex-provider-price-integration-build`:

```powershell
cmake -S . -B codex-provider-price-integration-build `
  -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_TESTING=ON `
  -DNATURALEHIA_CELLULAR_FINANCE_BUILD_CLI=ON `
  -DNATURALEHIA_CELLULAR_FINANCE_WARNINGS_AS_ERRORS=ON

cmake --build codex-provider-price-integration-build --config Debug --parallel
ctest --test-dir codex-provider-price-integration-build `
  -C Debug --output-on-failure

cmake --build codex-provider-price-integration-build --config Release --parallel
ctest --test-dir codex-provider-price-integration-build `
  -C Release --output-on-failure
```

Result: `28/28` tests passed in Debug and `28/28` in Release. The isolated
directory was removed after verification and its absence was checked.

## What was tested

The new core tests cover:

- hand-reconciled collateral carry, incremental economic-capital charge,
  fixed and variable expenses, cost recovery, target profit, and all-in floor;
- use of contractual maximum exposure rather than the smaller modeled maximum
  claim as the two disclosed cost-sizing bases;
- positive affine transformation of the complete claim-PV range and exact
  preservation of both endpoint probability witnesses;
- a valid all-in premium interval, no investor nonnegative premium capacity,
  and provider floor above investor ceiling;
- separate provider premium support and investor-target restoration, including
  their exact sum to the total all-in gap;
- the conservatively certified payable investor ceiling when it is below raw
  floating-point headroom;
- failure of reported selection when no certified investor-target-passing
  endpoint exists, with no maximum-coverage fallback;
- exact explicit-point matching and fresh arbitrary-coverage projection;
- rejection of negative carry under the v0.1 convention, missing accounting
  assertions, non-finite and out-of-range values, unsupported provider-default
  modeling, and fair-value claims; and
- stable positive results for funding/yield differences and capital charge
  rates around `1e-16` using `log1p`/`expm1` arithmetic.

The strict parser tests cover all 21 required `provider_price.*` keys,
explicit/reported selection and literal `none`, deterministic normalization and
roundtrip, caller stream-state preservation, comments/BOM/CRLF handling,
bounded input, duplicate/unknown/missing keys, malformed scalars, text safety,
non-finite/overflow inputs, every assertion and flag, and file errors.

The CLI regression runs the complete five-file chain, checks the hand fixture,
both binding witnesses, every support amount and reconciliation control, all
five normalized configurations, all 21 serialized provider key/value pairs,
usage errors, and the interpretation boundary. All pre-existing tests also
pass.

## Independent hand reconciliation

At `q=1`, selected coverage is the protection solver's certified passing upper
endpoint near `g=1/6`. With aggregate commitment `20`, contractual maximum
exposure is `10/3`; the largest modeled claim is `8/3`. Provider hurdle is zero
and settlement is two years.

```text
robust expected claim PV                           0.800000000
variable claim expense = 10% claim                0.080000000
collateral base = 50% * 10/3                      1.666666667
collateral carry = base * (1.06^2 - 1.02^2)       0.138666667
capital base = 30% * 10/3                         1.000000000
incremental capital charge = base * (1.10^2 - 1)  0.210000000
fixed upfront expense                             0.050000000
robust cost-recovery floor                        1.278666667
target underwriting profit                        0.100000000
robust all-in provider floor                      1.378666667
certified investor premium ceiling                0.000000000
provider premium support required                 1.378666667
investor-target restoration required              0.000000000
total all-in support gap                          1.378666667  DEMO million
```

The independently reconciled physical requirement ranges are:

```text
cost recovery  0.853333 / 0.985333 / 1.278667
all-in         0.953333 / 1.085333 / 1.378667
```

The investor-minimum and provider-maximum endpoints use the same adverse
fixture witness here, but the software retains them separately and does not
assume that different metrics always bind at one probability mix.

## Accounting convention audited

An independent math review found no remaining material arithmetic defect after
the correction below. Its approval depends on the convention remaining
explicit:

- collateral and capital fractions are independent exposure-sizing bases, not
  debt/equity funding shares;
- full collateral carry is a net incremental allowance already adjusted for
  any capital funding, premium float, collateral reuse, and treasury offsets;
- the capital charge is independently supplied and already net of investment
  income or collateral benefits;
- allocated capital stock and collateral principal are never expensed;
- capital does not reduce the collateral base in v0.1;
- fixed expense, claim-variable expense, carry, capital compensation, and
  target profit are mutually exclusive; and
- terminal accumulation and provider-hurdle discounting occur once.

If capital actually funds collateral, a different balance-sheet convention
must apply carry only to the treasury-funded balance and credit asset yield to
capital. The current rates cannot be reused unchanged in that model.

## Correction history

1. The initial integration calculated support gaps from raw signed investor
   headroom while interval status used the conservatively representable
   optional ceiling. An adversarial case with headroom `1.00`, certified ceiling
   `0.90`, and provider floor `0.95` exposed the contradiction. Margin, gaps,
   status, and support decomposition now consistently use the certified payable
   ceiling; negative headroom remains separately restored.
2. The first CLI fixture used an underscore spelling for reported selection,
   while the closed parser deliberately uses the canonical hyphenated token.
   The fixture was corrected; the parser remained strict.
3. Direct subtraction of nearly equal compounded growth factors was replaced
   with stable `log1p`/`expm1` arithmetic and tiny-rate regressions.
4. A final language audit removed implications that the configured support cap
   is legally validated, that allocated economic capital is returnable
   principal, or that a missing subsidy is created “catalytic value.” Reports
   now say configured monetary support cap, allocated capital stock, and
   required catalytic support.

These are verified corrections, not caveats applied after accepting a result.

## Residual boundary

Every provider cost input remains synthetic and analyst-supplied. The
nonduplication assertion is not evidence. Version 0.1 assumes both collateral
and allocated capital remain in place through settlement and excludes premium-
funded collateral, interim release, time-varying margin, provider default,
wrong-way risk, close-out, legal enforceability, disputes, delay, collateral
loss, tax, currency basis, regulatory capital, and contractual price ticks.

The next technical step is a separate correlated provider-performance and
counterparty-credit stress. It must not reduce the premium conditional on
provider performance or silently turn physical-NPV pricing into fair value.
