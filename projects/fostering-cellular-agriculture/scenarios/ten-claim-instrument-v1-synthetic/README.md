# Ten-claim instrument family synthetic fixture

This candidate fixture uses exactly ten disclosed cellular-agriculture claims
and nine explicit joint paths. It is the common input for the unsupported
milestone participation, the funded first-loss/priority variant, and the
failure-contingent partial-credit variant.

All cash, probabilities, recoveries, hurdles, provider terms, and counterparty
states are synthetic mechanics. They are not calibration, fair value, a
forecast, a rating, executed or legally validated terms, an offer, or evidence
that financing or public support exists.

The five declared project-factor groups overlap:

- biological/process: claims 1, 2, 3, 4, and 9; notional 38;
- scale-up/commissioning: claims 4, 5, 7, 8, and 10; notional 64;
- supplier/media: claims 1, 2, 5, 6, 8, and 10; notional 60;
- regulatory/qualification: claims 3, 6, 7, 9, and 10; notional 54; and
- buyer/product acceptance: claims 3, 4, 6, 7, 8, 9, and 10; notional 77.

These notionals are exposure classifications, not additive capital. Scenario
factor tags declare which common shock is active; the complete joint paths,
not independent project probabilities, determine loss.

The portfolio uses the narrow legacy at-par convention because each investor
milestone draw creates the same amount of contractual principal. Real acquired
claims with price, fees, capitalized amounts, or opening balances must use the
separate explicit contractual ledger.

The `*-v0.2.cfg` files deliberately pass that retained Portfolio through the
Capital Stack v0.2 bridge. In this fixture the aggregate project-outlay limit,
aggregate contractual asset-principal limit, and separately funded reserve and
issued-principal detachment `K` are all 100. Their numerical equality is caused
by the at-par fixture; v0.2 does not treat them as the same ledger. Asset
writeoff `L_s`, continuing contractual principal `O_s`, and issued-principal
cash shortfall `Q_s` remain separate outputs.

The fixed paths contain dated draws and path-contingent draw stops. They do not
encode real milestone predicates, certificates, or a separate explicit
writeoff ledger; those remain required contractual and evidence inputs for a
live claim.

From the project directory, reproduce the consolidated comparison with:

```powershell
.\build\dev\Debug\naturalehia-instrument-family.exe `
  .\scenarios\ten-claim-instrument-v1-synthetic\portfolio.cfg `
  .\scenarios\ten-claim-instrument-v1-synthetic\ambiguity.cfg `
  .\scenarios\ten-claim-instrument-v1-synthetic\success-participation.cfg `
  .\scenarios\ten-claim-instrument-v1-synthetic\capital-stack.cfg `
  .\scenarios\ten-claim-instrument-v1-synthetic\loss-protection.cfg `
  .\scenarios\ten-claim-instrument-v1-synthetic\provider-price.cfg `
  .\scenarios\ten-claim-instrument-v1-synthetic\provider-credit.cfg
```

## Capital Mobilization Frontier v0.2

The additive frontier copies `capital-stack-v0.2.cfg` for every candidate and
changes only success-cash participation `q` and junior issued principal `A`:

```powershell
docker run --rm --name fca-frontier-v02 `
  --mount type=bind,source=/mnt/host/c/Work/Naturalehia/projects/fostering-cellular-agriculture,target=/work `
  -w /work emscripten/emsdk:6.0.5 `
  node build-wasm-v02/naturalehia-capital-mobilization-frontier.js `
    scenarios/ten-claim-instrument-v1-synthetic/portfolio.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/event-polytope-v0.2.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/success-participation.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/capital-stack-v0.2.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/capital-mobilization-frontier-v0.2.cfg
