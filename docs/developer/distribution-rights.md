# Distribution rights and source review

This guide records material-specific findings from the public-source review on
22 October 2026, with actual package verification completed on 23 October.
The corresponding byte/origin inventory is
[`deploy/rights-review.json`](../../deploy/rights-review.json). It supplements
the [distribution procedure](releasing.md), not the scientific acceptance
contracts. No scientific input, generated table or reference was changed.

## Scientific inputs

On 23 October 2026, the owner instructed QualPF to follow QualX3's handling of
these five inherited table scopes. The tables, original references and preferred
source inputs are retained on the upstream project-level GPL distribution basis.
`scientific_data_policy` in `deploy/distribution.json` is the decision's single
source. The component and five material entries use `upstream-policy`, distinct
from `reviewed`; their previous blocked status and public findings remain recorded.
Missing separate data-grant evidence alone no longer blocks these five scopes.

This disposition is not evidence of a new NIST or other third-party grant. The
NIST compilation-copyright qualification and original file-specific terms remain
visible. Runtime reviews, databases, excluded examples/CIF and replaced artwork
or manual originals are outside this decision. New unreviewed material cannot
silently inherit it.

| Material | Retained public evidence and qualification |
| --- | --- |
| `AtomProperties.xen` | Its own header cites International Tables C 6.1.1.4, 4.2.6.8 and 4.2.4.3, Bondi/Rowland-Taylor radii and published atomic weights. The QualX GPL update establishes repository provenance; no separate grant for this combined compilation or its external tables was located in the public review. The owner has adopted the upstream distribution basis for the retained input and generated tables. |
| NIST FFAST extraction | The retained `nist_table.f90.txt` records the original NIST download/generation procedure for Z=1–92. The [official FFAST page](https://www.nist.gov/pml/x-ray-form-factor-attenuation-and-scattering-tables) identifies **Standard Reference Database 66** and reserves compilation copyright. NIST's [licensing statement](https://www.nist.gov/open/copyright-fair-use-and-licensing-statements-srd-data-software-and-technical-series-publications) distinguishes SRD from ordinary government data. The general non-SRD grant is not substituted for its terms. [FFAST.jl's Unlicense](https://github.com/usnistgov/FFAST.jl/blob/master/LICENSE) grants software rights without explicitly resolving this older SRD compilation. No separate NIST grant was located in the examined QualX materials; that qualification remains recorded under the chosen upstream policy. |
| Electron-scattering parameters | The active Lobato block cites [Lobato and Van Dyck (2014)](https://doi.org/10.1107/S205327331401643X). The author's [MULTEM source](https://github.com/Ivanlh20/MULTEM/blob/master/src/atomic_data.hpp) carries GPLv3-or-later, but its current parameter table differs from this retained version. Its license does not prove identity of these coefficients. The preferred Fortran input also retains unused Kirkland/Doyle-Turner tables. No replacement, rounding change or new product-derived reference was made. |
| `syminfo.lib` | [CCP4's symmetry documentation](https://www.ccp4.ac.uk/html/symlib.html) identifies the format. The [official libccp4 6.3 source directory](https://ftp.ccp4.ac.uk/ccp4/6.3.0/unpacked/lib/libccp4/) supplies GPLv3/LGPLv3 texts. CCP4 6.3 and 6.4 publish the same file (SHA-256 `d8ea437d3e60ab4ef2007d2f5cfcd1cc4f16c2761be2dce041589d396dbfc42a`), which differs from QualPF's retained file in aliases and settings as well as whitespace. This establishes a base-source lead, not clearance of every modified entry or the separate exact catalogue. |
| Exact symmetry catalogue | The frozen archive has 14 members, including four original source files. The generator uses `exact-table.json` and `symm_table.f90`; their independently recorded hashes remain unchanged. The underlying table compilation's origin/grant still needs confirmation. The separately licensed spglib naming supplement does not license this other catalogue. |
| Historical powder examples | No file-specific redistribution grant was located for `CPD2.dat`, `example1.dat` or `MIXT_5.dat`. They are excluded from installation and source export. Development inputs and tests retain their original bytes. Permission is needed only if these examples are reintroduced into delivery. |
| `CoO.cif` reference structure | The retained header identifies PAULING FILE Multinaries Edition 2012 / SpringerMaterials Release 2014, copyright Springer/MPDS/NIMS, **All Rights Reserved**. It is not a freely licensed COD fixture. This original remains in the development checkout with its existing assertions, but is excluded from source delivery. No replacement reference was generated. |

The scoped policy covers both the inherited source inputs and their generated
C++ representations. Preserve all scientific values, references, generator
inputs and original failures. Unused tables in a preferred input retain their
original source/rights qualifications; their absence from executable calculations
does not create a new grant.

## Inherited artwork and documents

All **29** formerly registered action images were independently redrawn on
23 October 2026. The old PNG/XPM files and `:/images/` registrations have been
removed from current source and delivery; new original SVG masters and 24px/48px
PNG exports use `:/icons/`. Their construction follows operation semantics and
the existing QualPF palette, without tracing or importing old artwork.

The rights inventory retains each original SHA-256, QualX Git blob/path and grant
finding, plus the last retained QualPF revision and replacement SVG path. It
marks the originals excluded and removed from source, not licensed or cleared.
Retained Git blob IDs identify repository content; `retained_checkout_eol: crlf`
records the Windows XPM checkout conversion needed to reproduce the original
review's byte hash. PNGs recover directly from the recorded Git revision/path.
The old PNG/XPM metadata did not
supply a usable author/license statement. In particular, a file name such as
`pan_121784.png` is a search lead, not attribution. The upstream GPL change alone
is not treated as proof that an external icon author's rights were granted.

The original documentation favicon and QualX SVG were removed from current source
on 23 October 2026 and remain excluded from delivery. Their original Git identities
are recorded in the documentation replacement inventory. QualPF branding and action SVG masters remain
separate from these excluded inherited graphics. Consistency checks reject old
resource registrations (including renamed prefixes), reintroduced old source
files, missing exclusion rules and absent/unregistered replacements. Runtime
tests decode every replacement at 100/150/200% scale, including disabled states.

All 17 user manual pages were independently authored on 23 October 2026 from
current UI implementation and accepted product contracts. The manual now uses
the reader's own measurement, without delivering historical experimental examples
or reproducing their claimed compositions. Its new analysis-route illustration
has an original editable SVG master and a reproducible PNG export. The existing
QualPF logo/favicon retain their independent branding source. Old analytics and
unused MathJax assets are removed from publication.

`documentation_replacement` in the rights inventory records the entry revision,
original Git blobs and hashes, authored sources and artwork derivation. Original
manual content is recoverable from that revision, not shipped as a second manual.
Bibliographic facts and required upstream/third-party attributions remain.
The `inherited-artwork-docs` component's reviewed status describes the completed
replacement/exclusion disposition; it grants no rights in excluded originals.
New contributions follow GPL-3.0-only to the extent applicable rights exist.
The scoped scientific-data policy and package-specific runtime reviews remain separate.

## Actual Windows runtime findings

The initial inspected artifacts identify payload revision
`a8bc3dbec1f80dfb916474e7f856a46c0c127d1d`. The later vendor comparisons and
[current acceptance](releasing.md#current-native-package-acceptance) record their
own artifact scopes; earlier findings do not silently approve different bytes.

- MinGW has 17 deployed Qt DLL/plugin files, and MSVC has 20, including Qt SVG.
  Each is byte-identical to its selected Qt 6.8.3 SDK file. The official QtBase
  archive's `.tag` equals SDK SBOM commit
  `c07c2d5a527a644d36e7853d55132ae38921682f`. The SDK SBOM also records original
  configuration arguments. QtBase/Qt SVG sources and their embedded notices
  are now carried in the optional offline bundle and bound by hash in the local
  MinGW staging review; generic LGPL text alone is insufficient.
  A source-SBOM comparison finds 7,117 raw matches and 14,500 line-ending-only
  matches in each kit. The sole content mismatch is archive `.tag` bookkeeping;
  377 `.gitignore` and 23 `.gitattributes` files are absent from the public
  archive. These differences are retained in full; this is not a claim that
  all 22,018 SBOM entries are byte-identical to the archive.
- `libwinpthread-1.dll` has its own upstream MIT and Lockless BSD notices. Both
  are now retained in `LICENSES/mingw-winpthreads-COPYING.txt`. It is no longer
  grouped under the GCC Runtime Library Exception. MinGW CRT notices are also
  retained separately.
- GCC 13.1.0 upstream source has been acquired and checked against GNU's official
  SHA-512 list. The SDK `build-info.txt` names ten GCC patches and exact configure
  arguments. All ten named patch files were located in MinGW-builds snapshot
  `7ff96692a8032d893781523b311ac9b6cbbda0a1`, at the public binary release date.
  That snapshot and MinGW-w64 v11.0.0 source were acquired too. The SDK omits its
  exact build-script Git revision, so the date-matched snapshot remains a
  reconstruction candidate rather than an attested vendor build input.
  The follow-up compared the selected SDK's three GCC DLLs, winpthreads and
  `build-info.txt` against the vendor's original
  [13.1.0-rt_v11-rev1 archive](https://github.com/niXman/mingw-builds-binaries/releases/tag/13.1.0-rt_v11-rev1):
  all five are byte-identical. The vendor archive SHA-256 is
  `6b41fdf246756c04d2ac413d8835e347ffc18fdeda265f5310ac6aeb3c1dbbcf`.
  The [tagged build workflow](https://github.com/niXman/mingw-builds-binaries/blob/ecaa196ea0a8c39f77a44cb9760c1cc8160f67dd/.github/workflows/build.yml)
  matches the SDK arguments but checks out mutable `develop`. No retained run
  was returned for that release commit. Binary identity is now established;
  the exact resolved source recipe commit remains unverified. These distinct
  results are recorded under `runtime_provenance_followup` in the rights inventory.
  A reconstruction applied all ten named patches successfully to the original
  GNU 13.1.0 archive. The first direct comparison found 668 matching libstdc++
  headers after LF normalization and retained 146 without a direct mapping.
  The 24 October follow-up resolves target copies/aliases: **809 of 814** actual
  headers match patched GNU source. Four generated thread headers reproduce
  exactly from GNU's Makefile/sed rules. The last `c++config.h` has its template,
  configure inputs, source version/date and actual generated SDK bytes retained.
  No unexplained header origin remains; factory configure probes were not rerun.
  The offline supplement supplies all actual C++/target SDK headers and original
  build metadata as additional source materials. This is not a factory-commit
  attestation or a bit-identical runtime rebuild. The candidate recipe retrieves
  a mutable MinGW-w64 v11 branch. Public branch history bounded by the original
  build date identifies `c3e587c067a00a561899d49d3e63a659e38802ec`; its SDK
  11.0.1 version-header Git blob matches, and **1,616/1,635** actual target files
  match after LF normalization, including winpthreads. The older v11.0.0 input
  matched 1,600. Both comparisons and all 19 remaining generated/extra files are
  retained. This narrows the original branch reconstruction; complete compiled
  CRT/DLL source/configuration correspondence still requires review. Earlier
  receipts remain unchanged; these comparisons alone do not clear the three DLLs.
  A further full-source review resolves all nineteen target-file origins:
  twelve match CRT `libsrc` files, two reproduce from MinGW configuration
  templates/rules, three match GNU libiconv 1.17 inputs and two come from zlib
  1.2.13. All **1,635** actual target files were rehashed. The three zlib patches
  named by the original SDK and the original configure sed rules reproduce
  `zconf.h` exactly. The direct-source mismatch and earlier probe failures are
  retained. Source/template origin coverage does not by itself establish complete
  compiled DLL/configuration correspondence.
- QtIFW 4.8.1 `installerbase.exe` contains the build marker **Qt 6.6.0**, and
  its embedded OpenSSL marker is **3.0.14**. Its corresponding sources and
  notices must be reviewed independently of application Qt 6.8.3. Versioned IFW,
  Qt 6.6.0 and OpenSSL sources have been acquired; exact static build settings,
  patches and all linked component provenance still require binding. The IFW
  GPL exception and actual libarchive/OpenSSL notices are retained.
  Its PE linker version is 14.36 and its import table names Windows system DLLs,
  with no separate VC++ runtime DLL import. That does not prove absence of a
  statically linked MSVC CRT. Application MinGW selection alone cannot clear
  the installer container's own build/redistribution obligations.
- The retained earlier packages also include graphics support code: `D3Dcompiler_47.dll`, and in
  MSVC, `dxcompiler.dll`, `dxil.dll` and `opengl32sw.dll`. Qt deployment is not a
  license grant for these Microsoft/Mesa components. Their exact source/terms
  remain separate obligations for those earlier artifacts.

New MinGW deployment omits the SDK's old system D3D compiler copy using
`windeployqt --no-system-d3d-compiler`. The supported Windows platform supplies
that operating-system component. Staging records/rejects a reintroduced copy for
formal distribution. The other graphics runtimes above describe the retained
MSVC development audit, which is excluded from formal release.

Actual IFW extraction also exposed different same-named GCC/winpthreads DLLs
in its two components: Qt deployment had copied `D:/mingv/mingw64/bin` through
the host PATH, while the application component used the selected compiler SDK.
Flat installation had overwritten these copies and concealed the conflict.
MinGW Qt deployment now disables compiler-runtime copying; the existing
compiler-matched deployer remains their single source. IFW staging rejects
different bytes targeting the same installed path even in audit mode. The
original conflicting installer and failed byte comparisons remain retained.

## Offline dependency materials

[`deploy/dependency-sources.json`](../../deploy/dependency-sources.json) pins sixteen
acquired upstream archives, their exact commits where available, SHA-256 and
official checksum URLs where published. Their availability does not erase the
vendor-build qualifications above. The preparer uses an existing local cache;
ordinary configuration/builds never download sources.

The additional IFW archives are official zlib **1.3.1**, bzip2 **1.0.8** and XZ
**5.6.2**, matching actual installerbase version strings. The bzip2 archive also
matches Sourceware's published SHA-512 list. IFW's own source contains libarchive
**3.7.4** and LZMA SDK **9.38.beta**; the SDK generic notice's 9.20 reference is
stale. The original archives, full notices and both versions of that evidence
remain retained. liblzma 5.6.2 is 0BSD; other XZ files keep their own terms.
The observed zlib 1.3.1 differs from stock Qt 6.6.0's bundled 1.2.13, so the
stock Qt source defaults alone do not establish the actual external-library or
patch configuration. Static configure/link coverage and CRT redistribution basis
remain unverified. No missing CRT DLL import is treated as proof that no static
CRT is present.

The local supplement `build/windows-source-20261024/offline-supplement` supplies
all thirteen source archives, full legal texts, actual SDK header sources,
original build metadata and the independently checked reconstruction receipts.
Its `SOURCE-MATERIALS.json` binds every delivered file by SHA-256 and keeps
`release_clearance:false`; its README identifies usable supplier build commands
and the remaining qualifications. It is separate from the frozen Linux candidate
bundle and does not overwrite any earlier audit installer or inventory.
The additional `build/windows-source-20261024/branch-source-supplement` retains
the public v11 branch snapshot, original notices, bounded history query, exact
version-header Git blob and both target-header comparisons. Retain both local
collections; neither declares complete Windows runtime/CRT clearance.

`build/windows-source-20261024/target-source-supplement-07` adds the exact GNU
libiconv 1.17 and zlib 1.2.13 sources, original legal texts and all nineteen
target-origin findings. The latter zlib version belongs to the compiler SDK;
it is separate from the IFW 1.3.1 source. All source pins and supplement hashes
were verified. Earlier raw mismatches and failed probes remain, followed by the
successful original-patch/template reproduction. No SDK/runtime was replaced,
and no frozen Linux candidate source or artifact was changed.

```sh
python -B tools/validation/prepare_dependency_bundle.py \
  --source-cache build/distribution-rights/sources \
  --output build/distribution-rights/dependency-bundle \
  --qt-sdk GNU=/path/to/Qt/6.8.3/mingw_64 \
  --mingw-prefix /path/to/mingw1310_64 \
  --eigen-prefix /path/to/Eigen/3.4.0 \
  --boost-include /path/to/Boost/include
python -B tools/validation/prepare_dependency_bundle.py \
  --verify-bundle build/distribution-rights/dependency-bundle
```

For a reproducible comparison with the vendor source SBOM, also run:

```sh
python -B tools/validation/prepare_dependency_bundle.py \
  --source-sbom /path/to/qtbase-6.8.3.source.spdx \
  --source-archive build/distribution-rights/sources/qtbase-everywhere-src-6.8.3.tar.xz \
  --report build/distribution-rights/qt-source-comparison.json
```

The report preserves every raw, line-ending, missing-file and content-mismatch
result. Non-bookkeeping differences fail the command; known archive bookkeeping
differences remain explicitly qualified and do not become a full-match claim.

Add `--inventory /path/to/distribution-inventory.json` to bind an actual retained
payload to its original staged files and selected SDK bytes. An optional second
`--qt-sdk MSVC=/path/to/Qt/6.8.3/msvc2022_64` supports the separate development
audit. These reports explicitly set `release_clearance` false and do not serve
as a completed runtime review.

The bundle carries full source archives, original legal/attribution files,
license files referenced by Qt attribution metadata, SDK SBOM/configuration and
MinGW build information. Eigen/Boost headers are copied as actual SDK source,
with original notices and per-file hashes, rather than asserting they match an
unexamined upstream version. Source text remains available for modification.

Set `QUALPF_DEPENDENCY_BUNDLE_DIR` to the prepared directory during CMake
configuration. The complete bundle installs under
`share/qualpf/DEPENDENCIES`. Configuration, installation and staged auditing
verify its file/source hashes; missing, modified or unlisted materials fail.
The empty default keeps ordinary builds independent of a local source cache.

The reviewed local MinGW stage binds 17 Qt DLLs/plugins and winpthreads to their
actual SDK bytes, notices and supplied source artifacts. Eigen/Boost reviews bind
the actual header archives. This leaves three GCC DLL source/build bindings and
the IFW container review incomplete. Scientific table qualifications remain
recorded under the owner's upstream policy. These are audit findings, not a
declaration of formal release readiness.

Source export excludes all `tests/real_example_data` and the three historical
installed examples above. Full normal regression tests still require the original
licensed development fixtures and independent archives. Their absence is a test
prerequisite failure, never permission to skip or regenerate an assertion. The
ordinary application build does not require these optional examples.

## Actual Linux runtime and source bindings

The Ubuntu 22.04 / Qt 6.8.3 AppImage payload contains **143 unique regular ELF
files**: the product and 142 runtime libraries/plugins. The actual libraries map
to 80 installed Ubuntu binary packages, official Qt 6.8.3 modules and ICU 73.2.
All **63 exact Ubuntu source-package versions**, including Eigen/Boost, were
downloaded through signed APT indexes with their original archives, Debian patches
and `.dsc` files. The 80 original package copyright notices and full referenced
common license texts are supplied, including notices not copied by linuxdeploy.
Qt's source SBOM comparison has 21,617 raw matches and no non-bookkeeping
differences; the `.tag` and absent Git attribute/ignore files remain qualified.

Deployment changes from loader-path editing and stripping were compared against
the original ELF files, preserving the raw differences. No unexplained change to
allocated code/data remains; dynamic symbol comparisons resolve section names
before comparing, so stripped section-index renumbering is not confused with
changed symbols.
No unresolved allocated-content differences or private build RPATHs remain.
Actual Eigen/Boost SDK headers and their original notices are supplied separately
for the Ubuntu and Debian kits.

The AppImage type-2 runtime is pinned to
[`fedfad51acc2860b2916a1eef32ba4a500ff31ef`](https://github.com/AppImage/type2-runtime/tree/fedfad51acc2860b2916a1eef32ba4a500ff31ef),
SHA-256 `617aa77314d57c8b8e4fae9c3f0b35af7b30d5529cb69e98f64f92510ed98a46`.
Its [public build log](https://github.com/AppImage/type2-runtime/actions/runs/39320578745)
identifies the static build and dependency versions. Original runtime, FUSE,
squashfuse, musl, zstd, zlib and mimalloc sources, full notices and patches are
retained. Exact Alpine package metadata binds the musl 1.2.5-r11, zstd 1.5.6-r2,
zlib 1.3.2-r1 and mimalloc2 2.1.7-r0 recipe commits. All 21 collected recipe/helper
Git blobs and 21 named source/patch SHA-512 checks match. File-specific terms are
retained; no blanket license for the entire Alpine repository is inferred.
The delivered runtime prefix may change only its 16-byte `.digest_md5` section;
the actual image inspection checks this and the SquashFS offset. This is a vendor
source/build binding, not a controlled bit-for-bit rebuild claim.

The Debian 13 package uses GCC 14.2 and system Qt **6.8.2+dfsg-9+deb13u2**,
Eigen **3.4.0-5** and Boost **1.83.0-4.2**. Their exact signed-APT source packages,
patches, SDK headers and original notices are separately supplied. This DEB does
not ship the system Qt DSOs. Its actual generated dependencies require the native
ABI, including Qt 6.8.2, glibc 2.38 and libstdc++ 14; it is not an Ubuntu 22.04 DEB.

The local combined offline bundle is
`build/runtime-acceptance/linux-dependency-bundle-01`. Actual staged reviews bind
hashes of delivered files to source and notice paths in that bundle, rather than
inferring clearance from a build-dependency list. The bundle also retains earlier
collected Windows sources with their original qualifications; unused archives do
not clear the Windows GCC/IFW gaps. Its collection-level `release_clearance` remains
false. Package-specific inventories and actual container inspections are separate.
The five scientific-table scopes still record `separate_data_grants_verified:false`.

The 24 October formal Linux candidate freezes these materials at clean source
`983b485bc00ee5d6c10fd5eae681ee644bca4ae0`, with audit mode OFF. Its matching
application source and independent 890-file dependency source archive are under
`releases/1.0.0/linux-candidate-20261024`. All 1,180 actual AppImage files and
native extract-and-run/database/settings/offline-text checks passed. The native
Debian build passed 127/127 normal tests, all four generators and zero-Fortran
checks. The first DEB was truncated by host disk exhaustion and rejected.
After storage recovery, all 107 compiled binaries, source/build fingerprints,
original test inputs and installed system package checksums matched the prior
evidence. The full regression is reused with unchanged dependencies. A fresh
container was generated in RAM and passed actual 920-file extraction/install
verification, native startup/database/settings/offline-text checks and removal.
Both accepted packages and their exact sources are now in the candidate directory.
Original failures and source/data qualifications remain. The later expanded Windows source catalogue does
not change or retroactively approve the frozen Linux inputs; see the
[current package acceptance](releasing.md#current-native-package-acceptance).

## Owner decisions and information still needed

The supplied public DEB maintainer email is **uncercalculate@163.com**. The CMake
cache default intentionally remains empty. Pass the address explicitly when
building a DEB; no placeholder or upstream author's email is used.

Windows formal binaries use **MinGW/GNU**. **MSVC remains a development/test
configuration**; formal IFW preflight rejects it, and formal staging rejects
VC++ runtime payloads. Historical MSVC audit artifacts remain evidence, not
release candidates. This decision does not waive the separately embedded
installer/graphics runtime review.

No private grants were supplied, and no authors were contacted. The owner chose
the upstream scientific-table distribution approach rather than making separate
data-grant evidence a prerequisite for those five scopes. Earlier findings,
original references and NIST's compilation-copyright statement remain recorded;
the policy is not a representation that separate grants were obtained. The
remaining package-specific runtime/source and platform acceptance work still
applies. Inherited action artwork and user manual originals have been replaced
or removed; no new grant is asserted for those originals.
The excluded examples and reserved CIF require permission if they are to be
redistributed later; they are no longer current payload prerequisites.
The decision authorizes no numerical changes, replacement scientific data or
weaker assertions. The earlier audits in the distribution guide predate this
policy and the completed artwork/manual replacements. Retain their recorded
provenance and review results; the current acceptance section identifies the
subsequently regenerated audit pairs. A formal candidate still requires its own
matching clean revision and completed package review.

The owner confirmed that the original MinGW build log/resolved source snapshot
and IFW static Qt/dependency/CRT build records are unavailable. No additional copy
of those owner-held records is available. The two remaining formal Windows
evidence groups are the resolved recipe/source binding for the three GCC DLLs
and the IFW container's
complete static sources, patches, configure/link inputs and CRT redistribution
basis. Public archive identity and embedded version strings alone do not establish
that all required source and redistribution terms are covered. Complete source,
patches and a usable build recipe established by independently checked public
materials can also resolve a gap; a private original vendor log is not the only
acceptable evidence. A controlled rebuild is an alternative when the supplied
artifact cannot otherwise be covered, not a default prerequisite. A different
tool build needs its own matching source and affected regression/package
acceptance. No kit or runtime has been silently substituted. The
[current acceptance scope](releasing.md#current-native-package-acceptance)
records the completed platform work and retained failures.

## QualX3 runtime distribution comparison

The public `v1.0.8` tag was checked on 23 October 2026 and resolves to
`6bdb8644b975efc394bf2116cf7405334b82782d`, also the inspected main revision.
Eight cached build/install/document files match that tree's Git blobs. The local
evidence is `build/runtime-acceptance/qualx-reference-20261023/evidence.json` and
`verified.json`. The comparison covers public scripts and release metadata;
upstream installer binaries and private supplier records were not inspected.

| Subject | Observed upstream procedure | QualPF application |
| --- | --- | --- |
| Windows compiler/runtime | The [Windows build example](https://github.com/ccorrado71/qualx3/blob/6bdb8644b975efc394bf2116cf7405334b82782d/script/build_windows.cmd) selects installed oneAPI/MSVC and Qt 6.8.1. [CMake](https://github.com/ccorrado71/qualx3/blob/6bdb8644b975efc394bf2116cf7405334b82782d/CMakeLists.txt#L188) copies the Intel runtime DLLs from the configured SDK directory and uses Qt's generated deployment script. It does not rebuild those SDKs. | Keep vendor-SDK deployment, using the owner's selected MinGW kit and its compiler-resolved DLLs. Do not import Intel/Fortran or switch formal distribution to MSVC. This upstream example supplies no source binding for QualPF's three GCC DLLs. |
| Installer container | [CMake](https://github.com/ccorrado71/qualx3/blob/6bdb8644b975efc394bf2116cf7405334b82782d/CMakeLists.txt#L395) uses the installed `C:/Qt/QtIFW-4.8.1`, selects IFW and displays the root GPL text. The [component script](https://github.com/ccorrado71/qualx3/blob/6bdb8644b975efc394bf2116cf7405334b82782d/deploy/Windows/installscript.qs) runs `vc_redist.x64.exe`. No IFW/static-Qt/CRT rebuild recipe is provided in these files. | Continue using the selected vendor IFW with canonical GPL and independent metadata. Keep the container's own terms/source review separate; do not add the upstream VC++ bootstrap to the MinGW application. |
| Linux dependencies | The [AppImage helper](https://github.com/ccorrado71/qualx3/blob/6bdb8644b975efc394bf2116cf7405334b82782d/script/build_appimage.sh) installs an AppDir and uses linuxdeploy/appimagetool to collect and package existing libraries. Its [Docker recipe](https://github.com/ccorrado71/qualx3/blob/6bdb8644b975efc394bf2116cf7405334b82782d/script/Dockerfile.appimage) obtains SDKs from Ubuntu APT and tools from `continuous` downloads. DEB uses system dependencies. | Retain these packaging mechanisms, plus the already verified file hashes, corresponding-source/notices and pinned AppImage runtime. Preserve the Qt version required by current QPF decoding. |
| Public source/release evidence | The [v1.0.8 release](https://github.com/ccorrado71/qualx3/releases/tag/v1.0.8) has three attached assets: Windows EXE, macOS DMG and Linux AppImage. The public repository/tag supplies application source. No separate vendor-runtime source bundle or original compiler/IFW build logs appear among those attached assets. | Preserve the stronger source bundles and actual-package inventories already prepared. Absence from the inspected asset list does not establish what an upstream binary contains or what private arrangements exist. |

QualPF therefore follows the vendor-SDK/binary-deployment route. Rebuilding GCC,
Qt or IFW is not required merely because an old original build log is unavailable.
[GPLv3 section 1](https://www.gnu.org/licenses/gpl.en.html#section1) defines the
required corresponding source, including necessary build/install scripts and its
exclusions; it does not prescribe a particular vendor-log or attestation format.
Qt's [installation guide](https://doc.qt.io/qtinstallerframework/ifw-installation.html)
also supports using prebuilt IFW. This procedural choice does not establish that
an unidentified static dependency is covered or supply missing redistribution
terms. It changes neither runtime review status nor the five-table policy.
