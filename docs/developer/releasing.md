# Independent distribution

QualPF's independent product version is **1.0.0**, its publisher is **adnxuic**,
and its application license is **GPL-3.0-only**. Earlier upstream CHANGELOG
versions remain historical records. `deploy/distribution.json` owns publisher,
license, description and project/support/manual URLs. Internal settings identities
and `QualXDB` remain compatibility keys, not publisher declarations.

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
It remains in Git at its original path/blob but is excluded from runtime/source
delivery and never displayed as the QualPF installer license. Removing it from
packaging does not establish grants for all inherited material.

The inventory distinguishes reviewed source grants, blocked material and
package-specific review. Outstanding rights include retained scientific
data: AtomProperties.xen, syminfo.lib, scattering/NIST source tables and the exact
symmetry catalogue. All 29 inherited runtime action images have
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
rights. `--release` rejects unresolved rights, dirty/unknown revisions, stale
build metadata, missing corresponding source and incomplete package reviews.
`QUALPF_DISTRIBUTION_AUDIT_ONLY=ON` permits local inspection artifacts with
`-audit` filenames, audit provenance and a separate audit installer title/path.
This flag does not waive licenses or clear redistribution. No tool here uploads
or publishes artifacts.
Both modes reject stale build revision/version metadata; reconfigure and rebuild
after committing before pairing binary and source artifacts.

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
  -DQUALPF_DISTRIBUTION_AUDIT_ONLY=ON
cmake --build --preset windows-gnu --parallel 4
cpack --config build/windows-gnu/CPackSourceConfig.cmake -G TGZ
cpack --config build/windows-gnu/CPackConfig.cmake -G IFW
```

Linux helpers accept `--audit-only`. DEB additionally requires an explicitly
supplied `QUALPF_RELEASE_CONTACT_EMAIL` environment value. Missing, placeholder
and Issues-URL contacts fail only when DEB packaging is selected, including an
audit DEB. Normal builds, IFW and AppImage need no email. The source helper
`scripts/package/make_tar_gz.sh` forwards CMake options, including the audit flag.

CPack checks component staging; AppImage inventory runs after linuxdeploy and
includes AppRun and transitive libraries. Inspect the final container/runtime as
well as the application payload. Verify publisher, canonical GPL text, icons,
desktop/shortcut targets, offline notices, installed startup, settings inheritance
and uninstall using isolated installation/settings directories. Record only the
native platforms and actions actually executed; retain original diagnostics.

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
includes it after rights clearance. The other 14 are independent test oracles,
provisioned through the existing
[restoration procedure](../../tests/README.md#independent-references). Tracked
scattering/chemistry/CIF catalogue inputs and generators are included as well.
A successful compile of generated `.inc` files alone is insufficient.

`distribution-source.json` records the source revision and every delivered
input's SHA-256. In an extracted archive without `.git`, run:

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

After rights clearance, configure audit mode OFF and supply:

- `QUALPF_CORRESPONDING_SOURCE_ARCHIVE`: the exact clean, non-audit source TGZ
  matching the binary revision/version. Provide equivalent access alongside the
  binary with clear source instructions; a moving branch link is insufficient.
  Fulfill applicable source obligations for copyleft dependencies as well.
- `QUALPF_DISTRIBUTION_RUNTIME_REVIEW`: JSON bound to `source_revision`. Its
  `files` object maps runtime paths to exact `sha256`, `component`, `copyright`,
  installed `notices` and hash-identified local `source_artifacts` (`path` and
  `sha256`), or a specific reviewed `source_exception`. Shared linking alone is
  not a source exception.
- That review's `components` object identifies actual Qt/Eigen/Boost versions,
  copyrights and installed notices. IFW/AppImage also require `package-runtimes`
  review covering the final container/runtime. Qt/Eigen reviews also bind
  hash-verified corresponding-source artifacts. Notices must cover the selected
  kit and embedded dependencies, not just generic license texts.
- For DEB, the actual public maintainer email in `QUALPF_RELEASE_CONTACT_EMAIL`.
  Never fabricate it or borrow an upstream author's address.

The checker verifies identities and missing-material rejection; it cannot invent
the underlying legal grant. Reconfigure/build after the final implementation
commit and pair binary/source artifacts at that revision. Release preparation
does not authorize scientific model, precision, compiler-flag or PFSM changes.
Pass `--build` when manually running formal staged checks, so preflight uses the
selected build's compiler and provenance rather than unspecified CLI defaults.

## Local 1.0.0 audit scope

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
review. Scientific and inherited-asset grants remain unresolved. Formal checks
still reject these audit artifacts. The earlier conflicting IFW artifact and
both failed byte comparisons are preserved.

The final source archive passed complete source/manifest checking and fresh
configuration without Git metadata. Its product/generator and remaining CMake
bytes match the earlier successful 279-step no-Git build (818 inputs compared);
the subsequent `QtDeployment.cmake` ownership fix was verified through fresh
configuration, component staging and actual archive extraction. This is not a
second full source compilation or a fresh 127-test platform matrix. The latest
installer's interactive install/uninstall and supported Linux package acceptance
remain **NOT_VERIFIED**. Later documentation-only commits do not change this
recorded artifact revision. No audit artifact is cleared for publication.
