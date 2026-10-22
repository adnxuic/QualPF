# Strict layer convergence

Current outcome: **STRICT_LAYER_CONVERGENCE_3_PLATFORM_PASS**. Independent Linux
validation now closes the Windows checkpoint's gap; see the
[closure report](strict-layer-linux-closure.md) and
[receipt](strict-layer-linux-validation.json). The implementation remains
`895e413878452a11bcbc69b456f8c1567de1034e`; no production correction was needed.

This separately authorized refinement starts at `f12870d78f407921d93f694ac5b17f216233e1f6`.
Milestone A and F acceptance, protected snapshots, original failures, independent
references and numerical contracts retain their existing authority. PFSM remains
not started. Unrelated `docs/maintenance/` work is excluded.

The destination is GUI -> Application -> Core, Infrastructure -> Application/Core,
with concrete providers assembled at the executable entrypoint. Application APIs
may expose existing domain value types. The refinement preserves product behavior,
formats, settings and the existing search/modal database execution models.

## Batches

1. **Reference database and synchronous query ownership**: Application now owns
   database selection/version checks and synchronous SearchEngine execution.
   SQL providers implement Qt-free repository/catalog ports. The entrypoint and
   test fixtures explicitly construct the concrete repository before its borrowing
   catalog/context. GUI requests compose CandidateQuery values instead of inheriting
   a SQL compiler; GUI metadata is an immutable Qt projection.
   Windows GNU product build and 11 affected CTests pass. Source boundary negative
   controls pass. Initial compile integration failures are preserved under
   `build/strict-layer-convergence/batch1-build-01.log` through `-03.log`; repaired
   builds `batch1-build-04.log` and `-05.log` pass. Query acceptance thresholds remain
   separate from scoring parameters, preserving the prior score calculation.
   Initial SQL fixture failures used the default
   temporary directory; using the repository runner's workspace-local TEMP/TMP
   resolved them without changing assertions (`batch1-tests-02.log` and XML).
   The final affected-suite rerun also passes (`batch1-tests-03.log` and XML).

2. **Product files and owned metadata**: GUI consumers borrow explicit Pattern,
   Project, Powder CIF, text and atomic-export ports. Application owns project
   open/save and peak import orchestration; additional patterns retain their raw
   comparison scale. JSON/QXP/provenance conversion stays in Infrastructure.
   Session projections now retain MetadataValue directly, including undefined,
   null and integer/double identity. Test-only peak serialization helpers moved
   to tests/support. Dirty comparison uses the existing preprocessingValues;
   independent Qt comparison controls verify equivalence for valid states.
   Windows GNU build `batch2-build-07.log` passes. Product/IO tests pass in
   `batch2-tests.log`; native settings fixtures require an unsandboxed rerun
   (documented in batch 3). The full suite's unchanged powder property test is
   still running at this commit. Integration compile failures `-01` through `-05`
   are retained; repaired builds `-06` and `-07` pass. Assertions/reference bytes
   are unchanged; tests now explicitly inject their file providers.

3. **Preferences**: concrete QSettings access was removed from GUI settings
   consumers. The root injects pure PreferenceStore/SearchSettingsProvider ports;
   Qt presentation values cross as owned bytes using QDataStream::Qt_6_0. The
   provider writes the original QVariant values, keys and identity. Search option
   conversion, defaults and source overrides retain the original Qt behavior.
   GNU build `batch3-build-03.log` passes. Nine affected GUI/CLI/startup tests pass
   in `batch3-tests.log`. The native registry fixture fails under the sandbox in
   batch 2 and 3, and passes with the required registry access in
   `batch3-settings-tests-02.log` / XML. New assertions verify original settings
   import, QPen, QColor, window bytes, key/array identity, types and fixed stream
   transport. Initial compile failures `batch3-build-01.log` / `-02.log` remain.

4. **Process and file discovery**: the root now supplies PlatformServices.
   Infrastructure owns QProcess, executable lookup and documentation discovery;
   Application owns maintenance request modes and result interpretation. GUI
   retains dialogs/notifications and consumes owned values. Saved executable,
   PATH and supplied-folder precedence, stale setting removal and the existing
   asynchronous process model are preserved. GNU build
   `batch4-process-build-02.log` and three affected CTests pass
   (`batch4-process-tests.log` / XML), including a real child process fixture,
   single callback delivery, result lifetime and program lookup controls.

