# A-01 bounded CLI correction

Correction baseline: `b4e506bba0d052c2e3c3f60ef6cce422b48e9a81`, following final
Phase37E closure `e6883edab2fb26d8f22866c0585b910322027c12`.
Resolve this correction commit with
`git log -1 -- docs/architecture/milestone-a/a01/correction.md`.
The [original blocked review](../independent-review.md), [matrix](../acceptance-matrix.md)
and [gate](../gate.json) retain their observations unchanged.

## Change and status ownership

Production change: four added lines in `apps/qualpf/main.cpp`, immediately after
the metadata query and before `Found N card(s)`. The branch checks the existing
`isCanceled()`/`searchError()` state, emits the same `Search failed: <diagnostic>`
as pattern matching, and returns `EXIT_FAILURE`. Empty results alone are never
treated as failure. No synchronous adapter or error interface change is needed.

The CLI constructs one Workspace and makes its synchronous query on that owner
thread; admission is neither Busy nor Closing and begins at TaskId zero.
`SearchCoordinator::begin` clears previous error/cancellation state and creates
the current token. `complete` retains this call's SQL error or cancellation before
the adapter returns. New integration checks exercise failure/cancellation/exception
followed by positive or genuinely empty success, with no publication or active task
left behind. Production engine, coordinator, repository, schema, scoring/ranking/
residual, Session, GUI/background scheduling and QXP code are unchanged.

## Causal validation

`qualpf.cli.search_boundary` executes the actual product. It creates the valid
database through `--createdb` with unchanged `cod-2300112.cif`, checks the original
Zn candidate and legitimate Xe empty result, then uses the exact four-file A-01
placeholder/infodb recipe. The failed query must exit nonzero, expose a nonempty
`Search failed:` diagnostic and omit successful `Found` output. A direct schema
control proves the candidate table is missing.

Existing pattern-match behavior is checked using the finite input samples from
`current-preprocessing-pool.qxp`, copied to text without producing expected
numerical values. Valid search, failed SQL, no-peak and missing-file paths run as
actual CLI processes. The approved numerical reference files are untouched.

`qualpf.cli.synchronous_boundaries` executes the real adapter/coordinator/engine
with a controlled reader/progress callback. It checks the existing cooperative
cancel API, callback exception handling, recovery on the next call and zero Session
publications. The command-line parser exposes no cooperative cancel command or
search SIGINT handler; this test does not create one or claim a new CLI cancellation
model. The new metadata boundary uses the existing pattern-match cancellation exit
and diagnostic convention.

Both new normal tests passed on Windows GCC 13.1/Qt 6.8.3, Windows MSVC 19.40/
Qt 6.8.3 and Ubuntu 26.04 GCC 15.2/Qt 6.10.2 after real affected-target builds.
The suite now contains 82 identities: the preceding 80 and these two additions.
All 169 reference lengths/SHA remain unchanged; current architecture checks pass.
Development raw runs are under `build/milestone-a01/development`.

The new regression was first run against the unchanged original product and
failed precisely at the A-01 query-failure assertion after positive/empty controls
passed. Original observations remain in `build/milestone-a01/original-cli-regression-002.*`.
Initial runner observations are preserved separately: SQLite handles needed explicit
closure for Windows cleanup, Qt reports failed prepare/exec as `No query Unable to
fetch row`, and Linux emits existing informational Qt messages to stderr. The
portable assertions check the specified failure boundary and candidate content,
without requiring one driver-specific wording or suppressing product diagnostics.

This correction commit is not final Milestone A acceptance. Independent source,
committed-input, build and actual execution rechecks follow it. No push or PFSM.
