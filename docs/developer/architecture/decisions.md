# Architecture decisions

The [retained product ADR index](adr/README.md) covers wavelength, numerical
acceptance, crystallography, Range and Kalpha2 contracts. The staged decisions
below retain their implementation context; current responsibility follows
[ownership and dependencies](overview.md).

## A37-1: Qt-free Application
Context: Qt transport and concrete providers impede isolated use cases.
Decision: pure standard C++ Core/Application, Qt adapters in GUI/Infrastructure;
explicit entrypoint composition, no service locator or generic DI framework.
Alternative: retain Qt in Application. Consequence: portable value conversions and
codec separation must preserve integer/double/null/unknown-field semantics.

## A37-2: Canonical source-bound transactions
Context: product snapshots/drafts already enforce accepted numerical contracts.
Decision: one product SearchSession, immutable source, private evaluation, validated
atomic commit, explicit subscriptions. Keep approved Range/Kalpha2 distinctions.
Alternative: writable mirrors or blanket no-op suppression. Consequence: observers
are non-owning lifetime-bound projections; offline tests reuse verified primitives.

## A37-3: Shared engine and staged background search
Context: database scoring/cancellation and GUI event pumps prevent safe workers.
Decision: synchronous SearchEngine shared by CLI/tests/GUI, Application cancellation
and commit, Qt private pool(max=1); responsibility separation precedes background scheduling.
Alternative: dual scoring pipelines or concurrent replacement queue. Consequence:
Busy admission, cooperative cancel, stale rejection, owned value messages and close drain.
Automatic preparation remains a pre-search commit; residual acceptance stays compound.

## A37-4: Repository and IO ownership
Context: SQL/file operations mix parsing, scoring, persistence and state publication.
Decision: ports return owned domain/transport records, providers own execution-thread
connections, pure parse/serialize separate from IO/use-case/presentation. No schema change.
Alternative: shared GUI SQL handles. Consequence: unique connection names/read lifetime,
per-card builder transactions and existing overwrite/rejection/cancel-prefix policy.

## A37-5: Structured errors
Context: bool/status/QString/result and GUI messages currently cross layer boundaries.
Decision: retain C++20 Result/Error, add use-case context at boundaries and map GUI text
only in GUI. No automatic fallback, zero score or empty success on failure/cancel/stale.
Alternative: global error rewrite. Consequence: incremental adapters preserve external
status/exit/selector errors until consumers migrate; diagnostics remain inspectable.

## ADR-37-06 — Canonical ownership and derived Qt views

Context: Existing Widgets and current product probes consume Qt card/peak values and QObject notifications. Moving ownership and every transaction independently of GUI scheduling keeps the staged B-before-D boundary reviewable.

Decision: The Qt-free ProductSearchSession owns ProductState, revision, edit barriers and subscriptions. The existing Qt adapter keeps an immutable projection explicitly bound to the canonical source. Qt drafts are converted before checked publication; the adapter has no transaction barriers or numerical authority. Environment selection is resolved at the adapter entry during the transition and moves to the composition root in B5. The adapter's physical location and names are transitional B5/C5 work.

Alternative: Simultaneously rewriting all Widgets and background scheduling in B2 would mix ownership and lifecycle changes. A second writable Qt owner would invalidate source identity and is excluded.

Consequence: GUI projections are allowed display/transport values. Worker preparation uses the canonical immutable handle directly. A canonical notification updates the projection, and RAII subscriptions disconnect safely when either endpoint is released. Existing codec validation is independently compared with the typed validator while JSON stays at the boundary.

## ADR-37-07 — Explicit workspace composition and portable service ports

Decision: Workspace owns one ProductSearchSession and one SearchCoordinator.
SessionView borrows that owner and keeps immutable Qt projections. Admission issues
monotonic TaskIds, an immutable source/database descriptor and an Application-owned
stop token. Completion checks task, source/revision, database epoch, cancellation
and Closing before the single canonical commit. Obsolete deliveries cannot retire
an active newer task. The synchronous Qt adapter uses prepare-only publication
until D2 replaces its GUI consumers; CLI keeps the same engine and output contract.

