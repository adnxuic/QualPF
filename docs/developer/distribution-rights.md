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

The inspected artifacts identify payload revision
`a8bc3dbec1f80dfb916474e7f856a46c0c127d1d`; these findings do not silently approve
future payloads with different bytes.

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

[`deploy/dependency-sources.json`](../../deploy/dependency-sources.json) pins ten
acquired upstream archives, their exact commits where available, SHA-256 and
official checksum URLs where published. Their availability does not erase the
vendor-build qualifications above. The preparer uses an existing local cache;
ordinary configuration/builds never download sources.

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
weaker assertions. Existing audit artifacts predate this policy and the completed
artwork/manual replacements; regenerate them before final distribution instead
of relabeling their recorded provenance or review results.

For the runtime gaps, useful owner-supplied material is the original MinGW build
log/resolved source snapshot and the IFW vendor's static Qt/dependency configure,
patch and link records, including CRT origin and redistribution terms. Public
release downloads and version strings alone do not supply these records. If
they are unavailable, record that fact; a controlled build from supplied sources
is an alternative, requiring its own artifact and regression verification.

QualX3's public Windows script at revision
`6bdb8644b975efc394bf2116cf7405334b82782d` uses oneAPI/MSVC and Qt 6.8.1. It
does not bind QualPF's MinGW DLLs or establish this IFW container's CRT source.
Its AppImage script uses the same linuxdeploy/appimagetool packaging mechanism;
QualPF retains that approach and audits the actual assembled libraries and
container separately. Do not infer a grant or a matching tool build from the
upstream project's use of a packaging tool.
