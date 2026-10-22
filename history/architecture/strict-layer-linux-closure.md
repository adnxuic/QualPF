# Strict layer convergence: Linux validation and closure

Outcome: **STRICT_LAYER_CONVERGENCE_3_PLATFORM_PASS**.
The implementation remains `895e413878452a11bcbc69b456f8c1567de1034e`;
entry HEAD was `24cdd26c8ef8257279d97f67c60bc9b5fd2a7a52` (Windows validation records).
No production, test, build configuration, resource or validation-tool file changed.
All 1,817 implementation inputs match the final Windows binding.

The previously accepted SDK at `/var/tmp/qualpf-layout-sdk-20261008` was reused
through a WSL2 private mount namespace and chroot. Its compiler and Qt metadata
hashes match the independent Milestone A toolchain record. A separate copy of all
2,062 tracked files was mounted read-only, with all 1,817 implementation inputs
byte-checked; the build started without
cache or objects. No dependency downloads or host package changes were required.

| Environment | Actual version |
| --- | --- |
| OS / architecture | Ubuntu 26.04.1 LTS / x86_64 |
| Compiler / Qt | GCC 15.2.0 (`15.2.0-16ubuntu1`) / Qt 6.10.2 |
| CMake / Ninja | 4.2.3 / 1.13.2 |
| Eigen / Boost | 3.4.0-5 / 1.90.0.1ubuntu3 |
| SQLite / QSQLITE package | 3.46.1-9ubuntu0.3 / 6.10.2+dfsg-7 |
| Python / GoogleTest / patchelf | 3.14.4 / 1.17.0-1build1 / 0.18.0-1.4build1 |

| Required check | Result |
| --- | --- |
| Empty-directory configure; explicit `qualpf`; complete product build | PASS, CXX only, 322 actual compile units |
| Mandatory strict gate before product target | PASS, 95 configured targets, 136 indexed Core operations; negative controls pass |
| Full normal CTest suite | **86/86 PASS**, zero failed/skipped, 921.25 seconds; exact coverage identities |
| Complete Qt-free Application client | PASS; 79 product Core/Application compile units have no Qt contamination |
| Independent public-header compilation | PASS, 40 Application + 32 Core headers, using the actual Qt-free consumer flags |
| Standalone snapshot contract/integration consumers | **2/2 PASS**; production has no compat/test-support dependency |
| Pattern/CIF/powder-CIF, SQL, preprocessing/search/ranking/residual | PASS in unchanged normal tests |
| GUI background search/cancel/stale/close, QXP/provenance, IO/settings | PASS in unchanged normal tests |
| Process discovery, network update boundary, database create/delete | PASS in normal tests; installed CIF/SQLite creation also passes |
| Staged installed CLI/GUI/QSQLITE/resources/RPATH/permissions | PASS; existing CLI regression plus six actual installed process cases, GUI through xcb/Xvfb |
| Supported DEB generation and private-root installation | PASS; dependency checks enabled; installed CLI regression and six process cases, including offscreen/xcb GUI |
| Packaging preserves tested implementation | PASS; all compile/test commands and properties unchanged; staged, DEB build and DEB installed executable `.text` identical |
| Protected numerical references | **169/169 PASS**, original byte lengths and SHA-256; no new golden or tolerance change |
| Current Zero-Fortran | PASS, 322 C++ units / 123 current binaries plus loaded library/provider inspection; no compiler requirement, origin symbols or runtime imports |
| Windows GCC / MSVC evidence | **86/86 PASS each, reused**; unchanged implementation inputs, 178 original evidence files and four package hashes rechecked |

The staged runtime trace covers 107 libraries across product and GUI harness
processes; the DEB trace covers 105. Staged QtDBus resolves to the qualified system
provider, which is byte-identical to its deployed Qt 6.10.2 copy. This verifies
installation on the supported Ubuntu SDK; execution without system Qt is not
claimed. DEB intentionally uses the matching SDK's system libraries. Its private
package database preserves the SDK and checks declared dependencies; warnings
about absent unrelated package file lists are retained. Installed binaries,
resource permissions, relative RPATH and actual QSQLITE providers were inspected.

Original failures remain available. An outer script edit during execution caused
a parser error after successful builds; fixed-script completion confirmed no
additional compilation/link and unchanged inputs. Two new smoke-harness attempts
incorrectly required a non-deployed offscreen plugin and exclusively bundled Qt
providers. All installed GUI cases were subsequently executed with supported
display/plugin settings; provider hashes and the original FAIL records remain.
No existing test assertion, input, timeout, algorithm or format was changed.

The [closure receipt](strict-layer-linux-validation.json) binds the actual logs,
JUnit, source inputs, compile/link observations, header checks and installed
runtime/package results. Exact raw bytes are retained locally under
`build/strict-layer-linux-validation/raw` and as compressed files under
`docs/validation/strict-layer-linux`. The unchanged
[Windows checkpoint receipt](strict-layer-validation.json) retains its original
Linux NOT_RUN observation, mixed-input attempts and other failures; this closure
supersedes that validation gap. The validated DEB is retained at
`build/strict-layer-linux-validation/artifacts/qualpf-1.0.5-resolute_amd64.deb`.

Resolve the closure record commit with
`git log -1 -- docs/architecture/strict-layer-linux-validation.json`.
Milestone F/A remain accepted in their original snapshot scopes. Unrelated
`docs/maintenance/` files are unchanged. **STOP. PFSM is NOT_STARTED.**
