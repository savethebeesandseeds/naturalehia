# Internal Software Verification Record — Evidence Gate v0.2

**Verification date:** 27 August 2026  
**Scope:** reference-project dossier loader, evidence manifest parser, compiled
requirements, assessment logic, report, CLI behavior, and Wilson public fixture  
**Result:** passed the checks below; residual limitations remain

This record is a software verification note, not validation of any source
document, facility, company, credit, valuation, transaction, or animal-impact
claim.

## Verified build

| Item | Value |
|---|---|
| Host | Windows |
| Generator | Visual Studio 17 2022 |
| MSVC compiler | 19.44.35227.0 |
| CMake | 4.3.3 |
| Language | C++20, extensions disabled |
| Diagnostics | MSVC `/W4 /permissive- /WX` |
| Configurations | Debug and Release |
| Test command | `ctest --test-dir build/dev -C <config> --output-on-failure` |
| Result | 10 / 10 passed in Debug and 10 / 10 passed in Release |

## Test inventory

1. Annual risk-model invariants and outputs.
2. Strict risk-scenario configuration parsing.
3. Evidence loader and assessor unit tests.
4. Illustrative risk CLI.
5. Adverse risk CLI.
6. Evidence CLI enforcing mode: valid incomplete dossier exits exactly `3`.
7. Evidence CLI report-only mode: exits `0` and prints the non-enforcing
   warning.
8. Evidence CLI invalid evaluation date: exits `2`.
9. Evidence CLI missing date value followed by another option: exits `2`.
10. Evidence CLI out-of-domain evaluation year: exits `2`.

## Evidence-gate regressions

The evidence unit test covers:

- a complete synthetic controlled dossier passing all four gates;
- public status failing closed even with otherwise complete synthetic records;
- open adverse evidence blocking while remaining visible;
- resolved adverse history remaining retained and counted;
- a later qualifying resolution target clearing a conflict;
- older, cross-requirement, stale, prematurely resolved, and future-dated
  resolution records failing;
- stale resolution targets returning to the unresolved count;
- controlled adverse history requiring a confined, hash-matching retained file;
- stale support and missing retained copies failing;
- primary and conjunctive source groups both being independently required;
- explicit `NOT_ESTABLISHED` metadata blocking promotion;
- decorated `UNKNOWN`, `TBD`, `PENDING`, and `UNRESOLVED` markers in record
  provenance or resolution authority failing closed;
- revalidation after in-memory mutation of schema, governance, and paths;
- source-date/access-date chronology and explicit evaluation-date controls;
- CRLF manifest portability;
- symlink escape rejection where the host permits symlink creation;
- SHA-256 mismatch rejection;
- the standard SHA-256 vectors for `abc`, the empty file, the 56-byte padding
  boundary, and one million `a` characters;
- exact gate totals of `11/15/22/9`, 57 compiled requirements, and 57 unique
  compiled IDs; and
- strict rejection of unknown requirements, duplicate IDs, and false governance
  commitments.

## Static and fixture checks

- The Wilson manifest contains 90 records with exactly 29 fields each.
- Those records comprise 33 public research rows and 57 explicit gap rows.
- It covers all 57 unique compiled requirements.
- Every compiled requirement has exactly one explicit gap row.
- The Wilson report is identical between Debug and Release when evaluated at
  27 August 2026.
- Both configurations report:
  - reference boundary: `0/11`;
  - model calibration: `0/15`;
  - cross-structure diligence checklist: `0/22`;
  - animal-impact claims: `0/9`;
  - highest allowed use: `PUBLIC RESEARCH / QUESTION FORMATION ONLY`; and
  - execution readiness: not assessed.
- Authored C++ source has no lines over 100 characters.
- All local Markdown link targets resolve.
- No stale 17-field/29-requirement identifiers, superseded evidence IDs, or old
  Wilson gate counts remain in the project documentation.

## Independent review incorporated

Three separate audits were used during implementation:

- a code-control audit identified false-promotion, loader-bypass, conjunction,
  resolution, freshness, path, portability, and CLI-exit risks;
- an institutional diligence audit identified stage circularity,
  structure-specific controls, observed-impact requirements, evidence
  governance, and missing supply, utility, IP, insurance, support-provider, and
  decommissioning workstreams; and
- an evidence-consistency audit identified the U.S./Israeli entity split,
  receivership and Chapter 15 record, current inactive FSIS status, evolving
  sale process, parcel instruments, FDA supplement, and cost/schedule
  assertions.

The implemented changes include atomic requirements, `partial`, disjoint
conjunctive source groups, schema 0.2, explicit evaluation dates, SHA-256,
review and approval metadata, conflict status, preserved/resolvable adverse
history, current court/regulator leads, and a Phase 0 authority/estate request.

## Residual limitations

- SHA-256 checks file-to-row consistency only while the manifest is held fixed.
  A coordinated replacement of file and manifest is not detected without an
  externally anchored signed release root or equivalent control. Hashing also
  does not authenticate origin, signature, completeness, or truth.
- Reviewer identity, competence, procedure quality, approval, and conflict
  declarations remain governance assertions.
- Public URLs have not been archived as controlled retained copies; this is
  deliberate, and every public Wilson requirement fails.
- The current model gate requires exact operating history. It does not yet
  implement separate pre-FID, construction, commissioning, and operating
  readiness tracks.
- The third gate is a common cross-structure diligence checklist. It neither
  approves comparison nor selects and evaluates one exact milestone, offtake,
  floor/CfD, or completion-support supplement.
- No independent implementation of SHA-256 was linked at runtime; correctness
  was checked with standard vectors, not formal verification or fuzzing.
- No legal, regulatory, tax, accounting, technical, credit, or impact
  professional has approved a real transaction through this software.
- The Wilson legal and sale status can change after the evidence cut-off. It
  must be re-evaluated against an explicit current date.

## Release decision

Evidence Gate v0.2 is suitable for fail-closed research dossier control and for
showing why the Wilson public record is not decision-ready. It is not approved
as an underwriting system, rating model, transaction execution control, or
impact assurance system.
