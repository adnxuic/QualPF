# Task context map

Load the row relevant to the authorized task. Read the linked current authority
before its implementation details. Documentation is maintained in its owning area;
this map contains navigation only.

| Task | Current authority | Implementation and verification entry |
| --- | --- | --- |
| Architecture, ownership, dependencies | [Overview](../docs/architecture/overview.md), [strict layers](../docs/architecture/strict-layer-convergence.md) | [Application public API](../src/application/include/qualpf/application), [boundary check](../tools/architecture/check_boundaries.py), [strict gate](../tools/architecture/check_strict_boundaries.py) |
| Transactions, search, cancellation, close/drain | [Ownership and lifetime](../docs/architecture/overview.md), [architecture decisions](../docs/architecture/decisions.md) | [Application](../src/application), [GUI](../src/gui), [coverage](../tests/coverage.json) |
| Numerical, wavelength, crystallography, Range, Kalpha2 | [Numerical policy](../docs/numerical/policy.md), [retained ADRs](../docs/architecture/adr/README.md) | [Core](../src/core), [reference manifest](../tests/reference/manifest.json), [reference checks](../tools/validation/check_references.py) |
| SQL, parsing, QPF, settings, exports | [Owner map](../docs/architecture/overview.md), [compatibility allowlist](../docs/architecture/compatibility-allowlist.json) | [Infrastructure](../src/infrastructure), [Application](../src/application), [test guide](../tests/README.md) |
| GUI, process/network capabilities, platform providers | [Strict layers](../docs/architecture/strict-layer-convergence.md) | [GUI](../src/gui), [Infrastructure](../src/infrastructure), [composition](../apps/qualpf) |
| Build, tests, CI, deployment | [Build/test guide](../docs/development/building.md), [test guide](../tests/README.md), [layout](../docs/development/repository-layout.md) | [Presets](../CMakePresets.json), [CMake](../cmake), [quality CI](../.github/workflows/quality.yml) |
| Change records and development documentation | [Changelog](../CHANGELOG.md), [contribution workflow](../docs/development/building.md#contribution-workflow) | Update Unreleased and affected current guides for the authorized task |
| Product identity and legacy settings import | [Compatibility allowlist](../docs/architecture/compatibility-allowlist.json) | [Identity checker](../tools/validation/check_product_identity.py), [reviewed identity exceptions](../tools/validation/product-identity.json) |
| Recorded project acceptance | [Project baseline](../docs/development/project-baseline.json), [architecture entry](../docs/architecture/README.md) | [Baseline checker](../tools/validation/check_project_baseline.py) |
| Historical approval or frozen evidence | [Historical reports and recovery](../docs/migration-history.md) | Use its protected snapshot/catalog workflow only for the affected authority |
| Agent rules and task handoff | [POLICY](POLICY.md), [WORKFLOW](WORKFLOW.md), [TASK_STATE](TASK_STATE.json) | [Context checker](../tools/agent/check_current_context.py) |

Normal verification never restores migration runners or raw historical archives.
Use current coverage and affected-owner checks; historical files keep their original
scope and are not an execution queue. Build commands and SDK details belong to the
build guide rather than this map.
