(function () {
  "use strict";

  const fixture = window.NaturalehiaInstrumentFixture;
  if (!fixture) return;

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

  document.querySelectorAll("[name='construction']").forEach((input) => {
    input.addEventListener("change", () => {
      if (input.checked) renderConstruction(input.value);
    });
  });

  renderClaims();
  renderRiskMatrix();
  renderScenarios();
  renderComparison();
  renderConstruction("core");
})();
