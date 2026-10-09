# Application

`qualpf_application` is a Qt-free C++20 library. `session` owns canonical product
state, revision, atomic transactions, task admission and the synchronous SearchEngine.
`preprocessing` prepares private values without live Session access. GUI views borrow
an explicitly composed `Workspace`; no Qt adapter owns a second product state.

SearchSettingsProvider, PatternSourceReader, CandidateReader/Factory and AtomicExportTarget
are external-service ports. Providers receive immutable values and return owned results.
Qt models, codec implementations and UI intent adapters live in Infrastructure or GUI.
CIF card evaluation and plot/export projection remain portable Application services.

The complete target is independently consumed by `tests/architecture`, which performs
no Qt discovery. See the [Phase37 plan](../../docs/architecture/phase-37/plan.md) and
[current state](../../.agent/CURRENT_STATE.md).
