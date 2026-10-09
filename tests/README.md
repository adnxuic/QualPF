# Current C++ tests

The normal development configuration registers 86 CTest entries, preserving the
preceding 82 and adding strict architecture, process, update and display-coordinate
boundary checks. The preceding set retains the 80 original regressions/contracts
and two CLI failure-boundary regressions.
[Coverage](coverage.json)
lists identities and correspondence. No assertion or expected value was removed
to obtain PASS. Windows x64 GCC/MSVC and Linux x64 GCC are supported.

Unit sources are organized by `unit/core`, `unit/application`, `unit/infrastructure`;
SQL and GUI integration are under `integration/database` and `integration/gui`.
`product` exercises GUI/CLI/import/export/persistence/lifetime; `profile` retains
isolated profile properties. `compatibility` contains current C++ regression replay
adapters, not an ABI/native runtime. `reference_checks` validates independent
numerical/qualified-reference and protocol contracts. Retired-state observations
in `support` are test-only absence/compatibility fixtures.

## Running

Use the shared `windows-gnu`, `windows-msvc`, or `linux-gnu` presets and a local
SDK preset when needed; see [building](../docs/development/building.md).

```sh
cmake --preset windows-gnu
cmake --build --preset windows-gnu --parallel
ctest --preset windows-gnu
```

Normal tests build no Fortran, execute no old runner, fetch no evidence objects
and require no migration archive or pre-existing build directory. Configuration
downloads no dependencies. Matching Qt, Eigen/Boost and a C++20 compiler are
developer prerequisites. Optional GoogleTest entries retain their existing
assertions; standalone Qt-free instructions are in the build guide.

Numerical properties can take several minutes; preserve timeouts. Use the existing
Qt DLL paths on Windows. Tests writing system settings need a writable test store;
run complete Windows suites serially to avoid their shared system-scope fixture.

## Independent references

[Manifest](reference/manifest.json) pins 169 frozen inputs. `data` contains current
wires/transcripts, numerical archives, tolerance policies and CIF fixtures.
`regression/references` holds small algorithm-organized accepted/proved resources
and inert `.txt` provenance. Values and hashes are unchanged; no golden is
generated from C++. See [numerical policy](../docs/numerical/policy.md) and
[data provenance](data-guide.md). Logical historical keys are resolved locally
by `reference_paths.py`, without restoring full phase packages.

```sh
python -B tools/validation/check_references.py
python -B tools/architecture/check_boundaries.py --self-test
python -B tools/agent/check_current_context.py --self-test
python -B tools/validation/check_project_baseline.py --self-test
python -B tools/validation/check_product_identity.py --self-test
```

Some test/data names retain `fortran_capture`, `phase6` or `legacy`: these check
frozen provenance and valid compatibility behavior in current C++, not migration
execution. Missing/changed input fails. Full old audits and capture tools are
available through [historical recovery](../docs/migration-history.md) independently
of normal development.
