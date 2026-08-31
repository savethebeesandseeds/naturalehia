(function () {
  "use strict";

  window.NaturalehiaInstrumentFixture = {
    meta: {
      schemaVersion: "1.0",
      modelVersion: "ten-claim-instrument-v1-synthetic",
      status: "synthetic-mechanics-only",
      runtime: "precomputed-cpp-fixture",
      calibrated: false,
      investable: false,
      unit: "DEMO million",
      horizonMonths: 60,
      referencePrincipal: 100,
      physicalHurdlePercent: 8,
      claimCount: 10,
      scenarioCount: 9,
      factorCount: 5,
      verification: {
        date: "2026-08-30",
        debug: "68/68",
        release: "68/68",
        reportsIdentical: true,
        reconciliationMaximum: 0
      }
    },
    claims: [
      { id: 1, key: "cell-line-media", short: "Cell-line + media", name: "Cell-line and media platform", stage: "Research", limit: 6, draws: [2.4, 1.8, 1.8], receipt: 11.4, receiptMonth: 60, factors: ["biology", "supplier"] },
      { id: 2, key: "serum-free-media", short: "Serum-free media", name: "Serum-free media platform", stage: "Research", limit: 7, draws: [2.8, 2.1, 2.1], receipt: 12.95, receiptMonth: 60, factors: ["biology", "supplier"] },
      { id: 3, key: "beef-pilot", short: "Beef pilot", name: "Cultivated beef pilot", stage: "Pilot", limit: 8, draws: [2.8, 2.8, 2.4], receipt: 14, receiptMonth: 54, factors: ["biology", "regulatory", "buyer"] },
      { id: 4, key: "poultry-pilot", short: "Poultry pilot", name: "Cultivated poultry pilot", stage: "Pilot", limit: 9, draws: [3.15, 3.15, 2.7], receipt: 15.75, receiptMonth: 54, factors: ["biology", "scale", "buyer"] },
      { id: 5, key: "perfusion-demo", short: "Perfusion demo", name: "Continuous-perfusion demonstration", stage: "Demonstration", limit: 10, draws: [3, 3, 4], receipt: 16.5, receiptMonth: 48, factors: ["scale", "supplier"] },
      { id: 6, key: "scaffold-pilot", short: "Scaffold pilot", name: "Food-grade scaffold pilot", stage: "Pilot", limit: 7, draws: [2.45, 2.45, 2.1], receipt: 11.9, receiptMonth: 54, factors: ["supplier", "regulatory", "buyer"] },
      { id: 7, key: "beef-facility", short: "Beef facility", name: "First-industrial beef facility", stage: "First industrial", limit: 15, draws: [3.75, 5.25, 6], receipt: 23.25, receiptMonth: 48, factors: ["scale", "regulatory", "buyer"] },
      { id: 8, key: "poultry-facility", short: "Poultry facility", name: "First-industrial poultry facility", stage: "First industrial", limit: 14, draws: [3.5, 4.9, 5.6], receipt: 21.7, receiptMonth: 48, factors: ["scale", "supplier", "buyer"] },
      { id: 9, key: "fat-demo", short: "Fat demo", name: "Cultivated-fat demonstration", stage: "Demonstration", limit: 8, draws: [2.4, 2.4, 3.2], receipt: 13.2, receiptMonth: 48, factors: ["biology", "regulatory", "buyer"] },
      { id: 10, key: "modular-line", short: "Modular line", name: "Repeat modular production line", stage: "Repeat production", limit: 16, draws: [3.2, 4.8, 8], receipt: 24, receiptMonth: 42, factors: ["scale", "supplier", "regulatory", "buyer"] }
    ],
    factors: [
      { key: "biology", short: "Biology", name: "Biological / process", projects: [1, 2, 3, 4, 9], notional: 38, probability: [2, 13, 32], lossContribution: [0.2626, 4.1415, 11.0568], note: "Cell behavior, process stability and media performance can affect several claims together." },
      { key: "scale", short: "Scale-up", name: "Scale-up / commissioning", projects: [4, 5, 7, 8, 10], notional: 64, probability: [2, 13, 32], lossContribution: [0.463, 4.8429, 12.66], note: "Commissioning and first-of-a-kind scale risk crosses pilots, demonstrations and facilities." },
      { key: "supplier", short: "Supplier", name: "Supplier / media", projects: [1, 2, 5, 6, 8, 10], notional: 60, probability: [1, 12, 30], lossContribution: [0.10675, 4.7607, 12.695], note: "Media, scaffold and equipment dependencies create correlated interruption and cost exposure." },
      { key: "regulatory", short: "Regulatory", name: "Regulatory / qualification", projects: [3, 6, 7, 9, 10], notional: 54, probability: [0, 11, 28], lossContribution: [0, 4.1202, 11.2005], note: "Qualification delays can leave principal continuing without yet producing a resolved loss." },
      { key: "buyer", short: "Buyer", name: "Buyer / product acceptance", projects: [3, 4, 6, 7, 8, 9, 10], notional: 77, probability: [2, 13, 32], lossContribution: [0.3594, 5.3781, 14.0757], note: "Adoption and product acceptance form the broadest overlapping exposure in the synthetic pool." }
    ],
    scenarios: [
      { key: "perform", label: "All perform", weight: [40, 58, 72], path: "PPPPPPPPPP", loss: 0, continuing: 0, factors: [] },
      { key: "biology-shock", label: "Biological / process shock", weight: [2, 7, 16], path: "LDELPPPPLP", loss: 13.13, continuing: 7, factors: ["biology"] },
      { key: "scale-shock", label: "Scale-up / commissioning shock", weight: [2, 7, 16], path: "PPPLLPLLPL", loss: 23.15, continuing: 0, factors: ["scale"] },
      { key: "supplier-shock", label: "Supplier / media shock", weight: [1, 6, 14], path: "DLPPDEPLPD", loss: 10.675, continuing: 32, factors: ["supplier"] },
      { key: "regulatory-delay", label: "Regulatory / qualification delay", weight: [0, 5, 12], path: "PPDPPDDPDD", loss: 0, continuing: 54, factors: ["regulatory"] },
      { key: "buyer-shock", label: "Buyer / acceptance shock", weight: [2, 7, 16], path: "PPDLPDLLLD", loss: 17.97, continuing: 31, factors: ["buyer"] },
      { key: "biology-scale", label: "Biology + scale-up compound", weight: [0, 4, 10], path: "LDECLPLLLL", loss: 35.56, continuing: 7, factors: ["biology", "scale"] },
      { key: "supplier-reg-buyer", label: "Supplier + regulatory + buyer", weight: [0, 4, 10], path: "DLDLDCCCLC", loss: 58.005, continuing: 24, factors: ["supplier", "regulatory", "buyer"] },
      { key: "systemic", label: "Systemic commercialization freeze", weight: [0, 2, 6], path: "CCCCCCCCCC", loss: 90, continuing: 0, factors: ["biology", "scale", "supplier", "regulatory", "buyer"] }
    ],
    constructions: {
      core: {
        label: "Unsupported core",
        shortLabel: "Core",
        npv: [-18.717674, 0.661828, 15.440326],
        cashPaid: 94.624,
        cashReceived: 121.6489,
        cashNote: "Projects and recoveries",
        cashSources: { commercial: 77.149, licensing: 40.8265, recovery: 3.6734, external: 0, reserve: 0 },
        grossLoss: 9.9806,
        retainedLoss: 9.9806,
        continuingExposure: 8.52,
        impairment: "37%",
        hurdle: "At stated 8% hurdle",
        es95: 70.803,
        es99: 90,
        title: "A positive center does not make a robust asset.",
        copy: "The central mix clears the stated hurdle by only 0.662, while a permitted adverse probability mix produces an 18.718 shortfall. The unsupported claim is measurable, but not yet shown investable.",
        changes: "Combines milestone claims and makes cash, exposure, loss, return and dependence measurable.",
        unchanged: "It does not improve project cash or remove common shocks.",
        decision: "Reject current terms"
      },
      stack: {
        label: "Funded first-loss + priority",
        shortLabel: "Funded stack",
        npv: [-27.985093, -8.32175, 6.71582],
        cashPaid: 101,
        cashReceived: 128.0249,
        cashNote: "Includes 6.376 returned reserve",
        cashSources: { commercial: 77.149, licensing: 40.8265, recovery: 3.6734, external: 0, reserve: 6.376 },
        grossLoss: 9.9806,
        retainedLoss: 9.9806,
        continuingExposure: 8.52,
        impairment: "37% pool / 17% priority",
        hurdle: "Aggregate at common 8% hurdle",
        es95: 70.803,
        es99: 90,
        title: "Priority moves loss; prefunding consumes value.",
        copy: "Twenty of funded first-loss capital protects the 20–100 priority claim, but the whole commitment is paid at the start. The resulting prefunding drag leaves both classes below their stated central hurdles.",
        changes: "Allocates gross loss first to a funded 0–20 claim and principal cash first to the 20–100 priority claim.",
        unchanged: "Gross project cash, loss, continuing exposure and diversification do not change.",
        decision: "Reject current terms"
      },
      guarantee: {
        label: "30% partial-credit guarantee",
        shortLabel: "30% guarantee",
        npv: [-14.925982, 2.699617, 16.389832],
        cashPaid: 94.624,
        cashReceived: 124.64308,
        cashNote: "Includes 2.994 external support",
        cashSources: { commercial: 77.149, licensing: 40.8265, recovery: 3.6734, external: 2.99418, reserve: 0 },
        grossLoss: 9.9806,
        retainedLoss: 6.98642,
        continuingExposure: 8.52,
        impairment: "37%",
        hurdle: "Before premium, provider performs",
        es95: 49.5621,
        es99: 63,
        title: "The transfer helps, but it does not close the gap.",
        copy: "The guarantee pays 30% of resolved loss and improves investor NPV. Its adverse result remains negative before premium, while the provider requires funding. Support is visible here as outside cash—not project performance.",
        changes: "Transfers 30% of final resolved principal loss, subject to a cash cap and provider performance.",
        unchanged: "Gross loss, project cash, impairment events and continuing exposure remain visible and unchanged.",
        decision: "Protection underfunded"
      }
    },
    comparison: [
      { key: "core", label: "Unsupported core", central: 0.661828, adverse: -18.717674, note: "8% hurdle" },
      { key: "stack", label: "Funded stack, aggregate", central: -8.32175, adverse: -27.985093, note: "common 8% view" },
      { key: "guarantee", label: "Guarantee, provider performs", central: 2.699617, adverse: -14.925982, note: "before premium" },
      { key: "credit", label: "Guarantee, credit-stressed", central: 2.617773, adverse: -15.161094, note: "before premium" }
    ],
    tail: {
      standaloneEs95: 78.717,
      pooledEs95: 70.803,
      es95Benefit: 7.914,
      es95BenefitPercent: 10.053737,
      standaloneEs99: 90,
      pooledEs99: 90,
      es99Benefit: 0
    },
    guarantee: {
      providerPayment: [0.64575, 2.99418, 6.457605],
      providerCap: 30,
      modeledMaximumClaim: 27,
      investorPremiumCapacity: null,
      allInProviderFloor: 13.54135,
      adverseInvestorGap: 14.925982,
      totalSupportGap: 28.467332,
      creditStressedGap: 28.702444
    }
  };
})();
