# Workflow settings

Application owns project configuration as four independent `WorkflowProfile`
values in `ProductState::projectSettings`: Preprocessing, CSM, PFSM and Peak
Search. These values contain input parameters, not measurements, resolved
automatic parameters or numerical caches.

Composition supplies a `DefaultSettingsStore` that outlives the Workspace.
The Infrastructure provider stores one versioned document under
`workflowDefaults/v1`. It imports established CSM/PFSM preferences once without
deleting old keys or unrelated values. Validation precedes writing; unknown
metadata survives round trips. Execution does not consult global preferences
when a project option is missing.

New projects and replacement main-pattern imports copy saved defaults. Existing
projects restore their own configuration. Legacy project migration uses a fresh
default snapshot and the file's existing fields, never the previous project's
options. Comparison-pattern imports do not reset configuration. Search option
dialogs update the canonical project only.

The pure Application API also supports existing explicitly constructed states
without project settings. Those callers retain their existing algorithm defaults;
the desktop composition always injects the versioned default provider.

`prepareWorkflow` plans a fixed dependency path in a private draft. Standalone
Preprocessing uses its profile; CSM, PFSM and Peak Search resolve missing steps
from their respective profiles. Only compatible manual Preprocessing evidence
may cross flow boundaries. Same-flow automatic reuse checks requested parameters,
module identities, input data and dependency status. It does not use revision
equality as a scientific compatibility shortcut.

Managed Gaussian preparation publishes the missing background and initial
candidates together with width evidence. Exact compatible initial candidates may
be reused. Fixed width does not require an initial search; PFSM automatic width
uses its existing geometric calculation. PFSM preparation freezes its checked
problem separately from database search, so subsequent search failure retains
the successfully published preparation. Numerical implementations are unchanged.

Canonical workflow records retain requested profiles, actual parameters, input
snapshots and dependency IDs. Automatic compound preparation has logical
background, initial, width and extraction checkpoints. A search publishes as one
step, while phase changes and RIR revisions have separate checkpoints. Records
never treat a failed calculation as a valid output.

Session-owned immutable checkpoints implement dependency-closure rollback.
Preparation is read-only; committing rechecks the source and closure, restores
affected result fields and advances revision. Configuration and unrelated display
state remain current. Workspace rejects settings changes and rollback during
running tasks, edits and close. History starts empty when a new session opens;
it is not a project-file undo stack.

The native Setting dialog edits validated defaults without requiring a pattern.
Its four left-hand workflows have independent module drafts. This dialog has no
current-project mode. It loads all four typed profiles once, creates a workflow's
editors on first selection and retains those editors until the dialog closes.
Saving validates the complete draft and preserves workflows whose editors were
never opened. Switching workflows preserves incomplete text and field errors.
The `Kα2` label uses an ordinary digit to avoid a cold subscript-font fallback on
Windows while retaining the system font.
PFSM exposes its existing `initial_peaks` profile in an Initial Peaks page between
Background and FWHM. This adds no execution stage: automatic PFSM still uses the
geometric width path, whose internal local-maximum detection supplies candidates
for half-height measurements. It does not consume the standalone SG module's
candidates or parameters. The page documents that distinction, and storage
continues to use the existing typed profile and persistence schema.
Options retain their established individual Qt dialogs and
only read/write the corresponding canonical project profile. CSM/PFSM configuration
saves use checked settings transactions and dependency confirmation. Interactive
preprocessing previews publish requested parameters and successful results in one
source-checked transaction; failed previews and Cancel leave the profile unchanged.
Native Range retains its permissive crop aperture; saved bounds are intersected
with the physical 0–180 degree domain, without substituting sample-aligned results
or changing the crop calculation.
The original estimator dialogs likewise save their requested project configuration
with successful results. No Options action writes saved defaults.
Dependency confirmation belongs to GUI support, independently of the defaults
dialog. The defaults menu has its own private entrypoint with no project mode,
execution option or fallback return value.
The Workflow dock observes immutable canonical records and transient task activity.
Failed/cancelled attempts publish status without valid outputs or undo checkpoints.

## QPF 1.0

The native project format is now `.qpf`, an independent SQLite/CBOR document.
QXP v1/v2/v3 files are rejected; no production legacy reader, writer or migration
path remains. See the [format contract](project-format.md) for the storage schema,
precision rules, resource limits and extension policy.

Projects preserve all four profiles, original/current/comparison patterns,
preprocessing inputs and evidence, workflow dependencies, algorithm identities,
full cards, current results and project presentation. PFSM preserves its frozen
problem and universe, complete recall/fit evidence, selected shifts/scales,
joint baseline, normalization, quantification references and RIR revisions.
Restoration reconstructs executable templates and verifies the saved evidence
without optimizing or searching. Existing scientific tolerances remain unchanged.

Application Workspace owns an immutable ProjectViewState with an independent
revision. Visual changes do not replace ProductState or invalidate running
scientific work. Files omit tasks, threads, weak sources, runtime selection
versions, caches and session undo. Successful restoration installs scientific and
presentation values before notifying observers, then starts with empty undo.

Projects open without consulting saved defaults or the reference database. The
existing database identity contract (member paths, lengths and timestamps)
continues to guard requests for new references. Frozen-data operations remain
available offline; a different database requires restarting the analysis before
retrieving new references.

Open/save reject running work, active scientific edits and Closing. Saving creates
and validates a temporary SQLite document, closes its execution-thread connection,
then streams it through QSaveFile with direct-write fallback disabled. A failed
save preserves the previous file and project path. A failed open preserves the
scientific state, presentation, undo and current file. Save As preserves project
UUID and creation time. Window geometry, dock layouts and user defaults stay local.

Historical QXP codecs are compiled only into explicit test support to retain
scientific comparison controls. They are absent from product targets.
