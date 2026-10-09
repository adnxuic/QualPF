# Independent A-01 correction reacceptance

Outcome: **CPP_ARCHITECTURE_REFACTOR_COMPLETE**. All 22 mandatory Milestone A
criteria pass. A-01 is closed; Milestone F is preserved; PFSM is NOT_STARTED.
See the [matrix](acceptance-matrix.md) and [gate/result](gate.json).

Reviewed implementation: `5a247152c9a57bdab4f0ba348218dec80044d49c`.
Correction baseline: `b4e506bba0d052c2e3c3f60ef6cce422b48e9a81`.
The actual Phase37E closure remains `e6883edab2fb26d8f22866c0585b910322027c12`;
`d2915b0bd32b3644b00bc343440dd24548571bd6` is the earlier fresh full-suite
checkpoint. These identities are distinct. Resolve the final acceptance-record
commit with `git log -1 -- docs/architecture/milestone-a/a01/gate.json`.

## Independence and bounded source review

Review followed the committed correction. A new detached checkout supplied the
committed inputs; its changed files were byte-checked against each actual SDK
checkout. Each compiler rebuilt `main.cpp` and linked the product, then the new
executables ran. A separate observation script, which does not import the new
regression runner or trust the correction report, compared the original and new
products against identical real databases, settings and pattern inputs.

The only production/build/runtime delta since the final Phase37E closure is four
lines in `apps/qualpf/main.cpp`. The metadata CLI checks `isCanceled()` and
`searchError()` immediately after the synchronous query, before printing `Found`.
Failure uses the existing pattern-match `Search failed: <diagnostic>` convention
and `EXIT_FAILURE`. An empty vector alone is never classified as failure. The
pattern-match branch itself is unchanged.

The synchronous adapter and `SearchCoordinator::begin/complete` were read directly.
The standard CLI constructs one Workspace, issues one owner-thread query, starts
with zero issued tasks, and is neither Busy nor Closing. Admission clears previous
error/cancellation state. Completion retains this call's structured query error or
cancellation before the adapter returns. Existing status accessors suffice here;
no interface, admission, SQL or engine change was necessary. The real synchronous
adapter test verifies error/cancel/exception recovery, no active task left behind,
unchanged canonical snapshot and zero subscriber publications.

The [original independent review](../independent-review.md),
[blocked matrix](../acceptance-matrix.md) and [blocked gate](../gate.json) retain
their original FAIL and reproduction bytes. This reacceptance does not rewrite
those observations or an earlier authority.

## Actual CLI observations

Every platform executed the real `--createdb` path on unchanged
`cod-2300112.cif`, then eight regression subprocess cases. Independent old/new
comparison added eleven process observations per platform:

| Case | Original product | Corrected product | Independent comparison |
| --- | --- | --- | --- |
| Zn metadata search | Exit 0, one `zinc_oxide` card | Same | stdout/stderr bytes equal |
| Xe metadata search | Exit 0, legitimate zero cards | Same | stdout/stderr bytes equal |
| Finite pattern, valid DB | Exit 0 | Same | stdout bytes equal; stderr equal except existing elapsed-time field |
| Same pattern, broken DB | Exit 1 with `Search failed:` | Same | stdout bytes equal; stderr equal except existing elapsed-time field |
| Frozen A-01 metadata recipe | Exit 0 and `Found 0 card(s)` | Exit 1, nonempty `Search failed:`, no `Found` | Original defect reproduced and corrected |

The broken database uses the original four `.sq*` files, placeholder table and
main `infodb` row, with required candidate tables absent. A read-only schema
control in the regression verifies the missing `chemical` table. The independent
script reconstructs the same recipe separately. No fake Engine result substitutes
for the actual failed product query.

The existing finite input samples in `current-preprocessing-pool.qxp` are copied
to text; no expected numerical result is generated. Normal pattern, failed SQL,
no-peak and missing-file paths are exercised. Qt's existing Linux informational
stderr is retained. Only the numeric value in the existing named elapsed-time
diagnostic is normalized for old/new comparison; all original bytes and hashes
are recorded. Candidate fields, output, errors and numerical diagnostics are not
normalized.

There is no CLI cooperative-cancel command or search SIGINT handler to invoke.
The reachable programmatic progress callback uses the existing cancel API and
exception boundary. It preserves `Search canceled`, distinguishes cancellation
from true empty success, and resets status on the next call. No new cancellation
model or unexecuted OS-signal behavior is claimed.

