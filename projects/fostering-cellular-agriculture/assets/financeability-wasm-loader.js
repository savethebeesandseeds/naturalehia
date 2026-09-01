// SPDX-License-Identifier: MIT

const browserModuleUrl = new URL(
  "./wasm/naturalehia-issue-price-support-browser.mjs",
  import.meta.url
);

const browserArguments = Object.freeze([
  "/financeability-inputs/portfolio.cfg",
  "/financeability-inputs/event-polytope-v0.2.cfg",
  "/financeability-inputs/success-participation.cfg",
  "/financeability-inputs/capital-stack-v0.2.cfg",
  "/financeability-inputs/market-priority-cap-v0.2.cfg",
  "/financeability-inputs/issue-price-support-v0.2.cfg",
  "--json"
]);

const maximumCapturedCharacters = 4 * 1024 * 1024;

function isRecord(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function appendCapturedLine(lines, state, values) {
  const line = values.map((value) => String(value)).join(" ");
  state.characters += line.length + 1;
  if (state.characters > maximumCapturedCharacters) {
    state.overflow = true;
    return;
  }
  lines.push(line);
}

function extractObjectCandidates(text) {
  const candidates = [];
  let start = -1;
  let depth = 0;
  let inString = false;
  let escaped = false;

  for (let index = 0; index < text.length; index += 1) {
    const character = text[index];
    if (start < 0) {
      if (character === "{") {
        start = index;
        depth = 1;
        inString = false;
        escaped = false;
      }
      continue;
    }

    if (inString) {
      if (escaped) {
        escaped = false;
      } else if (character === "\\") {
        escaped = true;
      } else if (character === "\"") {
        inString = false;
      }
      continue;
    }

    if (character === "\"") {
      inString = true;
    } else if (character === "{") {
      depth += 1;
    } else if (character === "}") {
      depth -= 1;
      if (depth === 0) {
        candidates.push(text.slice(start, index + 1));
        start = -1;
      }
    }
  }

  return candidates;
}

function parseFinanceabilityOutput(stdout) {
  const trimmed = stdout.trim();
  if (!trimmed) {
    throw new Error("the browser model returned no JSON output");
  }

  const candidates = [trimmed, ...extractObjectCandidates(trimmed).reverse()];
  for (const candidate of candidates) {
    try {
      const parsed = JSON.parse(candidate);
      if (isRecord(parsed) && isRecord(parsed.financeabilityWindow)) {
        return parsed.financeabilityWindow;
      }
    } catch (_error) {
      // A report wrapper may surround the JSON. Continue to bounded objects.
    }
  }

  throw new Error(
    "the browser model did not return an object containing financeabilityWindow"
  );
}

function normalZeroExit(error, exitStatus) {
  if (!error) return true;
  const status = typeof error.status === "number" ? error.status : exitStatus;
  return status === 0;
}

export async function loadFinanceabilityWindow() {
  let moduleNamespace;
  try {
    moduleNamespace = await import(browserModuleUrl.href);
  } catch (cause) {
    throw new Error(
      "the financeability WebAssembly asset is not published or could not be loaded",
      { cause }
    );
  }

  const createModule = moduleNamespace?.default;
  if (typeof createModule !== "function") {
    throw new TypeError("the financeability WebAssembly module has no factory");
  }

  const stdout = [];
  const stderr = [];
  const stdoutState = { characters: 0, overflow: false };
  const stderrState = { characters: 0, overflow: false };
  let exitStatus = null;
  let executionError = null;

  try {
    await createModule({
      arguments: [...browserArguments],
      print: (...values) => appendCapturedLine(stdout, stdoutState, values),
      printErr: (...values) => appendCapturedLine(stderr, stderrState, values),
      onExit: (status) => {
        exitStatus = status;
      },
      locateFile: (fileName) => new URL(`./wasm/${fileName}`, import.meta.url).href
    });
  } catch (error) {
    executionError = error;
  }

  if (stdoutState.overflow || stderrState.overflow) {
    throw new Error("the browser model exceeded the structured-output capture limit");
  }
  if ((exitStatus !== null && exitStatus !== 0) || !normalZeroExit(executionError, exitStatus)) {
    const diagnostic = stderr.slice(-3).join(" ").trim();
    throw new Error(
      diagnostic ? `the browser model failed: ${diagnostic}` : "the browser model failed"
    );
  }

  return parseFinanceabilityOutput(stdout.join("\n"));
}
