# Building and contributing

Use CMake 3.20+, Ninja, a C++20 compiler, Qt 6 matching the compiler ABI, Eigen 3.4
and Boost headers, with compiler OpenMP support for PFSM candidate execution.
Run Windows MSVC from its developer shell. Windows GNU requires
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

Before regression verification, provision the 15 sealed reference ZIPs with
`python -B tools/validation/restore_reference_archives.py`. The main tree and
source packages contain their manifests and consumers, while payloads remain in
the protected local development history and pinned main ancestor. See
[independent reference prerequisites](../../tests/README.md#independent-references)
for shallow clones and source archives. Missing/changed data remains a failure.

The example keeps the shared `binaryDir`, so shared build/test presets apply.
Use `linux-gnu` for all three commands when system dependencies are discoverable.
Windows product deployment includes the native Windows platform plugin. Tests
that launch the deployed executable with `QT_QPA_PLATFORM=offscreen` also need
the matching Qt kit's offscreen plugin. Set `QT_PLUGIN_PATH` to that kit's
`plugins` directory in the test shell (for example,
`$env:QT_PLUGIN_PATH = "C:/your-sdk/Qt/plugins"` in PowerShell). A Qt `bin`
directory on PATH alone does not supply plugins beside a deployed `qt.conf`.

Windows GNU stages `libgomp-1.dll`, `libgcc_s_seh-1.dll`, `libstdc++-6.dll` and
`libwinpthread-1.dll` beside the build-tree executable on every product build.
Installation copies the same compiler-resolved files after Qt deployment.
Configuration fails if the selected MinGW compiler cannot locate a runtime;
an unrelated MinGW directory on PATH is not a deployment fallback. Keep the
corresponding Qt kit for Qt DLLs/plugins; MSVC and Linux deployment are unchanged.

Direct configuration also works:

```sh
cmake -S . -B build/linux-gnu -G Ninja -DCMAKE_BUILD_TYPE=RELEASE \
  -DQUALPF_BUILD_PRODUCT_TESTS=ON -DQUALPF_BUILD_PREPROCESSING_TESTS=ON
cmake --build build/linux-gnu --parallel
ctest --test-dir build/linux-gnu -L normal --output-on-failure
```

The normal development suite has 122 CTest registrations (including all 89 pre-remediation
identities, six SQL/provenance/payload/CLI/QXP regressions, two geometric FWHM
Core/Application tests, two configured FWHM Application/GUI tests, two PFSM Core/Application
contracts, a headless PFSM CLI contract, frozen-input adapter checks and native
PFSM GUI integration, four PFSM/RIR Core/Application/GUI/export contracts, and
the candidate-recall comparator and shadow/enabled workflow/reuse controls, plus
initial peak Application/GUI checks and background options/display controls, plus
workflow settings/isolation, three scaled GUI checks and QXP v3 restoration).
Numerical property tests can take several minutes. Preserve existing timeouts and
avoid competing large builds while running them. PFSM uses OpenMP across candidate cards with
Eigen's internal parallelism disabled; Classical numerical compilation is unchanged.

Workflow settings and QXP v3 verification is retained under
`build/workflow-settings`:

| Executed scope | Result | JUnit evidence |
| --- | --- | --- |
| Windows GCC normal suite | 120/120 | `final-gnu-normal-02.xml` |
| Windows MSVC normal suite | 120/120 | `final-msvc-normal-01.xml` |
| Linux GCC normal suite | 119/120; old GUI QXP field-path assertion failed | `final-linux-normal-01.xml` |
| Linux final QXP v3 and GUI persistence checks | 2/2 after the assertion/portable initialization fixes | `final-linux-focused-03.xml` |
| Standalone Qt-free Application, each of the three platforms | 11/11 each | `final-gnu-qtfree-01.xml`, `final-msvc-qtfree-01.xml`, `final-linux-qtfree-03.xml` |

The Linux snapshots differ only in the two retested translation units and the
uncompiled Agent handoff file; `linux-final-source-difference.txt` records that
comparison. These are separate evidence scopes, not one fresh Linux 120/120
invocation. Original failures and intermediate cache/build diagnostics remain
beside the final logs. The three native settings scale checks (100/150/200%),
1,981-input source boundary check, configured strict boundaries (127 targets,
183 Core operations), and 169 byte-preserved reference checks pass. The deployed
Windows GCC runtime also exits successfully from automatic GUI startup with a
system-only PATH (`final-runtime-smoke-02.log`).

The subsequent restoration of individual Options dialogs has affected validation
under `build/native-project-options`: Windows MSVC and Linux GCC each pass 19/19;
Windows GCC passes the 16 unaffected registrations and all three corrected GUI
registrations (`gnu-tests-02.xml`, `gnu-gui-04.xml`). Each platform's standalone
Qt-free checks pass 11/11. The GUI coverage includes original menu routes,
project-only saves, estimator results, cancellation and repeated dependency
confirmation at 100/150/200% scale. Original failures from offscreen reactivation
and retained Linux archive timestamps remain beside the corrected runs. The
deployed Windows runtime passes automatic startup with a system-only PATH.

For Setting opening profiles, run `qualpf_workflow_settings_gui_tests
--measure-settings <flow>` with flow 0–3. The fixture initializes the main window
and a temporary persisted defaults store, then reports construction, loading,
show and first-paint times for three successive openings. With the native Windows
platform it renders hidden windows, preserving the system style and font without
opening benchmark windows on the desktop. Timing is diagnostic, not a fixed CI
threshold. Five fresh processes per entry recorded median first paints of
303–316 ms before the on-demand/editor-font correction and 8.6–10.0 ms afterward
(`build/settings-opening-latency/baseline-menu-native-01.json` and
`optimized-menu-native-01.json`); the separate glyph probe identifies the cold
subscript fallback. Other hardware, fonts and desktop composition can differ.

The maintained PFSM tools build as `qualpf_pfsm_benchmark`,
`qualpf_pfsm_candidate_recall_replay` and `qualpf_pfsm_candidate_recall_probe`.
The benchmark defaults to recall off; the current option is
`--pfsm-candidate-recall off|shadow|enabled` with v2 as the only implementation.
See the [CLI/report migration](../numerical/pfsm-benchmark.md#current-candidate-recall-entry-points)
for compatibility aliases and preserved historical evidence names. The standalone
Qt-free build exposes `pfsm_probe` and `pfsm_candidate_recall_probe` for the
independent formal and recall reference checks.

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
The complete Application library also has a standalone C++20 consumer with no Qt
discovery. It demonstrates common preprocessing, independent formal extraction,
shared search preparation, Classical publication, phase acceptance and RIR,
geometric FWHM evaluation and one matcher replacement. Its eleven tests cover the
consumer, Core algorithms, independent initial peak evaluation, geometric Core/Application contracts and configured
fixed/Gaussian/geometric FWHM policy, plus PFSM expansion, NNLS/TRF fitting and
parallel Application transactions, quantification and export:

```sh
cmake -S tests/architecture -B build/qtfree-application -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/qtfree-application --parallel 4
ctest --test-dir build/qtfree-application --output-on-failure
```

Supply the same Eigen/Boost paths as the product configuration. With the accepted
MSVC toolset, also use `-DCMAKE_TRY_COMPILE_CONFIGURATION=Release` and the verified
compiler-specific `CMAKE_CL_SHOWINCLUDES_PREFIX`. Check that Ninja records nonzero
header dependencies; a stale zero-dependency directory cannot validate interface
changes. See the [example](../../tests/architecture/application_boundary.cpp) and
[Search-Match contracts](../architecture/search-algorithms.md).

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

## Contribution workflow

Develop features, fixes and optimizations as ordinary scoped tasks with descriptive
names. Phase numbering served the completed refactoring; new work requires no
Phase plan, phase gate, separate validation index or completion report.

Record notable completed changes under Unreleased in [CHANGELOG.md](../../CHANGELOG.md),
grouped by Added, Changed, Fixed or Removed as appropriate. Describe the resulting
behavior or maintenance without Phase numbers or raw execution logs. Update
affected guides and contracts in their owning locations. At release, move the
Unreleased entries into a dated version section.

Before committing, run checks appropriate to the actual change and distinguish
fresh validation from reused results. Follow the [Agent workflow](../../.agent/WORKFLOW.md)
and use its single task handoff only for ongoing work. Recorded refactoring acceptance
lives in [project-baseline.json](project-baseline.json), separately from current
task status; it is not a progress tracker for new features. Preserve
historical authority in its protected archives, frozen inputs, original FAILs, qualifications and
acceptance meaning. Current project documentation uses English; original frozen
or archived documents retain their bytes and language.
See the [repository layout](repository-layout.md) and
[historical reports and recovery](../migration-history.md).

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
When scanning a source archive without Git metadata, the input collector excludes
generated Python `.pyc`/`.pyo` caches, matching the working checkout's ignore rules.
Source and reference additions, changes and removals remain checked; negative
controls exercise these distinctions. Preserve the initial failure and a scoped
input-difference record when repairing a collector after an otherwise valid run.

The earlier PFSM GUI functional checkpoint passes all 104 normal registrations
in fresh Windows MSVC and Linux GCC executions, with actual zero tool/runner
statuses, exact test identity coverage and source/binary checks before and after.
The Linux read-only compiled snapshot contains 2,257 files; the working checkout
differs only in four uncompiled documentation/handoff files at preparation.
`build/pfsm/current-platform-regression-12` retains the raw logs, JUnit results
and terminal receipts. Windows GCC retains its earlier 102/102 normal rerun plus
two separately qualified unchanged Core-property passes; it is not reported as
one 104/104 invocation. Fresh hash comparison verifies all 112 GNU binaries,
compiler commands, cache and build graph are unchanged. Original failures and
unchanged timeouts remain preserved. Fresh MSVC and Linux zero-Fortran checks
also pass; these regression outcomes do not certify PFSM real-data identification.

The PFSM identification-acceptance closeout reused these runs for unchanged implementation,
reference, runner and toolchain dependencies; it does not report them as fresh
closeout executions. Subsequent differences in the 2,257-file source inventory
are confined to uncompiled documentation and Agent handoff files. Closeout
checks current source/reference bindings, documentation and architecture gates
without another scientific fit. The [recorded acceptance disposition](../numerical/pfsm-benchmark.md#acceptance-disposition-2026-10-12)
retains the failed development case, unresolved inputs and unranked holdout.

The PFSM/RIR quantification checkpoint registers 108 normal tests. Its full
Linux GCC invocation passes 108/108. Full Windows GNU and MSVC invocations each
pass 106/108 inside the restricted environment: native settings migration and
the localhost update fixture are denied by that environment. Those two exact
tests pass with the required fixture access, without source or assertion changes.
The numeric-RIR precision/quantification checkpoint rebuilds all three supported
configurations and passes the 22 affected Core/Application,
GUI, SQL, algorithm-source, export, CLI, lifetime and architecture registrations.
The final GUI-only canonical-change projection then passes all six affected
GUI/lifetime registrations on each platform, including direct Application RIR
editing/restoration without disturbing candidate selection.
These full invocations and final affected runs are separate evidence scopes,
not a claimed single fresh 108/108 Windows run. Current binary import/symbol
checks pass on all three platforms; 169 independent reference resources retain
their bytes. Logs, JUnit and original failures are under
`build/pfsm/quantification-01` (`*-normal`, `*-final-focused`,
`*-canonical-focused`, `*-canonical-zero-fortran`).
The [real-mixture mass errors](../numerical/pfsm.md#real-mixture-semi-quantification-assessment-2026-10-12)
are reported independently of these software regression results.

Fresh verification uses an independently checked out commit with empty build
directories and the byte-verified reference ZIP prerequisites. Configure/build
and normal tests never restore full migration archives or execute old runners.
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
an empty build directory. All 89 normal tests, public-header/Qt-free checks and
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

QPF persistence adds `qualpf.project.qpf` and the independent standard-library
`qualpf.project.qpf_protocol` controls. The former QXP codec regressions retain
test-only codecs; they do not exercise an accepted product input format. See the
[QPF contract](../architecture/project-format.md). Keep failure logs under the
ignored build tree and report GCC/MSVC/Linux validation separately.

The QPF checkpoint freshly passes seven persistence/GUI controls on Windows MSVC
19.40 with Qt 6.8.3, and seven on Linux GCC 15.2.0 with Qt 6.10.2. The Linux run
uses the existing isolated SDK, a byte-checked read-only source copy and a private
build directory. All six directed file exchanges between Windows GCC 13.1.0,
Windows MSVC and native Linux GCC pass, including full-width metadata integers
and negative zero. These exchanges use the portable protocol fixture; full PFSM
restoration and continuation are separately exercised by each native QPF test.
Evidence and original failed runs are retained under `build/qpf-*`.

The QPF checkpoint final Windows GCC normal invocation passes 120/121 registrations. The unchanged
`qualpf.card.properties` runner exceeds its existing 180-second subprocess limit
in the parallel invocation and in both serial retries, including ordinary execution.
Its SHA-256 still matches the earlier passing run; Core sources, inputs, assertions
and timeout have not changed. That checkpoint retains this validation limitation and does not claim a
full normal-suite PASS. The other registry entry, `qualpf.powder.properties`,
reuses the earlier passing run after checking the identical executable SHA-256,
unchanged Core/checker sources and all 169 byte-preserved reference resources.
`build/qpf-normal-final-02.log`, `build/qpf-card-serial.log`,
`build/qpf-card-unrestricted.log` and `build/qpf-powder-reuse.json` retain those scopes.
