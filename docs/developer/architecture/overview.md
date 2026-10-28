# Ownership and dependency boundaries

Final independent Milestone A accepts `5a247152` as
**CPP_ARCHITECTURE_REFACTOR_COMPLETE**. See the
[reacceptance](../../../history/architecture/milestone-a/a01/independent-review.md) for the bounded CLI correction,
all 22 mandatory criteria and actual three-platform evidence. The original blocked
review remains unchanged. The map below describes the current stricter owners and
dependency contracts added by [strict layer convergence](strict-layer-convergence.md).
PFSM now has a continuous-profile workflow and GUI entry, described in the
[Search-Match contracts](search-algorithms.md). QPF 1.0 preserves its complete frozen analysis under the
[workflow persistence contract](workflow-settings.md#qpf-10); PFSM/RIR semi-quantification
uses the same immutable selected-phase snapshot as the GUI and report.

| Layer | Responsibility | Allowed dependencies |
| --- | --- | --- |
| Core | Domain values; qualified numerics, crystallography, preprocessing, classic scoring, memory Pattern/CIF parsers | Standard C++ and configured numerical headers |
| Application | Workspace, canonical ProductSearchSession, SearchEngine/Coordinator, transactions, common preprocessing/formal extraction, ports and use cases | Core |
| Infrastructure | Execution-thread SQL, builders/writers/mappers, files, codecs, QPF and settings providers | Application/Core; explicit Qt Core/Sql or Core/Network adapters |
| GUI | User intent, immutable SessionView, editors, presenter, widgets and Qt task scheduling | Application; Qt Widgets/PrintSupport for presentation |
| Composition | GUI/CLI startup explicitly constructs owner, concrete providers, borrowing context and window in lifetime order | Lower layers |

Core/Application contain no Qt includes, containers or configuration values.
Infrastructure never reads live Session or GUI objects. Application exports only its public include tree; implementation include roots
are private. CMake expresses these dependencies without a service locator.

## Canonical state and transactions

Domain responsibility is assigned as follows:

| Domain | Owner |
| --- | --- |
| Pattern/CIF/powder parsing | Core memory parsers; Infrastructure file/binary32 boundaries |
| Crystallography, scattering, chemistry, powder, reference cards | Qualified Core kernels and exact RNE providers |
| Preprocessing, peaks, Range, Kalpha2 | Core evaluation; Application source-bound transactions and private drafts |
| Search, ranking, residual | Shared Application SearchEngine/Coordinator and canonical ProductSearchSession |
| Database access/build/import | Infrastructure execution-thread repositories/schema/writers/mappers; Application ports/use cases |
| QPF, settings, exports | Infrastructure codecs/providers; Application canonical values/formatting |
| Display coordinate arithmetic | Core expressions; Application API and admission |
| Process/network/settings capabilities | Infrastructure providers; Application owned requests/results and policy |
| GUI/plot/edit/lifetime | Intent/presenter/SessionView, QtPlotSink and drained task controller |
| Profile kernels | Qt-free Core IL templates and NNLS/TRF fitting; Application owns PFSM orchestration |
| PFSM candidate recall | Core correlation/local evaluation/selection; Application owns immutable round inputs, formal-set selection and diagnostics |

One Application Workspace owns one ProductSearchSession per product workspace.
Measurement, preprocessing, peaks, candidates, accepted phases, residual and
QPF-represented values belong to it. GUI SessionView is an immutable projection
with scoped subscriptions. Private preprocessing edits own drafts. Workers own
immutable inputs and private workspaces. Metadata preserves undefined/null and
integer/floating distinctions; Infrastructure alone performs JSON conversion.

Evaluation never changes live state. Publication validates source identity,
revision, task identity, database selection version, cancellation and Closing,
then replaces state once. Failure, cancellation, stale work and revision exhaustion
produce structured errors without partial numerical commit. Range and Kalpha2
retain their distinct approved no-op/dirty/provenance/invalidation behavior.

## Search and lifetime

SearchEngine is synchronous, Qt-free and nonpublishing. Tests/CLI and every GUI
search entry use it. CandidateReader returns owned records; SQL owns access and
decoding, while the engine delegates to the concrete Classical or PFSM workflow described
in [Search-Match contracts](search-algorithms.md).
Application resolves immutable algorithm specifications and dependencies; Core owns
background/peak stages, stored-index filtering, matching/FOM, residual and RIR.
Application query planning and candidate retention preserve operation-specific rules.
Exactly seven scientific replacement points remain: background, initial peaks,
physical FWHM, formal extraction, Classical matching, profile expansion and profile
fitting. FOM, residual, query, ranking, PFSM joint-fit orchestration and RIR are
fixed operations. GUI sorting affects display only; canonical ranking,
identity-based mutations and QPF order belong to Application.
PFSM candidate recall is a fixed method-dependent prescreen operation, separate
from profile expansion and formal fitting. It selects candidates and reports
approximate local evidence; it neither supplies formal fit starts nor calculates
the final FOM. Formal evidence flows through fixed PFSM FOM calculation into
common result management. The [workflow module map](search-algorithms.md#workflow-modules-and-ownership)
defines three subworkflows and seventeen logical modules without adding a
scientific replacement point or a generic workflow engine.
Composition supplies defaults; each prepared task retains its snapshot's resolved
module set. Algorithm changes use the canonical revision and existing invalidation
rules, with Busy/edit/Closing admission. They add no persisted schema or second state.

SearchCoordinator owns TaskId, stop_source, Busy admission and commit rules.
SearchTaskController adapts this to a dedicated QThreadPool with one worker;
running/cancelling tasks reject a second start. Inputs freeze before dispatch.
Progress is coalesced and queued; obsolete messages are ignored. SQLite connections
are created, used, closed and removed in their executing thread, with unique names.
A nonblocking reference-read guard excludes managed writes through publication.

The PDF-2 writer latches every SQL preparation, insert and transaction result;
its provider reports success only when both parsing and writing succeed. GUI
registration and CLI success therefore refuse a failed build. Existing write
order, successful records and partial-commit cancellation semantics remain;
this error propagation does not make the four-file writer an atomic transaction.

PFSM consumes a source-bound, explicitly declared continuous observation problem
and uses an explicit fixed width, compatible manual measurement or automatic
geometric FWHM without formal peak extraction. SQL finishes before the
OpenMP candidate loop; compute workers share the immutable discrete library and
each own their matrices and bounded template cache. A fixed result slot per
original candidate preserves deterministic ordering. Accepted phases and their
joint scales/shifts, common baseline and all candidate evidence publish atomically.
GUI presents Application-owned relative FOM and exact joint trial profiles.
Phase removal jointly refits and reranks the frozen universe. Core converts final
joint scales using frozen full-card intensity maxima and effective RIR values.
Application owns typed quantification availability and analysis-local RIR revisions;
GUI and reports consume that result. Revisions use checked atomic publication and
never refit, rerank or write the reference database. QPF 1.0 preserves the frozen analysis and revisions; see [workflow settings](workflow-settings.md).

Automatic peak preparation is a separate worker calculation and owner-thread
commit before search takes a new snapshot. Later search cancellation retains
those peaks. Phase acceptance, residual subtraction and related rescoring publish
once; duplicate acceptance does not subtract twice; removal does not restore residual.

Confirmed close stops admission, requests cancellation, unbinds subscriptions and
discards private child drafts. The ordinary event loop drains tasks before owned
dependencies are released. Closing suppresses late commit, plotting and action
restoration. Search/drain use no processEvents, blocking GUI waits or forced thread
termination. The two existing event pumps in modal database creation are separate
non-search work and remain outside this repository-consolidation scope.

## Preserved contracts and future work

Initial matching uses `score > minimum`, sorting/capping only when over the cap.
Rescoring uses `score > 0 && score >= minimum`, stable order and original scale.
Scored/unscored browse, stored strongest indexes, SQL order and merge contracts
remain operation-specific. QPF unknown fields, SQL schema, settings import,
CLI output/exit, formats, precision and tolerance policies remain unchanged.

The [compatibility allowlist](compatibility-allowlist.json) explains retained
binary32 providers, Application Kalpha2 policy, GUI reference utilities and test-only
snapshot transport. src/compat and its product targets are retired. These are consumed responsibilities, not native execution providers.
No Fortran ABI/runtime, canonical mirror or production AppState remains.

PFSM and algorithm/performance changes need separate authorization and evidence.
Future OpenMP candidate parallelism is profiling-driven; normally keep oneMKL
single-threaded under an outer OpenMP loop. No such optimization is enabled by
repository consolidation. Current coverage is indexed in [tests/coverage.json](../../../tests/coverage.json).

## Strict boundary enforcement

GUI includes Application interfaces and borrows immutable owned values; it does
not include Infrastructure or Core headers or call Core computations. Existing
Core domain value types cross through Application headers. Database selection,
version checks, synchronous query orchestration, file admission/save, typed
preferences, maintenance and update policy belong to Application. Infrastructure
implements the corresponding owned ports. The composition root constructs
providers before Workspace/use cases, GUI context and windows, and releases them
in reverse order. Modal database build execution and the search worker model are
unchanged.

GUI Qt values use opaque bytes with QDataStream::Qt_6_0; settings storage retains
the original keys and QVariant types. Display coordinate expressions live in a
separate Core target retaining the former GUI floating-point compilation mode.
Only actual standalone contract/search integration consumers build
qualpf_test_snapshot. Production targets cannot depend on test support.

The default build runs both the original source observations and the strict gate.
The latter resolves header ownership, indexes Core free operations and traverses
configured target link/interface properties, including facade dependencies and
public include exports. Negative controls run as qualpf.architecture.strict.
