# Release inputs

This directory contains the maintained inputs for QualPF's Windows and Linux
packages. The [distribution guide](../docs/developer/releasing.md) owns the build,
source-delivery and acceptance procedure. Generated packages belong in
`releases/`; build trees, downloaded sources and verification logs belong in
ignored local build directories.

| Input | Consumer and responsibility |
| --- | --- |
| `distribution.json` | CMake product identity, About/legal resources and release checks: publisher, license, project/support/manual URLs, component dispositions and exclusions |
| `Windows/controlscript.qs`, `Windows/installscript.qs` | Qt Installer Framework page selection, installation shortcuts and uninstall entry; CPack configures the product tokens |
| `linux/qualpf.desktop.in` | CMake-installed desktop entry, resolving the actual installation prefix at install time |
| `linux/qualpf.appimage.desktop.in` | Relocatable AppImage desktop entry, configured by CMake |
| `linux/icons/*/qualpf.png` | Independent QualPF launcher icons in four maintained sizes |
| `dependency-sources.json`, `linux-dependency-sources.json` | Pinned source/notice inputs and their hashes; these catalogues contain no source archives |
| `rights-review.json` | Scientific-material findings, retained original identities, actual runtime/source evidence and recorded qualifications |

The application remains version **1.0.0**, published by **adnxuic** under
**GPL-3.0-only**. Product values come from `distribution.json` and the top-level
CMake project; do not maintain another product-version or publisher definition
here. The optional desktop-entry `Version` key identifies the
[file-format specification](https://specifications.freedesktop.org/desktop-entry/latest/recognized-keys.html),
so these templates omit it rather than substituting the application version.

Windows formal candidates use the matching MinGW kit and the pinned official
QtIFW. MSVC remains a development/validation configuration. The installer's license
comes directly from [the root GPL text](../LICENSE), and installed notices and
[source-retrieval instructions](../SOURCE_CODE.md) are staged by CMake. DEB
packaging additionally requires an explicitly supplied real
`QUALPF_RELEASE_CONTACT_EMAIL`; its default stays empty. Both binary platforms
can use separate, verified application and dependency source archives while
retaining original legal materials offline.

Application icons are exported from the existing independent raster master; see
the [branding guide](../src/gui/resources/branding/README.md). No old QualX artwork
or restrictive Windows agreement is an active deployment input. Their original
paths remain on the exclusion list to reject accidental delivery. The
`retired_deployment_assets` entries in `rights-review.json` bind each removed
original to its retained Git revision, blob and SHA-256. Recover the original
bytes with `git show <retained_revision>:<path>` using a binary-safe tool; Git
history is not needed for ordinary builds or delivered-source checks.

The scientific-data upstream policy and official-IFW vendor policy retain their
recorded unverified qualifications. Removing obsolete files does not establish
new material grants or verify private vendor builds. Run
`python -B tools/validation/check_distribution.py --self-test` and
`python -B tools/validation/check_product_identity.py --self-test` from the
repository root after changing these inputs, then exercise the affected native
package configuration and staging before preparing new candidates.
