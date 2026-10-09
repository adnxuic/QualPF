# Building and contributing

Use CMake 3.20+, Ninja, a C++20 compiler, Qt 6 matching the compiler ABI, Eigen 3.4
and Boost headers. Run Windows MSVC from its developer shell. Windows GNU requires
MinGW and the corresponding Qt `bin` on PATH. Linux GNU uses installed C++/Qt/
Eigen/Boost development packages. Configuration downloads no dependencies.
Developers supply optional GoogleTest; Google Benchmark remains a reserved entry.

## Shared presets and local SDKs

`CMakePresets.json` provides `windows-gnu`, `windows-msvc` and `linux-gnu`.
These development presets explicitly enable Product/Preprocessing tests and
compile-command export, using `build/<configuration>`. Original test-option
defaults remain OFF. Existing build-type conversions, including
`Release`/`RelWithDebInfo`, remain unchanged for direct CMake configuration.

Put tools on PATH. Keep machine SDK paths in ignored `CMakeUserPresets.json`:

```json
{
  "version": 2,
  "configurePresets": [{
    "name": "local-windows-gnu",
    "inherits": "windows-gnu",
    "cacheVariables": {
      "Qt6_DIR": "C:/your-sdk/Qt/lib/cmake/Qt6",
      "Eigen3_DIR": "C:/your-sdk/Eigen/share/eigen3/cmake",
      "QUALPF_CORE_BOOST_INCLUDE_DIR": "C:/your-sdk/Boost/include",
      "QUALPF_POWDER_BOOST_INCLUDE_DIR": "C:/your-sdk/Boost/include"
    }
  }]
}
```

Local presets may also set `CMAKE_CXX_COMPILER`/`CMAKE_MAKE_PROGRAM` when those
tools are absent from PATH. Local MSVC and Linux presets inherit their matching
shared preset. Use separate build directories for different Qt kits.

```sh
cmake --preset local-windows-gnu
cmake --build --preset windows-gnu --parallel
ctest --preset windows-gnu
```

The example keeps the shared `binaryDir`, so shared build/test presets apply.
Use `linux-gnu` for all three commands when system dependencies are discoverable.
Direct configuration also works:

```sh
cmake -S . -B build/linux-gnu -G Ninja -DCMAKE_BUILD_TYPE=RELEASE \
  -DQUALPF_BUILD_PRODUCT_TESTS=ON -DQUALPF_BUILD_PREPROCESSING_TESTS=ON
cmake --build build/linux-gnu --parallel
ctest --test-dir build/linux-gnu -L normal --output-on-failure
```

The normal development suite has 86 CTest registrations (all 82 preceding identities
plus strict boundaries, platform process/update and display-coordinate checks). Numerical property
tests can take several minutes. Preserve existing timeouts and avoid competing
large builds while running them. Candidate-card parallelism and OpenMP/MKL policy
remain unchanged.

## Optional Qt-free tests

`tests` can be configured independently using CXX only, building Core, Session,
snapshot and their supported tests:

```sh
cmake -S tests -B build/qtfree -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQUALPF_BUILD_CONTRACT_TESTS=ON -DQUALPF_BUILD_NUMERICS_TESTS=ON \
  -DQUALPF_BUILD_CLASSIC_SEARCH_TESTS=ON \
  -DQUALPF_BUILD_SEARCH_INTEGRATION_TESTS=ON \
  -DQUALPF_BUILD_PREPROCESSING_TESTS=ON
cmake --build build/qtfree --parallel
ctest --test-dir build/qtfree \
  -R '^qualpf\.(contracts|numerics|classic|search\.integration|preprocessing\.contracts)$' \
  --output-on-failure
```

Supply `GTest_DIR`, `Eigen3_DIR` and Boost paths when needed. Product tests require
the full Qt product configuration. `tests/profile` retains its independent entry.
Check references and current context with:

```sh
python tools/validation/check_references.py
python -B tools/agent/check_current_context.py --self-test
python -B tools/validation/check_project_baseline.py --self-test
python -B tools/validation/check_product_identity.py --self-test
```

## Installation, packaging and documentation

Run Linux build and packaging commands inside the configured Linux environment.
The qualified WSL2 SDK used for the Linux closure requires no Docker; the native
packaging entry points below use that environment's toolchain.

```sh
cmake --install build/linux-gnu --prefix /your/staging/prefix
bash scripts/package/build_deb.sh
bash scripts/package/build_appimage.sh
```

