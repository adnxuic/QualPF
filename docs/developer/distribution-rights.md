# Distribution rights and source review

This guide records material-specific findings from the public-source review on
22 October 2026. The corresponding byte/origin inventory is
[`deploy/rights-review.json`](../../deploy/rights-review.json). It supplements
the [distribution procedure](releasing.md), not the scientific acceptance
contracts. No scientific input, generated table or reference was changed.

## Scientific inputs

| Material | Public evidence and remaining gap |
| --- | --- |
| `AtomProperties.xen` | Its own header cites International Tables C 6.1.1.4, 4.2.6.8 and 4.2.4.3, Bondi/Rowland-Taylor radii and published atomic weights. The QualX GPL update establishes repository provenance, but no specific grant for this combined compilation or its external tables was located. The compilation and generated mass/scattering tables remain blocked. |
| NIST FFAST extraction | The retained `nist_table.f90.txt` records the original NIST download/generation procedure for Z=1–92. The [official FFAST page](https://www.nist.gov/pml/x-ray-form-factor-attenuation-and-scattering-tables) identifies **Standard Reference Database 66** and reserves compilation copyright. NIST's [licensing statement](https://www.nist.gov/open/copyright-fair-use-and-licensing-statements-srd-data-software-and-technical-series-publications) distinguishes SRD from ordinary government data. The general non-SRD grant cannot clear this extraction. [FFAST.jl's Unlicense](https://github.com/usnistgov/FFAST.jl/blob/master/LICENSE) grants software rights without explicitly resolving this older SRD compilation. Applicable redistribution/derivative permission remains required. |
| Electron-scattering parameters | The active Lobato block cites [Lobato and Van Dyck (2014)](https://doi.org/10.1107/S205327331401643X). The author's [MULTEM source](https://github.com/Ivanlh20/MULTEM/blob/master/src/atomic_data.hpp) carries GPLv3-or-later, but its current parameter table differs from this retained version. Its license does not prove identity of these coefficients. The preferred Fortran input also retains unused Kirkland/Doyle-Turner tables. No replacement, rounding change or new product-derived reference was made. |
| `syminfo.lib` | [CCP4's symmetry documentation](https://www.ccp4.ac.uk/html/symlib.html) identifies the format. The [official libccp4 6.3 source directory](https://ftp.ccp4.ac.uk/ccp4/6.3.0/unpacked/lib/libccp4/) supplies GPLv3/LGPLv3 texts. CCP4 6.3 and 6.4 publish the same file (SHA-256 `d8ea437d3e60ab4ef2007d2f5cfcd1cc4f16c2761be2dce041589d396dbfc42a`), which differs from QualPF's retained file in aliases and settings as well as whitespace. This establishes a base-source lead, not clearance of every modified entry or the separate exact catalogue. |
| Exact symmetry catalogue | The frozen archive has 14 members, including four original source files. The generator uses `exact-table.json` and `symm_table.f90`; their independently recorded hashes remain unchanged. The underlying table compilation's origin/grant still needs confirmation. The separately licensed spglib naming supplement does not license this other catalogue. |

Unresolved grants apply to compiled/generated data as well as preferred source
inputs. An unused input table does not become cleared merely because its values
are absent from the executable. Keep the independent generator inputs and
original failures available during review.

## Inherited artwork and documents

All **29** images still registered under `:/images/` are listed individually with
SHA-256 and original QualX Git blob identities. Their PNG/XPM metadata did not
supply a usable author/license statement. In particular, a file name such as
`pan_121784.png` is a search lead, not attribution. The upstream GPL change alone
is not treated as proof that an external icon author's rights were granted.

The inactive original documentation favicon and QualX SVG are now excluded from
source delivery as well as from current documentation branding. Their originals
are retained. Current QualPF branding and original action SVG masters remain
separate from these inherited graphics. Remaining inherited text/example grants
also require confirmation; the inventory does not relabel them as QualPF-owned.

## Actual Windows runtime findings

The inspected artifacts identify payload revision
`a8bc3dbec1f80dfb916474e7f856a46c0c127d1d`; these findings do not silently approve
future payloads with different bytes.

- MinGW has 17 deployed Qt DLL/plugin files, and MSVC has 20, including Qt SVG.
  Each is byte-identical to its selected Qt 6.8.3 SDK file. The official QtBase
  archive's `.tag` equals SDK SBOM commit
  `c07c2d5a527a644d36e7853d55132ae38921682f`. The SDK SBOM also records original
  configuration arguments. QtBase/Qt SVG sources and their embedded notices
  still need to accompany the actual package review; generic LGPL text alone
  is insufficient.
- `libwinpthread-1.dll` has its own upstream MIT and Lockless BSD notices. Both
  are now retained in `LICENSES/mingw-winpthreads-COPYING.txt`. It is no longer
  grouped under the GCC Runtime Library Exception. MinGW CRT notices are also
  retained separately.
- GCC 13.1.0 upstream source has been acquired and checked against GNU's official
  SHA-512 list. The SDK `build-info.txt` names ten GCC patches and exact configure
  arguments; the upstream archive alone is not the complete modified source.
- QtIFW 4.8.1 `installerbase.exe` contains the build marker **Qt 6.6.0**, and
  its embedded OpenSSL marker is **3.0.14**. Its corresponding sources and
  notices must be reviewed independently of application Qt 6.8.3. Versioned IFW,
  Qt 6.6.0 and OpenSSL sources have been acquired; exact static build settings,
  patches and all linked component provenance still require binding. The IFW
  GPL exception and actual libarchive/OpenSSL notices are retained.
- The packages also include graphics support code: `D3Dcompiler_47.dll`, and in
  MSVC, `dxcompiler.dll`, `dxil.dll` and `opengl32sw.dll`. Qt deployment is not a
  license grant for these Microsoft/Mesa components. Their exact source/terms
  remain separate package blockers. No functionality was removed to hide them.

## Owner decisions and information still needed

The supplied public DEB maintainer email is **uncercalculate@163.com**. The CMake
cache default intentionally remains empty. Pass the address explicitly when
building a DEB; no placeholder or upstream author's email is used.

Windows formal binaries use **MinGW/GNU**. **MSVC remains a development/test
configuration**; formal IFW preflight rejects it, and formal staging rejects
VC++ runtime payloads. Historical MSVC audit artifacts remain evidence, not
release candidates. This decision does not waive the separately embedded
installer/graphics runtime review.

No private grants were supplied, and no authors were contacted. The remaining
external information is an applicable public license or rights-holder grant for
the exact scientific compilations and inherited graphics/text/examples above.
Any grant must cover modification and redistribution with the intended GPL
application, including corresponding-source delivery. If such permission cannot
be found, a separately authorized replacement/removal design is needed; this
audit does not authorize changing scientific data or weakening comparisons.
