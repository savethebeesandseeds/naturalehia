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