Package intermediates belong to `build/linux-deb` and `build/linux-appimage`;
release copies belong to `releases/<version>/<type>`. Direct CPack output belongs
to the selected configuration's `packages` directory. Portable Linux Qt deployment
requires patchelf. The AppImage script also needs curl and caches linuxdeploy/
appimagetool under its configuration's build directory after first retrieval.

Root `install.sh` remains the installation entry; its implementation is in
`scripts/install.sh`. Historical qmake/Fortran build fragments are recoverable
from the protected archive. Current product resources, databases and installation
prefixes retain their accepted locations.

```sh
python -m pip install -r docs/requirements.txt
python -m mkdocs build --strict -f docs/mkdocs.yml
bash docs/make_pdf.sh
```

MkDocs reads `docs/user` and writes `docs/site`, preserving page filenames and
URLs. PDF generation requires Pandoc, XeLaTeX and DejaVu fonts; the default output
remains `docs/qualpf_manual.pdf`, optionally installed at its existing location.

Before committing, run checks appropriate to the actual change and distinguish
fresh validation from reused results. Follow the [Agent workflow](../../.agent/WORKFLOW.md)
and update only its single task handoff when needed. Recorded product acceptance
lives in [project-baseline.json](project-baseline.json), separately from current
task status. See the [context maintenance result](agent-governance.md). Preserve
historical authority in its protected archives, frozen inputs, original FAILs, qualifications and
acceptance meaning. Current project documentation uses English; original frozen
or archived documents retain their bytes and language.
See the [repository layout](repository-layout.md) and
[consolidation batches](repository-consolidation/batches.md).

## Current validation helpers

`tools/agent/check_current_context.py --self-test` checks Agent rules, navigation,
task-state types and safe paths. `tools/validation/check_project_baseline.py --self-test`
checks recorded acceptance against its independent gates and recovery identities;
it does not reopen acceptance or bind it to mutable Agent files.
`tools/validation/check_product_identity.py --self-test` checks current identity
and reviewed legacy import exceptions; `--build <directory>` also checks generated
identity and the executable. All three support nonzero failure exits without
requiring a historical archive. Agent state commands are indexes, never executed.

`tools/architecture/check_boundaries.py --self-test` checks dependencies, ownership
and search event-pump constraints. `tools/validation/check_references.py` verifies
independent resource bytes. `run_windows.ps1` and `run_linux.sh` under
`tools/validation` record fresh build/full normal suite inputs and actual artifacts,
including a separate Qt-free Application client. Windows parameters explicitly
supply the matching SDK/toolchain; MSVC selects the accepted 14.40 toolset.
`check_zero_fortran.py --build ... --objdump ... --nm ...` inspects actual C++ units,
language/cache/link graph and executable imports/symbols. Keep logs, including failures.

Fresh verification uses an independently checked out commit with empty build
directories. No archive restoration belongs to configure/build/normal tests.
Historical inspection uses the [separate recovery workflow](../migration-history.md).


The product build now requires Python 3 for the mandatory strict boundary target.
It records configured link/include properties in strict-targets.json and runs the
original source checks plus the stricter gate on every default build. To repeat
its negative controls explicitly:

```sh
python tools/architecture/check_strict_boundaries.py \
  --graph build/windows-gnu/strict-targets.json --self-test
```

Current Application headers are exported only from
src/application/include/qualpf/application. Snapshot support is compiled only
when the standalone contract or search-integration consumers request it.

The [independent Linux closure](../architecture/strict-layer-linux-closure.md)
records Ubuntu 26.04.1 x64, GCC 15.2.0, Qt 6.10.2, CMake 4.2.3 and Ninja 1.13.2.
It reused the accepted isolated SDK, copied current source inputs and started with
an empty build directory. All 86 normal tests, public-header/Qt-free checks and
staged/DEB installed-runtime checks pass. Preserve the SDK/plugin/settings
environment when reproducing those results; an incomplete host toolchain is not
a product failure.

For a separate system-dependency DEB build, the supported CMake/CPack mode is:

```sh
cmake -S . -B build/linux-deb -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_DEB_PACKAGE=ON
cmake --build build/linux-deb --parallel 2
cpack --config build/linux-deb/CPackConfig.cmake -G DEB \
  -B build/linux-deb/packages
```

Use xcb with an available display (or Xvfb) for the staged Qt deployment. The
normal test configuration continues to use its existing offscreen convention.
