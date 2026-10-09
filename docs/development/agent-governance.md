# Agent engineering context maintenance

Entry: `c78d6af01d2b5f53d4b238bd34d9893281b6c4af`, including the subsequent
strict-layer three-platform closure. This maintenance changes Agent governance,
current documentation and Python validation only. Product behavior, numerics,
threading, persistence and PFSM are unchanged.

## Result and authority

The [Agent entry](../../AGENTS.md) loads README, POLICY and TASK_STATE: three
mandatory Agent files instead of eight. The directory contains exactly six files;
WORKFLOW is read before changes, and CONTEXT_MAP supplies task-specific navigation.
Long-term rules and one mutable handoff replace duplicate phase/status summaries.

TASK_STATE uses `qualpf.agent_task_state.v1`. It stores one active task or null,
with bounded scope, complete Git identities, a checkpoint, next action, blockers
and validation indexes. Commands are never automatically executed. Historical STOP
and current task status are independent. The empty state is restored on completion.

[Project baseline](project-baseline.json) uses `qualpf.project_baseline.v1`.
Milestone F, final independent Milestone A, repository consolidation and recovery
identities retain distinct scopes. The strict-layer refinement is preserved exactly
from the entry record, including its implementation identity, both receipts and
three-platform closure. No accepted outcome is inferred from today's Agent files.

The [retained ADRs](../architecture/adr/README.md) preserve identifiers, K1-P1/K3-P1
proposal hashes, numerical contracts and raw-failure meaning. Current ownership
follows Workspace/ProductSearchSession and C++ providers. Historical AppState,
native oracle and phase-boundary descriptions are marked as historical context.
Forty generic history links now use fourteen verified phase-prefix locators.
Underlying external approval members remain NOT_VERIFIED by this maintenance.

Product naming exceptions and their checker live under tools/validation. The
previous settings-store identity and reviewed exceptions are unchanged. The Agent
checker retains --self-test and nonzero failure exits, adds --root for isolated
inspection and validates paths, navigation, state types and retired current inputs.
Independent baseline checks retain all 22 mandatory acceptance criteria and their
negative controls. These tools use only the Python standard library.

## Batches and verification

| Batch | Result |
| --- | --- |
| 1, `5de9a20d` | Product ADRs, baseline and identity validation relocated; existing context and new checks pass |
| 2, `7fce9af5` | Six-file Agent context, task schema, current checker and CI entry installed; active/null handoff controls pass |
| 3 | Navigation, guides, source-package inspection and final handoff closure |

| Check | Result |
| --- | --- |
| Agent context, baseline and identity self-tests | PASS on Windows Python 3.11 and Linux Python 3.14 |
| Source/ownership and configured-target strict controls | PASS; 95 configured targets and 136 indexed Core operations on each supported configuration |
| Windows GCC 13.1 / Qt 6.8.3 configure and explicit product build | PASS, new empty build directory |
| Windows MSVC 19.40 / Qt 6.8.3 configure and explicit product build | PASS, new empty build directory |
| Linux GCC 15.2 / Qt 6.10.2 configure and explicit product build | PASS, existing qualified SDK and a separate source copy/new empty build directory |
| Independent reference bytes | PASS, 169 original lengths/SHA-256 |
| Source-package contents | PASS, 2,183 files; all 32 current/moved inputs and 2,073 protected inputs match; Agent/baseline self-tests pass after extraction without .git |
| Original inputs and user work | PASS, 2,073 product/test/frozen/validation files and one unrelated user document retain their entry bytes; tags unchanged |

The current checks were executed with these arguments on Windows and Linux:

```sh
python -B tools/agent/check_current_context.py --self-test
python -B tools/validation/check_project_baseline.py --self-test
python -B tools/architecture/check_boundaries.py --self-test
python -B tools/validation/check_product_identity.py --self-test
python -B tools/validation/check_references.py
```

Configured-target validation also used check_strict_boundaries.py --graph with
--self-test and the actual per-configuration graph. Product identity --build was
checked against all three new product builds. The Windows command indexes are
windows-gnu-commands.json and windows-msvc-commands.json; Linux commands are in
build_linux.sh and linux-final-commands.log under the local evidence directory.

The 86-entry C++ normal suite was not rerun for this documentation/tooling change.
Existing acceptance and normal-suite records keep their original execution/reuse
meaning. No new numerical acceptance or hosted-CI execution is claimed.

Commands and logs are preserved locally under build/agent-governance, including
the initial Linux inspection on the Windows mount. That inspection completed PASS
before a proposed I/O-bound cancellation was needed; its attempt record says
ALREADY_FINISHED. Later Python checks/source packaging use a native Linux copy.
Local outputs are not normal-build prerequisites or an offsite availability claim.

The first CPack archive was generated and passed inspection, but its wrapper
exited 1 after it was edited during execution. Original wrapper output and the
archive are retained. Final packaging uses a fixed wrapper and native temporary
staging. Inspection rejects retired paths, generated caches and unrelated user
files, and compares the archive's inputs with their actual working bytes.

The unrelated docs/maintenance/settings-identity/report.md is neither modified
nor staged. No push, history rewrite, remote change, protection-tag deletion or
Git/LFS pruning is performed. Original acceptance reports, receipts, FAIL records
and frozen resources remain byte-identical.
