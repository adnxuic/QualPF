# Agent engineering context

Read the root [AGENTS.md](../AGENTS.md), this file,
[POLICY](POLICY.md), then [TASK_STATE](TASK_STATE.json).
Read [WORKFLOW](WORKFLOW.md) before changing files. Use
[CONTEXT_MAP](CONTEXT_MAP.md) to load the authorities needed by the task.

This directory tells AI agents how to work: establish scope, select relevant
documentation, implement, verify and hand off one task. It does not own product
architecture, algorithm specifications or user instructions.

After the startup files, open the [developer guide](../docs/developer/README.md)
and select the relevant authority through [CONTEXT_MAP](CONTEXT_MAP.md). Read the
owning contract before its implementation and tests. Use the
[user manual](../docs/user/index.md) to check observable behavior and wording.
Use [history](../history/README.md) only when evidence or earlier approval is needed.
Do not load every historical report or treat a recorded next action as permission.

This directory owns Agent working rules, navigation and one current task handoff.
TASK_STATE is the only mutable task record. An empty active_task means no task is
being handed off; start new work only from actual user authorization.
Recorded product acceptance lives in the [project baseline](../docs/developer/project-baseline.json),
independently of the current task. Historical STOP records do not prevent a newly
authorized maintenance task and do not authorize PFSM.

Keep only README.md, POLICY.md, WORKFLOW.md, CONTEXT_MAP.md, TASK_STATE.json and
.gitattributes here. Record notable completed changes in [CHANGELOG.md](../CHANGELOG.md).
Product contracts and decisions belong to their owning documentation; ordinary
tasks need no Phase documents or separate completion reports. Raw logs belong to
the existing validation locations or ignored build directories. Do not add an archive or a
second progress summary here. Current documents use English, UTF-8 and LF.
