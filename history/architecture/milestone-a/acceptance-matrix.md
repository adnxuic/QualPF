# Milestone A acceptance matrix

Candidate: `e6883edab2fb26d8f22866c0585b910322027c12`.
Overall: **ACCEPTANCE_BLOCKED**. Mandatory FAILs below refer to one finding, A-01.
All rows are mandatory unless explicitly marked optional in the last table.
Evidence and limits: [independent review](independent-review.md), [gate](gate.json),
[existing raw index](../../development/repository-consolidation/validation-index.json).

| ID | Criterion | Result | Evidence / limit |
| --- | --- | --- | --- |
| A01 | Core independent of Qt, GUI, SQLite implementation and Application | PASS | Actual source/CMake edges, current fitness negative controls and 70 pure compile units per platform |
| A02 | Complete Application Qt-free and independent of MainWindow | PASS | Actual target/headers and standalone complete Application builds; three clients freshly executed |
| A03 | Unique canonical state owner and scoped derived views | PASS | Workspace/ProductSearchSession source, ownership checker, product-session/workspace tests |
| A04 | Snapshot/revision/commit/publication and preprocessing contracts | PASS | Literal tests for stale/failure/exhaustion/private edits; actual Range/Kalpha2/dialog workflows |
| A05 | Database/IO responsibility and dependency separation | PASS | CandidateReader/SQL factory, Core memory parsers, Infrastructure codec/storage, existing SQL/IO/QXP suites |
| A06 | SQLite execution-thread ownership and managed write exclusion | PASS | Fresh actual thread traces, unique names, foreign-thread rejection and queued lease/cleanup tests |
| A07 | Shared synchronous search core and scoring/ranking/residual contracts | PASS | One SearchEngine in CLI/tests/GUI; unchanged numerical fixtures and threshold/tie/scale/duplicate/residual assertions |
| A08 | All GUI search entries background; no search event pump | PASS | Actual MainWindow routes/controller; static search/drain scan and blocked-worker event-loop tests |
| A09 | Busy, cooperative cancellation, stale rejection and owner-thread atomic commit | PASS | Controlled reader/semaphore tests before/during/after queued work, database/source/TaskId guards, exception cleanup |
| A10 | MainWindow/Dialog/QAction/late-callback/close-drain lifetime | PASS | Fresh refused/confirmed/queued/child-close tests; presenter subscription/plot sink/source review |
| A11 | Structured failure/cancellation/stale boundaries never become empty success | **FAIL** | A-01: metadata CLI ignores failed SQL search, prints zero cards and exits 0 |
| A12 | 169 reference byte lengths/SHA and accepted numerical policy preserved | PASS | Actual files and protected authority multiset match; no expected-value/tolerance regeneration |
| A13 | Current numerical and ordinary product regression coverage | PASS | Actual 80 normal identities per platform; 26 existing causal tests freshly rerun, zero failure/skip; optional external workflows not claimed |
| A14 | GUI/CLI/import/export product boundary behavior | **FAIL** | Normal GUI/IO/CLI tests pass; additional real CLI failure workflow A-01 violates required error behavior |
| A15 | Windows x64 GCC 13.1 / Qt 6.8.3 clean build and full normal suite | PASS | Bound earlier fresh clean build, 80/80 reused with unchanged dependencies; 69 artifacts and commands rehashed |
| A16 | Windows x64 MSVC 19.40 / Qt 6.8.3 clean build and full normal suite | PASS | Bound earlier fresh clean build, 80/80 reused; 69 binaries and 317 COFF/archive inputs rehashed |
| A17 | Ubuntu 26.04 x64 GCC 15.2 / Qt 6.10.2 clean build and full normal suite | PASS | Actual SDK/toolchain/source/artifacts rechecked; bound clean build and 80/80 reused; 70 artifacts |
| A18 | Clean repository/checkout/current tests independent of migration facilities | PASS | Current CMake/CTest/CI consumers, original clean builds with verified input reuse; final detached checkout configure |
| A19 | Final source package exclusion actually effective | PASS | Fresh final-candidate CPack TGZ: exact 1,935 files/169 references; Git/cache/build controls absent |
| A20 | Historical index, actual local recovery and unchanged authority | PASS | Six real archived objects and three protected Git records freshly restored/hash-matched; local-only availability risk recorded |
| A21 | Milestone F preserved | PASS | CXX-only graph, source/link/import/COFF-origin evidence; zero active Fortran/ABI/runtime/state |
| A22 | PFSM architecture boundary and no unauthorized start | PASS | Reusable current owners/ports; PFSM NOT_STARTED. Final handoff awaits A-01 correction and independent acceptance. |

The full 80-test results at acceptance are **REUSED_UNCHANGED_DEPENDENCIES**,
not new full runs at the closure commit. Original fresh build checkpoint:
`d2915b0bd32b3644b00bc343440dd24548571bd6`. Final source packaging, reference/raw
identity checks, targeted workflows, Qt-free client execution and sampled recovery
are **FRESH_VALIDATION**. Passing old tests does not override A11/A14 FAIL.

| Optional / unclaimed observation | Result | Reason |
| --- | --- | --- |
| Hosted GitHub CI | NOT_VERIFIED | Workflow inspected; not dispatched |
| Clang and macOS configurations | NOT_VERIFIED | Outside the required release matrix; not executed |
| Remote/offsite archive disaster recovery | NOT_VERIFIED | Only local recovery verified; no remote availability guarantee |
| New complete installer/provider matrix | NOT_VERIFIED | Inputs unchanged; only bound existing evidence and prior isolated installation observations reused |

No missing optional observation was promoted to PASS, and no mandatory scope was
redefined to exclude A-01. A separate bounded correction and independent recheck
are required; this task changes acceptance documentation/current context only.