5. **Network updates**: an explicit Qt Core/Network transport owns HTTP and JSON.
   Application evaluates owned replies; GUI retains notification dialogs and
   current-version/platform presentation. Original redirect/error precedence,
   header bytes and malformed-version conversion are preserved. GNU product
   build `batch4-network-build-03.log` and four affected CTests pass in
   `batch4-network-tests-03.log` / XML. The local HTTP test covers redirects,
   single completion and reply ownership; 144 version pairs are checked against
   the prior Qt comparison. Initial constructor integration failure and sandbox
   loopback failures remain logged. An unsandboxed diagnostic exposed an overly
   strict header-case assertion; it now respects HTTP's case-insensitive field
   names while checking the original value.

6. **Database build/removal**: Application now owns CIF batch orchestration and
   modal cancellation, borrowing factories for SQL writers, source readers and
   PDF jobs. The same pure use cases serve CLI and Qt progress projections.
   Infrastructure preserves schema/reader construction order, callback ordering,
   filename identities, sorting and diagnostics. Directory/companion discovery
   and executable build information also use injected platform capabilities.
   Draft removal acquires its write lease before any mutation and retains it
   across failure presentation and settings persistence, preserving their order.
   GNU full build `batch4-database-build-04.log` and 12 affected CTests pass
   (`batch4-database-tests-02.log` / XML). The additional removal control passes
   after its observer was corrected to read settings directly rather than reload
   database metadata (which would recreate a deleted SQLite file). That original
   failure and the initial build-information conversion error remain logged.

7. **Kalpha2 and compatibility retirement**: Application now reads environment
   values for every operation, selects the route and owns the source-bound edit.
   Dedicated selector priority, size checks and nonfinite-result mapping retain
   their original order/text. Core's numerical implementation is unchanged.
   Snapshot source/header bytes moved exactly to tests/support/snapshot; only
   contract and search-integration consumers build the new test support target.
   Unused product includes/links and src/compat were removed. GNU build
   `batch5-compat-build-04.log` and six affected normal tests pass. Fresh standalone
   GNU contract/integration configure, build and two CTests also pass
   (`batch5-standalone-*`). New selection controls cover invalid/default tokens,
   priority, per-operation reads and failure publication. Initial missing explicit
   numerical includes and an include accidentally placed inside a test namespace
   were repaired without assertion changes; original failures `-01` to `-03` remain.

8. **Display coordinates and GUI dependency closure**: Core now owns the former
   scalar and graph coordinate expressions, with their evaluation order and
   certifiedAsin64 transport unchanged. A separate Core target preserves the
   former default floating-point compilation mode. Application provides checked
   gesture conversion and domain-input admission. GUI directly links only
   Application, Qt Widgets/PrintSupport and its presentation library; unused
   legacy file/cell helpers and SearchRequest's no-op SQL-era method are removed.
   A bitwise comparison against the baseline expressions passes. GNU full build
   `batch5-plot-build-03.log` and 13 affected normal tests pass in
   `batch5-plot-tests.log` / XML. The first build caught a real Infrastructure
   consumer of the diagnostic timer; its original header was retained and GUI
   received its own presentation timer. Original failures remain logged.

9. **Mandatory strict boundaries and public API**: 17 Application headers now
   live under include/qualpf/application; the whole src tree is no longer a public
   export. Infrastructure exports no broad source root. Every boundary target
   declares its Qt modules explicitly. Configured target properties drive the
   default mandatory strict gate; the original checker also runs. Header-owner
   resolution covers short/relative names and visible include directories. Core
   free-operation references, SQL inheritance, pure-layer Qt contamination,
   indirect/facade links and test-support product links have negative controls.
   Application reuses its existing Abscissa value without exporting the Core
   plotting operation header. GNU full build `batch6-strict-build-03.log`, 26
   affected normal tests (`batch6-strict-tests.log` / XML), current-context checks
   and all 169 byte-preserved references pass. The coverage map retains all 82
   prior normal identities and adds four tests (86 total). Initial gap diagnostics
   and integration compile failures are retained; narrowing include exports
   required explicit test includes and the SQL writer's Application dependency.

10. **File metadata closure**: final source inspection found report rendering still
    used QFileInfo to read modification time. PlatformServices now returns owned
    existence/epoch values; GUI retains the identical local date formatting. An
    independent prior-Qt comparison and missing-file control pass. Strict negative
    controls also reject concrete GUI file/SQL/settings/process/network services.
    GNU full build and four affected tests pass (`batch6-file-metadata-*`).
    The first fresh Windows attempts were already running when this interface
    correction changed their inputs; their mixed-input linker failures are retained
    under final-evidence and disqualified. Final validation restarts from empty
    directories against the stable corrected source, under final-v2-evidence.

