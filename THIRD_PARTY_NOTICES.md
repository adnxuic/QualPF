# Third-party notices

The application license is GPL-3.0-only. This does not relicense the components,
data or assets below. Preserve their notices and the supplied license texts.
The machine-readable [distribution inventory](deploy/distribution.json) records
the evidence, modifications and unresolved obligations. **An inventory entry is
not permission to distribute an unresolved work.**

| Component | Origin and copyright | License and obligations |
| --- | --- | --- |
| QualX3-derived source | IC-CNR / QualX3 contributors; upstream GPLv3 update `f98a51da8435fef0f730b513e76c820fb879865d` | GPLv3; preserve upstream attribution, identify modifications and supply corresponding source. The stale LGPL README and historical installer agreement remain documented. |
| QCustomPlot 2.1.1, 6 November 2022 | Copyright (C) 2011-2022 Emanuel Eichhammer; <https://www.qcustomplot.com/> | GPL-3.0-or-later, as stated in both vendored file headers. The GPLv3 option is used for the combined application. See [LICENSE](LICENSE). Integration changes do not remove its original notices. |
| Qt libraries, plugins and translations | The Qt Company and Qt contributors; exact versions and component copyrights come from the selected SDK's notices/SBOM | Selected LGPLv3/GPLv3 terms and component-specific terms. [LGPLv3](LICENSES/LGPL-3.0-only.txt) supplements GPLv3. Preserve Qt and embedded third-party notices and provide the applicable corresponding sources. Shared linking alone does not discharge these obligations. |
| Eigen headers | Eigen contributors; <https://eigen.tuxfamily.org/> | Primarily [MPL-2.0](LICENSES/MPL-2.0.txt), with file-specific licenses including [BSD notices](LICENSES/Eigen-BSD.txt). Audit the headers actually used; do not infer that every Eigen file has one license. No Eigen upgrade or compile-policy change is made here. |
| Boost headers | Boost contributors; <https://www.boost.org/> | [Boost Software License 1.0](LICENSES/BSL-1.0.txt); preserve the original file notices. Record the actual header version used by each build. |
| spglib v2.5.0 naming supplement | Copyright (c) 2024, Spglib team | [BSD-3-Clause notice](LICENSES/spglib-BSD-3-Clause.txt), copied from the existing source notice. Applies to the supplement, not to all crystallographic tables. |
| GCC/libstdc++/libgcc/libgomp | Free Software Foundation and respective contributors; exact MinGW/GCC kit recorded per package | Component-specific GPL terms and the [GCC Runtime Library Exception 3.1](LICENSES/GCC-exception-3.1.txt) where applicable. The exception must not be assumed for unrelated DLLs. Review source availability and other runtime notices. |
| MinGW-w64 CRT | MinGW-w64 contributors | [Upstream CRT notices](LICENSES/mingw-w64-COPYING.txt), including their file-specific exceptions. Preserve the actual SDK build information and copyright materials. |
| winpthreads | Copyright (c) 2011 mingw-w64 project; derived parts (C) 2010 Lockless Inc. | Both the MIT and BSD notices are retained in [upstream COPYING](LICENSES/mingw-winpthreads-COPYING.txt). They apply separately from the GCC Runtime Library Exception. |
| Microsoft Visual C++ Redistributable | Microsoft Corporation | Proprietary redistribution terms for the selected toolset; not relicensed under the application's GPL. Verify the actual redistributable and permitted delivery before shipping an MSVC installer. |
| Installer/AppImage runtime and bundled dependencies | Qt Installer Framework, AppImage runtime and the actual staged transitive libraries | IFW 4.8.1 uses [GPLv3 with the Qt exception](LICENSES/QtIFW-GPL3-Exception.txt); its actual installer embeds Qt 6.6.0 and OpenSSL 3.0.14, separately from application Qt. Preserve [libarchive notices](LICENSES/IFW-libarchive-COPYING.txt) and the [OpenSSL license](LICENSES/OpenSSL-3.0-LICENSE.txt), plus the exact linked Qt/component sources and notices. AppImage and graphics support libraries retain their own terms. Versioned upstream sources alone do not clear an unbound static build. |
| Scientific tables and examples | Retained QualX3 inputs and cited scientific sources, including NIST and International Tables references | Provenance and scientific citations do not by themselves establish a redistribution grant. Rights review remains open for the retained tables, their preferred modification inputs and example data. Do not alter accepted values to avoid this review. |
| Original QualX3 artwork and documentation | Original authors and any external asset authors | Removed QualX/CNR/IC brand images are excluded from current runtime resources. Other inherited action images and documentation assets still require a rights inventory before release. |
| Independent QualPF artwork | QualPF contributors; application raster master/prompt and action SVG masters are retained in the resource guides | Supplied with the QualPF additions under GPL-3.0-only to the extent applicable rights exist. This is not a claim to third-party institutional marks. |

Material-specific findings are recorded in `deploy/rights-review.json` and the
developer rights review. NIST FFAST is SRD 66 with a reserved compilation
copyright; the general grant for non-SRD NIST data must not be substituted for
its actual terms. Retained scientific data and inherited image grants remain
unresolved. The owner has selected MinGW for formal Windows binaries; MSVC and
VC++ runtime delivery are excluded from that release scope.

## Package-specific materials

This source-level list is supplemented by each package's `distribution-build.json`
and `distribution-inventory.json`: exact revision, toolchain, file sizes and
SHA-256 identities. A release must also carry the selected SDK/runtime notices,
an exact source retrieval location (and local corresponding-source artifacts for
redistributed copyleft dependencies as required), and a completed rights review.
Missing runtime provenance blocks release; it is not silently classified as a
system-library exception.

The standard texts in LICENSES are unmodified legal documents. Their origins
and hashes are recorded in the distribution inventory. Generic license texts do
not replace a component's specific copyright and attribution notices.