```

The declared grids are:

```text
q = 0.5, 0.625, 0.75, 0.875, 1
A = 10, 20, 30, 40, 50 DEMO million
```

All 25 pairs were evaluated in the checked Emscripten Release/Node regression.
No pair passed every declared synthetic mandate:

```text
feasible candidate indices                 none
nondominated feasible candidate indices    none
minimum tested feasible q                  none
```

At `q=1,A=20`, market issued principal is 80, robust market NPV is
`-25.733095`, worst expected market `Q/M` is `27.377875%`, Q-ES95/M and
Q-ES99/M are both `87.5%`, maximum `Pr[Q>0]` is `60%`, and maximum negative-NPV
probability is `100%`. At `q=1,A=50`, robust market NPV improves to
`-5.569641`, but expected `Q/M` remains about `12.161%`, both Q tails are 80%
of the 50-unit market claim, and negative-NPV probability reaches 54%.

This is a finite-grid rejection under invented mandates. It is not a proof
that untested terms fail, a market calibration, or evidence of demand or
capital mobilization. `A` is a funded junior layer of liability shortfall `Q`;
it is not causal assignment of asset loss.

## Fixed priority cap and conditional issue-price/support window

The downstream records hold `q=1` and `A=20` fixed. First test only the market
claim's lifetime non-principal priority cap:

```powershell
docker run --rm --name fca-priority-cap-v02 `
  --mount type=bind,source=/mnt/host/c/Work/Naturalehia/projects/fostering-cellular-agriculture,target=/work `
  -w /work emscripten/emsdk:6.0.5 `
  node build-wasm-v02/naturalehia-market-priority-cap.js `
    scenarios/ten-claim-instrument-v1-synthetic/portfolio.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/event-polytope-v0.2.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/success-participation.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/capital-stack-v0.2.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/market-priority-cap-v0.2.cfg
```

That synthetic sensitivity selects tested `B=24` under a **separate, relaxed
issue-price sensitivity mandate**. Its limits are 30% expected `Q`, 90%
Q-ES95/99, 60% `Pr[Q>0]`, and five-year WAL; they are not the strict
frontier's 10%, 50%/60%, 35%, and ten-year limits. The strict 25-candidate
frontier rejects this same `q=1`, `A=20`, `M=80` point. Price or support cannot
change those fixed-risk metrics or cure that rejection. With that boundary
explicit, hold the claim and all future cash paths fixed and evaluate the
supplied buyer-price, issuer-cost, support, and independent-hurdle
sensitivities:

```powershell
docker run --rm --name fca-issue-price-v02 `
  --mount type=bind,source=/mnt/host/c/Work/Naturalehia/projects/fostering-cellular-agriculture,target=/work `
  -w /work emscripten/emsdk:6.0.5 `
  node build-wasm-v02/naturalehia-issue-price-support.js `
    scenarios/ten-claim-instrument-v1-synthetic/portfolio.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/event-polytope-v0.2.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/success-participation.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/capital-stack-v0.2.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/market-priority-cap-v0.2.cfg `
    scenarios/ten-claim-instrument-v1-synthetic/issue-price-support-v0.2.cfg
```

With `M=80`, issuer cost `F=0`, and wholly synthetic maximum no-rights support
`G=20`, the issuer floor is 60. The checked conditional results are:

| Independent synthetic hurdle | Robust gross price ceiling `P*` | Arithmetic window |
|---:|---:|---|
| 0% | 74.575200 | `[60,74.575200]` |
| 5% | 60.955502 | `[60,60.955502]` |
| 8% | 54.266905 | none |
| 10% | 50.313857 | none |
| 15% | 41.898625 | none |
| 20% | 35.170807 | none |

The status `financeable-window-found` means only that at least one supplied
arithmetic hurdle case overlaps the synthetic floor. The same report says that
no hurdle case is covered by funded or escrowed support. The reference price,
hurdles, and support capacity are invented; settled support is zero; and no
provider authority, budget, commitment, funding, escrow, or settlement is
evidenced. A “separate sensitivity limits pass” result is not a frontier pass.
These files are scenario mechanics, not an offer or executable financing
terms.
