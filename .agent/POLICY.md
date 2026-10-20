# Agent engineering policy

## Authorized work and ownership

Work from the actual branch, HEAD, worktree and authorized task. Preserve unrelated
user changes. Completed migration queues and old investigation records are not
new task instructions. PFSM, algorithm changes and threading changes need their
own authorization; file organization implies none of them.

QualPF uses C++20, CMake and Qt 6 Widgets, with Eigen/Boost supporting current
numerics. QualX is an algorithm/behavior reference. Core and Application stay
Qt-free. Application uniquely owns canonical product state. Infrastructure
implements ports using owned values and never accesses GUI or a live Session.
GUI depends on Application and borrows immutable views. Explicit composition
retains dependencies through their required lifetimes. Mandatory source/header
and configured-target gates enforce the [current architecture](../docs/architecture/overview.md).

Keep source/revision-checked atomic publication, owned worker inputs, cooperative
cancellation, execution-thread SQLite connections, queued owner-thread delivery,
safe close/drain and subscription lifetimes. Do not introduce writable canonical
mirrors, search event pumps or forced thread termination. Preserve the current QPF, SQL, CLI,
settings, export and retained [compatibility contracts](../docs/architecture/compatibility-allowlist.json).
Keep unknown fields and values at persistence/settings boundaries unchanged.

## Scientific integrity and evidence

Preserve approved values, formulas, ordering, precision, rounding, per-source
floating flags, tolerances, structured errors, references and meaningful assertions.
Never generate expected values from product output, waive unexplained mismatches,
widen tolerances or reproduce undefined Legacy behavior. Substantive numerical
corrections need independent exact/analytic/property evidence and bounded review.
Existing accepted contracts need causal conformance checks; organization does not
reopen their approvals. Follow the [numerical authority](../docs/numerical/policy.md).

FRESH_VALIDATION means actually executed. REUSED_UNCHANGED_DEPENDENCIES requires
matching implementation, inputs, references, runner, compiler/options and relevant
environment. Preserve original failures and qualifications. Do not infer full-suite,
platform or hosted-CI PASS from a subset or local run. Historical gates bind their
original snapshots, never mutable Agent-file SHA equality. Do not hash gates
recursively or create per-file proposal/review/gate packages.

Supported product configurations are Windows x64 GCC/MSVC and Linux x64 GCC;
macOS releases remain disabled. Optimize only from profiling. Future OpenMP
normally runs across candidate cards, with oneMKL single-threaded underneath;
no scheduling or numerical change follows from maintenance. GoogleTest/Benchmark
and clang-format/clang-tidy remain optional development tools.

## Git and documentation

Maintain [CHANGELOG.md](../CHANGELOG.md) as the ongoing change record. Add notable
completed changes under Unreleased and update affected current guides. Phase and
Milestone identifiers belong to retained refactoring history; new work needs no
numbered phase, phase gate or separate completion report. Existing scientific
evidence and acceptance contracts retain their scope and preservation rules.

Use bounded batches and proportional verification under [WORKFLOW](WORKFLOW.md).
Inspect diff, run the required checks and git diff --check, explicitly stage only
the batch files and make a local commit before continuing. Do not use blind
git add -A or WIP commits.

Do not push/force-push, reset/stash user work, change remotes, rebase/rewrite history,
delete protection tags or prune referenced Git/LFS objects. Preserve original
approval, receipt and failed-comparison bytes and scope. Current documentation uses
English; original frozen evidence retains its language and bytes. Use
[historical recovery](../docs/migration-history.md) only for affected authority,
independently of normal builds and tests.
