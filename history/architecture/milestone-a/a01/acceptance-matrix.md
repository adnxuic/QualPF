# Milestone A reacceptance matrix

Implementation: `5a247152c9a57bdab4f0ba348218dec80044d49c`.
Outcome: **CPP_ARCHITECTURE_REFACTOR_COMPLETE**. All rows below are mandatory.
Evidence, original failures and reuse limits: [independent review](independent-review.md)
and [gate/raw index](gate.json). The [original blocked matrix](../acceptance-matrix.md)
is unchanged.

| ID | Criterion | Result | Evidence |
| --- | --- | --- | --- |
| A01 | Core dependency and Qt independence | PASS | Unchanged source/targets; 70 pure Core/Application compile units per configuration; fitness negative controls |
| A02 | Complete Application dependency and Qt independence | PASS | Unchanged complete target, commands/libraries and bound standalone client evidence |
| A03 | Canonical state ownership and derived views | PASS | Workspace/Session owners unchanged; fresh product-session/workspace workflows and zero-publication synchronous checks |
| A04 | Snapshot/revision/commit/preprocessing contracts | PASS | Fresh Session, coordinator and preprocessing workflows; literal stale/exhaustion/private-draft/no-op assertions retained |
| A05 | Database and IO responsibility separation | PASS | Unchanged ports/parsers/codecs/scoring owners; fresh reader, project and CIF workflows |
| A06 | SQLite execution-thread ownership and write exclusion | PASS | Fresh threaded-reader connection/lease/cleanup assertions and unchanged repository implementation |
| A07 | Shared engine/scoring/ranking/residual contracts | PASS | Unchanged engine/numerics/fixtures, fresh search/rounding/classic tests and original bound regression evidence |
| A08 | Background GUI search and zero search event pumps | PASS | Fresh controller/blocked-worker event-loop workflows and source fitness check |
| A09 | Busy/cancellation/stale/owner-thread atomic publication | PASS | Fresh controlled worker/coordinator tests, unchanged commit guards; new synchronous cancel/error recovery |
| A10 | GUI/dialog/actions/closing/drain lifetime | PASS | Fresh actual MainWindow refused/confirmed/queued/child-close and controller tests |
| A11 | Structured error boundaries do not return empty success | PASS | A-01 corrected at the CLI consumer; independent real old/new frozen SQL failure on all platforms; true empty, cancel and exception distinctions retained |
| A12 | 169 reference byte identities and approved numerical policy | PASS | All current bytes/lengths/SHA/provenance match protected authority; no regenerated expectations/tolerances |
| A13 | Current normal numerical/product regression coverage | PASS | 82 identities per configuration: 28 fresh + 54 strictly reused; original 80 retained, zero failure/skip |
| A14 | GUI/CLI/import/export product boundary behavior | PASS | Actual new products execute positive/empty/SQL failure/pattern/missing/no-peak paths; fresh GUI/IO workflows and unchanged regression inputs |
| A15 | Windows GCC 13.1 / Qt 6.8.3 supported build/tests | PASS | Actual corrected main compilation/link/run, 28 fresh tests; clean qualified build and 54 unchanged tests reused; 70 artifacts rehashed |
| A16 | Windows MSVC 19.40 / Qt 6.8.3 supported build/tests | PASS | Actual corrected main compilation/link/run, 28 fresh tests; 54 unchanged tests reused; 70 artifacts and 319 COFF/archive inputs scanned |
| A17 | Ubuntu 26.04 GCC 15.2 / Qt 6.10.2 supported build/tests | PASS | Actual qualified SDK corrected compilation/link/run, 28 fresh tests; 54 unchanged tests reused; 71 artifacts rehashed |
| A18 | Clean repository/checkout independent of migration facilities | PASS | New detached committed checkout configure; current inputs and original clean build dependencies checked; historical tools not required |
| A19 | Actual source package exclusions | PASS | Fresh implementation CPack TGZ: exactly 1,941 tracked files/169 references; actual Git/cache/build/history controls excluded |
| A20 | Historical index and actual local recovery | PASS | Six actual archived samples and three protected Git records fetched/hash-verified; local-only risk retained |
| A21 | Milestone F preservation | PASS | Fresh three-platform source/graph/import scans; fresh MSVC COFF-origin scan; zero active Fortran/ABI/runtime/state |
| A22 | PFSM architectural boundary/no unauthorized start | PASS | Current reusable ownership/ports and accepted handoff; PFSM NOT_STARTED; STOP |

**22 PASS, 0 FAIL, 0 NOT_VERIFIED mandatory criteria.** A11/A14 are resolved by
new actual execution, not by changing their definitions. Full normal coverage
combines explicitly identified FRESH_VALIDATION and REUSED_UNCHANGED_DEPENDENCIES;
it is not described as a new full 82-test run.

| Optional / unclaimed observation | Result |
| --- | --- |
| Hosted GitHub CI dispatch | NOT_VERIFIED |
| Clang or macOS configurations | NOT_VERIFIED |
| Remote/offsite archive recovery | NOT_VERIFIED |
| Newly replayed complete installer/provider matrix | NOT_VERIFIED |
