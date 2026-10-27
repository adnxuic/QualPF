# Building and contributing

Use CMake 3.20+, Ninja, a C++20 compiler, Qt 6.7+ matching the compiler ABI, Eigen 3.4
and Boost headers, with compiler OpenMP support for PFSM candidate execution.
Run Windows MSVC from its developer shell. Windows GNU requires
MinGW and the corresponding Qt `bin` on PATH. Linux GNU uses installed C++/Qt/
Eigen/Boost development packages. Configuration downloads no dependencies.
Developers supply optional GoogleTest; Google Benchmark remains a reserved entry.
Current QPF decoding uses the [Qt 6.7 stream-reader APIs](https://doc.qt.io/qt-6/qcborstreamreader.html#readAndAppendToString).
The native package checks used Qt 6.8.3 on Ubuntu 22.04 and Qt 6.8.2 on Debian 13;
Ubuntu 22.04's original Qt 6.2 package is insufficient. Use Python 3.11+ for the
normal validation suite, which uses `hashlib.file_digest`. Supply that interpreter
with `Python3_EXECUTABLE` when the system Python is older.

## GUI icon resources

GUI action icons are committed as PNG resources with high-density companions.
Normal builds need no image-generation tools or Qt SVG. For editable SVG masters,
offline PNG regeneration and contact sheets, see the
[action icon guide](../../src/gui/resources/icons/README.md).

The PFSM application mark, Windows ICO, Linux launcher icons and documentation
assets share one preserved raster master. Its sizes, consumers and offline export
command are documented in the
[application icon guide](../../src/gui/resources/branding/README.md).

Publisher, application license, project/support/manual URLs and description come
from `deploy/distribution.json`; settings namespaces remain compatibility keys.
The native Qt About view embeds the canonical GPL text, origin/third-party notices
and supplemental licenses and does not require network access. Its three scaled
checks render hidden native Windows windows (or Linux offscreen windows).
The origin page also opens the embedded source-retrieval instructions, including
the separate dependency archives used by slim Windows and Linux packages.
Methodological acknowledgment of Match!'s public PFSM documentation is separate
from code provenance and the numerical acceptance contracts.

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
source packages contain their manifests and consumers. The symmetry generator ZIP
is also a preferred modification input in corresponding-source exports; other
payloads remain in
the protected local development history and pinned main ancestor. See
[independent reference prerequisites](../../tests/README.md#independent-references)
for shallow clones and source archives. Missing/changed data remains a failure.
Full regression also needs the unchanged private development fixtures under
`tests/real_example_data` and the three original powder examples in
`share/qualx/examples`. They are intentionally excluded from deliverable archives;
provision authorized original copies for development and verify their recorded
bytes. Their absence does not prevent the ordinary product build or the four
maintained table-generator checks. Never manufacture replacements from product
results or include these private fixtures in a release package.

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

The normal development suite has 127 CTest registrations (including all 89 pre-remediation
identities, six SQL/provenance/payload/CLI/QXP regressions, two geometric FWHM
Core/Application tests, two configured FWHM Application/GUI tests, two PFSM Core/Application
contracts, a headless PFSM CLI contract, frozen-input adapter checks and native
PFSM GUI integration, four PFSM/RIR Core/Application/GUI/export contracts, and
the candidate-recall comparator and shadow/enabled workflow/reuse controls, plus
initial peak Application/GUI checks and background options/display controls, plus
workflow settings/isolation, three scaled GUI checks and QXP v3 restoration, plus
three scaled About/offline-notice checks, distribution rejection controls and
offline dependency-bundle integrity controls).
Numerical property tests can take several minutes. Preserve existing timeouts and
avoid competing large builds while running them. PFSM uses OpenMP across candidate cards with
Eigen's internal parallelism disabled; Classical numerical compilation is unchanged.
Native Windows validation needs access to its UUID-owned registry fixtures and
loopback test server. A restricted application sandbox can reject these operations;
retain its failures and rerun the unchanged native controls in a regular Windows
test shell. Do not replace the native settings store or waive networking assertions.

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
See the [CLI/report migration](numerical/pfsm-benchmark.md#current-candidate-recall-entry-points)
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
[Search-Match contracts](architecture/search-algorithms.md).

Check references and current context with:

```sh
python tools/validation/check_references.py
python -B tools/agent/check_current_context.py --self-test
python -B tools/validation/check_project_baseline.py --self-test
python -B tools/validation/check_product_identity.py --self-test
```

## Local build storage

Retired task-build object files and static libraries are disposable; complete
build-tree backups are unnecessary. Keep current configurations, active or paused
campaign inputs, release copies, frozen references and original failed
comparisons. Inspect `git worktree list` before cleaning nested directories.
Preserve modified worktrees. A retired clean verification checkout can be removed
with `git worktree remove` after checking ignored inputs and confirming that its
commit remains reachable through retained refs; do not force removal.
On Windows, retained historical logs, result tables,
source snapshots and retired executables/private DLL copies can use lossless NTFS
compression after checking file hashes before and after. Leave current build
configurations and running campaigns alone. Keep runtime-source caches and package
acceptance directories untouched while their owning task remains open, including
when it is blocked.

An explicit owner request for storage recovery can authorize retiring proven
duplicate outputs during an open task. First verify retained release/source
artifacts, source changes, independent inputs and required raw logs. Compare
unpacked payloads to the retained archives before deleting them; preserve unique
metadata and failures. Delete disposable copies in place when relocation is
excluded. Do not treat protected Git/LFS objects or unique scientific evidence
as build cache. The [release guide](releasing.md#current-native-package-acceptance)
records the scoped WSL recovery, RAM packaging and in-place allocation recovery.

An explicit build-storage cleanup may also remove stopped project builds' object
files and project static libraries while retaining their configuration, runnable
executables and DLLs, SDKs and source. Rebuild before subsequent linking,
installation or configured-binary validation. The
[retired-build recovery index](../../history/README.md#retired-local-build-records)
locates the verified original records; compiled caches are intentionally discarded.

## Installation, packaging and documentation

Run Linux build and packaging commands inside the configured Linux environment.
The qualified WSL2 SDK used for the Linux closure requires no Docker; the native
packaging entry points below use that environment's toolchain.
Use the native Linux filesystem for source/build/staging during WSL validation.
Scanning the Windows-mounted tree can exceed the unchanged strict-check timeout:
the retained mounted run passed 126/127, while that same strict control passed
in 4.79 seconds on native storage. The final native Debian run passed 127/127.
When intentionally building a Windows checkout through WSL, match its Git
`core.autocrlf` setting through process-local Git configuration. Otherwise CRLF
conversion can be reported as source edits; do not overwrite dirty provenance.

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
For a formal AppImage, set `QUALPF_APPIMAGE_RUNTIME_FILE` to the exact reviewed
type-2 runtime. Its SHA-256 must equal `components.package-runtimes.runtime_sha256`
in `QUALPF_DISTRIBUTION_RUNTIME_REVIEW`. The runtime is passed to appimagetool
explicitly and recorded with the packaging tools; an audit may inspect a default
runtime but that does not establish its source provenance. The helper registers
`.DirIcon` before inventorying the tree so the final image has no unlisted icon
entry. See [actual package acceptance](releasing.md#current-native-package-acceptance).

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
fresh validation from reused results. Recorded refactoring acceptance
lives in [project-baseline.json](project-baseline.json), separately from current
task status; it is not a progress tracker for new features. Preserve
historical authority in its protected archives, frozen inputs, original FAILs, qualifications and
acceptance meaning. Current project documentation uses English; original frozen
or archived documents retain their bytes and language.
See the [repository layout](repository-layout.md) and
[historical reports and recovery](../../history/README.md).

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

Fresh verification uses the intended revision and records its actual source,
reference, runner and toolchain inputs. Configure/build and normal tests never
restore full migration archives or execute old runners. Original PFSM, QPF and
platform checkpoint outcomes are retained in the
[historical build guide](../../history/README.md#build-and-maintenance-records); they are not a
claim that the current tree passed those runs.

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
[QPF contract](architecture/project-format.md). Keep failure logs under the
ignored build tree and report GCC/MSVC/Linux validation separately.

## Documentation checks

Run `python -B tools/validation/check_documentation.py --self-test` for current
links, directory ownership, user navigation/PDF coverage, retained evidence bytes
and archive-index consistency. Use `python -B tools/validation/history_archive.py
--archive <zip>` separately to verify full external evidence, or add `--restore-to
<new-directory>` for checked recovery. Ordinary build and documentation checks do
not require this archive.
Build the website with `python -m mkdocs build --strict -f docs/mkdocs.yml` after
installing `docs/requirements.txt`. See [documentation ownership](../README.md).
Generated website and PDF outputs are ignored; the website publishes users' guides
only. Developers read the Markdown in this directory.
