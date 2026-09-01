(function () {
  "use strict";

  const instrument = window.NaturalehiaInstrumentFixture;
  if (!instrument) return;
  const fixture = instrument;
  const investorViewScriptUrl = document.currentScript?.src || new URL("./assets/investor-view.js", document.baseURI).href;
  const investorViewAssetBaseUrl = new URL("./", investorViewScriptUrl);

  const DOMAIN_MIN = -30;
  const DOMAIN_MAX = 20;
  const stateNames = {
    P: "Performs",
    D: "Continuing exposure",
    E: "Stops after first draw",
    L: "Stops after two draws",
    C: "Resolves with 10% recovery"
  };

  const format = (value, digits = 3) => {
    const absolute = Math.abs(value).toFixed(digits);
    if (value < 0) return `−${absolute}`;
    return absolute;
  };

  const position = (value) => {
    const bounded = Math.max(DOMAIN_MIN, Math.min(DOMAIN_MAX, value));
    return ((bounded - DOMAIN_MIN) / (DOMAIN_MAX - DOMAIN_MIN)) * 100;
  };

  const setText = (id, value) => {
    const node = document.getElementById(id);
    if (node) node.textContent = value;
  };

  const factorByKey = (key) => fixture.factors.find((factor) => factor.key === key);
  const factorLabel = (key) => factorByKey(key)?.short || key;

  function renderConstruction(key) {
    const data = fixture.constructions[key];
    if (!data) return;

    const [minimum, central, maximum] = data.npv;
    setText("npv-central", format(central));
    setText("npv-min", format(minimum));
    setText("npv-max", format(maximum));
    setText("npv-hurdle", data.hurdle);
    setText("cash-paid", format(data.cashPaid));
    setText("cash-received", format(data.cashReceived));
    setText("cash-received-note", data.cashNote);
    setText("loss-retained", format(data.retainedLoss));
    setText("continuing-exposure", format(data.continuingExposure));
    setText("impairment-probability", data.impairment);
    setText("reading-title", data.title);
    setText("reading-copy", data.copy);
    setText("model-decision", data.decision);
    setText("cash-construction-label", data.label);
    setText("cash-equation-paid", format(data.cashPaid));
    setText("cash-equation-received", format(data.cashReceived));
    setText("cash-equation-continuing", format(data.continuingExposure));
    setText("cash-equation-gross-loss", format(data.grossLoss));
    setText("cash-equation-retained-loss", format(data.retainedLoss));
    setText("construction-changes", data.changes);
    setText("construction-unchanged", data.unchanged);

    const range = document.getElementById("npv-range");
    const point = document.getElementById("npv-point");
    const axis = document.getElementById("npv-axis");
    if (range) {
      const left = position(minimum);
      range.style.left = `${left}%`;
      range.style.width = `${position(maximum) - left}%`;
    }
    if (point) point.style.left = `${position(central)}%`;
    if (axis) axis.setAttribute("aria-label", `Expected NPV ranges from ${format(minimum)} to ${format(maximum)}, with a central value of ${format(central)} DEMO million.`);

    Object.entries(data.cashSources).forEach(([source, value]) => {
      const row = document.querySelector(`[data-cash-source="${source}"]`);
      if (!row) return;
      const bar = row.querySelector("i");
      const output = row.querySelector("strong");
      if (bar) {
        const percentage = Math.min(100, Math.max(0, (value / 80) * 100));
        bar.style.setProperty("--cash", `${percentage}%`);
        bar.classList.toggle("has-value", value > 0);
      }
      if (output) output.textContent = format(value);
    });

    document.querySelectorAll("[name='construction']").forEach((input) => {
      const selected = input.value === key;
      input.checked = selected;
      input.closest("label")?.classList.toggle("is-active", selected);
    });
  }

  function renderClaims() {
    const rail = document.getElementById("claim-stage-rail");
    if (!rail) return;
    rail.innerHTML = fixture.claims.map((claim) => `
      <button type="button" data-claim="${claim.id}" aria-pressed="${claim.id === 1}">
        <span>${String(claim.id).padStart(2, "0")}</span>
        <strong>${claim.short}</strong>
        <small>${claim.stage} · ${format(claim.limit)}</small>
      </button>
    `).join("");

    rail.querySelectorAll("[data-claim]").forEach((button) => {
      button.addEventListener("click", () => selectClaim(Number(button.dataset.claim)));
    });
    selectClaim(1);
  }

  function selectClaim(id) {
    const claim = fixture.claims.find((candidate) => candidate.id === id);
    if (!claim) return;
    document.querySelectorAll("[data-claim]").forEach((button) => {
      const selected = Number(button.dataset.claim) === id;
      button.classList.toggle("is-active", selected);
      button.setAttribute("aria-pressed", String(selected));
    });
    document.querySelectorAll("#risk-matrix tbody tr").forEach((row) => row.classList.toggle("is-selected-claim", Number(row.dataset.claimRow) === id));

    setText("claim-name", claim.name);
    setText("claim-stage", claim.stage);
    setText("claim-limit", format(claim.limit));
    setText("claim-draws", claim.draws.map((value) => format(value)).join(" / "));
    setText("claim-receipt", `${format(claim.receipt)} at month ${claim.receiptMonth}`);
    setText("claim-factors", claim.factors.map(factorLabel).join(" · "));
  }

  function renderRiskMatrix() {
    const table = document.getElementById("risk-matrix");
    if (!table) return;

    table.innerHTML = `
      <thead><tr><th scope="col">Claim</th>${fixture.factors.map((factor) => `<th scope="col" data-factor-column="${factor.key}"><button type="button" data-factor="${factor.key}">${factor.short}</button></th>`).join("")}</tr></thead>
      <tbody>${fixture.claims.map((claim) => `
        <tr data-claim-row="${claim.id}">
          <th scope="row"><button type="button" data-matrix-claim="${claim.id}"><span>${String(claim.id).padStart(2, "0")}</span>${claim.short}</button></th>
          ${fixture.factors.map((factor) => {
            const exposed = claim.factors.includes(factor.key);
            return `<td data-factor-cell="${factor.key}" class="${exposed ? "has-exposure" : ""}"><span aria-label="${exposed ? "Exposed" : "Not exposed"}">${exposed ? "●" : "—"}</span></td>`;
          }).join("")}
        </tr>
      `).join("")}</tbody>
    `;

    table.querySelectorAll("[data-factor]").forEach((button) => button.addEventListener("click", () => selectFactor(button.dataset.factor)));
    table.querySelectorAll("[data-matrix-claim]").forEach((button) => button.addEventListener("click", () => selectClaim(Number(button.dataset.matrixClaim))));
    selectFactor("biology");
  }

  function selectFactor(key) {
    const factor = factorByKey(key);
    if (!factor) return;

    document.querySelectorAll("[data-factor]").forEach((button) => {
      const selected = button.dataset.factor === key;
      button.classList.toggle("is-active", selected);
      button.setAttribute("aria-pressed", String(selected));
    });
    document.querySelectorAll("[data-factor-column]").forEach((cell) => cell.classList.toggle("is-selected-factor", cell.dataset.factorColumn === key));
    document.querySelectorAll("[data-factor-cell]").forEach((cell) => cell.classList.toggle("is-selected-factor", cell.dataset.factorCell === key));
    document.querySelectorAll("#risk-matrix tbody tr").forEach((row) => row.classList.toggle("is-exposed-claim", factor.projects.includes(Number(row.dataset.claimRow))));

    setText("factor-name", factor.name);
    setText("factor-note", factor.note);
    setText("factor-claims", `${factor.projects.length} / 10`);
    setText("factor-notional", format(factor.notional));
    setText("factor-probability", `${factor.probability[1]}%`);
    setText("factor-loss", format(factor.lossContribution[1]));
  }

  function renderScenarios() {
    const list = document.getElementById("scenario-list");
    if (!list) return;
    list.innerHTML = fixture.scenarios.map((scenario, index) => `
      <button type="button" data-scenario="${scenario.key}" aria-pressed="${index === 0}">
        <span>${String(index + 1).padStart(2, "0")}</span>
        <strong>${scenario.label}</strong>
        <small>${scenario.weight[1]}% central · loss ${format(scenario.loss)}</small>
      </button>
    `).join("");
    list.querySelectorAll("[data-scenario]").forEach((button) => button.addEventListener("click", () => selectScenario(button.dataset.scenario)));
    selectScenario("perform");
  }

  function selectScenario(key) {
    const scenario = fixture.scenarios.find((candidate) => candidate.key === key);
    if (!scenario) return;
    document.querySelectorAll("[data-scenario]").forEach((button) => {
      const selected = button.dataset.scenario === key;
      button.classList.toggle("is-active", selected);
      button.setAttribute("aria-pressed", String(selected));
    });

    setText("scenario-name", scenario.label);
    setText("scenario-weight", `${scenario.weight[1]}%`);
    setText("scenario-loss", format(scenario.loss));
    setText("scenario-continuing", format(scenario.continuing));
    setText("scenario-range", `${scenario.weight[0]}%–${scenario.weight[2]}%`);

    const grid = document.getElementById("claim-state-grid");
    if (grid) {
      grid.innerHTML = fixture.claims.map((claim, index) => {
        const state = scenario.path[index];
        return `<div class="claim-state claim-state-${state.toLowerCase()}" title="${claim.name}: ${stateNames[state]}"><span>${String(claim.id).padStart(2, "0")}</span><strong>${state}</strong><small>${claim.short}</small></div>`;
      }).join("");
      grid.setAttribute("aria-label", `${scenario.label}: ${fixture.claims.map((claim, index) => `${claim.name} ${stateNames[scenario.path[index]]}`).join("; ")}.`);
    }
  }

  function renderComparison() {
    const container = document.getElementById("npv-comparison");
    if (!container) return;
    container.innerHTML = `
      <div class="comparison-scale" aria-hidden="true"><span>−30</span><span class="comparison-zero-label">0</span><span>+20</span></div>
      ${fixture.comparison.map((item) => {
        const adverse = position(item.adverse);
        const central = position(item.central);
        return `<div class="comparison-row" aria-label="${item.label}: central expected NPV ${format(item.central)}, adverse expected NPV ${format(item.adverse)} DEMO million">
          <div><strong>${item.label}</strong><small>${item.note}</small></div>
          <div class="comparison-track"><i class="comparison-zero"></i><i class="comparison-span" style="left:${adverse}%;width:${central - adverse}%"></i><b class="comparison-adverse" style="left:${adverse}%" title="Adverse ${format(item.adverse)}"></b><b class="comparison-central" style="left:${central}%" title="Central ${format(item.central)}"></b></div>
          <div><span>Adverse <strong>${format(item.adverse)}</strong></span><span>Central <strong>${format(item.central)}</strong></span></div>
        </div>`;
      }).join("")}
    `;
  }

  const isRecord = (value) => value !== null && typeof value === "object" && !Array.isArray(value);
  const finiteNumber = (value) => typeof value === "number" && Number.isFinite(value) ? value : null;
  const nonemptyText = (value) => typeof value === "string" && value.trim() ? value.trim() : null;
  const normalizedStatus = (value) => (nonemptyText(value) || "").toLowerCase().replace(/[_\s]+/g, "-");

  function humanize(value) {
    const text = nonemptyText(value);
    if (!text) return null;
    return text.replace(/[_-]+/g, " ").replace(/\b\w/g, (letter) => letter.toUpperCase());
  }

  function financeabilityNode(tag, className, textValue) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (textValue !== undefined && textValue !== null) node.textContent = String(textValue);
    return node;
  }

  function formatFinanceabilityNumber(value, unit) {
    const number = finiteNumber(value);
    if (number === null) return "Not applicable";
    return `${format(number)}${nonemptyText(unit) ? ` ${unit}` : ""}`;
  }

  function formatHurdle(rate) {
    const value = finiteNumber(rate);
    if (value === null) return null;
    const percentage = value * 100;
    return `${Number.isInteger(percentage) ? percentage.toFixed(0) : percentage.toFixed(2)}%`;
  }

  function renderFinanceabilityUnavailable(reason) {
    const cases = document.getElementById("financeability-cases");
    const scale = document.getElementById("financeability-scale");
    const legend = document.getElementById("financeability-legend");
    const riskGate = document.getElementById("financeability-risk-gate");
    const riskMetrics = document.getElementById("financeability-risk-metrics");
    const fixedTerms = document.getElementById("financeability-fixed-terms");
    if (scale) scale.hidden = true;
    if (legend) legend.hidden = true;
    if (riskGate) riskGate.dataset.status = "unavailable";
    setText("financeability-risk-scope", "Separate relaxed issue-price sensitivity mandate");
    setText("financeability-risk-status", "Model unavailable");
    setText("financeability-risk-note", "This is not the strict Capital Mobilization Frontier mandate. That 25-candidate frontier rejects the same q=1, A=20, M=80 point, and changing price or support cannot cure its fixed issued-principal-risk failure.");
    if (riskMetrics) riskMetrics.replaceChildren();
    if (fixedTerms) {
      fixedTerms.replaceChildren();
      fixedTerms.hidden = true;
    }
    if (!cases) return;

    const unavailable = financeabilityNode("div", "financeability-unavailable");
    unavailable.setAttribute("role", "status");
    unavailable.append(
      financeabilityNode("span", "", "Model unavailable"),
      financeabilityNode("strong", "", "Financeability-window output has not been published for this instrument."),
      financeabilityNode("p", "", nonemptyText(reason) || "The page will show tested investor ceilings, issuer floors, reference price and support gaps only after a verified model result is supplied.")
    );
    cases.replaceChildren(unavailable);
  }

  function renderFinanceabilityRiskGate(data) {
    const container = document.getElementById("financeability-risk-gate");
    const metrics = document.getElementById("financeability-risk-metrics");
    const gate = isRecord(data) ? data : null;
    if (!container || !metrics) return;

    if (!gate) {
      container.dataset.status = "unavailable";
      setText("financeability-risk-scope", "Separate relaxed issue-price sensitivity mandate");
      setText("financeability-risk-status", "Separate sensitivity unavailable");
      setText("financeability-risk-note", "The strict 25-candidate frontier rejects this same q=1, A=20, M=80 point. These price rows remain arithmetic sensitivities, and price or support cannot cure that fixed-risk failure.");
      metrics.replaceChildren();
      return;
    }

    const rawStatus = normalizedStatus(gate.status);
    const gateStatus = rawStatus === "pass" ? "pass" : rawStatus === "fail" ? "fail" : rawStatus === "not-applicable" ? "not-applicable" : "unavailable";
    const fallbackLabel = rawStatus === "pass"
      ? "Separate sensitivity limits pass"
      : rawStatus === "fail"
        ? "Separate sensitivity limit fails"
        : rawStatus === "not-applicable"
          ? "No separate sensitivity limits declared"
          : "Separate sensitivity unavailable";
    container.dataset.status = gateStatus;
    setText("financeability-risk-scope", nonemptyText(gate.mandateLabel) || "Separate relaxed issue-price sensitivity mandate");
    const publishedLabel = nonemptyText(gate.label);
    const displayLabel = publishedLabel && !/mandates?\s+(?:pass|fail)/i.test(publishedLabel)
      ? publishedLabel
      : fallbackLabel;
    setText("financeability-risk-status", displayLabel);
    const mandateSourceNote = nonemptyText(gate.mandateSourceNote) || "This separate relaxed sensitivity is not the strict Capital Mobilization Frontier mandate. The strict 25-candidate frontier rejects the same q=1, A=20, M=80 point, and price or support cannot cure that fixed-risk failure.";
    const riskNotes = [mandateSourceNote, nonemptyText(gate.note)].filter(Boolean);
    setText("financeability-risk-note", riskNotes.join(" "));

    const metricRows = Array.isArray(gate.metrics) ? gate.metrics.filter(isRecord) : [];
    const fragments = metricRows.map((metric) => {
      const row = financeabilityNode("div", "");
      const metricStatus = normalizedStatus(metric.status);
      if (metricStatus) row.dataset.metricStatus = metricStatus;
      row.appendChild(financeabilityNode("dt", "", nonemptyText(metric.label) || "Unnamed risk test"));

      const definition = financeabilityNode("dd", "");
      const displayValue = nonemptyText(metric.displayValue);
      definition.appendChild(document.createTextNode(displayValue || formatFinanceabilityNumber(metric.value, nonemptyText(metric.unit) || "")));

      const displayLimit = nonemptyText(metric.displayLimit);
      const limit = finiteNumber(metric.limit);
      if (displayLimit || limit !== null) {
        const comparator = normalizedStatus(metric.comparator) === "minimum" ? "≥" : "≤";
        definition.appendChild(financeabilityNode("small", "", displayLimit || `${comparator} ${formatFinanceabilityNumber(limit, nonemptyText(metric.unit) || "")}`));
      }
      row.appendChild(definition);
      return row;
    });
    metrics.replaceChildren(...fragments);
  }

  function renderFinanceabilityFixedTerms(data, unit) {
    const container = document.getElementById("financeability-fixed-terms");
    if (!container) return;
    const terms = isRecord(data) ? data : null;
    if (!terms) {
      container.replaceChildren();
      container.hidden = true;
      return;
    }

    const definitions = [
      ["q", "Participation", "participationFraction", ""],
      ["A", "Junior principal", "juniorIssuedPrincipal", unit],
      ["K", "Stack detachment", "stackDetachment", unit],
      ["M", "Market principal", "marketIssuedPrincipal", unit],
      ["B", "Priority cap", "selectedMarketPriorityNonprincipalCap", unit],
      ["G", "Support capacity", "maximumSupportCapacity", unit],
      ["L", "Issuer floor", "issuerFundingFloor", unit]
    ].map(([symbol, label, key, valueUnit]) => ({
      symbol,
      label,
      value: finiteNumber(terms[key]),
      unit: valueUnit
    })).filter((entry) => entry.value !== null);

    if (definitions.length === 0) {
      container.replaceChildren();
      container.hidden = true;
      return;
    }

    const title = financeabilityNode("span", "financeability-fixed-terms-title", "Fixed tested terms");
    const items = definitions.map((entry) => {
      const item = financeabilityNode("span", "");
      const display = formatFinanceabilityNumber(entry.value, entry.unit);
      item.setAttribute("aria-label", `${entry.label}: ${display}`);
      item.append(
        financeabilityNode("small", "", `${entry.symbol} · ${entry.label}`),
        financeabilityNode("strong", "", display)
      );
      return item;
    });
    container.replaceChildren(title, ...items);
    container.hidden = false;
  }

  function resolveFinanceabilityDomain(data, cases, referencePrice) {
    const declared = isRecord(data.priceDomain) ? data.priceDomain : {};
    const declaredMinimum = finiteNumber(declared.minimum);
    const declaredMaximum = finiteNumber(declared.maximum);
    const values = [referencePrice];

    cases.forEach((entry) => {
      values.push(
        finiteNumber(entry.investorCeiling),
        finiteNumber(entry.issuerFloor),
        isRecord(entry.window) ? finiteNumber(entry.window.lower) : null,
        isRecord(entry.window) ? finiteNumber(entry.window.upper) : null
      );
    });

    const available = values.filter((value) => value !== null);
    const minimum = declaredMinimum !== null ? declaredMinimum : available.length ? Math.min(...available) : null;
    const maximum = declaredMaximum !== null ? declaredMaximum : available.length ? Math.max(...available) : null;
    if (minimum === null || maximum === null || maximum <= minimum) return null;
    return { minimum, maximum };
  }

  function financeabilityPosition(value, domain) {
    const number = finiteNumber(value);
    if (number === null || !domain) return null;
    const bounded = Math.max(domain.minimum, Math.min(domain.maximum, number));
    return ((bounded - domain.minimum) / (domain.maximum - domain.minimum)) * 100;
  }

  function appendFinanceabilityBand(track, className, start, end, domain) {
    const left = financeabilityPosition(start, domain);
    const right = financeabilityPosition(end, domain);
    if (left === null || right === null || right < left) return;
    const band = financeabilityNode("span", `financeability-band ${className}`);
    band.setAttribute("aria-hidden", "true");
    band.style.setProperty("--price-left", `${left}%`);
    band.style.setProperty("--price-width", `${right - left}%`);
    track.appendChild(band);
  }

  function appendFinanceabilityMarker(track, className, symbol, value, domain) {
    const left = financeabilityPosition(value, domain);
    if (left === null) return;
    const marker = financeabilityNode("span", `financeability-marker ${className}`);
    marker.setAttribute("aria-hidden", "true");
    marker.style.setProperty("--price-left", `${left}%`);
    if (left > 95) marker.classList.add("is-edge-right");
    marker.appendChild(financeabilityNode("span", "", symbol));
    track.appendChild(marker);
  }

  function financeabilityCaseState(entry) {
    const status = normalizedStatus(entry.status);
    const relation = normalizedStatus(entry.referencePriceRelation);
    const ceiling = finiteNumber(entry.investorCeiling);
    const floor = finiteNumber(entry.issuerFloor);
    const lower = isRecord(entry.window) ? finiteNumber(entry.window.lower) : null;
    const upper = isRecord(entry.window) ? finiteNumber(entry.window.upper) : null;
    const validWindow = lower !== null && upper !== null && lower <= upper;
    const unavailable = status === "unavailable" || status.includes("data-unavailable");
    const ineligible = relation !== "independent" || status.includes("not-independent") || status === "not-applicable";
    const noPrice = status.includes("no-nonnegative");
    const blocksWindow = unavailable || ineligible || noPrice || status.includes("do-not-overlap") || status.includes("no-financeable-window");
    const statusPermitsWindow = !status || status === "financeable-price-window" || status === "conditional-price-window";
    if (validWindow && statusPermitsWindow && !blocksWindow) return { className: "is-window", hasWindow: true, lower, upper };
    if (noPrice) return { className: "is-no-price", hasWindow: false, lower: null, upper: null };
    if (unavailable || (!status && ceiling === null && floor === null)) return { className: "is-unavailable", hasWindow: false, lower: null, upper: null };
    if (ineligible) return { className: "is-ineligible", hasWindow: false, lower: null, upper: null };
    return { className: "is-gap", hasWindow: false, lower: null, upper: null };
  }

  function financeabilityStatusLabel(entry, state) {
    const explicit = nonemptyText(entry.statusLabel);
    if (explicit) return explicit;
    if (state.hasWindow) return "Conditional price window";
    if (state.className === "is-no-price") return "No non-negative investor price";
    if (normalizedStatus(entry.status) === "not-applicable") return "Not applicable";
    if (state.className === "is-unavailable") return "Boundary data unavailable";
    if (state.className === "is-ineligible") return "Hurdle not financeability-eligible";
    const status = humanize(entry.status);
    if (status) return status;
    return "Investor and issuer requirements do not overlap";
  }

  function renderFinanceabilityScale(domain) {
    const scale = document.getElementById("financeability-scale");
    if (!scale) return;
    if (!domain) {
      scale.hidden = true;
      scale.replaceChildren();
      return;
    }

    const axis = financeabilityNode("div", "financeability-scale-axis");
    const midpoint = domain.minimum + (domain.maximum - domain.minimum) / 2;
    [domain.minimum, midpoint, domain.maximum].forEach((value, index) => {
      const tick = financeabilityNode("span", "", format(value));
      tick.style.left = `${index * 50}%`;
      axis.appendChild(tick);
    });
    scale.replaceChildren(axis);
    scale.hidden = false;
  }

  function renderFinanceabilityCase(entry, index, domain, referencePrice, unit) {
    const state = financeabilityCaseState(entry);
    const ceiling = finiteNumber(entry.investorCeiling);
    const floor = finiteNumber(entry.issuerFloor);
    const row = financeabilityNode("article", `financeability-case ${state.className}`);

    const heading = financeabilityNode("header", "financeability-case-heading");
    const hurdle = nonemptyText(entry.hurdleLabel) || formatHurdle(entry.hurdleRate);
    heading.append(
      financeabilityNode("span", "", hurdle ? `${hurdle} supplied hurdle` : "Supplied hurdle"),
      financeabilityNode("strong", "", nonemptyText(entry.label) || `Hurdle case ${index + 1}`)
    );
    const sourceParts = [nonemptyText(entry.sourceLabel), humanize(entry.referencePriceRelation)].filter(Boolean);
    if (sourceParts.length) heading.appendChild(financeabilityNode("small", "", sourceParts.join(" · ")));

    const plot = financeabilityNode("div", "financeability-case-plot");
    const track = financeabilityNode("div", "financeability-track");
    track.setAttribute("role", "img");
    if (!domain) track.dataset.unscaled = "true";
    if (domain) {
      if (ceiling !== null) appendFinanceabilityBand(track, "financeability-band-investor", domain.minimum, ceiling, domain);
      if (floor !== null) appendFinanceabilityBand(track, "financeability-band-issuer", floor, domain.maximum, domain);
      if (state.hasWindow) {
        appendFinanceabilityBand(track, "financeability-band-window", state.lower, state.upper, domain);
      } else if (ceiling !== null && floor !== null && ceiling < floor) {
        appendFinanceabilityBand(track, "financeability-band-gap", ceiling, floor, domain);
      }
      appendFinanceabilityMarker(track, "financeability-marker-ceiling", "U", ceiling, domain);
      appendFinanceabilityMarker(track, "financeability-marker-floor", "L", floor, domain);
      appendFinanceabilityMarker(track, "financeability-marker-reference", "P", referencePrice, domain);
    }

    const statusText = financeabilityStatusLabel(entry, state);
    const ariaParts = [
      nonemptyText(entry.label) || `Hurdle case ${index + 1}`,
      `investor ceiling ${formatFinanceabilityNumber(ceiling, unit)}`,
      `issuer floor ${formatFinanceabilityNumber(floor, unit)}`,
      referencePrice === null ? "reference price not applicable" : `reference price ${formatFinanceabilityNumber(referencePrice, unit)}`,
      statusText
    ];
    track.setAttribute("aria-label", ariaParts.join("; "));

    const readout = financeabilityNode("div", "financeability-price-readout");
    [["U", ceiling], ["L", floor], ["P ref", referencePrice]].forEach(([label, value]) => {
      const item = financeabilityNode("span", "");
      item.append(document.createTextNode(`${label} `), financeabilityNode("strong", "", formatFinanceabilityNumber(value, unit)));
      readout.appendChild(item);
    });
    plot.append(track, readout);

    const decision = financeabilityNode("div", "financeability-case-decision");
    decision.append(
      financeabilityNode("span", "", state.hasWindow ? "Overlap" : state.className === "is-gap" ? "Gap" : "Status"),
      financeabilityNode("strong", "", statusText)
    );
    const supportParts = [];
    const minimumSupport = finiteNumber(entry.minimumSupport);
    const supportShortfall = finiteNumber(entry.supportShortfall);
    if (state.hasWindow) {
      supportParts.push(`Overlap ${formatFinanceabilityNumber(state.lower, unit)}–${formatFinanceabilityNumber(state.upper, unit)}`);
    } else if (state.className === "is-gap" && ceiling !== null && floor !== null) {
      supportParts.push(`Price gap ${formatFinanceabilityNumber(Math.max(0, floor - ceiling), unit)}`);
    } else {
      supportParts.push("Overlap / gap Not applicable");
    }
    if (state.hasWindow || minimumSupport !== null) {
      supportParts.push(`Minimum support ${formatFinanceabilityNumber(minimumSupport, unit)}`);
    } else if (state.className === "is-gap" || state.className === "is-no-price" || supportShortfall !== null) {
      supportParts.push(`Shortfall ${formatFinanceabilityNumber(supportShortfall, unit)}`);
    } else {
      supportParts.push("Minimum support / shortfall Not applicable");
    }
    const note = nonemptyText(entry.note);
    if (note) supportParts.push(note);
    if (supportParts.length) decision.appendChild(financeabilityNode("small", "", supportParts.join(" · ")));

    row.append(heading, plot, decision);
    return row;
  }

  function renderFinanceabilityWindow(data = instrument.financeabilityWindow) {
    const casesContainer = document.getElementById("financeability-cases");
    const legend = document.getElementById("financeability-legend");
    if (!casesContainer) return;

    if (!isRecord(data) || normalizedStatus(data.status) === "unavailable" || !Array.isArray(data.cases) || data.cases.length === 0) {
      renderFinanceabilityUnavailable(isRecord(data) ? data.unavailableReason : null);
      return;
    }

    const cases = data.cases.filter(isRecord);
    if (cases.length === 0) {
      renderFinanceabilityUnavailable(data.unavailableReason);
      return;
    }

    const unit = nonemptyText(data.unit) || nonemptyText(instrument.meta?.unit) || "";
    const referencePrice = finiteNumber(data.referencePrice);
    const domain = resolveFinanceabilityDomain(data, cases, referencePrice);
    renderFinanceabilityFixedTerms(data.fixedTerms, unit);
    renderFinanceabilityRiskGate(data.riskGate);
    renderFinanceabilityScale(domain);
    casesContainer.replaceChildren(...cases.map((entry, index) => renderFinanceabilityCase(entry, index, domain, referencePrice, unit)));
    if (legend) legend.hidden = false;
  }

  function hasPublishedFinanceabilityWindow(data) {
    return isRecord(data) &&
      normalizedStatus(data.status) !== "unavailable" &&
      Array.isArray(data.cases) &&
      data.cases.some(isRecord);
  }

  function setFinanceabilityRuntimeNotice(label, copy) {
    const notice = document.querySelector(".runtime-notice p");
    if (!notice) return;
    const heading = document.createElement("strong");
    heading.textContent = label;
    notice.replaceChildren(heading, document.createTextNode(` ${copy}`));
  }

  async function hydrateFinanceabilityWindowFromWasm() {
    const section = document.getElementById("financeability-window");
    const publishedFallback = instrument.financeabilityWindow;
    if (section) section.dataset.modelRuntime = "loading";

    try {
      const loaderUrl = new URL("financeability-wasm-loader.js", investorViewAssetBaseUrl).href;
      const loader = await import(loaderUrl);
      if (typeof loader.loadFinanceabilityWindow !== "function") {
        throw new TypeError("financeability WebAssembly loader has no callable entry point");
      }
      const financeabilityWindow = await loader.loadFinanceabilityWindow();
      renderFinanceabilityWindow(financeabilityWindow);
      if (section) section.dataset.modelRuntime = "wasm";
      setFinanceabilityRuntimeNotice(
        "Browser-computed synthetic financeability window.",
        "The C++ WebAssembly module ran locally from six embedded ten-claim v0.2 inputs. Browser execution is not calibration, fair value, a bid, provider capacity, or evidence of demand."
      );
    } catch (_error) {
      if (hasPublishedFinanceabilityWindow(publishedFallback)) {
        renderFinanceabilityWindow(publishedFallback);
        if (section) section.dataset.modelRuntime = "fixture";
        setFinanceabilityRuntimeNotice(
          "Frozen analytical financeability preview.",
          "The browser C++ module is unavailable, so this page is showing only the published synthetic fixture. It is not a live price, calibration, or market-demand observation."
        );
        return;
      }

      renderFinanceabilityUnavailable(
        "The browser C++ module is unavailable or did not return valid structured output. No financeability window is inferred from the frozen page fixture."
      );
      if (section) section.dataset.modelRuntime = "unavailable";
      setFinanceabilityRuntimeNotice(
        "Browser financeability model unavailable.",
        "The frozen instrument preview remains readable, but no in-browser issue boundary has been accepted as valid structured output."
      );
    }
  }

  document.querySelectorAll("[name='construction']").forEach((input) => {
    input.addEventListener("change", () => {
      if (input.checked) renderConstruction(input.value);
    });
  });

  renderClaims();
  renderRiskMatrix();
  renderScenarios();
  renderComparison();
  renderFinanceabilityWindow();
  renderConstruction("core");
  void hydrateFinanceabilityWindowFromWasm();
})();
