# Task context map

After the startup rules, read the [developer entry](../docs/developer/README.md).
Load only the row relevant to the authorized task, then read its owning contract,
implementation and tests in that order. Use the [user manual](../docs/user/index.md)
for user-visible behavior. This map is navigation, not a duplicate specification.

Update the same owning guide when a contract changes; update user instructions
when observable behavior changes. Keep AI execution rules here and frozen records
in [history](../history/README.md). If a guide and implementation disagree, inspect
the relevant evidence and report the discrepancy before changing accepted behavior.

| Task | Current authority | Implementation and verification entry |
| --- | --- | --- |
| Architecture, ownership, dependencies | [Overview](../docs/developer/architecture/overview.md), [strict layers](../docs/developer/architecture/strict-layer-convergence.md) | [Application public API](../src/application/include/qualpf/application), [boundary check](../tools/architecture/check_boundaries.py), [strict gate](../tools/architecture/check_strict_boundaries.py) |
| Transactions, search, cancellation, close/drain | [Ownership and lifetime](../docs/developer/architecture/overview.md), [architecture decisions](../docs/developer/architecture/decisions.md) | [Application](../src/application), [GUI](../src/gui), [coverage](../tests/coverage.json) |
| Numerical, wavelength, crystallography, Range, Kalpha2 | [Numerical policy](../docs/developer/numerical/policy.md), [retained ADRs](../docs/developer/architecture/adr/README.md) | [Core](../src/core), [reference manifest](../tests/reference/manifest.json), [reference checks](../tools/validation/check_references.py) |
| SQL, parsing, QPF, settings, exports | [Owner map](../docs/developer/architecture/overview.md), [compatibility allowlist](../docs/developer/architecture/compatibility-allowlist.json) | [Infrastructure](../src/infrastructure), [Application](../src/application), [test guide](../tests/README.md) |
| GUI, process/network capabilities, platform providers | [Strict layers](../docs/developer/architecture/strict-layer-convergence.md) | [GUI](../src/gui), [Infrastructure](../src/infrastructure), [composition](../apps/qualpf) |
| Build, tests, CI, deployment | [Build/test guide](../docs/developer/building.md), [test guide](../tests/README.md), [layout](../docs/developer/repository-layout.md) | [Presets](../CMakePresets.json), [CMake](../cmake), [quality CI](../.github/workflows/quality.yml) |
| Change records and development documentation | [Changelog](../CHANGELOG.md), [contribution workflow](../docs/developer/building.md#contribution-workflow) | Update Unreleased and affected current guides for the authorized task |
| Product identity and legacy settings import | [Compatibility allowlist](../docs/developer/architecture/compatibility-allowlist.json) | [Identity checker](../tools/validation/check_product_identity.py), [reviewed identity exceptions](../tools/validation/product-identity.json) |
| Recorded project acceptance | [Project baseline](../docs/developer/project-baseline.json), [architecture entry](../docs/developer/architecture/README.md) | [Baseline checker](../tools/validation/check_project_baseline.py) |
| Historical approval or frozen evidence | [Historical reports and recovery](../history/README.md) | Use its protected snapshot/catalog workflow only for the affected authority |
| Documentation organization and publication | [Documentation ownership](../docs/README.md), [developer entry](../docs/developer/README.md) | [Documentation checker](../tools/validation/check_documentation.py), [MkDocs](../docs/mkdocs.yml), [PDF inputs](../docs/make_pdf.sh) |
| Agent rules and task handoff | [POLICY](POLICY.md), [WORKFLOW](WORKFLOW.md), [TASK_STATE](TASK_STATE.json) | [Context checker](../tools/agent/check_current_context.py) |

Normal verification never restores migration runners or raw historical archives.
Use current coverage and affected-owner checks; historical files keep their original
scope and are not an execution queue. Build commands and SDK details belong to the
build guide rather than this map.
