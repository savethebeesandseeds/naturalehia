# Participation-Pool v0.1 Verification

Status: verified synthetic contract-mechanics implementation, 2026-08-29.

## Scope

This record covers the explicit-joint-scenario participation-pool kernel, its
strict reloadable configuration format, canonical printer, human-readable CLI,
the controlling two-project fixture, and the first staged-capital provider-cash
adapter. It does not validate any real project, scenario probability, recovery,
contract, market price, or investor return.

Authoritative implementation surfaces:

- `include/naturalehia/cellular_finance/portfolio.hpp`
- `include/naturalehia/cellular_finance/portfolio_config.hpp`
- `include/naturalehia/cellular_finance/portfolio_adapters.hpp`
- `src/portfolio.cpp`
- `src/portfolio_config.cpp`
- `src/portfolio_adapters.cpp`
- `apps/portfolio_cli/main.cpp`
- `scenarios/two-project-participation-pool-synthetic.cfg`

## Reproducible build gate

The project was configured with CMake 4.3.3 and compiled with MSVC
19.44.35227.0 as C++20 using `/W4`, `/permissive-`, and `/WX`. Both Debug and
Release builds succeeded. The complete CTest suite passed in each build:

```text
Debug:   16/16 passed
Release: 16/16 passed
```

The suite includes the pre-existing annual, evidence, and staged-capital
regressions as well as the portfolio kernel, strict parser, and end-to-end CLI
fixture. The adapter suite adds actual-path, mixed-source, payer-identity,
principal/PIK, monthly cash, amount-bound, and liquidity-order regressions. The
new integration did not require weakening compiler diagnostics or removing an
existing test.

Reproduction from the project directory:

```powershell
cmake -S . -B build/verify `
  -DNATURALEHIA_CELLULAR_FINANCE_WARNINGS_AS_ERRORS=ON
cmake --build build/verify --config Debug
ctest --test-dir build/verify -C Debug --output-on-failure
cmake --build build/verify --config Release
ctest --test-dir build/verify -C Release --output-on-failure
```

## Kernel controls exercised

Deterministic unit tests establish that:

1. every joint scenario contains every configured project exactly once;
2. accepted near-one weights are stably normalized and actual weight mass is
   used at quantile and expected-shortfall boundaries;
3. draws cannot exceed commitment and principal cannot be returned before or
   above funded principal;
4. all project receipts that reference one scenario-level source share its
   finite cumulative cash budget;
5. a continuing claim's unreturned principal is outstanding exposure, while a
   resolved claim's unreturned principal is realized loss;
6. draws and pool costs settle before same-month receipts for gross liquidity;
7. source cash, monthly pool cash, project loss, tail contributions, and
   optional loss layers reconcile;
8. scenario, project, source, and cash-record input order does not change
   published financial results;
9. perfect joint loss produces no diversification benefit, while mutually
   exclusive loss reduces pool expected shortfall without reducing expected
   loss or creating cash; and
10. resource and numeric bounds prevent accepted dimensions from causing
    unbounded dense work or non-finite outputs.

An end-to-end regression initially exposed an economically exact zero ES99
benefit formatted as negative zero after floating-point subtraction. The
engine now converts only differences inside its scale-aware reconciliation
tolerance to positive zero and throws if diversification is materially
negative. A dedicated four-state regression preserves this behavior without
silently flooring a real adverse result.

## Strict format controls exercised

The parser tests cover a complete canonical round trip, UTF-8 BOM placement,
classic-locale output, stream-state restoration, every enum family, zero and
nonzero nested counts, and semantic validation after parsing. They reject:

- unknown, duplicate, and missing keys;
- malformed booleans, integers, decimals, and enum labels;
- a BOM anywhere other than the first bytes of the file;
- files above 16 MiB and lines above 4,096 bytes;
- declared counts beyond project, scenario, source, factor, layer, pair, and
  aggregate cash-record guards; and
- syntactically valid files that violate financial invariants.

`--print-normalized` emits the complete one-based configuration and the result
can be loaded again. It does not add hidden defaults or inferred scenarios.

## Staged-capital adapter controls exercised

The adapter tests and runtime hard gates establish that:

1. actual configured paths, rather than the all-provider-performs fee replay,
   preserve their IDs, raw physical weights, dated provider cash, principal
   loss, and provider-hurdle NPV;
2. positive completion repayments are exhausted by finite positive allocations
   to safe caller-declared source IDs;
3. several payers may share one taxonomy, while one source ID cannot change
   taxonomy across cases or collide with generated fee and recovery buckets;
4. each source's monthly budget equals mapped receipts, and independent
   normalized-weight source totals reconcile to the portfolio summary;
5. principal return is capped at funded principal and divided pro rata across
   mixed completion sources; paid excess is yield, while unpaid PIK claim
   writeoff is not relabeled as principal loss;
6. a repayment equal to funded principal produces zero portfolio impairment
   even if contractual PIK is written off;
7. factor tags remain empty rather than being inferred from outcome labels;
8. a staged amount outside the portfolio's `1e6`-million common bound is
   rejected rather than scaled, split, or clipped; and
9. monthly net cash reconciles while the portfolio's draws-before-receipts
   liquidity convention is conservatively allowed to exceed exact staged-ledger
   peak outlay.

## Hand-checked economic fixture

The controlling fixture has two 10-million commitments, 20 drawn in every
state, a 0.2 pool cost, a 13 success receipt, and a 2 failure recovery. Its
four declared state weights are 0.62, 0.18, 0.18, and 0.02.

The executable reproduces:

```text
expected realized principal loss       3.2
probability of any impairment          38%
expected NPV at the declared 0% hurdle 1.4
negative-NPV probability               38%
pool loss p95 / p99                    8 / 16
pool ES95 / ES99                       11.2 / 16
sum standalone ES95 / ES99             16 / 16
diversification benefit ES95 / ES99    4.8 / 0
diversification ratio ES95 / ES99      30% / 0%
pairwise loss correlation              -0.125
peak gross funding need                20.2
```

The result is intentionally mixed: idiosyncratic states reduce ES95, but the
rare common-loss state eliminates the benefit at ES99. This demonstrates why
project count alone is not a diversification argument.

## Residual boundary

The verified implementation remains synthetic-only and physical-measure only.
It has no empirical calibration, risk-neutral probability measure, liquidity
premium, market spread, legal waterfall, tax or accounting treatment,
counterparty default model, empirical payer evidence, or annual path adapter.
Completion payer IDs and allocations remain analyst inputs; non-completion cash
remains one project-level recovery bucket rather than payer-level provenance.
The optional loss layers allocate principal loss but do not receive a cash
waterfall or a solved price. No output is suitable for an offering, valuation,
rating, or real investment decision.