The complete Application library links without Qt. Search settings and pattern input
come from ports whose Infrastructure providers receive immutable snapshots, never a
live Session. Pure preprocessing request construction also belongs to Application.

Repository connections and pool draining follow Qt's documented thread/lifetime
constraints: [SQL thread affinity](https://doc.qt.io/qt-6.8/threads-modules.html),
[QSqlDatabase removal](https://doc.qt.io/qt-6.8/qsqldatabase.html) and
[QThreadPool lifecycle](https://doc.qt.io/qt-6.8/qthreadpool.html).

## ADR-37-08 — Execution-thread SQL repositories and read admission

Decision: SqlCandidateReaderFactory creates, opens and destroys four private read-only connections in the calling execution thread. Names contain a UUID, task ID, role and open generation. Managers are noncopyable, reject foreign-thread access, and release temporary query/database values before removing connections. Cancellation is checked before and after each connection open, query/evaluation boundaries and final delivery. No Session or widget enters a repository.

Project-managed writers and reference readers share nonblocking path admission. Read leases count independent owners and may nest in a synchronous progress callback on the same thread; they do not recursively lock a shared_mutex. Writers acquire exclusive admission before deleting or creating files and retain it through their existing transaction lifetime. Existing CIF refusal/atomic-per-card and PDF2 overwrite/prefix semantics remain intact. Application/GUI task admission will also exclude managed writes while a prepared result awaits owner publication in D2/D4.

A controlled reader fixture independently checks same-basename isolation, worker SQL open/access/removal, foreign-thread rejection, cancellation before/during open and during query, missing-role/query failures, and read/write cleanup. See [Qt SQL ownership](https://doc.qt.io/qt-6.8/threads-modules.html) and [connection removal](https://doc.qt.io/qt-6.8/qsqldatabase.html).

## ADR 37-09 — Database construction use cases and storage responsibilities

Accepted in C3. `CifImportService` and `runCifBatch` are Qt-free Application use cases with standard C++ source/writer ports. The existing selected-block, source-scoped policy, calculation ticket, cancellation and per-card transaction rules are retained. Infrastructure supplies QFile reading, environment interpretation, SQL writing and Qt report/display conversions. GUI/CLI use the same batch implementation.

`SqlSchemaBuilder` owns thread-confined connection creation and historical overwrite admission. SQL statement generation is in `sql_schema`; the four existing schemas are unchanged. `Pdf2DatabaseWriter` owns inserts, statistics and existing 500-card transaction boundaries; `Pdf2RecordMapper` owns the unchanged deduplication, stable intensity selection and decimal formatting. PDF-2 committed-prefix behavior is retained. The old CIF capture populator has an identified qualification consumer and is assessed in C5 rather than deleted without evidence.

C3 does not change any Core numerical producer, parser, source policy, frozen comparison or tolerance. Memory use-case checks independently exercise failed reads, pre-cancellation, progress-boundary cancellation and callback exception cleanup without Qt or SQL. Existing SQL properties and product probes verify real transactions, indexes, rollback and cancellation prefixes.

## ADR 37-10 — Persistence codecs and canonical export/quantification

Accepted in C4. QXP v1/v2 parsing, model conversion and encoding live in Infrastructure. They consume and return owned ProductState values and preserve existing default wavelength, scoring/scale, range, preprocessing, residual, provenance, array validation and error contracts. QFile/QSaveFile implement storage; Application exposes a standard reader port and atomic session-open use case. MainWindow owns intent and presentation. Save preparation consumes its canonical ProductHandle and calculates weights from the canonical phases. Opaque top-level extension values now have an explicit ProductState envelope; diffraction extensions keep their existing metadata owner. Both retain wide integer, float and null distinctions.

Quantification arithmetic is Qt-free Application code. Qt RIR parsing stays at the Infrastructure text boundary, retaining its Unicode trimming and accepted numeric text semantics. An independent test executes the original Qt arithmetic and compares exact values across valid, unscored, zero/negative, missing and nonfinite inputs. Pure export preparation consumes ProductState and supplied legacy radiation-code interpretation; atomic QSaveFile writing is Infrastructure. Powder CIF preparation/completion/commit is a pure Application use case, with GUI-only callbacks and provenance JSON conversion adapted at the boundary.

Infra peak serialization consumes ProductPeaks and has no GUI header dependency. Existing GUI probes explicitly use the long-term Qt DTO conversion adapter; their inputs, assertions and expected results are unchanged. Card projection of the Qt presentation DTO belongs to GUI adapters. Historical reference snapshots and frozen producer bytes are not rewritten.

## ADR 37-11 — Consumed compatibility and presentation boundaries

Accepted in C5. Database connection/query/repository classes use explicit Infrastructure
names. Startup and preprocessing DTOs belong to Application/GUI respectively. Empty
Fortran link/ABI/search/dataset facades, the unused CifReader and constant product
selectors are removed after consumer inspection. Their unreachable branches are removed;
no numerical algorithm or selected error route changes.

Qt colours/geometry/plot lookup helpers are relocated to GUI reference utilities with
only Qt Widgets required. Binary32 snapshot conversion and selected Kalpha2 size/error
transport remain explicit Qt-free compatibility modules, with a recorded long-term
allowlist. The retired CIF capture writer/record, observation shell and call counter
are test-only. Public configuration tokens, diagnostics, schema and precision rules
are preserved. Stable source checkout attributes exclude sealed producers/references.

## ADR 37-12 — One pooled worker and queued, checked publication

Accepted in D1. SearchTaskController owns a dedicated QThreadPool with maximum one
thread. Application admission remains active through runnable teardown; a queued
completion that arrives early schedules a normal timer retry until the pool is idle.
No GUI wait/event pump is used. Workers capture owned input, task/token, factory and
private delivery values. Each reader is opened/used/destroyed inside the worker.
Progress updates coalesce into one queued message, and obsolete/Closing messages are
ignored. Owner publication always passes through SearchCoordinator's atomic checks.

Composition must retain the controller and its dependencies until drained. Closing
cancels admission/publication and suppresses presentation callbacks. QPointer guards
protect queued callbacks; callback exceptions do not strand the admission slot. Each
execution receives the caller's floating environment (including MXCSR on supported
x86 platforms) and restores the pooled thread's prior environment afterwards.

Controlled semaphores and queued-completion instrumentation prove cancellation before
execution, during reads, and after completion is enqueued; normal GUI timers run while
the reader is blocked. Source/database staleness, Busy, errors, exception cleanup,
owner-thread commit, reader destruction and exact synchronous/worker scoring parity
are verified without sleep-based races on GNU/MSVC/Linux.

## ADR 37-13: queued product search publication

All GUI search intents use the same synchronous Application SearchEngine through the dedicated controller. Table ordering uses the unchanged Qt row mapper and a private owner-thread model before a single canonical commit. Repository lifetime ends on the worker; an independent read guard excludes managed writes through pending owner delivery. Automatic peak preparation commits independently before a fresh matching snapshot. Selected-phase acceptance continues in the original row order, one atomic acceptance/residual/rescore transaction per phase. Busy admission rejects external replacement requests; only accepted internal completions continue a selected list.

Source proof at entry 185e4e36 shows matching/browsing cleared comparison presentation while preserving canonical accepted phases. The prepared query case now preserves that state before it becomes the GUI executor. Acceptance erases the selected candidate before residual rescoring, exactly as the entry GUI compound transaction did. New independent literal/shared-fixture assertions cover those mappings without changing goldens. ID lookup retains the existing certified asin angle conversion, omission of NaN angles only, and uncapped optional scoring.

The minimum confirmed-close cancellation/drain barrier is introduced with D2 because switching the producer to a worker requires it for safe composition immediately. D3 completes child-window and reentry guards. Test drivers wait for completion signals with normal event loops; input, expected values and old workflow assertions are unchanged.

## ADR 37-14: explicit GUI shutdown barrier

Confirmation is the only transition to Closing. A refused close leaves the worker and active preprocessing draft alone. Confirmed Closing cancels admission, disables actions, rejects private child drafts, removes the presenter binding, and prevents numerical publication, plot application and action restoration. Releasing an editor admission remains necessary cleanup, without publishing its numerical draft. The normal Qt event loop retires worker completion and the private pool before a queued final close releases SQL/UI dependencies. The Exit action follows closeEvent; no process exit bypass or blocking GUI drain is used.

QtPlotSink independently rejects a Closing view. Preprocessing controllers use QPointer for their window and reject preview/commit after Closing. Session notifications and root presentation callbacks guard Closing. MainWindow destroys child edits before presentation widgets and asserts drained worker ownership. Controlled tests operate real QMessageBox buttons, not dialog result-code shortcuts, and cover refused/confirmed child close, blocked reader close and already-queued completion close without sleep-based races.

The startup composition also drains `--auto --search` using the ordinary QApplication event loop and final completion signal. It preserves automatic-mode exit codes and avoids returning into an active pool destructor. Actual executable startup regression now exercises this parameter combination in addition to its original interactive startup assertions.

## ADR 37-15: explicit workspace composition and instance presentation

The executable constructs the Qt-free Application Workspace, a borrowing GUI WorkspaceContext and then its window. The context owns immutable Qt projections, reference adapters and a factory supplied by the composition root. Designer-created children resolve that explicit root through their QObject parent chain; an unbound object is rejected. No production AppState or static product/presenter owner remains. Test-only AppState names adapt existing fixture setup, with application-owned lifetimes, and are excluded from production dependencies.

Saved search options take an explicit immutable ProductHandle. Numeric request options belong to the GUI SearchRequest DTO, while SqlCandidateQuery contains only filters, projection and SQL construction. All scoring still occurs in the shared SearchEngine. SynchronousSearchService is retained solely for CLI and compatibility fixtures. SessionView remains the required immutable Qt view/intent adapter and releases its canonical subscription on destruction.

Database settings, default-directory discovery and metadata reads belong to Infrastructure. Keys, directory discovery order, SQL and conversions remain unchanged. The instance selection epoch rejects exhaustion before changing persistence or opening a connection. GUI update notifications and external-program prompts are relocated to GUI; Infrastructure no longer includes GUI headers or links Widgets. Existing nonsearch update behavior is retained.

Controlled tests construct two complete windows with independent Application owners, check their child bindings and view identities, then destroy the windows and verify presenter/subscription cleanup. An expired presenter cannot be called, foreign unbind requests have no effect, Closing rejects redraw, and Busy/exhausted selection cannot partially change settings, SQL or Session. Existing search and persistence assertions retain their inputs and expected values.

## ADR 37-16: final source and raw-evidence closure

Source boundary observations, configured CMake target/compile properties and
runtime transaction/lifetime tests provide different evidence and cannot replace
one another. Preserve independent numerical inputs and original failures.
The historical CMake boundary-helper name remains a forwarding entry only for
immutable reference consumers, as recorded in the compatibility allowlist.

The [original decision record](../../../history/README.md#recorded-design-decisions)
retains its accepted revision, historical test counts and STOP/PFSM scope. Those
outcomes apply to that checkpoint rather than the current development task.

## ADR 37-17: authorized strict layer convergence, without PFSM

This separate post-acceptance task narrows GUI product dependencies to Application.
It preserves the Milestone A conclusion and frozen evidence. Application owns
query/catalog, file/save, preferences and platform policies through pure ports;
Infrastructure owns SQL/files/Qt serialization/process/network implementations.
Kalpha2 compatibility selection/errors move into Application and snapshot
transport becomes test-only. Display arithmetic moves into Core unchanged.

Application public headers live under include/qualpf/application. The default
build checks actual configured links, header owners and Core operation references;
normal tests include causal negative controls. No algorithm, tolerance, schema,
execution model or PFSM behavior is authorized by this decision. The ongoing [strict layer contracts](strict-layer-convergence.md) link to the
original validation records and their platform scopes.
