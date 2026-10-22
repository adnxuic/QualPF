# Agent task workflow

## Start and resume

1. Inspect git status, branch and HEAD. Read the startup files in README order.
   Identify unrelated user work and any existing active task before writing.
2. Resolve task scope and success criteria from user authorization. Follow
   [CONTEXT_MAP](CONTEXT_MAP.md) from the [developer entry](../docs/developer/README.md)
   to inspect the owning contract, implementation and tests. Check the user manual
   for observable behavior; consult history only for the affected evidence.
   Use a descriptive task name; no Phase number, phase plan or phase gate is required.
3. Start one active task or resume the matching task. Compare its base/checkpoint
   with actual Git state. Reconcile differences by inspection; never reset or
   blindly replay its next_action or a completed migration queue.

## Batches, verification and completion

Group each batch by shared owner, contract and validation. Complete implementation,
review and required checks, preserve failures, inspect the diff, explicitly stage
the batch and locally commit it. A batch boundary alone is not an approval request.

| Risk | Scope | Verification |
| --- | --- | --- |
| A | Documentation, local generated files | Paths/references, format and diff |
| B | Proven ownerless code/tools/tests or equivalent organization | Consumers/build owners, affected targets and causal tests |
| C | CMake/CI/test/resource integration | Supported configure/build, affected regressions, packaging when inputs change |
| D | Numerical/persistence/product behavior | Separate authorized behavior scope, independent evidence and bounded review |

Run affected checks per batch. Full normal regressions belong to checkpoints and
final verification when the affected behavior requires them. Repeat after a
meaningful source/runner/configuration repair, not merely to accumulate PASS logs.
Use the [build/test guide](../docs/developer/building.md) and
[normal coverage](../tests/coverage.json). Preserve timeouts and original inputs.

Fix routine build/test/integration errors with original failures retained. Pause
only affected dependencies for genuinely blocked verification, unresolved material
decisions or unproved behavior changes; continue unaffected work. Record unexecuted
checks honestly. No historical archive is a normal-build/test prerequisite.

After a completed batch or before a session ends, update only concise completed
items, actual checks, concrete blockers and the next step. Keep one writer for the
task state. A paused status requires the user's explicit pause request.

Finish with a scoped result and actual validation. Record notable completed
changes under Unreleased in [CHANGELOG.md](../CHANGELOG.md), using Added, Changed,
Fixed or Removed as appropriate. Describe the resulting behavior or maintenance,
not Phase numbers or execution logs. Update affected guides and contracts in place;
ordinary tasks need no separate plan, validation index or completion report. Keep
necessary scientific evidence with its owning contract and raw logs in existing
validation locations. Clear active_task to null after completion. Future work
starts from new authorization. Do not leave a permanent completion log in this directory.

## Task state interface

[TASK_STATE.json](TASK_STATE.json) uses qualpf.agent_task_state.v1. Its only root
fields are schema, baseline_ref and active_task. baseline_ref points to
[project-baseline.json](../docs/developer/project-baseline.json); baseline
acceptance is checked separately and does not dictate the current task's status.

active_task is null or an object containing exactly:

| Field | Type and meaning |
| --- | --- |
| id, goal | Nonempty strings identifying the authorized task and outcome |
| in_scope | Nonempty array of repository-relative paths/globs, using forward slashes |
| out_of_scope | Array of nonempty descriptions of excluded work |
| status | in_progress, blocked or paused |
| base_commit | Complete 40-character lowercase Git SHA at task entry |
| checkpoint_commit | Existing batch commit SHA, or null before the first checkpoint |
| completed | Array of concise completed items |
| next_action | Nonempty description of the next concrete step; never auto-executed |
| blockers | Array of concrete blockers; nonempty when status is blocked |
| checks | Array of validation indexes described below |

Each check contains command (nonempty string), result (PASS, FAIL or NOT_VERIFIED),
kind (FRESH_VALIDATION, REUSED_UNCHANGED_DEPENDENCIES or NOT_RUN), and evidence_ref
(repository-relative file or null). NOT_RUN requires NOT_VERIFIED; reuse requires
an evidence reference and matching dependencies under POLICY. Commands are records,
not executable configuration. Absolute paths and parent traversal are rejected.

Evidence links are indexes, not embedded reports or new gates. Missing ignored
local logs on another checkout do not prevent startup, but require fresh verification
before claiming their results apply. Keep original failures in their actual log
locations. A checkpoint refers to an already existing implementation/batch commit;
do not insert the task-state commit's own SHA or construct a recursive hash gate.

The Agent checker supports --self-test and an optional --root directory for
isolated checks, exits nonzero for invalid inputs, and works without .git.
