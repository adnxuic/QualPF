# QualPF Agent engineering

QualPF is a pure C++20 scientific application derived from QualX. QualX supplies
algorithm/behavior reference; current product ownership follows the accepted
architecture. Current documentation uses English; frozen evidence keeps its bytes
and language.

Read in order:

1. [.agent/README](.agent/README.md)
2. [.agent/POLICY](.agent/POLICY.md)
3. [.agent/TASK_STATE](.agent/TASK_STATE.json)

Read [.agent/WORKFLOW](.agent/WORKFLOW.md) before changes. Use
[.agent/CONTEXT_MAP](.agent/CONTEXT_MAP.md) to find task-specific authority.
Use [.agent](.agent/README.md) for AI working rules and task navigation.
[docs/user](docs/user/index.md) explains program use;
[docs/developer](docs/developer/README.md) owns technical contracts and developer
guides. Consult [history](history/README.md) only for original evidence and recovery.
Develop by ordinary scoped tasks; Phase numbering ended with the refactoring.
Record notable changes in [CHANGELOG.md](CHANGELOG.md) under Unreleased and update
affected current guides. Routine work needs no Phase plan or completion report.
Application uniquely owns canonical product state; Core/Application stay Qt-free.
Keep immutable GUI views, checked atomic transactions, owned/cancellable worker
inputs, execution-thread SQL, queued publication and safe close/drain lifetimes.

Preserve approved numerics, precision, errors, independent references and original
failed comparisons. Never generate goldens from product output or weaken assertions.
Work in bounded verified batches, explicitly stage and locally commit. Preserve
user work; no push, history rewrite, reset/stash, remote changes, protection-tag
deletion or referenced Git/LFS pruning. PFSM requires its own authorization.
