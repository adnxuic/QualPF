# Current C++ tests

The normal development configuration registers 127 CTest entries, including
Classical/PFSM numerical and workflow contracts, independent references,
settings, QPF persistence, GUI/CLI, SQL, export, lifetime and architecture checks.
[Coverage](coverage.json)
lists identities and correspondence. No assertion or expected value was removed
to obtain PASS. Windows x64 GCC/MSVC and Linux x64 GCC are supported.

Unit sources are organized by `unit/core`, `unit/application`, `unit/infrastructure`;
SQL and GUI integration are under `integration/database` and `integration/gui`.
`product` exercises GUI/CLI/import/export/persistence/lifetime; `profile` retains
isolated profile properties. `compatibility` contains current C++ regression replay
adapters, not an ABI/native runtime. `reference_checks` validates independent
numerical/qualified-reference and protocol contracts. Retired-state observations
in `support` are test-only absence/compatibility fixtures.

## Candidate recall architecture

PFSM maintains only the v2 candidate-recall implementation. The existing
`qualpf.pfsm.fast_comparator` / `fast_validation` CTest names remain compatibility
identities; their current Python entry points use `pfsm_candidate_recall_*`.
Core tests preserve independent correlation and analytic template properties,
backprojection diagnostics and v2 protection/selection checks. Application tests
cover off/shadow/enabled, direct/cache, multiphase reselection, baseline replay,
formal reuse/invalidation, threads and atomic failure. The strict gate rejects
formal-to-recall dependencies and whole-analysis access from recall modules.

The retired v1-only local solver/transport and its specific extra-subset replay
counts remain reproducible at `b59fff282bc0dc38f226ebea66d86aafdd93b1b4`.
Current physical-recall tests retain the analytic zero-target, duplicate-template
and positive-gain properties; tolerances and independent reference values do not
change. Frozen scientific evidence is not rewritten. The optional CLI
`--comparison-runner` verifies exact regression equivalence against a separately
retained pre-refactor executable, in addition to independent scientific tests.

## Running

Use the shared `windows-gnu`, `windows-msvc`, or `linux-gnu` presets and a local
SDK preset when needed; see [building](../docs/developer/building.md).

```sh
cmake --preset windows-gnu
cmake --build --preset windows-gnu --parallel
ctest --preset windows-gnu
```

Provision the 15 independent reference ZIPs as described below before running
regressions. Normal tests build no Fortran, execute no old runner and fetch no
evidence objects. They require no full migration archive or pre-existing build
directory. Configuration downloads no dependencies. Matching Qt, Eigen/Boost and a C++20 compiler are
developer prerequisites. Optional GoogleTest entries retain their existing
assertions; standalone Qt-free instructions are in the build guide.

Numerical properties can take several minutes; preserve timeouts. Use the existing
Qt DLL paths on Windows. Tests writing system settings need a writable test store;
run complete Windows suites serially to avoid their shared system-scope fixture.

## Independent references

