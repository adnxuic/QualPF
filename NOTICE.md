# QualPF copyright, origin and distribution notice

QualPF is an independently maintained, modified derivative of QualX3. Its
publisher is **adnxuic**. QualPF is not an official IC-CNR or QualX distribution;
no endorsement by the original authors or their institutions is claimed.

QualPF additions and the combined application are offered under the **GNU
General Public License, version 3 only (GPL-3.0-only)**. See [LICENSE](LICENSE).
Individual third-party works retain their own copyright notices, licenses and
applicable exceptions; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
This notice does not grant rights in third-party trademarks or independently
licensed data and artwork.

## Origin and modifications

QualX3 was developed at the Institute of Crystallography, CNR (IC-CNR), Bari,
Italy. Preserve the original notices and credit to its contributors, including
A. Altomare, N. Corriero, C. Cuocci, A. Falcicchio, A. Moliterni and R. Rizzi.
Research references are retained in the
user manual; an academic citation does not replace a copyright/license notice.

- Upstream: <https://github.com/ccorrado71/qualx3>.
- On 30 June 2026, the upstream author's commit
  [f98a51da8435fef0f730b513e76c820fb879865d](https://github.com/ccorrado71/qualx3/commit/f98a51da8435fef0f730b513e76c820fb879865d)
  explicitly replaced the license with GPLv3. The upstream README still says
  LGPLv3. This conflict and the older installer agreement are recorded in the
  distribution inventory, rather than silently treated as grants for every asset.
- Copyright (C) 2026 QualPF contributors for their additions and modifications.
  QualPF replaces the computational implementation with C++20, introduces its
  Application-owned workspace, QPF project format, qualified search workflows,
  independent product identity and distribution configuration. Relevant dates
  and changes are recorded in Git and CHANGELOG.md.
- Independent distribution notices and branding were revised on 22 October 2026.
- All 29 inherited runtime action images were replaced by independently drawn
  QualPF SVG artwork on 23 October 2026. Removed files retain their original
  provenance and grant findings in the rights inventory and Git history.
- The 17 Markdown pages of the user manual were independently authored from current product
  contracts and implementation on 23 October 2026, with an original workflow
  illustration. Inherited document branding was removed. The rights inventory
  records the old source identities; no ownership of the old text is asserted.

Original copyrights are not transferred to the QualPF publisher. Existing
third-party file notices must remain intact. The GPL text's Free Software
Foundation copyright concerns that license document, not authorship of QualPF.

## Inherited scientific tables

On 23 October 2026, the QualPF publisher chose to follow QualX3's distribution
approach for AtomProperties, the NIST FFAST extraction, electron-scattering
parameters, `syminfo.lib` and the exact symmetry catalogue. These tables and their
preferred modification inputs remain supplied on the upstream project-level GPL
basis, with their original scientific references and corresponding source.
QualPF's generated C++ representation preserves the accepted numerical values.

The scoped decision is recorded in `deploy/distribution.json`; earlier public
findings remain in `deploy/rights-review.json`. Separate third-party data grants
have not been independently verified. In particular, NIST identifies FFAST as
Standard Reference Database 66 with reserved compilation copyright; the general
non-SRD NIST grant is not substituted for its terms. This upstream-based
disposition makes no claim that NIST or another data author supplied a new grant.
Excluded examples/CIF fixtures, replaced artwork/manual originals and databases
retain their separate dispositions and terms.

## Your rights and warranty

You may copy, modify and redistribute the covered application under GPLv3,
including its corresponding-source requirements. There is **no warranty**, to the
extent permitted by applicable law; see sections 15 and 16 of LICENSE. No
academic-only or noncommercial-only restriction is added to the GPL grant.

Crystallographic databases, user data and separately licensed components are
subject to their own terms. PDF-2 and COD databases are not bundled with QualPF.
Search results and RIR estimates retain the qualifications in the current manual;
the application is described as qualitative phase identification software, not a
crystal-structure solution/refinement product.

## Official prebuilt Windows installer framework

On 24 October 2026 the publisher adopted QualX3's vendor-distribution approach
for the unchanged official Qt Installer Framework 4.8.1. Its GPLv3/Qt exception,
third-party notices and all collected public source archives accompany the
Windows package. The package inventory identifies the official tool hashes and
this scoped distribution decision separately from verified source findings.

The complete vendor static Qt source/patch/configuration mapping and the
integrated CRT redistribution/source-exclusion basis have not been fully
independently verified. These qualifications remain disclosed; the supplied
versioned sources do not claim to reproduce every factory input. The MinGW
application carries no separate Visual C++ Redistributable payload. Other
components retain their own review and terms.

## Copyright and licensing concerns

Please report copyright or licensing concerns to **uncercalculate@163.com**,
identifying the material and the basis of the concern. The publisher will review
credible reports promptly and remove affected material from official distribution
if infringement is confirmed. This process does not replace applicable licenses
or grant permission for third-party material.

## Support and source

- Project: <https://github.com/adnxuic/QualPF>.
- QualPF support: <https://github.com/adnxuic/QualPF/issues>.
- Current manual: <https://github.com/adnxuic/QualPF/blob/main/docs/user/index.md>.

Each distributed binary must identify its exact committed source revision and
provide the matching corresponding source, notices and build instructions.
For filenames, checksums and the slim Windows/Linux packages' independent dependency
source archive, see [Obtaining QualPF source code](SOURCE_CODE.md). Keep those
source downloads available alongside the binary at no additional charge.
A link to a moving branch alone is not a corresponding-source delivery record.
Local packages marked **audit** are for inspection and are not cleared for
distribution. Outstanding rights and package-specific obligations are recorded
in `deploy/distribution.json` and the installed distribution inventory.
