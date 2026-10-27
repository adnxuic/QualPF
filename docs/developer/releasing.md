# Independent distribution

QualPF's independent product version is **1.0.0**, its publisher is **adnxuic**,
and its application license is **GPL-3.0-only**. Earlier upstream CHANGELOG
versions remain historical records. `deploy/distribution.json` owns publisher,
license, description and project/support/manual URLs. Internal settings identities
and `QualXDB` remain compatibility keys, not publisher declarations.

The [deployment-input index](../../deploy/README.md) lists the maintained installer
scripts, QualPF desktop templates, independent icons and source/rights catalogues.
Legacy deployment artwork and the old Windows agreement have been removed from
the active tree with hash-bound Git recovery identities; their original paths
remain excluded from both binary and corresponding-source deliveries. Linux
desktop templates use the QualPF name, omit the optional file-format version and
quote the installed executable path, including when an install prefix has spaces.
Changing these inputs requires fresh configuration/staging for the affected kit;
the earlier frozen 1.0.0 candidates and their checksums remain historical artifacts.

Read the [origin notice](../../NOTICE.md),
[third-party notices](../../THIRD_PARTY_NOTICES.md) and component inventory.
For material-specific public evidence, byte identities and the remaining grant
questions, see the [rights and source review](distribution-rights.md).
The README acknowledgment of Match!'s public PFSM method describes methodological
inspiration, separately from source provenance, licensing and the
[PFSM numerical contract](numerical/pfsm.md).

## Rights and review

The upstream author's GPLv3 update is
`f98a51da8435fef0f730b513e76c820fb879865d`; the upstream README still says LGPL.
The old Windows agreement refers to EXPO2013/GTK and restricted redistribution.
It remains in the recorded Git revision at its original path/blob but is excluded
from runtime/source delivery and never displayed as the QualPF installer license. Removing it from
packaging does not establish grants for all inherited material.

The inventory distinguishes reviewed grants/dispositions, blocked material,
package-specific review and the owner's scoped `upstream-policy`. On 23 October
2026 the owner chose to handle AtomProperties, FFAST, electron-scattering
parameters, `syminfo.lib` and the exact symmetry catalogue as QualX3 does: retain
the tables, scientific references and corresponding sources on the upstream
project-level GPL basis. These five scopes no longer fail release preflight
solely because separate data grants were not found. Original findings and the
NIST compilation-copyright qualification remain recorded; no new third-party
permission is asserted. All 29 inherited runtime action images have
been independently redrawn and their original files excluded from current source
and delivery. The rights inventory retains original identities and replacement
paths; their exclusion does not grant rights to the old artwork. The user manual
has also been independently authored, its inherited branding removed, and its
originals indexed by Git identity. The reviewed artwork/documentation component
records this replacement disposition, not a grant for the old materials.
Citations do not by themselves grant rights.
Preserve accepted data, frozen evidence, original failures and notices throughout
the review; never generate replacement references from product output.

Review actual Qt libraries/plugins/embedded dependencies, instantiated Eigen/Boost
headers, compiler runtimes, redistributables and installer/AppImage runtime for
each kit. Available original Qt SBOMs and Eigen notices are installed under
`share/qualpf/SDK-NOTICES`. Their presence is not a compatibility/source audit.

Ordinary consistency checks can succeed with explicitly recorded unresolved
rights. `--release` rejects blocked material, an invalid/widened upstream policy,
dirty/unknown revisions, stale build metadata, missing corresponding source and
incomplete package reviews. Inventories carry the scoped scientific-data policy
and its explicit unverified-grant qualification alongside the runtime findings.
`QUALPF_DISTRIBUTION_AUDIT_ONLY=ON` permits local inspection artifacts with
`-audit` filenames, audit provenance and a separate audit installer title/path.
This flag does not waive licenses or clear redistribution. No tool here uploads
or publishes artifacts.
Both modes reject stale build revision/version metadata; reconfigure and rebuild
after committing before pairing binary and source artifacts.

