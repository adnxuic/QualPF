# Agent engineering context

Read the root [AGENTS.md](../AGENTS.md), this file,
[POLICY](POLICY.md), then [TASK_STATE](TASK_STATE.json).
Read [WORKFLOW](WORKFLOW.md) before changing files. Use
[CONTEXT_MAP](CONTEXT_MAP.md) to load the authorities needed by the task.

This directory owns Agent working rules, navigation and one current task handoff.
TASK_STATE is the only mutable task record. An empty active_task means no task is
being handed off; start new work only from actual user authorization.
Recorded product acceptance lives in the [project baseline](../docs/development/project-baseline.json),
independently of the current task. Historical STOP records do not prevent a newly
authorized maintenance task and do not authorize PFSM.

Keep only README.md, POLICY.md, WORKFLOW.md, CONTEXT_MAP.md, TASK_STATE.json and
.gitattributes here. Product contracts, decisions and completion reports belong
to the corresponding project documentation. Raw logs belong to the existing
validation locations or ignored build directories. Do not add an archive or a
second progress summary here. Current documents use English, UTF-8 and LF.
