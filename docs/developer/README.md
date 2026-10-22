# QualPF developer guide

This guide explains how QualPF is built, how its components work and which
contracts a change must preserve. For program operation, start with the
[user manual](../user/index.md).

## Start here

1. [Build and test](building.md) with a supported compiler and matching Qt SDK.
2. Read the [repository layout](repository-layout.md) and
   [ownership model](architecture/overview.md).
3. Read the contract for the area you are changing and its linked tests before
   editing implementation. The table below provides the main entry points.

| Area | Current documentation |
| --- | --- |
| Components, state ownership and lifetimes | [Architecture](architecture/README.md), [strict boundaries](architecture/strict-layer-convergence.md) |
| Search and preprocessing | [Algorithm workflows](architecture/search-algorithms.md), [FWHM workflow](architecture/fwhm-workflow.md) |
| Scientific behavior and references | [Numerical policy](numerical/policy.md), [accepted ADRs](architecture/adr/README.md), [geometric FWHM](numerical/geometric-fwhm.md) |
| PFSM contracts and developer tools | [PFSM](numerical/pfsm.md), [benchmark usage](numerical/pfsm-benchmark.md) |
| Settings and persistence | [Workflow settings](architecture/workflow-settings.md), [QPF format](architecture/project-format.md), [compatibility allowlist](architecture/compatibility-allowlist.json) |
| Regression coverage | [Tests](../../tests/README.md), [coverage index](../../tests/coverage.json) |
| Contributions and documentation | [Contribution workflow](building.md#contribution-workflow), [documentation ownership](../README.md#maintaining-documentation), [changelog](../../CHANGELOG.md) |
| Independent release and legal materials | [Distribution and corresponding source](releasing.md) |

## Contracts and evidence

Current guides and accepted decisions define the product's technical contracts.
Historical identifiers on an ADR do not make its accepted numerical contract
obsolete. The [project baseline](project-baseline.json) indexes recorded acceptance;
it is not a current progress report or a claim of fresh validation.

Use the [history index](../../history/README.md) for retained acceptance records
and explicit recovery of archived reports, logs and failures. Ordinary checks work
without the external archive; archive availability requires a separate explicit check. Historical owners, test counts and STOP records
apply to their recorded revisions. Current behavior follows the owning guide and
implementation; contradictions require investigation, not silent changes to evidence.

AI agents additionally follow [AGENTS.md](../../AGENTS.md) and the
[task context map](../../.agent/CONTEXT_MAP.md). Human contributors do not need an
Agent task-state file to use these guides.
