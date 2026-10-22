# License texts

The application GPLv3 text is the root `LICENSE`. The additional texts here are
retained verbatim, with line endings normalized only where necessary for portable
text comparison. Component copyrights remain in their source files and notices.
Upstream MPL and GCC exception whitespace is preserved; their scoped Git
whitespace attributes prevent rewriting the legal texts to satisfy formatting.

- LGPL-3.0-only.txt: SPDX license-list-data, Git blob
  `513d1c01fe5b5b9724e999d8eb041ff17d444eaa`; supplements the GPLv3 text.
- BSL-1.0.txt: boostorg/boost `LICENSE_1_0.txt`, Git blob
  `36b7cd93cdfbac762f5be4c6ce276df2ea6305c2`.
- GCC-exception-3.1.txt: gcc-mirror/gcc `COPYING.RUNTIME`, Git blob
  `e1b3c69c179dfaff5744ea4bf853e6b0d90c9ef9`.
- MPL-2.0.txt and Eigen-BSD.txt: unmodified Eigen 3.4.0 distribution
  `COPYING.MPL2` and `COPYING.BSD`.
- spglib-BSD-3-Clause.txt: unmodified project source notice
  `src/core/io/cif/site_symmetry_spglib_COPYING` (spglib v2.5.0 supplement).
- mingw-winpthreads-COPYING.txt and mingw-w64-COPYING.txt: original notice files
  from the selected MinGW 13.1.0 / MinGW-w64 v11 SDK. Winpthreads retains both
  the mingw-w64 MIT and Lockless BSD notices.
- QtIFW-GPL3-Exception.txt: original QtIFW 4.8.1 `LICENSE.GPL3-EXCEPT`.
- IFW-libarchive-COPYING.txt: QtIFW 4.8.1's bundled libarchive `COPYING`.
- OpenSSL-3.0-LICENSE.txt: OpenSSL 3.0.14's original `LICENSE.txt`, including
  Apache License 2.0. These installer dependencies are not relicensed as QualPF.

These files are not a blanket license for the installed SDK or runtime. Collect
the actual package's component-specific notices and corresponding sources before
clearing a release. See the root third-party notices and developer release guide.