The [public build CI](../docs/developer/building.md#public-build-ci) builds the Qt
product plus existing regression executables through `qualpf_public_ci` and runs
19 registrations labeled `public_ci`. This subset uses public committed/sealed
inputs on MinGW and Linux, preserving the full normal suite and its private-input
prerequisites. Linux packaging/install controls and pinned-tool corruption tests
also run in the quality job. Hosted results are distinct from local verification.

[Manifest](reference/manifest.json) pins 169 frozen inputs. `data` contains current
wires/transcripts, tolerance policies and CIF fixtures.
`regression/references` holds small algorithm-organized accepted/proved resources
and inert `.txt` provenance. Values and hashes are unchanged; no golden is
generated from C++. See [numerical policy](../docs/developer/numerical/policy.md) and
[data provenance](data-guide.md). Logical historical keys are resolved locally
by `reference_paths.py`, without restoring full phase packages.

The 15 sealed ZIPs are excluded from the main tracked tree. Source delivery
additionally carries `symmetry-reference.zip` as the space-group generator's
preferred input under the recorded scientific-table upstream policy; the other
14 are independent test oracles, excluded from source delivery.
[Archive locations](reference/archives.json) records their byte lengths and
SHA-256 hashes, the immutable source commit already present in main history,
and the local protection tag. Original branch/tag data and all reference
expectations are unchanged. Restore only these payloads, without network access:

```sh
python -B tools/validation/restore_reference_archives.py
# Alternatively, from the locally preserved development tag:
python -B tools/validation/restore_reference_archives.py --ref fast-pfsm-v2-pre-squash-20261020
```

The tool checks every existing/source archive before writing, preserves existing
matching files and rejects corrupt data without overwriting it. Missing archives
remain verification failures, never skipped tests. A shallow clone may first need
the pinned source commit (also used explicitly by quality CI):

```sh
git fetch --no-tags --depth=1 origin b5933bba78dc3f02d8d68f0dc11d2eed4385de51
python -B tools/validation/restore_reference_archives.py
```

Source archives without Git metadata require these same independently supplied
payloads at their recorded paths. Verify them with `--verify-only`; no generated
product output can substitute for a missing reference. Full regressions also
need the original `tests/real_example_data` and historical powder examples under
applicable permission. These are excluded from delivery: in particular, the
retained `CoO.cif` has a PAULING FILE/SpringerMaterials All Rights Reserved notice.
The development checkout preserves its bytes and assertions. Missing originals
remain test failures, not skipped checks. An ordinary application build needs
none of these optional example fixtures.

```sh
python -B tools/validation/check_references.py
python -B tools/architecture/check_boundaries.py --self-test
python -B tools/agent/check_current_context.py --self-test
python -B tools/validation/check_project_baseline.py --self-test
python -B tools/validation/check_product_identity.py --self-test
```

Some test/data names retain `fortran_capture`, `phase6` or `legacy`: these check
frozen provenance and valid compatibility behavior in current C++, not migration
execution. Missing/changed input fails. Full old audits and capture tools are
available through [historical recovery](../history/README.md) independently
of normal development.

## Search-Match workflow

[Algorithm contracts](../docs/developer/architecture/search-algorithms.md) map the seven
replacement points to their numerical owners and product consumers. The
`qualpf.algorithms.*` tests retain independent scientific/error assertions and
exercise common preprocessing, physical FWHM, fixed parameter mapping, independent
formal extraction, matching evidence, stage counts and malformed-output refusal.
The compiled `tests/architecture/application_boundary.cpp` demonstrates the complete
Qt-free Classical workflow and a lawful matcher replacement.

PFSM Core contracts run as `qualpf.algorithms.pfsm_core` (or `pfsm.core` in the
Qt-free architecture harness). Analytic templates/NNLS, prescribed shift fits,
active bounds, degenerate columns, final-shift rank with all scales inactive, and
cancellation are covered. The optional
independent SciPy/Brent comparison is documented in [the PFSM contract](../docs/developer/numerical/pfsm.md).

`qualpf.quantification.core` independently checks profile/RIR analytic fractions,
reference and observation scaling, common IL integral cancellation, radiation
compatibility and malformed/overflow inputs. `qualpf.quantification.application`
fits a prescribed 25/75 mixture through real joint acceptance and exercises RIR
revision, restore, stale/busy/edit/closing rejection and removal/reselection without
new database reads or fitting on a RIR edit. Classical exact-rounding references
remain unchanged.

The retained measurable-candidate behavior is covered by independent analytic measurements in
`qualpf.algorithms.core_modules`, by subtraction/default SG/initial/formal/range/
reselection/cache/publication checks on example1, CPD2 and MIXT_5 in
`qualpf.application.preprocessing_evaluation`, and by the real screenshot-parameter
dialog plus rejection details in `qualpf.preprocessing.current_probe_state`.
Malformed diagnostic payloads fail at the Application boundary. Current product
checks use the measurable-candidate contract; their historical support sources,
frozen numerical comparisons and tolerances remain unchanged.

Initial v5 accepts explicit detection options and outputs only ordered derivative-trigger
candidates; FWHM v3 receives
the analysis spectrum and owns Gaussian fitting, fixed refinement and reduction.
Core module tests prove spectrum consumption with analytic widths, detection/fit
failure separation, radius retries, adjacency suppression, broad-width fallback
and usable-empty fallback. Strongest-three/tie/zero/invalid-width assertions use
the internal fixed reduction rather than prefilled initial widths. Product workflow
tests verify the actual postprocessing signal, single-module replacement, stage
counts, malformed candidate/FWHM results, zero-width mapping and cache invalidation.
Older initial/FWHM identities are refused; formal extraction uses version 3.

`qualpf.application.initial_peak_search` checks owned standalone evaluation, private
background preparation, frozen options, valid empty results and source-bound
invalidation. `qualpf.gui.initial_peak_options` checks real Options tabs, read-only
candidates without plot changes, result lifetime, Gaussian redetection routing and running/queued
cancellation, stale delivery and confirmed close. It accepts `--capture DIRECTORY`
to save rendered Qt settings/results tabs and the unchanged plot. Project boundary tests retain
initial settings and opaque extensions while excluding runtime candidate evidence.

`qualpf.plot.sink.controls` imports an independent XY input before any New or plot
warm-up and checks report font inheritance and measured viewport bounds. It also
checks single-publication New, save success/cancellation/failure, complete clearing
of layered comparisons and previews, repeated New, reimport and empty QXP loading.
For native Windows timing diagnostics, run the same executable in separate fresh
processes with `--startup-timing` and `--new-import-timing`, using
`QT_QPA_PLATFORM=windows` and the matching Qt kit. These modes print import times;
regressions assert behavior and font inheritance rather than timing thresholds.
Restricted Windows runs need workspace-local `TEMP`/`TMP` for temporary inputs.

Fixed operations have causal behavior coverage: real SQLite constraints, NULL and
stored-index semantics, empty preselection and row order; real CLI cap/tie and
threshold behavior; worker cancellation, source/database changes, errors and drain;
Widget display sorting with unchanged canonical snapshot, stable later ties,
identity-based mutation/acceptance and QXP candidate order. Widget and QXP share
fixed RIR with analytic 25/75, all-or-empty and ordered binary64 rounding checks.

The candidate-reader control also checks apostrophes in every textual restraint,
leading-zero and SQL-looking card IDs, embedded NUL identifiers, preserved LIKE
wildcards and literal single-card lookups. Matching, strongest-index, constrained
browse and preselected-ID routes must return the prescribed rows without widening
their predicates. Missing/closed metadata reads return defined empty information.
Catalogue metadata probes retain their unavailable sentinel and never create a
missing database as a side effect.
Closed card and metadata lookups also reject an unrelated Qt default connection.

The workspace GUI control also renders card metadata containing HTML delimiters,
entities and link-like text through the real card-selection signal. It checks
literal text, the encoded official COD destination and unchanged canonical state.

Provenance tests retain incompatible residual/scale refusal, source/task mismatch,
non-default QXP save refusal, unknown legacy provenance and explicit original-signal
restart. Method/source mismatch precedes database protection and workers.
PFSM uses continuous observations without formal peaks; its Application test
also checks a prescribed overlapping-peak baseline against independent dense/golden
search, bounded convergence failure and cancellation with no partial publication.
It covers 1/2/4/available-thread equality, candidate order independence, joint
acceptance, complete-pool retention, worker cancellation/errors, stale delivery
and QXP refusal. SQL coverage checks strict paired references and nd=0 templates.
`qualpf.gui.pfsm` drives the real Composition method buttons, typed search,
automatic width, options, independent relative-FOM projection, display threshold
and pagination, joint acceptance/removal, original intensity units and close/drain
on prescribed analytic profiles. The two-tab options checks cover default-on
recall, disabled preference reload, draft cancellation/default restoration,
validation, unknown-value preservation and frozen settings. An independent
604-card pool verifies actual screening with the default 500-card recall target
and full-pool fitting after disabling recall for a new analysis. It can capture
the native controls with `qualpf_pfsm_gui_tests --capture <directory>`.
Application coverage additionally
checks fixed/manual/automatic width priority, stale/history rejection, explicit
statistics, frozen denominators, plot curves and checked method transitions.
Source/configuration gates
reject extra replacement ports and combined Core preprocessing in Application.
The [coverage registry](coverage.json) preserves all 95 preceding normal identities,
adds geometric FWHM Core/Application, configured FWHM Application/GUI and PFSM Core/Application coverage, and maps
the former eleven modules to replacements or fixed-operation behavior; cancelled
replacement-only assertions are not reintroduced as injection seams.

`qualpf.quantification.gui` checks the real panel/report projection, analytic 25/75
percentages, RIR source escaping, editing/restoration, cancellation, stale rejection
missing-data suppression of both panel and report pies, and owner-window close
that discards the private RIR draft. Widget and direct Application RIR revisions
preserve candidate selection without a worker or database reread.

`qualpf.quantification.export` reads CSV with an independent RFC4180 parser,
checks UTF-8, embedded delimiters/newlines/quotes, binary64 round trips and
unavailable rows, and injects atomic open/write/commit failures. The GUI test
also writes through the real atomic factory and prints the shared report to PDF.

## QPF project persistence

`qualpf.project.qpf` exercises exact binary/typed metadata, independent CBOR bytes,
project identity, graph state, corruption/reference/resource-limit rejection,
atomic replacement, offline CSM/PFSM restoration and continuation. The independent
standard-library Python `qualpf.project.qpf_protocol` checks the SQLite tables,
hashes, CBOR types and IEEE bytes and writes optional extensions that the C++
reader/writer must preserve. QXP v1/v2/v3 are negative product inputs. Existing QXP
regressions use `tests/support/legacy_qxp` solely as historical scientific controls;
those codecs are not linked into the product.
