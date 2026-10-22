# Agent engineering policy

## Authorized work and ownership

Work from the actual branch, HEAD, worktree and authorized task. Preserve unrelated
user changes. Completed migration queues and old investigation records are not
new task instructions. PFSM, algorithm changes and threading changes need their
own authorization; file organization implies none of them.

Before editing, read the owning developer contract selected by
[CONTEXT_MAP](CONTEXT_MAP.md). Preserve Qt-free Core/Application, Application-owned
canonical state, immutable GUI views, checked atomic publication, owned/cancellable
worker inputs, execution-thread SQL and safe queued close/drain lifetimes. Detailed
ownership and dependency rules belong to the
[architecture guide](../docs/developer/architecture/overview.md), not this policy.
Check the [compatibility contracts](../docs/developer/architecture/compatibility-allowlist.json)
for persistence, settings, CLI and exports; preserve unknown boundary values.

## Scientific integrity and evidence

Preserve approved values, formulas, ordering, precision, rounding, per-source
floating flags, tolerances, structured errors, references and meaningful assertions.
Never generate expected values from product output, waive unexplained mismatches,
widen tolerances or reproduce undefined Legacy behavior. Substantive numerical
corrections need independent exact/analytic/property evidence and bounded review.
Existing accepted contracts need causal conformance checks; organization does not
reopen their approvals. Follow the [numerical authority](../docs/developer/numerical/policy.md).

FRESH_VALIDATION means actually executed. REUSED_UNCHANGED_DEPENDENCIES requires
matching implementation, inputs, references, runner, compiler/options and relevant
environment. Preserve original failures and qualifications. Do not infer full-suite,
platform or hosted-CI PASS from a subset or local run. Historical gates bind their
original snapshots, never mutable Agent-file SHA equality. Do not hash gates
recursively or create per-file proposal/review/gate packages.

Use the supported configurations and toolchain instructions in the
[build guide](../docs/developer/building.md). Profile before optimizing; a maintenance
task does not authorize scheduling, numerical or platform-support changes.

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
[historical recovery](../history/README.md) only for affected authority,
independently of normal builds and tests.