For the unchanged official QtIFW 4.8.1, the owner adopted a scoped vendor
disposition on 24 October 2026. `ifw_vendor_policy` pins the official SDK/tool
hashes and preserves two unverified static-build/CRT qualifications. Those
findings alone no longer block this vendor-based candidate. Formal checks still
require the selected tool bytes, collected sources and notices. This does not
clear other runtimes or relabel historical audit inventories. See the
[IFW disposition](distribution-rights.md#official-ifw-vendor-disposition) and
the copyright/licensing report process in NOTICE.md.

## Local package inspection

The owner's current Windows formal-distribution choice is MinGW/GNU; MSVC is
retained for development validation. MSVC formal IFW binaries and VC++ runtime
payloads are rejected. The supplied DEB maintainer address is
`uncercalculate@163.com`; pass it explicitly as
`QUALPF_RELEASE_CONTACT_EMAIL` (the cache default remains empty).

Use the [supported compiler and Qt kits](building.md). For Windows, select the
actual QtIFW installation and build before packaging:

```sh
cmake --preset local-windows-gnu -DCPACK_IFW_ROOT=/path/to/QtIFW \
  -DQUALPF_DISTRIBUTION_AUDIT_ONLY=OFF \
  -DQUALPF_DEPENDENCY_BUNDLE_DIR=/path/to/verified-bundle \
  -DQUALPF_DEPENDENCY_SOURCE_ARCHIVE=/path/to/qualpf-1.0.0-Windows-Dependencies.tar.gz \
  -DQUALPF_DISTRIBUTION_RUNTIME_REVIEW=/path/to/runtime-review.json \
  -DQUALPF_CORRESPONDING_SOURCE_ARCHIVE=/path/to/qualpf-1.0.0-Source.tar.gz
cmake --build --preset windows-gnu --parallel 4
cpack --config build/windows-gnu/CPackSourceConfig.cmake -G TGZ
cpack --config build/windows-gnu/CPackConfig.cmake -G IFW
```

Linux helpers accept `--audit-only`. DEB additionally requires an explicitly
supplied `QUALPF_RELEASE_CONTACT_EMAIL` environment value. Missing, placeholder
and Issues-URL contacts fail only when DEB packaging is selected, including an
audit DEB. Normal builds, IFW and AppImage need no email. CPack forwards this
contact through both preflight and staged-payload checks; the cache default
remains empty. The source helper
`scripts/package/make_tar_gz.sh` forwards CMake options, including the audit flag.
Both Linux binary helpers respect `CMAKE_BUILD_PARALLEL_LEVEL`; set it explicitly
on a memory-constrained runner. AppImage forwards the runtime-review and exact
source-archive environment values into CMake as well as the final staged check.
Its private SQL plugin directory retains only `libqsqlite.so`, matching the
product's database backend; optional SDK drivers and their unsupplied clients
are excluded before linuxdeploy walks plugin dependencies.
Each AppImage run retains the previous AppDir below the build directory and
assembles a fresh payload; cached tools and compiled objects are reused.
DEB, AppImage, CPack staging and installations with a nonempty `DESTDIR` install
desktop entries/icons only inside the package prefix. Direct non-staged installs
retain user desktop registration. The source-to-DEB wrapper preserves the full
release directory, including package checksums and `artifacts.json`, before
removing its temporary build tree.

CPack checks component staging; AppImage inventory runs after linuxdeploy and
includes AppRun and transitive libraries. Inspect the final container/runtime as
well as the application payload. Verify publisher, canonical GPL text, icons,
desktop/shortcut targets, offline notices, installed startup, settings inheritance
and uninstall using isolated installation/settings directories. Record only the
native platforms and actions actually executed; retain original diagnostics.

The repeatable installed-product control accepts an existing installation or
extracted package, uses a new isolated settings/data directory, verifies offline
legal texts, builds/searches a local independent COD CIF database, checks legacy
settings migration/preservation and starts the native GUI with `--auto`:

```sh
python -B tools/validation/check_installed_product.py \
  --executable /path/to/installation/bin/qualpf \
  --prefix /path/to/installation \
  --work-dir build/installed-check-01
```

Windows uses `qualpf.exe` and system-only PATH. Linux uses `xcb` by default and
requires an available display or Xvfb; it clears inherited SDK/library overrides.
For an AppImage, pass the image as `--executable` and the extracted `usr` tree as
`--prefix`; the image runs in extract-and-run mode. The resulting observations
identify the exact executable hash. This control does not install, uninstall,
verify shortcut creation or replace the final container/source review.

Installed `distribution-inventory.json` records actual file sizes/SHA-256,
component classifications and review blockers. Missing/stale notices fail even
audit staging. It excludes itself from hashing. Artifact sidecars identify exact
revision/version, toolchain, artifact SHA-256 and packaging-tool hashes. Generic
component labels are review aids, not inferred grants.
IFW post-build checks also inspect generated installer/component XML, the displayed
canonical license and the actual copied product icons. Component options are set
in one call so later script configuration cannot discard the license.
MSVC packaging must also run in the matching Visual Studio developer shell.
If deployment omits the VC++ runtime bootstrap required by the installer script,
the generated-IFW check rejects the artifact, including in audit mode.

An existing local PDF is preserved but installed only with matching documentation,
builder, publisher-input and PDF hashes from `docs/make_pdf.sh` or the portable
`tools/documentation/build_manual.py` entry. The generated proof stays under
`build/documentation` and accompanies an installed manual. Generate before CMake
configuration; stale local manuals must not silently carry old identity/contact
information into a new package. HTML Help always points to the current guide.

## Corresponding source and formal configuration

The source exporter copies tracked files only, plus independently hash-verified
preferred generator inputs. Local presets, caches, untracked/private examples,
the old agreement and superseded brand images stay outside delivery. The three
historical powder examples (`CPD2.dat`, `example1.dat`, `MIXT_5.dat`) and all
`tests/real_example_data` are excluded too: the retained `CoO.cif` explicitly
reserves PAULING FILE/SpringerMaterials rights. Originals
remain in the repository/protected history; no historical bytes or Git objects
are removed by packaging.

Of the 15 sealed reference ZIPs, `tests/data/symmetry-reference.zip` is also the
preferred input of the maintained space-group generator. Corresponding source
includes it under the recorded scientific-data policy. The other 14 are
independent test oracles, provisioned through the existing
[restoration procedure](../../tests/README.md#independent-references). Tracked
scattering/chemistry/CIF catalogue inputs and generators are included as well.
A successful compile of generated `.inc` files alone is insufficient.

`distribution-source.json` records the source revision and every delivered
input's SHA-256 and executable paths. Exports preserve file modes and apply Git's
executable bits even when exported from a Windows checkout. Source-tree checks
on POSIX and archive checks reject lost executable permissions. In an extracted
archive without `.git`, run:

```sh
python -B tools/validation/check_distribution.py --source-tree
cmake -S . -B build/source-check -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/source-check --target qualpf --parallel 4
python -B tools/crystallography/generate_space_groups.py --check
```

Supply normal SDK paths. Modifying/rebuilding the source remains possible;
modified delivered inputs become dirty rather than blocking an ordinary build.
New product sources also mark the delivered revision dirty. Source-boundary checks
use archive contents directly, including when extraction occurs under another Git
checkout; they cannot borrow the parent's index and report zero bound inputs.

After completing the recorded material dispositions and package-specific review,
configure audit mode OFF and supply:

- `QUALPF_CORRESPONDING_SOURCE_ARCHIVE`: the exact clean, non-audit source TGZ
  matching the binary revision/version. Provide equivalent access alongside the
  binary with clear source instructions; a moving branch link is insufficient.
  Fulfill applicable source obligations for copyleft dependencies as well.
- `QUALPF_DISTRIBUTION_RUNTIME_REVIEW`: JSON bound to `source_revision`. Its
  `files` object maps runtime paths to exact `sha256`, `component`, `copyright`,
  installed `notices` and hash-identified local `source_artifacts` (`path` and
  `sha256`), or a specific reviewed `source_exception`. Separate delivery uses
  `bundle_path` and `sha256` for entries inside the independently verified
  dependency archive. Shared linking alone is not a source exception.
- That review's `components` object identifies actual Qt/Eigen/Boost versions,
  copyrights and installed notices. IFW/AppImage also require `package-runtimes`
  review covering the final container/runtime. Qt/Eigen reviews also bind
  hash-verified corresponding-source artifacts. Notices must cover the selected
  kit and embedded dependencies, not just generic license texts.
- For official IFW, the `package-runtimes` entry additionally records
  `distribution_basis: official-prebuilt-ifw`, the exact `vendor_tools` mapping,
  `vendor_build_details_verified: false` and unchanged policy `qualifications`.
  Its hash-verified `source_artifacts` must include all eleven IFW-related
  catalogue archives. CPack checks binarycreator/installerbase against the pinned
  official bytes before packaging and after assembly. These checks preserve the
  source findings and implement the owner's vendor disposition separately.
- For DEB, the actual public maintainer email in `QUALPF_RELEASE_CONTACT_EMAIL`.
  Never fabricate it or borrow an upstream author's address.

The checker verifies identities and missing-material rejection; it cannot invent
the underlying legal grant. Reconfigure/build after the final implementation
commit and pair binary/source artifacts at that revision. Release preparation
does not authorize scientific model, precision, compiler-flag or PFSM changes.
Pass `--build` when manually running formal staged checks, so preflight uses the
selected build's compiler and provenance rather than unspecified CLI defaults.

The [upstream runtime comparison](distribution-rights.md#qualx3-runtime-distribution-comparison)
records the adopted vendor-SDK/binary-deployment approach. Original private vendor
logs, a vendor attestation or a bit-identical rebuild are not mandatory evidence
formats. Independently verified source, patches, notices and a complete applicable
build recipe can establish coverage instead. Actual corresponding-source and
redistribution obligations still apply to delivered components; the existence of
a QualX release does not clear an unknown QualPF runtime. Keep the selected MinGW
formal configuration and current package-specific checks.

## Current native package acceptance

### Separate dependency source delivery

Set `QUALPF_DEPENDENCY_SOURCE_ARCHIVE` to the exact dependency TGZ accompanying
the binary package, together with `QUALPF_DEPENDENCY_BUNDLE_DIR`. This opt-in
layout installs all original notices, attribution files, SDK metadata and supplier
instructions, plus the exact source index and `dependency-delivery.json` under
`share/qualpf/DEPENDENCIES`. The large `SOURCES` archive directory and nested
`SDK-SOURCE/*/SOURCES` archives stay in the independent dependency TGZ.
Bundled delivery remains the default when the archive option is unset;
existing Linux and earlier Windows candidate bytes retain their original scopes.

Configuration and CPack staging read every separate archive member without
extracting or executing it, compare its index and source pins with the selected
committed catalogue, and verify the installed notices. Runtime/header/IFW
reviews resolve `bundle_path` against that verified index; missing or altered
sources, notices, runtime bytes or the delivery descriptor still fail. Supply
`--dependency-archive /path/to/exact-dependency.tar.gz` when manually checking a
separate-source stage or installation. The archive's filename, size, SHA-256 and
index hash are retained in the installed delivery record and artifact sidecar.

Keep the independent dependency TGZ and matching application-source TGZ beside
the binary, with the candidate manifest and SHA256SUMS. Before publication,
provide equivalent free source access from the binary download location and
clear retrieval instructions. The offline source-code link in Origin and changes
opens [SOURCE_CODE.md](../../SOURCE_CODE.md). Separate delivery changes no
third-party terms or unverified vendor/scientific-data findings, and the collection
still records `release_clearance:false`.

Linux DEB and AppImage use the same options and exact-source checks. Their
`deploy/linux-dependency-sources.json` pins every original file of the previously
accepted Linux collection independently of the editable bundle index. It retains
the original ten base archives and the full distro, static-runtime, SDK and notice
evidence. A new application source archive is added after freezing the clean
revision; earlier application archives remain historical evidence. Each current
runtime review identifies the matching current source. The collection's false
clearance flag and recorded scientific/vendor qualifications remain unchanged.

Both Linux binary helpers forward `QUALPF_DEPENDENCY_BUNDLE_DIR` and
`QUALPF_DEPENDENCY_SOURCE_ARCHIVE` from the environment through CMake, staged
inventory and artifact recording. Use the exact `qualpf-1.0.0-Linux-Dependencies.tar.gz`
next to the matching application source and binary artifacts. DEB still requires
the supplied real maintainer email and uses system Qt/runtime dependencies;
AppImage retains its pinned runtime, reviewed bundled libraries and SQLite-only
SQL plugin layout. No platform, algorithm, precision or database contract changes
are implied by this packaging layout.

### Slim Linux candidates, 27 October 2026

The local DEB/AppImage pair and both independent source archives freeze clean
main revision `9bbbbcee1dc4e0e9fd69be38b5efc7f58545ff0e`, under
`releases/1.0.0/linux-slim-candidate-20261027`. The DEB is **12,382,246 bytes
(11.8 MiB)** and the AppImage **51,952,120 bytes (49.5 MiB)**. The common
2,335-input application source has SHA-256
`f6bb7e4516eeaf2028dd8bacf6cc6dddadfcc9200739be9b72708d162f54ec3b`;
the independent 805,869,206-byte Linux dependency archive has SHA-256
`521cdd29ae2ac4a6e3fd882269e39739430d36a02201980e9b9ee123c34c8f64`.
All original 890 source/notice files remain byte-identical; the new matching
application archive brings its current index to 891 files. The 258 source
members stay outside the binary packages, including nested Debian SDK archives.
The collection retains `release_clearance:false` and its original qualifications.

Both configurations built from the delivered source without Git metadata and
passed all four maintained table-generator checks. Debian 13 used GNU 14.2.0,
Qt 6.8.2 and Boost 1.83.0; **127/127 fresh normal tests** passed in 675.20 seconds.
Zero-Fortran checks covered 435 C++ commands and 107 binaries. Ubuntu 22.04 used
GNU 11.4.0, Qt 6.8.3 and Boost 1.74.0 for AppImage; all 181 runtime paths matched
the independent review, with only the owned AppRun product link rebound. About,
offline legal/source resources and 100%/150%/200% scaling passed on both Qt kits.

The actual DEB's **670 payload files** matched extraction and installation.
Its real maintainer, desktop entry, independent icon, Debian copyright materials,
offline legal texts and exact source delivery passed. Actual `dpkg --install`,
Xvfb/xcb startup, SQLite creation/search and old settings inheritance passed;
`dpkg --remove` removed every package file and desktop entry. Both isolated
settings hashes stayed unchanged. The package retains its actual Debian ABI
dependencies, including libc6 >= 2.38 and libstdc++6 >= 14.

The actual AppImage's **930 inventory files** matched extraction. Its pinned
runtime differs only in the expected 16-byte digest field; AppRun/.DirIcon, icon,
SQLite-only SQL plugin, RPATH and full notices/source bindings passed. Running
the actual image in extract-and-run mode passed Xvfb/xcb startup, SQLite, settings
and offline-text checks with SDK overrides cleared. An identical disposable
portable copy was run and removed; both isolated settings hashes stayed unchanged.
Neither package has an outstanding package-review blocker under the recorded
owner dispositions. Full GNOME/KDE integration, package-manager UI and FUSE
mounting were not exercised. AppImage does not claim the Debian full-suite run
as its own; its build, three About tests and actual image checks have separate
receipts. Scientific-data grants and retained vendor findings are not newly verified.

The final manifest, fifty-file SHA256SUMS, per-artifact hashes and release notes
identify the exact acceptance scopes. Raw evidence is under
`build/linux-slim-20261027`; key receipts are `debian-acceptance-01.json`,
`appimage-acceptance-01.json`, `debian-normal-01.xml`,
`native-source-inputs-01.json` and `candidate-final-verification-01.json`.
Earlier Windows/Linux candidates and the separate PFSM experimental branch remain
preserved. No package upload, push or third-party contact is included.

### Slim Windows candidate, 24 October 2026

The separately sourced MinGW candidate freezes clean revision
`6e09a99fc5bb0ed04166a0149ea1ec350bf56f13`. Its local delivery directory is
`releases/1.0.0/windows-slim-candidate-20261024`; acceptance-guide edits after
that revision do not change the frozen binary or corresponding-source identity.
The installer is **52,140,466 bytes (49.7 MiB)**, down from the earlier
384,752,117-byte bundled-source installer. Its SHA-256 is
`e68578899a005c7b24d2a7d6861edc3145d05db4e3c46ddeea127b0222bf2032`.
The matching application source includes 2,334 indexed inputs, builds without
Git metadata in 279 steps, and passes all four maintained table generators.
The full dependency TGZ retains the earlier nineteen archives and 527 indexed
files byte for byte; its SHA-256 remains
`b559f9402d3a58f5ba29909f695e4162edcbce0fec8ab9fd04e26de827413606`.

All **127/127 fresh native normal regressions** passed in 1,121.21 seconds,
including the original long-running powder property test. The final manifest
and SHA256SUMS bind the installer, both source archives and acceptance receipts.

The actual five component archives contain 564 files matching the staged payload.
The installed inventory checks 576 files, including the generated maintenance
tool; formal runtime/source review reports no package review blockers. Native GUI
installation, canonical GPL display, product icons, publisher/version registration
and all three shortcuts passed. Installed startup with system-only PATH, SQLite
database creation/search, legacy-settings inheritance and offline legal texts
passed. Interactive inspection covered GPL header/final appendix, origin and
third-party notices, retained vendor/scientific-data qualifications, and the new
local source-retrieval link. About scale checks cover 100%, 150% and 200%.

After normal application exit, the official IFW CLI purge returned zero and
removed the installation, maintenance tool, three shortcuts, Start Menu directory
and OS uninstall registration. The two isolated settings files retained identical
hashes. This records an actual CLI uninstall, not an interactive uninstall-wizard
run. Evidence is under `build/windows-slim-20261024`, particularly
`actual-container-02.json`, `installed-runtime-inventory-01.json`,
`gui-acceptance-01.json` and `windows-lifecycle-01.json`. Final regression and
checksum results are recorded in the candidate manifest. Earlier Windows/Linux
candidate checksum lists still verify unchanged. No upload or third-party contact
is included, and the owner dispositions do not become verified vendor/data grants.

### Earlier bundled-source Windows candidate

The local Windows MinGW **non-audit** candidate prepared on 24 October uses clean
revision `6b343c324b30563b3c59136c625e1485ae379567`. Its installer, matching
application/dependency sources, notices, manifest and SHA-256 list are under
`releases/1.0.0/windows-candidate-20261024`; raw receipts are under
`build/windows-candidate-20261024`. No upload occurred.

The fresh GNU 13.1 / Qt 6.8.3 build completed 631 steps and **127/127** normal
regressions. A fresh no-Git archive extraction checked all **2,333** inputs,
completed all **279** product steps, reported 1.0.0 and passed four generators.
The complete **527-file/nineteen-archive** Windows dependency collection remains
intact. The actual EXE's five component archives contain **584** files matching
CPack staging, including the installed inventory. GPL, publisher and independent
icons match. The selected official IFW tool hashes and generated installer and
maintenance-tool code sections match the reviewed vendor template; the two
vendor-build/CRT qualifications remain explicitly unverified.

The full native GUI installation completed and its wizard closed. All **584**
installed payload files match. Native startup with system-only PATH, SQLite
create/search, legacy settings import/preservation, offline legal text and three
shortcut targets passed. The generated maintenance tool has its own exact-byte
source/notice entry; the complete **596-file** installed inventory has zero
review blockers. Native About/offline checks at 100%, 150% and 200% passed and
their layouts were inspected.

**Scoped local acceptance is complete.** After the owner resumed control and
restored the minimized task window, all three installed legal tabs were inspected
interactively. GPL header/end, origin/modifications, original third-party notices,
the IFW qualifications and the copyright contact/removal process rendered locally.
The installed About identity and project/Issues links matched the candidate.

The task application exited normally. Actual official IFW CLI `purge` returned
zero and removed the entire installation, application and maintenance tool,
all three task shortcuts and the matching Windows uninstall registration
(`QualPF`, 1.0.0, adnxuic). Both isolated settings files retained identical hashes.
No manual file deletion substituted for uninstall. This is full GUI installation
and actual CLI removal; an interactive uninstaller wizard is not claimed.

The fresh interactive/lifecycle receipts are `gui-acceptance-02.json` and
`windows-lifecycle-02.json` under the candidate build directory. The original
Escape interruption, incomplete manifest/notes, sandbox temporary-directory
failure and missing-maintenance-tool review remain preserved alongside successful
bounded retries. Final acceptance metadata, notes and checksums do not retarget
the frozen binary/source revision. Its 127-test/no-Git build results retain their
original scope; guide-only acceptance edits did not trigger a new full-suite run.
No fresh MSVC lifecycle, clean-machine Windows VM or code signing is claimed.
The unverified vendor/scientific-grant qualifications and accepted Linux artifacts
below remain unchanged. No upload, publication or third-party contact occurred.

The 24 October 2026 local Linux **non-audit** candidate uses clean revision
`983b485bc00ee5d6c10fd5eae681ee644bca4ae0`. Files are under
`releases/1.0.0/linux-candidate-20261024`; receipts and original failures are
under `build/linux-candidate-20261024`. Later Windows-source/guide commits do
not relabel those source or binary bytes. No upload or publication occurred.

| Candidate or check | Freshly executed scope |
| --- | --- |
| Ubuntu 22.04 AppImage, GCC 11.4 / Qt 6.8.3 | Audit mode OFF, clean source, zero material/runtime blockers. All **1,180** actual extracted files match; pinned runtime, independent icon, SQLite-only SQL plugin and exact application source passed inspection. Actual extract-and-run/Xvfb startup, SQLite create/search, offline texts and legacy settings checks passed with SDK overrides cleared. |
| Matching application and dependency source | `qualpf-1.0.0-Source.tar.gz` is a tracked-only export of 2,329 delivered inputs, with original executable modes and commit timestamp. Fresh no-Git builds completed 277 AppImage product steps and 629 Debian product/test steps; all four maintained generators passed. The separate `qualpf-1.0.0-Linux-Dependencies.tar.gz` supplies all **890** exact source/patch/notice/SDK inputs from the candidate bundle. |
| Debian 13 build and regression, GCC 14.2 / Qt 6.8.2 | **127/127** normal tests passed under Xvfb; zero-Fortran checking covered 435 C++ units and 107 binaries. After storage recovery, all 107 binaries, build/compile fingerprints, 2,329 bound source inputs, 479 original private test inputs and installed system package checksums matched. The full-suite result is **REUSED_UNCHANGED_DEPENDENCIES**, not a new full-suite run. Fresh About checks at 100%, 150% and 200% also passed. The initial missing-display failures and five-test recheck remain preserved; no assertion or timeout changed. |
| Formal DEB actual lifecycle after recovery | **PASS**: all **920** package files and their installed copies matched. Actual `dpkg --install`, system-only-PATH startup, SQLite create/search, offline legal texts, legacy settings import/preservation and removal passed in isolated Debian 13. The executable and desktop entry were removed; isolated settings remain. Maintainer is `adnxuic <uncercalculate@163.com>`. The original truncated package was rejected; the accepted container was regenerated from unchanged verified code using RAM for staging/extraction, then persisted and rehashed in the project output. |

`RELEASE_NOTES.md`, `candidate-manifest.json` and `SHA256SUMS` identify both
accepted packages, matching application/dependency sources, toolchains and
verification scope. DEB SHA-256 is
`f12836a9b1a1fa81f1e237a66c0375f7bf798c494c93dba843e689acef0d5b07`;
AppImage SHA-256 is
`2bc34e39c0948f863764eaca731bbd927a474a34a392e887fcf9dac0f53bd4bc`;
application source SHA-256 is
`ba8360bb68245b5de1a5c410d2c69a8aa66054bdd8b65327f6744261cad2e072`;
dependency source SHA-256 is
`828e6fc569d0bb2809bacb2c6c7ad3c551da6ee2246d796f785ede5900ad5145`.
The bundle is the exact frozen Linux input, not the later expanded Windows
source catalogue. It retains unused qualified Windows materials and collection
`release_clearance:false`; package-specific Linux reviews are separate. The five
scientific scopes still retain `separate_data_grants_verified:false`.

These checks cover native Xvfb and AppImage extract-and-run. Full GNOME and
desktop integration services were not exercised. The DEB targets **Debian 13**
with its generated native ABI minimums (Qt 6.8.2, glibc 2.38, libstdc++ 14),
not Ubuntu 22.04. Linux candidates do not inherit the separate Windows gaps;
each platform needs its own completed package/runtime review.

The WSL failure was followed up on the host: the Ubuntu VHD is
`D:/Ubuntu_WSL/ext4.vhdx`, and D: had only about **7.5 MiB** free. C:/E: free
space and the guest's virtual disk maximum did not measure that host capacity.
Normal WSL shutdown and explicit root/system startup probes initially failed.
The owner authorized deleting disposable caches/old derived outputs and
explicitly rejected cross-volume relocation. Download caches were deleted in
place, and WSL returned to a writable state. Inspection found that the first
DEB had persisted only **390,070,272** of its recorded **801,362,516** bytes;
the extracted inventory was empty. Original expected/actual hashes, sizes and
sidecars remain in `build/wsl-storage-recovery-20261024`; damaged generated
outputs were discarded instead of changing their expected values.

Every retained image/source and every compiled Debian input was reverified.
Seven derived directories, 272 retired object/archive files and one further
metadata-variant extraction were deleted after comparison with the actual
retained AppImages. The two unique original metadata files were preserved.
All canonical source trees, current Debian binaries/configuration, independent
references, protected historical backups and required logs remain. No cleanup
directory was moved to another drive. The new DEB's actual lifecycle passed;
the earlier audit lifecycle was not substituted for it.

The native packaging/extraction workspace used `/dev/shm`, with the completed
artifact copied to its normal project output. Its seven task mounts were verified
detached and temporary RAM copies removed. The installed WSL **3.0.3** provides
`wsl --manage Ubuntu --compact` for in-place VHD allocation recovery after the
authorized deletions. Check installed support, drain workloads and retain source,
valid artifacts and raw failure evidence before using this maintenance command.
It does not relocate the VHD. Actual recovery receipts remain under
`build/wsl-storage-recovery-20261024`; no reinstall or unsafe sparse-VHD option
was used.

The in-place command completed successfully on 24 October: the VHD fell from
151,755,161,600 to **126,018,912,256 bytes**, releasing about **24.0 GiB**;
D: then had about **26.3 GiB** free. Its original path did not change. Normal
default-user Ubuntu startup passed, and all source/build fingerprints, 107
binaries, original private inputs and installed system checksums matched again
after compaction. Final candidate hashes also passed independently.

Before substantial WSL builds/staging, check the Windows volume that contains
the registered VHD, as well as guest free space. For the `Ubuntu` instance in
PowerShell, this read-only probe identifies the actual host drive:

```powershell
$qualpfDistroKey = Get-ChildItem HKCU:\Software\Microsoft\Windows\CurrentVersion\Lxss |
    Where-Object { $_.GetValue('DistributionName') -eq 'Ubuntu' }
$qualpfVhdItem = Get-Item -LiteralPath (Join-Path $qualpfDistroKey.GetValue('BasePath') 'ext4.vhdx')
$qualpfVhdItem | Select-Object FullName, Length
$qualpfVhdItem.PSDrive | Select-Object Name, Used, Free
```

Budget for source/SDK copies, build objects, staging, compressed artifacts and
extraction/installation copies together. Recheck after large batches; the guest's
virtual free space cannot substitute for host capacity. Routine packaging does
not authorize unrelated host-file deletion or VHD relocation/repair. The scoped
storage recovery above follows the owner's separate explicit cleanup request.

### 23 October audit evidence

The 23 October 2026 follow-up evidence is under `build/runtime-acceptance`.
These are local **audit** artifacts, not published release files. Each binary is
paired with its own exact clean-revision source archive; later guide or helper
commits do not change the recorded artifact provenance. A formal Windows candidate
still needs regeneration from its intended committed revision with audit mode
OFF after its runtime/source/container review is completed.

| Configuration or action | Freshly executed scope |
| --- | --- |
| Windows MinGW 13.1 / Qt 6.8.3, clean `955d3a8355ffdfd55f0155670293b3f45c7fc8f8` | Product build and **127/127** normal tests passed, including About/offline text at 100%, 150% and 200%. The no-Git source archive completed all 279 product steps and all four generators. |
| Actual MinGW IFW lifecycle | All **537** installed payload files matched component bytes. Actual CLI installation, system-only-PATH startup, SQLite, legacy settings import/preservation, three shortcuts and CLI purge passed. The GPL installer page and native installed About/legal tabs were inspected interactively; a second complete GUI wizard run is not claimed. Installation and task shortcuts were removed, and isolated settings retained. |
| Linux GCC 14.2 / Debian Qt 6.8.2, clean `8e645c1e1f6ee9f5e453956f44a9b86257b5f52e` | Native no-Git product build and **127/127** normal tests passed; zero-Fortran checks inspected 435 C++ units and 107 binaries. All four preferred-input generator checks also passed on the clean source export. |
| Actual Debian 13 DEB lifecycle, same `8e645c1e` source | All **920** package files and their installed copies matched. The real maintainer field is `adnxuic <uncercalculate@163.com>`. Actual install, native Xvfb startup, SQLite, offline notices, legacy settings and removal passed; executable/desktop entry were removed and isolated settings retained. Qt is a system dependency, and the native Debian ABI minimums apply. No full GNOME session or interactive package-manager UI is claimed. |
| Actual Ubuntu 22.04 / Qt 6.8.3 AppImage, same `8e645c1e` source | All **1,180** extracted files matched their inventory, including `.DirIcon`; actual pinned type-2 runtime, independent icon and SQLite-only SQL plugin checks passed. Running the actual image in extract-and-run mode passed native Xvfb startup, SQLite create/search, offline notices and legacy settings checks with SDK overrides cleared. The inventory has zero material/runtime review blockers and retains the unverified separate-scientific-grant qualification. Desktop integration services were not exercised. |

Actual lifecycle/source/runtime receipts are `windows-lifecycle-01.json`,
`debian-lifecycle-04.json`, `appimage-inspection-06.json`,
`appimage-installed-smoke-06/observations.json` and the platform-specific
artifact sidecars. The final DEB and AppImage/source pair is in
`build/runtime-acceptance/linux-clean-packages`; MinGW files are in
`build/runtime-acceptance/windows-packages`. Source and artifact SHA-256 values
are recorded in those sidecars, together with the actual toolchain and packaging
tool hashes. The DEB has no missing runtime/source review item. Those Windows
receipts retain three GCC DLL binding gaps and the separate IFW review. The later
[public GCC corresponding-source disposition](distribution-rights.md#current-gcc-corresponding-source-disposition)
completes the source-material review for the pinned DLLs; it does not relabel
these older inventories. A new Windows candidate must bind the supplied source
materials and notices and apply the current scoped official-IFW disposition.

Original failures remain available: Ubuntu Qt 6.2 compilation, Python 3.10
validation prerequisites, mounted-filesystem strict-check timeout, missing private
test inputs, missing DEB `file` utility/contact forwarding, unused Mimer driver,
and the AppImage dirty-source/unlisted-`.DirIcon` checks. No assertion or timeout
was weakened. The mounted Ubuntu run was **126/127**, followed by the unchanged
strict test passing in **4.79 seconds** on native storage; those two runs are not
presented as one fresh 127-test invocation. The complete final 127/127 Linux run
above used native Debian storage and the exact original development fixtures,
including the three separately excluded powder examples. None entered delivery.

For reproducible AppImage assembly, set `QUALPF_APPIMAGE_RUNTIME_FILE` to the
reviewed runtime and provide its `runtime_sha256` in the package-runtime review.
The helper rejects a missing file, absent review/hash or mismatched bytes before
configuration and records the runtime alongside linuxdeploy/appimagetool. It
registers `.DirIcon` before inventorying rather than allowing appimagetool to add
an unlisted file. Retain tools and the runtime by hash; a mutable `continuous`
download alone is not a corresponding-source binding. See the
[Linux source findings](distribution-rights.md#actual-linux-runtime-and-source-bindings).

MSVC remains a development configuration. No fresh MSVC installation or complete
interactive Linux desktop integration is asserted by this follow-up. The remaining
two Windows evidence groups and the owner's lack of original vendor records are
listed in the [rights review](distribution-rights.md#owner-decisions-and-information-still-needed).
No package was uploaded or published.

## Earlier local 1.0.0 audit scope

This section retains earlier checkpoint scopes. The current native acceptance
above supersedes their then-unexecuted Linux and MinGW lifecycle items.

The earlier local audit artifacts checked on 22 October 2026 identify clean payload
revision `a8bc3dbec1f80dfb916474e7f856a46c0c127d1d`. Both Windows IFW packages
and the corresponding-source TGZ have matching version/revision records and
verified SHA-256 sidecars. Later documentation-only commits do not change that
recorded payload identity. They precede the follow-up material exclusions and
MinGW-only release decision and remain historical audit evidence. The rights
and runtime source reviews above remain prerequisites for formal distribution.

| Configuration or action | Executed scope and qualification |
| --- | --- |
| Windows MinGW 13.1 / Qt 6.8.3 | All 126 normal tests executed: 124 passed in the restricted run; the original native-settings/loopback failures are retained, and both unchanged controls passed with native permissions. After the final Help changes, all 31 affected GUI/CLI/distribution/native controls passed. Zero-Fortran checks passed. |
| Windows MSVC 19.40 / Qt 6.8.3 | All 126 normal tests and 11 standalone Qt-free tests passed. Build-input capture and zero-Fortran checks passed. |
| About scaling and offline text | Checks at 100%, 150% and 200% passed on both Windows kits. Native MinGW screenshots and installed offline legal tabs were inspected. |
| Source TGZ without Git metadata | The product built through all 279 steps in a fresh directory; all four maintained table generators reproduced their committed outputs, and the executable reported version 1.0.0. |
| IFW packages | Final generated MinGW/MSVC metadata, canonical displayed GPL, copied icons, notices and payload identities passed inspection. The MSVC payload includes the required runtime bootstrap. |
| Native installation lifecycle | MinGW revision `322351d9` was installed and checked for startup with system-only PATH, database access, unchanged legacy settings import, shortcuts, offline notices and uninstall. The task installation and shortcuts were removed and settings preserved. Final revision checks above cover subsequent Help and packaging controls. An actual MSVC installation/uninstall remains **NOT_VERIFIED**. |
| Supported Linux GCC / DEB / AppImage | **NOT_VERIFIED**: the available WSL Ubuntu 22.04 lacks the supported SDK and the Docker engine is unavailable. Shell syntax was checked, but supported native builds, regressions, extracted package contents and installed behavior still require execution. The owner has now supplied `uncercalculate@163.com`; pass it explicitly when building DEB. |

Original diagnostics and local package/test evidence remain under the ignored
`build/distribution-readiness` directory. None of these artifacts was uploaded
or published; passing local checks does not close the recorded authorization gaps.

The follow-up [material/source review](distribution-rights.md) records the
current MinGW-only formal policy, excluded optional examples and offline
dependency bundle. Its fresh MinGW staging checks bind 18 runtime files and
Eigen/Boost header sources; three GCC DLL bindings and the IFW container review
remain incomplete. The six affected distribution/About/startup controls passed
with the documented SDK DLL paths, and the installed application passed startup,
SQLite and settings migration checks using only Windows system directories on
PATH. The original missing-DLL test run remains retained. The full normal suite
now registers 127 entries; this follow-up does not claim a fresh full matrix run.
After the material exclusions, a fresh export without Git metadata completed all
279 application build steps and all four maintained generator checks. Original
diagnostics and follow-up evidence remain in `build/distribution-rights`.

On 23 October, the final follow-up MinGW IFW audit and source TGZ were paired at
clean revision `7690fb9372bc950af9ea7af12c9e735ca13d95fd`. Their exact hashes and
six build/packaging tool hashes are recorded in
`build/distribution-rights/clean-artifact-pair-03.json`. The actual installer was
dumped and all **537** payload files matched the component staging bytes, with
no duplicate destinations. Its publisher, independent icons and canonical GPL
were checked from the dumped metadata. The uncleared optional examples, old D3D
copy and VC redistributable were absent. The extracted application passed the
isolated system-only-PATH startup, SQLite and legacy settings checks.

`clean-artifact-inspection-03.json` retains those comparisons;
`actual-ifw-reviewed-03-inventory.json` binds 18 runtime files and the header/SDK
materials, retaining three unreviewed GCC DLLs and the incomplete IFW container
review. At that checkpoint the scientific and inherited-asset findings were still
unresolved; the later scoped table policy and independent replacements are recorded
above. Formal checks
still reject these audit artifacts. The earlier conflicting IFW artifact and
both failed byte comparisons are preserved.

The final source archive passed complete source/manifest checking and fresh
configuration without Git metadata. Its product/generator and remaining CMake
bytes match the earlier successful 279-step no-Git build (818 inputs compared);
the subsequent `QtDeployment.cmake` ownership fix was verified through fresh
configuration, component staging and actual archive extraction. This is not a
second full source compilation or a fresh 127-test platform matrix. The latest
installer's interactive install/uninstall and supported Linux package acceptance
were **NOT_VERIFIED** at that checkpoint. Later documentation-only commits do not change this
recorded artifact revision. No audit artifact is cleared for publication.
