# Clean main integration: 2026-10-10

Status: validation complete; prepared for the approved fast-forward of local main.
No remote publication is authorized for the agent; the user performs the push.

## Integration and preservation

The approved original-main baseline is
`66aea023898c9fec964f8b7859eb6c607fab958f`, fetched from `origin/main`.
The fixed final refactor candidate is
`0881f6b71a9ebeeae7996536eb0c15bc938abaad`.
Squash checkpoint `d0b07f7a95ab9e61957ae7204fb5b37d3b1ffcfe` has the
original-main baseline as its sole parent. The 202 intermediate refactor commits,
historical oversized blobs and LFS pointers remain reachable through the original
local branches/protected tags, but are not ancestors of this integration.
Original QualX history, license text, attribution, scientific references,
acceptance/provenance records and original failures remain preserved.

The publication includes the following current tracked content:

- `src/` and `apps/qualpf/`: the C++20 product and its composition root.
- `cmake/`, `scripts/`, `deploy/`, `third_party/`, root CMake files and
  `.github/workflows/`: build, deployment, dependency notices and CI inputs.
- `tests/`, `tools/` and `share/`: current tests, independent frozen references,
  validation tools and installed product resources.
- `docs/`, `.agent/`, `AGENTS.md`, README, LICENSE, `.gitignore` and
  `.gitattributes`: current documentation, retained audit/provenance records,
  agent governance and repository rules.

Build directories, release installers, `CMakeUserPresets.json`, local `DB/`,
`.evidence/`, materialized historical evidence and untracked maintenance drafts
are excluded. Only the tracked CoO fixture is included from the otherwise local
`tests/real_example_data/` collection. No upstream updates beyond the original
private-repository main are imported by this integration.
The user-owned `docs/maintenance/settings-identity/report.md` remains untracked
and byte-identical. Original branches, protection tags, Git/LFS objects and local
release artifacts are retained. Historical commit identifiers in acceptance
records remain original identities, not newly remote-accessible history claims;
recovery follows the existing local/external workflow in
[migration history](../migration-history.md).

## Bounded cleanup

Root `.gitignore` now scopes build/package/database/IDE outputs to local locations
and leaves scientific formats visible. The existing CoO fixture is explicitly
visible inside the otherwise local real-example collection. All 35 positive and
negative ignore probes pass and no tracked file is ignored.
Four active composition/GUI/export files received whitespace-only edits.
Exactly 73 frozen fixture paths received whitespace-check attributes; their
content and line-ending bytes were not changed. Product/composition/export
changes relative to the fixed refactor candidate are empty when whitespace is
ignored. No numerical implementation, tolerance, assertion, public API or
execution strategy changed; PFSM remains unstarted.

## Fresh validation

| Check | Result |
| --- | --- |
| Agent, baseline, source boundaries, identity, independent resources | PASS |
| Four numerical-table generators `--check` | PASS |
| Complete staged diff whitespace check | PASS |
| Clean Windows GCC/MSVC and Linux GCC builds | PASS |
| Qt-free complete Application clients, all three compilers | PASS |
| Independent public headers, all three compilers | 72/72 PASS each |
| Linux standalone contract/search consumers | 2/2 PASS |
| Full normal Linux GCC suite | 86/86 PASS, zero failures/skips |
| Full normal Windows GCC suite, isolated complete rerun | 86/86 PASS, zero failures/skips |
| Full normal Windows MSVC suite | 86/86 PASS, zero failures/skips |
| Windows installed CLI and SDK-free GUI, both compilers | PASS |
| Linux system-dependency staged and installed DEB runtime | PASS |
| Zero-Fortran source, build, symbol and binary scans | PASS on all three platforms |
| Strict documentation build | PASS |
| New reachable-object audit | PASS; no new LFS pointers or excluded artifacts |

Complete-suite elapsed times were 1,302 seconds (Windows GCC isolated rerun),
1,304 seconds (Windows MSVC) and 746 seconds (Linux GCC). All 86 test identities
match `tests/coverage.json` on every platform. Zero-Fortran scans inspected
323 compile units / 95 binaries on Windows GCC, 320 / 99 on Windows MSVC and
322 / 77 on Linux GCC. Public-header, Qt-free consumer and installed-runtime
checks are separate from those complete normal suites. No hosted-CI execution
or macOS validation is claimed.

Each validation build started empty from the clean committed squash checkpoint.
The 1,817 implementation/test/configuration input hashes bind the Windows and
Linux runs. Initial runs used four CTest workers per suite with Linux in its independent
namespace. After the GNU timeout below, full Windows suites run serially with one
CTest worker and no competing numerical validation. Test definitions, timeouts,
assertions and product execution strategy are unchanged. Builds are complete
before full numerical regression starts.

One Linux runtime invocation initially selected the portable mode against a
`BUILD_DEB_PACKAGE=ON` staged installation. Original process cases passed, then
the portable-only bundled-Qt assertion failed. The original log, runtime report
and wrapper are preserved. The unchanged validator was rerun in its existing
system-provider mode with a new attempt label; all process, SQLite, resource,
RPATH and recursive runtime checks passed. Neither product nor assertion changed.

The first GNU suite timed out inside the original card-properties subprocess
limit of 180 seconds while numerical suites and supplemental header probes were
concurrent. No numerical mismatch was reported. The same binary and test passed
in an isolated serial rerun in 116.39 seconds, under the unchanged limit. This
supports an execution-environment qualification; the exact scheduling cause was
not independently proven. The entire first run remains 85 PASS / 1 timeout FAIL;
the complete isolated GNU rerun passed 86/86 with the same assertions/timeouts.

Raw scripts, manifests, original attempts, build logs and regression outputs are
local ignored files under `build/main-integration-20261010`. Current builds and
normal tests do not depend on that directory or historical archives. This record
does not relabel earlier acceptance evidence as fresh validation.

Local evidence indexes are `final-validation-matrix.json`,
`final-object-preservation-audit.json`, `final-static-checks.json` and the original
`gnu-attempt-01-summary.json` under that ignored directory. The final matrix binds
the original test XML/logs, compiler/source fingerprints, public-header results,
runtime checks and Zero-Fortran results by SHA-256. The documentation/governance
closure is committed separately; its changes do not alter the 1,817 tested
implementation/test/configuration input hashes in the validation checkout.

## User publication

Local main is advanced only by a verified fast-forward after this closure commit
and final remote-base/preservation checks. The integration consists of the one
clean squash checkpoint and one verification/documentation closure commit above
the original QualX main. Original refactor branches and protection tags are not
advanced or deleted. For publication, push only main:

```powershell
git -c push.followTags=false push origin refs/heads/main:refs/heads/main
```

This activates the existing documentation deployment workflow. No other branch,
protected tag, release installer or GitHub Release is included. Recheck the remote
main identity immediately before publication; remote advancement requires a new
integration check, never force-push. The existing README/license-label mismatch
was not resolved or relicensed by this maintenance.
