# Settings and workflow history

**Setting** on the menu bar opens four independent defaults: Preprocessing, CSM,
PFSM and Peak Search. These settings initialize new projects; saving defaults
never changes the current project. No imported data is required.

The left column selects the workflow and the top tabs follow its module order.
Switching tabs preserves drafts. **Restore Page Defaults** restores the current
module's factory parameters. **Save Defaults** validates all edited pages and
locates the first error. Unopened workflows retain their saved defaults. Cancel
discards all drafts.

**Setting → PFSM → Initial Peaks** edits PFSM's own derivative threshold,
automatic/manual window and SG polynomial degree. These values are saved
independently of the other workflows. Current automatic PFSM preparation uses
geometric FWHM, which detects its own local peak candidates before measuring
half-height crossings. It does not call the separate SG initial-peak module, so
these SG parameters do not affect automatic geometric FWHM; the page states this
explicitly.

**Setting → PFSM → Profile Expansion** enables template caching by default.
During fitting iterations, cached interpolation reduces repeated profile
calculations and can speed up searches. Cache preparation takes time and uses
extra memory, so small searches may not benefit. Final fits always use direct
profile evaluation for verification. Turning caching off evaluates profiles
directly throughout fitting and avoids interpolation and its cache overhead,
but repeated calculations can make searches slower. Existing saved choices and
project settings are preserved; **Restore Page Defaults** enables the cache.

Each **Options** entry opens its established independent dialog, with its original
controls, tabs, preview and buttons. CSM retains its weight sliders; PFSM retains
General and Candidate Recall; Initial Peak Search retains Settings and Initial
Peak Candidates. Options edit only the current project's corresponding workflow.
OK saves CSM/PFSM options; preprocessing editors apply their preview or calculate
on OK as before. Cancel discards the draft. Range, smoothing and Kalpha2 remain
explicit operations. Setting is the separate default editor for new projects.

Missing dependencies use the target workflow's own parameters. Compatible manual
preprocessing can be reused across workflows; automatic results from another
workflow do not count as manual preparation. PFSM automatic width is geometric.

**View → Workflow** shows completed steps, their sources and the last attempt.
Select a step for **Details** or **Undo Selected**. Undoing an earlier step also
removes dependent results, with a confirmation listing the affected steps. Cancel
leaves the project unchanged. Undo history is restricted to this session; there
is no redo. Search is one undo step; accepting/removing phases has its own step.

QPF 1.0 saves project parameters, current results and their sources, including a
continuable PFSM analysis and project graph settings. Reopening starts a new undo
history. QXP v1/v2/v3 files are rejected. A different
reference database permits viewing saved results but requires restarting the
analysis before retrieving new references. Failed saves leave the original file.