11. **Public header consumer repair**: both fresh product builds pass. Their
    separate Qt-free clients exposed three Powder CIF API includes that still
    required a private src-root path. They now use the same Core provider's
    exported header name. Both complete Qt-free clients and both reconfigured
    product builds pass after this mechanical include repair; initial failures
    remain in final-v2-evidence. Strict controls now also reject public Application
    headers that need private include directories. The initial source-input binding
    is retained separately before rebinding the final corrected inputs. No numerical
    source or test assertion changes in this repair.

12. **Explicit product build prerequisite**: the strict gate is also a dependency
    of the qualpf executable target. Both compilers' explicit target builds and
    strict CTests pass (`windows-*-target-gate-*`). This changes build ordering
    only; compile commands and executable hashes are checked against the preceding
    full normal-suite validation before final input binding.

## Windows checkpoint and original validation gap

This section preserves the observations at `24cdd26c`, before the independent
Linux closure linked above. Its original receipt and failure bytes are unchanged.

Implementation is complete on codex/strict-layer-convergence, with an explicit
Linux validation gap. Production has no src/compat or snapshot-support dependency.
GUI directly links Application and Qt Widgets/PrintSupport; Core/Application stay
Qt-free. Historical Milestone A/F conclusions and frozen evidence remain unchanged.
This task makes no new three-platform acceptance claim. PFSM is NOT_STARTED; STOP.

| Check | Windows x64 GCC 13.1 | Windows x64 MSVC 19.40 | Linux x64 GCC |
| --- | --- | --- | --- |
| Empty-directory configure and product build | PASS, including public-header repair | PASS, including public-header repair | NOT RUN |
| Original 82 normal identities + four additions | 86/86 PASS | 86/86 PASS (85 parallel + isolated registry migration) | NOT RUN |
| Complete Qt-free Application client | PASS | PASS | NOT RUN |
| Standalone snapshot contract/search consumers | 2/2 PASS | 2/2 PASS | NOT RUN |
| Staged install, ZIP and QtIFW generation | PASS | PASS | NOT RUN |
| Installed CLI/QSQLITE without SDK on PATH | PASS | PASS | NOT RUN |
| Actual compile/link and Zero-Fortran observations | PASS | PASS | NOT RUN |

Both fresh starts had no cache or objects. The independent public-header clients
exposed three include spellings that required repair; original failing logs and
initial source binding are retained. After this repair both complete clients and
products were rebuilt. The four changed inputs were those headers and their
boundary negative control; no numerical implementation/assertion changed.
The final build-only prerequisite was verified with explicit product builds and
strict CTests on both compilers. All compile commands and 73 executable hashes per
compiler match the preceding full runtime validation. Final inputs are bound in
[the validation receipt](strict-layer-validation.json).

GCC's full suite completes in 1005.45 seconds; MSVC's parallel 85-entry run in
1545.46 seconds, with its registry fixture separately passing. This retains the
original tests and timeout/assertion behavior. Both snapshot consumers retain
their ownership, length/overflow, unknown-radiation and missing-field assertions.
All 169 independent reference byte lengths/hashes still match, and Git reports
no changes to the frozen acceptance/reference paths.

The installed Ubuntu environment has x64 GCC 11.4 but no CMake, Ninja, pkg-config,
Qt 6 or Eigen configuration. The actual configure attempt exits 127 (cmake not
found). Linux build/tests/install/packages are therefore unverified. Provisioning
that toolchain and executing the existing Linux guide remains a validation gap,
not an inferred PASS or PFSM authorization.

Raw logs/XML, including all failed attempts, remain under
build/strict-layer-convergence. The receipt records their byte hashes and package
hashes. The first mixed-input build attempts are disqualified. Additional repaired
harness failures include MSVC's default Debug try-compile/PDB mismatch (the
standalone retry uses Release), CLI capitalization in a new smoke harness, and a
resource compiled by the composition target initially misclassified by source
folder. Product diagnostics and original approved assertions were not changed.
Staged installation and installer generation were exercised; no interactive
machine-wide installer run is claimed. Unrelated docs/maintenance work is retained.
