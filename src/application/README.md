# Application

`qualpf_application` is a Qt-free C++20 library. `session` owns canonical product
state, revision, atomic transactions, task admission and the synchronous SearchEngine.
`preprocessing` prepares private values without live Session access. GUI views borrow
an explicitly composed `Workspace`; no Qt adapter owns a second product state.

SearchSettingsProvider, PatternSourceReader, CandidateReader/Factory and AtomicExportTarget
are external-service ports. Providers receive immutable values and return owned results.
Qt models, codec implementations and UI intent adapters live in Infrastructure or GUI.
CIF card evaluation and plot/export projection remain portable Application services.

AlgorithmSpec and its explicit factory assemble seven typed replacement modules:
background, initial peaks, physical FWHM, formal extraction, Classical matching,
profile expansion and profile fitting.
Canonical snapshots and prepared tasks own the immutable selection. Public common
preprocessing and formal extraction evaluate source-bound values; shared search
preparation drives GUI and CLI. SearchEngine delegates to a concrete Classical
workflow with fixed FOM/residual processing and common query/ranking operations.
See [algorithm contracts](../../docs/developer/architecture/search-algorithms.md).
PFSM fits continuous observations and freezes its discrete reference universe.
Source-bound PFSM/RIR quantification reads the selected joint baseline and frozen
full-card maxima, with typed RIR values supplied by the reference reader. RIR
revisions are checked Workspace transactions, scoped to that frozen analysis;
they never modify the database or rerun the fit. QPF preserves complete frozen
PFSM evidence and supports validated continuation.

The complete target is independently consumed by the compiled example in
[`tests/architecture`](../../tests/architecture/application_boundary.cpp), which
performs no Qt discovery. See the [build guide](../../docs/developer/building.md)
for standalone consumption and [ownership](../../docs/developer/architecture/overview.md)
for publication and lifetime rules.