## Regression and platform binding

| Configuration | Fresh corrected build/workflows | Strict unchanged reuse | Compile units / artifacts |
| --- | --- | --- | --- |
| Windows x64 GCC 13.1 / Qt 6.8.3 | PASS, 28 normal identities | 54 identities | 304 / 70 |
| Windows x64 MSVC 19.40 / Qt 6.8.3 | PASS, 28 normal identities | 54 identities | 301 / 70 |
| Ubuntu 26.04 x64 GCC 15.2 / Qt 6.10.2 | PASS, 28 normal identities | 54 identities | 303 / 71 |

Each current suite contains the preceding 80 identities plus the two new normal
CLI tests: **82 covered, zero failed/skipped**. The full 82 are not claimed as
one new full run. `FRESH_VALIDATION` applies to the corrected executable, two new
tests and 26 existing affected workflows. `REUSED_UNCHANGED_DEPENDENCIES` applies
only to the other 54 identities from the original fresh checkpoint.

All 1,775 earlier bound inputs were rehashed in each SDK. Changed bound inputs are
exactly `main.cpp`, product-test CMake registration, coverage and the test guide;
the two new test sources match the committed checkout. Every existing compile
command and all 80 original test commands/properties remain identical. Each SDK
retains all original binaries except the rebuilt product and adds one new test
binary: 68/68/69 original artifacts remain byte-identical. Relevant Python
runners, fixtures, compiler/options and resource inputs are unchanged for the
reused identities. All 73 original compressed raw logs and their actual local
raw counterparts were independently length/hash-checked.

The original clean builds and complete Qt-free Application client evidence remain
valid for the unchanged libraries. All 70 Core/Application compile units per
platform remain Qt-free. Fresh architecture negative controls and source checks
pass. GUI search, SQL thread ownership, cancellation/stale/publication and actual
MainWindow close/drain workflows are among the 28 newly executed tests.

Original failed attempts are retained: Windows native settings fixtures required
the runner's test-owned TEMP/TMP and access to UUID-owned native registry keys;
Linux old/new diagnostics differed in elapsed time. Earlier development runner
failures also remain available. No test assertion, numerical tolerance or product
implementation was weakened to hide those observations.

## Remaining mandatory criteria

The other 20 criteria were rechecked against actual unchanged source/target
ownership and strict evidence bindings, rather than promoted from prior PASS
labels. Core/Application dependency direction and unique Workspace/Session
ownership remain intact. Infrastructure returns owned data and creates/uses/
removes SQLite handles on their execution thread. GUI uses immutable projections,
the shared SearchEngine and the existing one-worker controller; search/drain
event pumps remain zero. Closing/cancellation/late callbacks retain the checked
publication and lifetime rules. Scoring, stable ranking, residual, preprocessing,
crystallography, IO, QXP/provenance and resource code are unchanged.

All **169 reference lengths and SHA-256 identities** match the protected
pre-cleanup authority, including its provenance multiset. No golden, tolerance,
formula, SQL/QXP schema, parameter or accepted numerical decision changed.

The detached implementation checkout configured successfully. Its actual CPack
TGZ contains exactly **1,941 tracked files**, with all 169 references byte-identical.
Real worktree `.git`, Python-cache and build controls are absent from the package;
normal inputs need no migration runner, evidence archive or Fortran toolchain.
The acceptance-record commit adds current records/context and the minimal context
checker extension, not a different C++ implementation. Its checker is freshly
validated with acceptance, stale-candidate, missing-gate and failed-criterion
controls; this tool is not a normal CTest runner or product input.

Six actual historical object/catalog/member samples and three protected Git
receipts were fetched and hash-verified again. Local recovery passes; the existing
local backup has **no verified remote/offsite availability guarantee**. This risk
is recorded without inventing a disaster-recovery claim or changing the gate.

Fresh current Zero-Fortran scans pass for all three corrected builds. MSVC also
passes a fresh scan of **319 COFF objects/archives and 1,514,163 symbol lines**, with
zero Fortran-origin matches. Unchanged Core/Application, packaging/runtime rules
and current references preserve **MILESTONE_F_PRESERVED**.

Hosted GitHub CI, Clang/macOS, remote archive recovery and a newly replayed complete
installer/provider matrix are **NOT_VERIFIED** and are not claimed as executed.
PFSM architecture handoff is ready; Phase38/PFSM is not started. **STOP.**
