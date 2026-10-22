# PFSM numerical contract

The authorized PFSM implementation uses a fixed intermediate Lorentzian (IL)
template and variable-projection fitting. Core owns two replacement interfaces,
`IProfileExpander` and `IProfileFitter`; method orchestration and improvement scoring
belong to Application. Neither interface depends on Qt or mutable product state.
This document describes the numerical kernel. The integrated Application and
OpenMP workflow is specified in [Search-Match contracts](../architecture/search-algorithms.md).
Real-data acceptance remains separate evidence and cannot be inferred from the
synthetic numerical tests.
The separately authorized single-pattern phase-width experiment is an isolated
diagnostic, described in the [original benchmark record](../../../history/README.md#pfsm-benchmark-evidence). It changes
the common-width assumption only inside that experiment; its prototype and
synthetic evidence do not replace this product contract or authorize adoption.
The completed 464-card conditional round places the missing Bi2Mo3O12 card at
rank 16 and fails Top-5 despite lower fitted Rwp. Its audited result preserves
this default contract and the original failed comparisons.
The [headless benchmark contract](pfsm-benchmark.md) defines input freezing and
the assisted Top-5 acceptance rule.
The human-directed acceptance closeout retains 57/58 ready-development passes,
one Bi2Mo3O12 failure and nine unresolved nonpasses. The subsequently authorized
frozen holdout assessment passes 22/22 cases, identifies all 64 truth-phase
occurrences and recalls all six predeclared weak-phase occurrences under the
same Top-5 assistance rule. All 430 Top-5 results have independent direct-IL and
fixed-shift NNLS/KKT verification; these are not nonlinear global-optimality or
automatic-identification proofs. Final Rwp_net remains 20.39--54.88%. The assessment
changes no model or tolerance and does not reopen the closed development failure
experiments or resolve their retained nonpasses. Full evidence and limitations
remain in the [original benchmark record](../../../history/README.md#pfsm-benchmark-evidence).

## Candidate recall versus formal fitting

### Bounded integral baseline kernels

The resource-limit fallback uses the integral/old-shift baseline from
`3fa1123a16ab746f80f946dea55f4b9f5b633e48`. Its Core entry points are
`pfsm_integral_screen.h` and `pfsm_relaxed_screen.h`. The port only adapts the
current public `expandProfileLines` interface and separates integral evidence
from the unrelated historical prescreen API. Stage A (`1250a174`) and A+B
(`8c149f61`) are excluded: no solved-trial cache, shared weighted old banks,
incremental-column optimization, row compression or NNLS accuracy override is
introduced. Existing formal fitting and its optimizations remain unchanged.

`6c279439` adds acceptance records without changing the numerical source. Source
correspondence is fixed as follows (all baseline paths are at `3fa1123a`):

| Current source | Baseline source / adaptation |
| --- | --- |
| `src/core/profile/pfsm_integral_data.cpp`, `pfsm_integral_screen.cpp`, `pfsm_relaxed_screen.cpp` | Same paths; replace the old line-expansion helper with public `expandProfileLines` |
| `src/core/include/qualpf/profile/pfsm_integral_screen.h`, `pfsm_relaxed_screen.h` | Same paths; include the separated evidence header |
| `pfsm_integral_evidence.h` | Exact support/screen/relaxation structures from baseline `pfsm_prescreen.h`; unrelated legacy API omitted |
| `tests/unit/core/pfsm_integral_test.cpp`, `pfsm_integral_probe.cpp`, `pfsm_relaxed_probe.cpp` | Same baseline tests/probes, with the same public API adaptation |
| `tools/validation/pfsm_integral_reference.py`, `pfsm_relaxed_diagnostic.py`, `pfsm_shift_diagnostic.py` | Byte-identical baseline independent checkers; tolerances unchanged |

The task's `build/pfsm-integral-fallback-20261021/core-origin.json` and
`baseline-source-audit.json` bind original and adapted source hashes. The current
`pfsm_nnls.cpp` and `pfsm_internal.h` remain unchanged from the receiving branch.

Weighted midpoint cells preserve total weights and weighted net intensity,
including negative values, masks and disconnected regions. Fine bins number at
most min(2048, positive-weight samples); adjacent bins form the coarse layer.
Unit-area IL integrals truncate only this proxy at tail mass 1e-4, without window
or tail renormalization. Stable same-side differences retain narrow far-tail mass.
The proxy is not an equivalent transformation of the discrete formal objective.

Each candidate receives 65 coarse positions (plus zero when needed) and at most
32 fine positions in four neighborhoods. The old-shift channel releases at most
two old coordinates selected by weighted template similarity, within
min(1.5 H0, half the allowed shift span) of their formal shifts and the original
bounds. It uses at most 257 raw columns per old phase, 32 cached candidate
columns, two starts, four discrete stages and 205 cold NNLS evaluations per card.
Each trial builds the complete fine-layer weighted columns; no solved result is
memoized. Matched no-candidate searches release the same old coordinates.

The final union is fine Top-64, coarse Top-16, original Top-16, residual Top-16,
relaxed Top-32, boundary ties and numerical protection, without a final cap.
Memory/position bounds do not grow with inverse width; necessary input, phase,
reference and candidate counts still determine total cost. Screening parameters
never initialize formal fitting. Independent tests retain the original 2e-10
SciPy/integral tolerance and prescribed analytic controls.

### Resource-limit routing and settings

Normal recall uses v2 unchanged. Only `candidate_recall_grid_limit` (more than
131072 grid nodes) and `candidate_recall_scan_limit` (more than 4096 scan points)
select this baseline integral backend. `PfsmResourceFallback::bounded_integral`
is the default; `full_pool` preserves full formal fitting on those failures.
The policy freezes with the analysis. Off, explicit full rounds, replacements,
small pools and baseline-rebuild full replays retain precedence. Shadow computes
the integral union but fits the entire pool; enabled fits the union. Successful
switching records `switch_reason` without setting `full_fallback`. The v2 formal
underestimation gate applies only to v2 evidence.

Integral cells are immutable and cached with the frozen observations; old-phase
banks and matched controls are prepared for each baseline. Workers own their
NNLS workspaces. Recoverable individual numerical failures protect that card;
shared numerical or allocation failure discards partial evidence and fits the
full pool. Invalid input and cancellation abort without publishing a draft.
Integral diagnostics retain their final union in both recalled/retained sets;
state validation checks identities, baseline binding, resources and the channel
union without fabricating v2 selection or applying its review/formal caps.

QPF stores the policy, actual backend and integral/relaxation evidence as additive
extension fields. Missing policy defaults to bounded integral; missing backend
fields describe historical v2 diagnostics. Existing required fields stay required,
unknown fields remain preserved, and computational caches are not persisted.
Loading a saved analysis does not recompute results. Subsequent rounds reuse its
frozen parameters and select the current resource route.

### V2 candidate counts

Candidate counts are carried by `PfsmCandidateRecallLimits`: recall candidates
500, local shortlist 20, per-channel protection 5, review threshold 80 and
formal-set threshold 80 by default. Each is a positive signed integer; protection
must not exceed the shortlist, the shortlist must not exceed recall, and both
thresholds must be at least the shortlist. The local shortlist also selects the
initial gain rank used for `g_star` and the small-pool full-fit boundary.
Cutoff ties and numerical protection remain additional candidates. Exceeding
(not equalling) either threshold triggers the full pool, never truncation.
Configurations freeze with the analysis. The historical v2 assessments below
bind their original default counts; they do not attest arbitrary custom counts.
Seed counts, local steps, acceptance/risk coefficients, precision and all formal
fitting options remain fixed. Default-count diagnostics retain their historical
names; custom-count diagnostics record the effective limits and neutral Top-K
and fallback-limit reasons.

PFSM candidate recall is the preliminary candidate-selection module historically
called Fast-PFSM. Its correlation ranks, bounded local fits and gains are recall
diagnostics, not final FOM or formal candidate evidence. The formal path retains
its own expansion, scan points, multistart NNLS/TRF fit, direct final verification
and fixed FOM. Recall parameters cannot initialize or configure that path.
Shared numerical primitives do not merge these responsibilities.

The current product uses v2 with the resource-only bounded integral fallback;
new GUI analyses default to enabled, while CLI and low-level requests default to
off. The v1 local-fit route and its probe transport are
retired. Shared correlation and v2 backprojection-ablation diagnostics remain.
The current architecture names the module by this responsibility; historical
v1/v2 evidence below retains its original identity and scope. Neither a rename
nor removal of a historical execution route changes approved formulas, budgets,
tolerances, independent references, original failures or admission conclusions.

The naming/boundary refactor was checked with Windows GCC 13.1, Windows MSVC
19.40 and Linux GCC 15.2 builds. Each platform passes the ten standalone Qt-free
tests, 54 independent v2 recall cases and 24 direct plus 24 cached formal-fit
SciPy/Brent cases. MSVC and Linux additionally pass all twelve affected product
CTest registrations; the GNU affected batch passes its six selected contracts.
The retained pre-boundary GNU executable produces identical analytic CLI formal
rows, baseline curves, candidate order and recall evidence. Shared backprojection
diagnostics also pass the existing 24-case independent reference. These are
regression and numerical checks, not new real-data identification acceptance or
a proof that approximate recall never misses a candidate.

Local raw evidence is under `build/candidate-recall/` (`*-affected.xml`,
`gnu-batch3.xml`, `*-qtfree.xml`, `*-cancellation-{affected,qtfree}.xml`,
`*-independent-recall.json`,
`*-formal-{direct,cached}.json`, `boundary-exact-comparison.log` and
`gnu-backprojection.json`). The Linux source archive is bound to implementation
commit `b329c263`; the subsequent restoration of the pre-input cancellation check
is recorded as a separately hashed source update and affected-test rerun. Separate
executable hashes bind the WSL probe bridges used by the independent references.
MSVC's original PDB-configuration and stale-header-dependency failures remain in
the original logs; corrected compiler detection and clean rebuilds record nonzero
Ninja header dependencies. No reference input, tolerance or failed historical
scientific comparison was replaced.

The final Windows GNU normal run passes all 111 registrations with zero failures
or skips in 1,528.39 seconds (`gnu-normal-final.log` / `gnu-normal-final.xml`).
It includes the restored cancellation ordering and retains original timeouts and
tolerances. The earlier interrupted run is preserved separately; it was stopped
to include that source correction, not counted as a completed regression. Source
patch and result hashes are recorded in `final-validation-source.json` alongside
the logs. All 169 frozen normal reference files remain byte-preserved.

## Fixed templates

### Experimental Fast-PFSM v2

The separately authorized v2 branch starts from v1 delivery `ebc648b0`. Its first
ablation preserves every v1 grid dictionary/interval proposal, groups the NNLS
coefficients by phase and restores a single coefficient-weighted displacement.
Column normalization is undone by the existing NNLS before displacement recovery.
Each interval is then evaluated on the original positive-weight observations and
all phase scales are solved jointly. Zero-scale old phases retain their formal
baseline shifts; a zero-scale candidate retains that interval's initial shift. No proposal
parameters enter the formal fitter.

The common baseline is the supplied formal residual and parameters, checked for
consistency with direct IL templates. A real baseline-plus-zero-candidate state
is always feasible. Gain uses `2*r0.dot(m-m0)-||m-m0||^2`, cross-checked against
`q0-q`; negative trial gains remain visible. The best state changes only when the
gain exceeds `128*epsilon*max(1,q0,qtrial,qbest)`. This is a fast diagnostic rule,
not a formal solver tolerance. Native observation weights, signed net intensities,
all reference lines and the existing cold NNLS/KKT checks are retained.

`pfsm_fast_reference.py --backprojection` independently checks the linear NNLS
objective, coefficient feasibility, displacement recovery, native-point NNLS,
KKT and gain identity with pinned SciPy 1.16.2. It compares objective/feasibility
rather than requiring a particular nonunique rank-deficient coefficient vector.
The ablation is validation-only; no v2 admission or real-case improvement follows
from these numerical checks.

The anchored v2 correction is a separate diagnostic API. It starts each candidate
from the same formal selected-phase shifts, solves native-point nonnegative
scales, then solves Taylor columns `a+t_minus*H*da/dshift` and
`a+t_plus*H*da/dshift`. Restored single shifts receive another joint scale solve.
The initial radius is H/8; rejected or indistinguishable steps halve it. A step
is accepted only for resolvable physical improvement and actual/predicted gain
at least 0.1. Each initial pass costs at most three NNLS calls. Review completes
two steps for each of at most three seeds, at most fifteen calls including failed
calls. All valid visited physical fits can compete with the real zero-candidate
baseline; only accepted fits become subsequent local anchors.

Seeds come from the unchanged recall scan: primary channel, the other channel,
then original/residual local maxima in fixed channel and scan-tie order. Physical
endpoints are eligible maxima. Seeds closer than H/4 are removed. Capturing seeds
does not change recall arithmetic or selection; v1 does not allocate those lists.
The fast-only differentiated Clenshaw evaluator returns value and shift derivative
from the same polynomial, with paired direct fallback. Existing formal template
value/derivative entry points retain their bodies and numerical order.

`pfsm_fast_v2_reference.py` validates every local NNLS objective/KKT condition,
constraint-to-shift recovery, native physical re-fit, gain and step decision,
using 48 direct/cached cases including zero old scales and rank deficiency.
Six additional derivative cases use an independent 80-digit analytic IL formula.
These 54 checks do not establish real-case retention. The first reference report
failed while serializing a NumPy Boolean; its script/log are retained separately
from the repaired reporting run, with no tolerance or numerical assertion change.

V2 selection freezes `g_star = max(initial_gain_at_rank_20, 1e-12)` before review.
Initial Top-20 (including ties), channel Top-5 from the recalled set, and current
risk states request review. Each seed contributes only its last valid step to
unresolved distortion/boundary protection; numerical protection is sticky for the
round. Local Top-20, channel protection and unresolved risks form the proposed
formal set. Review or proposed cardinality above 80 selects the complete original
pool without truncating risks. Pure local ranking and all protection reasons
remain available even when review is skipped or full fallback is required.

Fast cache values and derivatives are evaluated together, including shared old
phase columns. A failed cache evaluation falls back as a pair. Formal evaluation
entry points and tolerances remain unchanged.

Backprojection records the initial shift associated with each proposed interval;
a zero candidate coefficient restores that interval's initial shift. The fast
physical and Taylor solves reuse private QR/SVD workspaces within a candidate
stage, while resetting all normalized-column validity and active-set state for
every cold solve. This introduces neither normal equations nor warm-start scales.

Anchored local steps release the temporary column dictionary after copying it
into the linear matrix, before allocating the NNLS workspace. The linear matrix
is released after prediction and trial-shift recovery, before physical-trial
sampling. Coefficients and prediction own their results at that boundary; no
solver or sample state borrows the released storage. Expressions, column order,
cold solves and candidate/protection decisions are unchanged. This bounds overlap
of dead temporaries on long spectra without reducing observations or candidates.

Each v2 seed retains its original fixed-shift NNLS fit separately from the current
accepted iterate. Initial negative gains therefore remain in the diagnostic
record after later accepted steps, alongside every valid physical trial gain.

The first v2 configuration is frozen before real replay:

| Rule | Fixed value |
| --- | --- |
| Recall / regular local set | 500 / 20, including cutoff ties and numeric protection |
| Channel protection | Top 5 per recall channel, including cutoff ties |
| Seeds / minimum spacing | At most 3 / H/4 |
| Initial radius / rejected radius | H/8 / halve, never enlarge |
| Ordinary / reviewed NNLS calls | At most 3 / 15, failed calls included |
| Reviewed steps | At most 2 per seed |
| Acceptance | Resolvable actual decrease, positive prediction, actual/predicted >= 0.1 |
| Model distortion | Last-step absolute prediction error > 0.1 max(g_star, abs(candidate gain)) |
| Local boundary | Contributing phase at a local edge, actual decrease > 0.05 g_star; exclude physical bounds |
| Correlation risk | Orthogonal residual energy ratio < 1e-3 |
| Review / proposed cardinality | Above 80 triggers full original pool; no risk truncation |
| Formal underestimation | formal_gain - local_gain > 0.25 max(formal_gain, g_star) |

The correlation basis is the normalized nonzero old-phase template SVD, with
threshold epsilon times the larger matrix dimension. A zero candidate template
is numerical protection, not an undefined ratio. Candidate states and temporary
parameters remain private; stage decisions use original library order after the
parallel barrier. Neither the Taylor representation nor the recall approximation
has a no-miss guarantee. Protection and full fallback are reported separately
from pure local recovery.

The [complete v2 replay and enabled assessment](../../../history/README.md#pfsm-benchmark-evidence)
records 263/263 exact new formal round comparisons and five supplemental
configurations. Pure reviewed Top-20 retains 256/258 executed reviews; channel
and unresolved-risk protection remain necessary. Nine full-pool fallbacks include
four grid-unavailable rounds, four formal-underestimation rounds and one skipped
review above 80 candidates. These measured outcomes do not create a no-miss
bound or authorize automatic enablement. Final Windows GNU normal regression
passes 111/111 at the original limits, separately recorded in the benchmark guide.

### Experimental Fast-PFSM numerical front end

The isolated Fast-PFSM experiment starts from no-prescreen commit
`3e304908da6f91d3ca44f3893dd506ed3ca34c3b`. Its `profile/pfsm_fast.h` API is
independent of the formal fitter and never supplies formal starts or parameters.
The fixed budgets are 500 recalled cards and 20 formal fits, plus cutoff ties and
numerically protected cards. These are experimental recall heuristics, not safe
objective bounds or a universal Top-5 guarantee.

With `y=Y-b` and current baseline residual `r`, the shared fields are
`F_y(t)=sum(w*y*phi(x-t))`, `F_r(t)=sum(w*r*phi(x-t))`, and
`G(t)=sum(w*phi(x-t)^2)`. Samples deposit their weights and weighted signals
linearly onto adjacent H/8 nodes. Negative signals remain signed; zero weights
contribute nothing. The grid includes [0,180], observations and both shift
directions, with interpolation guard nodes. Every finite grid lag is retained.
The explicitly selected Eigen 3.4 KissFFT backend computes zero-padded linear
convolution of M data nodes with 2M-1 kernel nodes; padding is at least 3M-2.
No circular wrap, fixed-width tail cutoff or added runtime dependency is used.
More than 131,072 grid nodes or 4,096 scan points uses the frozen resource-limit
policy: bounded integral by default, or full-search fallback when selected.

The H/4 scan includes bounds and feasible zero. Ordered radiation expansion and
validation are shared with the original template implementation. Only the fast
copy merges exactly coincident lines. Recall scores are
`max(0,sum(I*F(q+shift)))^2 / sum(I^2*G(q+shift)) / Dnet`.
The diagonal denominator omits cross terms and is used only for recall.
Original and residual rankings alternate, skipping duplicates until 500 scored
cards are chosen. Both cutoffs retain ties within
`128*epsilon*max(1,abs(a),abs(b))`. Protected cards are additional.

Local templates convolve all deposited lines with the complete finite-lag IL
kernel. NNLS operates on occupied bins with `sqrt(W/Dnet)` columns and
`V/sqrt(W*Dnet)` targets, so overlapping-line cross terms remain present. Each
old phase contributes its current and adjacent H/4 shifts clipped to the bounds;
each candidate contributes both endpoints of the left and right intervals around
each channel's best shift. Every solve uses the original cold NNLS/KKT checks.
The old-only local solution remains feasible; improvement compares the same
old-phase dictionary with and without the candidate. Reported local objectives
are normalized bin residuals and omit the common within-bin constant. They are
not formal Q or Rwp. Final selection uses local improvement with the same tie
rule, retaining all protected cards, and returns original library order.

Independent direct convolution and pinned SciPy 1.16.2 NNLS checks live in
`tools/validation/pfsm_fast_reference.py`. The 24 fixed cases use a 2e-10 absolute
comparison for normalized quantities. Scientific and speed admission require
separate full-search comparisons; numerical kernel checks alone admit neither.
The completed frozen 80-case reduced-workflow cohort demonstrates this limit:
the 500-card recall retains Top-5 in 263/263 rounds, while local reranking to 20
loses members in seven rounds/cases. Baseline/off/shadow formal rankings and
baseline curves agree for each failed case; all 263 rounds preserve surviving
formal evidence and baseline curves. Enabled confirms the selection losses. The
[benchmark evidence](../../../history/README.md#pfsm-benchmark-evidence) preserves
the excluded identities and ranks. This fixed configuration fails preservation
admission and remains experimental. The five supplemental initial-search groups
(two long spectra in direct/cache modes and the 88,312-card full library) preserve
Top-5 and surviving formal evidence, but do not waive the seven primary failures.
Full-library recall/formal counts are 501/21 because one zero fast template is
protected at both stages, with the fixed 500/20 scored budgets unchanged.

All values are binary64. Coordinates, widths and shifts are in 2theta degrees.
The peak-top-normalized kernel is
`phi(u,H) = (1 + 4*(2^(2/3)-1)*(u/H)^2)^(-3/2)`.
Its integral on the real line is `2*H/sqrt(4*(2^(2/3)-1))`.
Positive shifts move all reflections of a phase toward larger angles. A prepared
template owns its lines and is immutable and reentrant. Reflection and radiation
component order defines the direct sum order. The Core template default uses no
interpolation, tail cutoff or uniform grid assumption. Lines outside the
experimental window retain their tails. The derivative returned by a template
is with respect to the phase shift.

Paired d/I records require explicit radiation components. Each reachable component
uses Bragg's law and its normalized relative weight. Unreachable components are
omitted together with their intensity. Explicit angle/I records are already in the
requested radiation convention and are not expanded again. Relative line strengths
remain fixed and are not mass fractions. Application must supply one physical
FWHM for the analysis, with fixed/manual or geometric evidence; Core does not estimate a
width, apply the Classical 0.9 conversion, or invent a radiation/width fallback.

## Objective and solver

Observations own aligned coordinates, Y, fixed background b and nonnegative
weights w. Zero weights exclude samples from the objective but not returned curves.
Negative `Y-b` is retained. No noise model is inferred by the fitter.

`Q = sum(w * (Y-b-P*s)^2)`; `Dnet = sum(w*(Y-b)^2)`;
`Rwp_net = sqrt(Q/Dnet)`. A nonpositive or nonfinite Dnet is an error, not a score.
`Rwp_observed` is additionally reported when `sum(w*Y^2)` is positive. Internal
residuals use `sqrt(w)*(Y-b-P*s)/sqrt(Dnet)`; returned Q is independently recomputed
in original units. The background is subtracted once, without clipping.

At each shift evaluation NNLS jointly solves all nonnegative scales. Single-column
NNLS uses its analytic solution. Multiple columns use a Lawson-Hanson active set,
reversible norm scaling, column-pivoted QR and an SVD fallback on deficient free
sets. No regularizer is added. Rank and KKT diagnostics describe the scaled system;
rank deficiency, zero columns and zero-scale unidentifiable shifts are explicit.

The outer variable is `t=shift/H`. The bounded trust-region reflective solver uses
Coleman-Li distance scaling, its diagonal correction, a dense trust-region secular
solve, feasible/retracted reflected/Cauchy trial steps and reduction-based radius
updates. It is an independently written implementation of this mathematical
method, not a claim about Match!'s private solver. Three-point differences of the
complete reduced residual re-solve NNLS at every perturbation; boundaries use
second-order one-sided differences. Relative difference step is `epsilon^(1/3)`.

Defaults are shift bounds [-0.5,+0.5] degrees and ftol/xtol/gtol=1e-8. Each start
has 100 times max(1, phase count) in outer residual evaluations; difference
and final verification calls are separately included in diagnostics. Budget
exhaustion is distinct from convergence. Fixed coordinates are removed from the
outer problem. Near-boundary final solutions are checked at the exact feasible
bound and retained only if the re-solved objective does not rise. Supplied feasible
fallback parameters and initial solutions compete with every optimized start.
Final curves and Q are recomputed directly, including zero-weight samples.
An early implementation counted only free shifts in this budget; using all phases
matches the approved default even when some shifts are fixed. Runs with all shifts
free are unaffected. A single-column matrix's numerical rank is determined directly
from its nonzero norm and the same relative threshold; multi-column matrices retain
the SVD rank diagnostic. Full-matrix rank is computed at the shifts of each
returned `fit()` or `evaluateAt()` result, after restoring its template columns.
Intermediate residual/difference calls retain the same free-set QR/SVD and KKT
checks without repeating the separate full-matrix diagnostic. Inactive columns
still contribute to the reported rank. This changes no solve, reduction order,
threshold, budget or termination decision.

`IProfileFitter::scanAt` returns ordered original-unit Q values and accumulated
residual/NNLS counts for one phase's supplied shifts. Its default implementation
calls `evaluateAt` and applies the same full-payload validation as Application,
so replacement fitters remain compatible. The built-in override retains one
candidate-owned reduced problem across the scan: observation normalization and
unchanged template columns are reused. Each point still cold-starts NNLS and
checks KKT. Its phase-major curve sum and sample-major weighted-square reduction
are exactly those of the full result; normalized internal cost is not substituted
for Q. Only unused full curves and reporting-only matrix rank are omitted.
Public `evaluateAt` and `fit` retain complete curves, final rank and diagnostics.

The reduced problem also owns an NNLS storage workspace. It retains normalized
columns and their `stableNorm` only while the underlying weighted column is
unchanged. Shift changes and direct-final mode invalidate affected columns.
Vectors, passive-column matrices and QR/SVD objects reuse storage via fresh
computations; every solve still starts with zero coefficients and an empty
passive set. The ordered pivot rules, matrix expression types, thresholds,
free-set factorizations, residual operations and KKT checks are unchanged. No
factorization result or active-set solution is memoized across solves.

Core does not choose scan points, baseline rebuilding, candidate rankings or phase
acceptance. A caller must preserve a common evaluation problem across candidates.
The planned equal-weight rule for observations without a defensible variance is an
explicit engineering extension to the source's counting-statistics formula.
Radiation-component expansion and exact-bound verification are also explicit
engineering choices; the fixed IL shape, width and relative intensities are unchanged.

Profiling justified an exact-kernel optimization: ordinary finite distances use
`sqrt(1+v*v)` and extreme distances retain `hypot(1,v)` to avoid overflow. On the
supported x64 targets, SSE2 evaluates two independent coordinates together. The
per-coordinate reflection sum and arithmetic order remain fixed; scalar derivative
and independent power-law checks cover the ordinary and guarded paths. No fast-math, FMA contraction,
interpolation, resampling or tail removal is introduced. Independent power-law
checks span distances from the peak to the overflow-safe path.

AVX2-capable x64 CPUs with OS-enabled YMM state may evaluate four independent
coordinates in the same direct and cached recurrences. Only `pfsm_avx2.cpp` is
compiled for AVX2, under strict floating flags and without FMA; the detector and
all other sources retain the baseline ISA. The SSE2/scalar paths and every domain,
derivative, extreme-distance and invalid-cache fallback remain available. Configure
`QUALPF_PFSM_AVX2=OFF` to exercise the baseline-only build. CPU/OS detection follows
the [GCC independent-source constraint](https://gcc.gnu.org/onlinedocs/gcc/x86-Built-in-Functions.html)
and the CPUID/OSXSAVE/XGETBV checks on MSVC. Eigen vectorization remains disabled.
Direct AVX2 evaluates only complete groups of four. The original baseline-compiled
SSE2/scalar evaluator handles the remainder, including its platform `sqrt` call
for an odd last sample. On Windows GCC that provider can differ by one ULP from
an inlined hardware square root; retaining it prevents accepted-phase fits from
changing in their last bits. A strict reference-provider regression covers all
lengths from one to fifteen, including every vector remainder.

Each fit retains one unweighted sample column alongside each weighted column.
Final curve construction reuses those samples, including zero-weight points,
without undoing normalization or calling the full kernel again. Memory is bounded
by the active candidate's observation count times phase count. Switching from
interpolated iterations to direct verification invalidates both sets of columns.

An explicit `interpolation_window` additionally enables an optional approximation
cache, justified by the retained full-library and 32-card profiling. The Core API
default remains direct IL. Product `PfsmSettings` enables this cache by default;
missing preferences and factory restoration use that default, while saved off
choices and existing project profiles are preserved. The low-level `PfsmRequest`
and benchmark defaults remain direct. Each interval is at most H/2 wide and
represents the **complete** IL sum with 32 Chebyshev coefficients computed from
direct values. No reflection or tail is removed. A maximum of 16,384 intervals
bounds coefficient storage at
4 MiB per candidate; larger requests use direct evaluation. Outside the window,
for analytic derivatives, or on invalid interpolation values, evaluation is direct.
Samples need not be equally spaced. The analytic truncation bound is below 1e-18
times summed peak strengths (Bernstein ellipse rho=4); this is not a binary64
total-error guarantee, which also includes coordinate formation and rounding.
On x64, independent coordinate pairs use SSE2 Clenshaw recurrences with exactly
the scalar operation order in each lane. The coefficients, interval size and
approximation are unchanged. Tests compare grouped and individual evaluations
bit-for-bit, alongside the independent mathematical accuracy checks.

Cache nodes and line positions are formed relative to the window's lower bound;
evaluation subtracts that local origin before applying a trial shift. This keeps
narrow-peak finite-difference perturbations from disappearing when added to a
large physical angle, and avoids rounding the preparation nodes to that angle.
The translated line copy checks cancellation and its own SIMD distance bound.
Shifts larger than the cache span use the original direct evaluator. A numerical
overflow at an internal cache node also falls back to direct preparation, because
the actual observation samples may remain finite; cancellation and invalid-input
errors still propagate. Direct evaluation, fixed H, line intensities and the
fitting objective retain their existing contracts.

`evaluateAt` may return interpolated scan results, marked by
`exact_final_verification=false`. `fit` always re-solves all scales and recomputes
the final full curve/Q using direct IL, and compares direct initial/fallback
solutions before returning. Application rejects nonverified final evidence.
KKT and scales describe the direct final problem; outer termination/optimality
describe the iterative problem and `interpolated_iterations` records that fact.
This optional computational approximation is an explicit extension to the first
direct-only implementation, not a change of IL, width, intensities or objective.

## RIR semi-quantification

The fixed peak-top IL profile has integral
`C(H) = H/sqrt(2^(2/3)-1)`. With `b = 4*(2^(2/3)-1)/H^2`, its antiderivative
is `u/sqrt(1+b*u^2)`; the two infinite limits give `2/sqrt(b) = C(H)`.
A strongest reference line of relative intensity `M_i`
has fitted area `s_i*M_i*C(H)`. With the one common physical width, the factor
`C(H)` cancels when normalizing `a_i = (s_i*M_i)/RIR_i` to
`weight_percent_i = 100*a_i/sum(a)`. Radiation-component weights are normalized
by the existing expander; the fitted wavelengths, widths and coefficients are
unchanged. This is an RIR estimate, not an independent scattering-power calibration
or a Rietveld result. The denominator covers selected crystalline phases only.
Unidentified and amorphous material are not quantified by this normalization.

`M_i` is frozen once from the complete paired d/I reference card, before radiation
expansion, scan-window restriction or sampling. Selected baseline scales are used;
candidate trial scales, Classical score flags, sums of all line intensities and
maxima of sampled/overlapping curves are not substitutions. Reference intensity
rescaling with reciprocal coefficient scaling leaves mass fractions unchanged.
The Core profile/RIR operation uses ordered binary64 multiplication/division and
reduction under the existing strict compilation of `search/rir.cpp`. Invalid
inputs, nonfinite intermediates, underflow to zero and nonpositive amount sums
return errors. Classical `quantify_rir` remains byte-for-byte unchanged.
Numeric database RIR fields enter the typed PFSM reference directly as binary64;
their display strings are retained separately. Text fields use strict finite
positive validation. Classical database parsing retains its existing behavior.

The initial reference convention is Cu Kalpha. Positive radiation components
must lie within 0.0001 angstrom of the canonical Cu Kalpha1, Kalpha2 or weighted
Kalpha wavelength, accommodating independently recorded/rounded centroids such
as 1.5406 without modifying the actual fitting coordinates. Known neutron/electron
radiation and other wavelengths are refused. An absent radiation-kind declaration
is reported, never inferred as recorded evidence. Original database RIR source
conditions remain explicitly unverified. Only the fixed peak-top IL expansion
identity is supported; other template normalization conventions require their own
derivation. Width/model mismatch, preferred orientation and microabsorption can
bias estimates even when Rwp is small.

Application quantification requires a current identity-aligned baseline with final
direct/KKT evidence, full matrix rank and no zero templates. Every selected phase
requires a finite positive RIR and full-card maximum, including phases with zero
scale. A zero scale may yield 0% when the other conditions hold. One invalid phase
makes all percentages unavailable with phase-specific diagnostics. Budget or
feasible-fallback termination is reported as a qualification; no new Rwp cutoff or
uncertainty estimate is manufactured. RIR edits retain original values and source
notes, survive removal/reselection within the same frozen universe and disappear
when the analysis ends. They change neither baseline coefficients nor ranking.

Independent analytic/invariance tests are `qualpf.quantification.core`; prescribed
25/75 mixtures through the real Application search/acceptance path and atomic RIR
revision/error tests are `qualpf.quantification.application`. The mathematical
basis and limitations follow the [ICDD RIR tutorial](https://www.icdd.com/assets/tutorials/Quantitative-Analysis-RIR.pdf).

## Real-mixture semi-quantification assessment (2026-10-12)

The separate conditional assessment fixes phase identities, full reference cards
and their original RIR before fitting. It covers IUCr CPD 1a–1h and twenty
development binary scans, with one lowest-numbered non-deleted structurally
mapped card per phase. No target fraction calibrates RIR, width or scales. All
twenty-eight compositions are computationally available; this does not establish
quantitative accuracy. The frozen twenty-two-case identification holdout is not
used by this assessment.

| Input group | Phase occurrences | Mean absolute error (pp) | Maximum absolute error (pp) |
| --- | ---: | ---: | ---: |
| IUCr CPD 1a–1h | 24 | 13.55 | 39.44 |
| Development binary scans | 40 | 7.70 | 29.75 |
| Combined | 64 | 9.89 | 39.44 |

Errors are actual minus independently specified mass percentage, in percentage
points. The [complete assessment](../../../history/numerical/pfsm-quantification-assessment.json) retains
every expected/actual fraction, signed phase error, card ID, effective RIR, Rwp
and runner/source identifiers. These are observations, not product-generated
test expectations or an accuracy PASS. The
[independent frozen inputs](../../../tests/data/pfsm-quantification/real-inputs.json)
retain full card rows, database/source hashes and mass evidence.

CPD comparison uses the organizer's
[weighed results](https://www.iucr.org/__data/iucr/powder/QARR/results.htm),
explicitly normalizing the published 99.99 totals for 1c and 1e. Binary comparison
uses the original nominal mass tokens in the author sample filenames associated
with the [Dara source study](https://doi.org/10.1021/acs.chemmater.5c02820);
there is no separately retained weighing log. Repeated 2/8-minute scans are
separate observations of the same compositions, not independent mixture recipes.
Library RIR provenance/radiation remain unverified. These substantial errors
qualify current fixed-profile estimates; phase identification success and a
verified joint fit do not certify mass accuracy. The model and tolerances remain
unchanged. The [assessment protocol](../../../history/README.md#pfsm-benchmark-evidence)
describes reproduction; raw fits, CSV and original failed runs remain under
`build/pfsm/quantification-01`.

## Evidence and reproducibility

For redundant-computation investigations, set `QUALPF_PFSM_MEASURE_WORK=1`
when running the standalone formal or candidate-recall probes. They write
actual template preparations, paired/physical/formal sampling, Chebyshev table
construction, observation preprocessing and NNLS calls to stderr. Measurement
is scoped to the calling thread and does not alter scientific output or solver
diagnostics. These counters are work evidence, not independent expectations.
The paired timing driver accepts `--candidate-recall off|shadow|enabled` and
pins `--linear-algebra-cache-sizes` identically in both executables.
Use `--threads 12 --repetitions 1 --no-warmup` for exactly one before/after
execution per case/mode, without either warmup. The report records both counts.
Single-pair elapsed times are descriptive observations, not repeatability,
percentile or statistical-significance evidence.
The initial optimization acceptance requires unchanged scientific output and
solver diagnostics, fewer targeted operations and no reproducible end-to-end
median regression above five percent. A bounded shared-template cache also
requires measured net benefit before retention.

Anchored recall retains the accepted current sample state separately from each
trial. A rejected trial never replaces the current columns or model. The original
gain comparison may still select that trial as the best result; its candidate
column then supplies correlation risk. Fallbacks and review results without a
locally retained winning column use the original sampler. Workspaces are private
to a single candidate call, and initial-to-review results carry no matrices.
Selected phases continue to use their frozen baseline columns and derivatives.
The 32-node Chebyshev transform is initialized once, thread-safely, by a caller
in `FE_TONEAREST`. Other rounding modes construct their own table, so the first
caller cannot change subsequent coefficients. Node expressions and coefficient
summation order remain unchanged.

Built-in candidate recall shares templates through an Application-owned store
whose immutable entries live until the current analysis ends. Each operation
creates a private directory that borrows matching prior entries; new entries
are published only with the successfully checked analysis draft. Its frozen problem and ordered universe
bind the source revision, database, card index and algorithm selection. Core
additionally compares every reference-line identity, position, intensity and
order; coordinate type; ordered radiation components; width; interpolation
window; numerical policy; rounding and denormal mode. Floating fields use binary
equality, without a hash-only hit. Replacement expanders retain their own path.
No scales, shifts, Jacobians or NNLS state enter this store.

The 128 MiB retention budget includes slot metadata and conservatively accounted
template allocations, including owned expansion inputs. Reservation follows the known recalled-index order before
parallel work. A complete template is retained when its bound fits; otherwise
only validated expanded lines and their input identity are retained when possible.
Misses follow the original preparation path without dropping candidates, lines
or precision. Candidate slots have one writer in each joined stage; full templates
and expanded data are immutable. Active worker temporaries and the process runtime
are outside the retention budget. Cancellation, failure and stale publication
discard only the private draft and leave committed entries unchanged. The
derived cache is omitted from QPF and rebuilt on demand after restoration.
Reservation computes the same conservative bound from line counts and frozen
settings, without assembling or retaining a batch of raw d/I inputs. Preparation
builds each working input once; review and formal stages borrow the immutable
prepared input. Budget misses retain the original uncached path.

A separate 128 MiB analysis-local recall store holds a contiguous line table
with original-index offsets. It caches only radiation-expanded, stably sorted
and equal-position-merged references, with deterministic original-index budget
reservations and no removal of uncached cards. Estimated expanded tables below
4 MiB use the zero-retention path to avoid a measured small-pool RSS regression.
This is a storage admission rule, not candidate screening. Frozen problem/universe identity,
numerical policy, rounding and denormal mode bind the data. Accept/remove rounds
reuse these static lines while rebuilding residual correlations, scores and
local results. Formal fitting never consumes this reordered table. The first
scan builds it in private worker slices; only joined successful work can freeze
and publish it. The scorer shares the interpolation location of its three
fields without changing interpolation formulas or channel reduction order.

Benchmark performance records expose `template_prepare_calls`,
`template_cache_hits`, `template_cache_misses`, `template_expanded_hits`,
`template_cache_bytes`, `template_cache_peak_bytes`, `template_evictions`,
`template_budget_fallbacks` and
aggregate worker `template_prepare_seconds`. They do not enter scientific
candidate evidence and cover store lookups, not selected-baseline preparation or
disabled/replacement paths. The admission policy normally evicts nothing; a
mismatch cannot reuse the retained identity. Cancellation checks
remain active on hits, and formal scan/start selection remains independent of
recall parameters.

Built-in formal fitting prepares one owned, immutable copy of the observation
arrays per operation. Core validates and normalizes it in the original order;
fits reuse its normalized net signal, square-root weights and denominators only
with that exact snapshot, numerical policy and floating-point environment.
Mismatched or ordinary inputs retain the validating path. Replacement fitters
retain their original observations. This storage never enters canonical state.

Each reduced problem also retains weighted and unweighted columns at the current
finite-difference anchor. A column is restored only at the identical binary shift;
restoration invalidates its NNLS normalization cache. Capture requires already
available matching columns and never adds an evaluation. New starts, accepted
steps and exact-mode transitions invalidate anchors. Every original NNLS call,
active-set initialization and solver diagnostic remains; duplicate-solve
elimination is not part of this optimization. Probe work records additionally
report `observation_reuses` and `anchor_restores`.

The redundant-computation checks in `build/pfsm-reuse-20261020` cover Windows
GCC, Windows MSVC and Linux GCC. Each passes the affected Core/Application
contracts, 54 independent recall/derivative cases, and 24 SciPy/Brent cases in
each of Direct and Cache mode. Scientific probe output matches each platform's
own baseline exactly. Recall paired sampling decreases from 1,176 to 488 calls,
and physical-column construction from 912 to 434; its 438 NNLS calls remain.
Formal-column sampling decreases from 1,303 to 1,076 (Direct) and from 1,436 to
1,209 (Cache), with the original 937/984 NNLS calls. Observation contracts also
exercise an independently owned snapshot, source mutation, mismatched inputs,
cancellation, rounding and denormal-mode fallback.

The full frozen regression preserves all 80 ready cases and 263 rounds, including
candidate recall evidence, rankings, fitted parameters, solver diagnostics and
curve bytes. The original statuses remain 79 `passed_top5_assisted` and one
`failed_top5` for Bi2Mo3O12. Previously revealed holdouts are regression data.
`final-cohort-recovered/comparison.json` records 69 verified completed executions
and 11 subsequent executions. The first pass stopped before fitting case 70:
checkout CRLF conversion changed one source JSON's digest. A separate 185-file
input tree preserves the exact frozen manifest and restores only that JSON from
the original Git blob matching its pre-existing digest. The working reference,
manifest, original rejection and independent numerical expectations are retained;
`frozen-fixture-recovery.json` binds the recovered bytes.

The full Windows GCC Release build succeeds. Its normal suite passes 121 of 122
registrations in the first invocation; the card-property subprocess reaches its
original 180-second limit while the scientific replay is also running. After
those workloads exit, that registration passes in 118.11 seconds under the same
limit. `final-gnu-normal.xml` and `final-isolated-qualpf_card_properties.xml`
retain these separate scopes, including the original timeout.

Final timing is restricted by the owner to 12 candidate threads and one
before/after execution without warmup. The already completed example1/NIST_660c
groups are not rerun: their first measured pair is retained, and their earlier
warmups and five-pair measurements (three for the full library) remain explicitly
historical. Only the remaining NIST_676a Cache pair is newly executed with
`--repetitions 1 --no-warmup`. The stopped controllers and interrupted narrow
warmup retain actual exit receipts in `once-only-user-stop.json`.

`final-performance-once-summary.json` rechecks complete scientific comparisons,
binary/input bindings, round files and memory. Fixed acceptance identities replay
the same four example1 rounds and two NIST rounds; they are workflow inputs, not
numerical expectations. The following whole-case times exclude native-process
startup and are observations, not significance or percentile estimates.

| Case / mode | Rounds | Before (s) | After (s) | Observed elapsed change |
| --- | ---: | ---: | ---: | ---: |
| example1 Direct, reused pair | 4 | 7.520873 | 4.735534 | -37.03% |
| example1 Cache, reused pair | 4 | 5.857528 | 5.312075 | -9.31% |
| NIST_660c Direct, reused pair | 2 | 6.523482 | 5.476642 | -16.05% |
| NIST_660c Cache, reused pair | 2 | 1.586382 | 1.075680 | -32.19% |
| NIST_660c Cache, 88,312-card full library, reused pair | 2 | 18.727370 | 17.824151 | -4.82% |
| NIST_676a Cache, new pair without warmup | 2 | 692.828748 | 835.727161 | +20.63% |

All six selected pairs preserve scientific output and original diagnostics.
The largest selected retained-template peak is 126.24 MiB, below the unchanged
128 MiB budget. For example1 Cache, process peak rises from 58.80 to 186.06 MiB;
the full-library process peaks are 1,611.26/1,610.53 MiB. These process figures
include runtime, source data and worker temporaries, not just retained templates.

The narrow fallback is an unresolved performance result, not a passing gate.
Its first round improves from 130.448080 to 123.767674 s, but its second rises
from 561.479074 to 711.087445 s (+26.65%). Both variants evaluate the same
589/588 candidates with 647,401/1,182,630 residual evaluations. The new template
store records zero hits and only 51,832 retained metadata bytes in each round.
The second-round aggregate expansion/solve worker elapsed sums rise from
77.721063/6,591.149005 to 174.050645/8,302.415305 s. These overlapping sums
localize the observation but do not attribute it to code or external load.
No causal attribution follows from the available single pair.

At that checkpoint the single narrow observation exceeded the original
five-percent regression screen, and its repeated-median criterion remained
unestablished. Performance acceptance remained open; no reference, assertion or
threshold was weakened. The original comparison and failed descriptive assessment
are retained. Additional timing was then unauthorized. Earlier R4-versus-R3
example1 pairs remain separate evidence of unchanged science and within-five-percent
round medians, not proof that the narrow fallback passed.

The owner subsequently requested diagnosis using existing data/code only, with
no additional timing. `narrow-existing-data-diagnosis.json` binds that read-only
review. Both narrow rounds stop at `Fast grid node limit` before local recall
refinement, so R1 does not execute there. Candidate JSONL preserves 271,216 and
753,448 finite-difference evaluations and 0/2,365,128 NNLS iterations across the
one-/two-phase rounds. The NNLS source is unchanged from R0. The original
executables' inspected NNLS main function has 2,284 matching normalized
instructions apart from the inactive work-counter field offset; its inspected
matrix-vector helper has 668 matching instructions. Relocation normalization
does not establish equal runtime, cache state, callee internals or scheduling.

The pre-repair implementation has one definite redundant path: grid-limit fallback
never reaches template reservation, but each formal call still constructs a
non-retainable prepared wrapper, copies its input identity and creates an
expanded-only handle before discarding retention. This accounts for 1,177 wrapper
paths in the pair. The read-only review identified ordinary uncached fallback as a
bounded repair candidate; it did not modify the product or establish that this
bookkeeping explains the observed 20.63% slowdown.

The two-phase anchor is a separate, unproved cost hypothesis. A successful
weighted/unweighted snapshot copies 1,535,936 bytes at 47,998 points, versus
767,968 bytes for one phase. Stored finite-difference counts imply substantially
more potential copy exposure in round two, but the run does not record successful
captures, actual bandwidth, per-candidate CPU samples or machine-load/core traces.
No unique cause follows from these observations. No case-name/width-specific
bypass, threshold waiver or unverified performance claim is introduced.

The owner subsequently authorized a bounded repair and a new timing comparison.
Slots with no retention reservation now record a miss and return no prepared
handle, so their caller uses the original expander rather than constructing and
discarding cache wrappers/identity copies. Cancellation is checked before this
fallback, and the original expander retains input validation. The same rule covers
grid-limit fallback before reservation and a positive budget consumed by slot
metadata; reserved complete/lightweight entries retain their existing behavior.
`template_prepare_calls` counts store-initiated prepared constructions, not ordinary
expansions after this fallback: zero does not imply that physical templates are
unnecessary or that fitting was skipped. The preceding scientific/performance
evidence retains its original version scope and failed narrow result. A new
comparison remains 12-thread, one before/after pair per configuration, without
warmup; the repair alone is not a claim that the full elapsed regression is solved.

The unreserved-slot contract fails on the preceding implementation before the
repair (`repair-gnu-red-tests.xml`, preserved source/binary bindings), then passes
with both Direct/Cache fallbacks, metadata-only budgets, original validation and
pre-miss cancellation. The affected Core/Application suites pass 2/2 on Windows
GCC, Windows MSVC and Linux GCC in independent repair builds. The Windows GCC
headless CLI contract passes at its original limits in the native workspace-temp
environment; its initial sandbox run's `No report reader was exercised` failure
remains separately preserved. Source/configured boundaries and all 169 original
reference hashes pass. The earlier full-cohort and normal-suite evidence is not
relabeled as a new post-repair full run.

The authorized repair comparison is complete. `repair-verification.json` binds
the causal tests, independent builds and repair revision
`45c417293b46343eda33d02921f0122e12e50507`.
`repair-performance-assessment.json` verifies six fresh R0-versus-repair pairs,
16 round pairs and all scientific file/diagnostic comparisons. All 12 native
executions and three drivers exited zero, as did managed controller session 91709.
Each configuration ran exactly once per variant at 12 threads, without warmup;
there is no cold operating-system/database-cache guarantee.

| Case / mode | Before (s) | Repair (s) | Observed elapsed change |
| --- | ---: | ---: | ---: |
| example1 Direct | 26.716744 | 18.300834 | -31.50% |
| example1 Cache | 26.721519 | 24.740648 | -7.41% |
| NIST_660c Direct | 28.618163 | 25.865156 | -9.62% |
| NIST_660c Cache | 6.443026 | 3.937035 | -38.89% |
| NIST_660c Cache, 88,312-card full library | 56.598689 | 54.558234 | -3.61% |
| NIST_676a Cache | 1,521.449531 | 1,388.951857 | -8.71% |

Every round, whole-case time and native-process time satisfies the unchanged
five-percent descriptive regression screen in this new run. Narrow rounds change
391.057978 to 313.062830 s and 1,128.370650 to 1,073.712053 s. Their store records
zero prepared constructions and preparation time, with unchanged 589/588 misses
and candidate evaluations, no hits and 51,832 metadata bytes. The 1,177 discarded
wrapper paths are removed; physical expansion and all original fitting work remain.

The largest repaired retained-template peak is 126.24 MiB, below 128 MiB.
Full-library process peaks are 1,610.25/1,610.65 MiB; narrow process peaks are
166.30/177.41 MiB. Process memory includes inputs and worker temporaries, not just
template retention. The controller records 625 passive host/process samples;
sampled cumulative CPU ranges and RSS can omit tails or peaks and establish no
per-core scheduling, power or unique causal explanation.

This completes the owner's revised single-pair verification scope, not a repeated
median, stable percentile or statistical-significance claim. Absolute times also
change substantially for the immutable R0 binary between runs. This is not an
isolated R4-versus-repair experiment, cannot attribute the entire earlier 20.63%
regression to wrappers, and does not erase that failed observation or establish
the original repeated-median criterion. No extra pair or warmup is run.

The owner then authorized the remaining 79 cases against the previously measured
baseline. `build/pfsm-reuse-20261020/repair-remaining-79` contains that extension's
preflight, immutable original-receipt bindings, incremental comparisons, logs and
full `comparison.tsv` table. Only the unchanged repair executable is run: one new
12-thread Cache/enabled-v2 execution per case, with no warmup, repeat, baseline
execution or NIST_676a replay. The byte-identical restored manifest and original
database/runtime snapshot retain their verified hashes.

Final `summary.json` audits 79 cases/261 rounds, 1,621 bound baseline files and
862 byte-identical scientific files, including all candidate ordering, formal
ranking/diagnostics, recall files and baseline curves. Managed session 61456 and
the controller actually exit zero; all 79 native waits complete with the original
78 zero exits and one exit 2. The original Bi2Mo3O12 `failed_top5` remains failed;
the other 78 statuses remain `passed_top5_assisted`. This is exact replay
conformance, not a claim that all 79 scientific acceptance cases pass.

The baseline is the October 18 cohort at
`4469e12b596dc7406290ac5063ce993796f83951`, not a new same-session R0 measurement.
All 79 original receipts explicitly record `controlled_timing: false`.
The following are cross-session single observations, not repeated medians,
stable percentiles, statistical significance or isolated repair effects:

| Metric | Historical baseline | Repair replay |
| --- | ---: | ---: |
| Sum of whole-case elapsed, excluding native startup (s) | 1,675.575796 | 1,066.246991 |
| Sum of native-process elapsed (s) | 2,039.103494 | 1,411.684048 |
| Maximum retained-template memory (MiB) | Not the same store diagnostic | 126.59 |
| Maximum process peak over the cohort (MiB) | 251.54 | 328.60 |

Whole-case elapsed sums decrease 36.37%; 70 cases are lower and nine are higher.
Six whole cases and 24 individual rounds exceed the original five-percent
descriptive screen. Aggregate improvement does not erase those observations or
establish an overall controlled performance PASS. All six higher whole-case
observations exceeding five percent are retained:

| Case | Baseline (s) | Repair (s) | Observed elapsed change |
| --- | ---: | ---: | ---: |
| example1 | 5.952091 | 24.956825 | +319.30% |
| cpd-1a | 16.991981 | 71.872850 | +322.98% |
| cpd-1b | 10.510659 | 42.785847 | +307.07% |
| cpd-1c | 17.527485 | 62.908241 | +258.91% |
| cpd-1d | 97.436010 | 174.219045 | +78.80% |
| Dara_binary_In2O3_0p5002-MnCO3_0p5000--8min | 6.973055 | 7.461317 | +7.00% |

Template retention remains below the unchanged 128 MiB budget; process peaks
also include input data, numerical workspaces and worker temporaries. The
historical baseline, the earlier failed narrow comparison and every new increase
remain preserved. Cross-session host/cache conditions prevent attributing these
elapsed changes uniquely to code; no baseline remeasurement or extra repetition
is introduced to remove an unfavorable observation. This extends fresh repair
replay coverage without relabeling earlier unit, independent-reference or full
normal-suite results as newly executed tests.

The owner subsequently authorized a fresh before/after execution for each of the
nine higher-elapsed cases. `build/pfsm-reuse-20261020/repair-slower-9-once` records
the preflight, four mock-only orchestration controls, raw runs, `comparison.tsv`
and final `summary.json`. The nine identities remain selected from the hash-bound
historical table, irrespective of the new outcome. The original baseline
`4469e12b` and repair `45c41729` execute adjacently for each case using the same
restored full-workflow inputs, runtime and fixed Eigen hints. There are exactly
18 native executions, 12 candidate threads, Cache/enabled-v2, one pair per case
and no warmup. No product source or build changes accompany the measurement.

Managed session 63157, its controller and all 18 native processes actually exit
zero. All nine cases/35 paired rounds preserve complete scientific observations
against each other and the original runs; 114 scientific files per pair are
byte-identical. Every original observation, including the prior large elapsed
increases, remains unchanged. Fresh whole-case times exclude native startup:

| Case | Fresh baseline (s) | Fresh repair (s) | Observed elapsed change |
| --- | ---: | ---: | ---: |
| example1 | 5.873177 | 5.442764 | -7.33% |
| cpd-1a | 18.634589 | 20.098833 | +7.86% |
| cpd-1b | 10.924607 | 11.520063 | +5.45% |
| cpd-1c | 71.454298 | 47.942132 | -32.91% |
| cpd-1d | 100.179830 | 67.761832 | -32.36% |
| Dara_binary_In2O3_0p5002-MnCO3_0p5000--8min | 7.032430 | 7.439923 | +5.79% |
| Dara_ternary_In2O3_0p3022-La_OH_3_0p5014-TiO2_0p2004--8min | 17.576405 | 9.907642 | -43.63% |
| Dara_ternary_NiO_0p1031-La_OH_3_0p2025-TiO2_0p7014--8min | 13.131019 | 12.946929 | -1.40% |
| Dara_ternary_NiO_0p2030-Li2CO3_0p2999-TiO2_0p5025--8min | 8.936245 | 8.609941 | -3.65% |

Six whole cases are lower and three are higher. Whole-case sums change from
253.742601 to 191.670058 s (-24.46%); native-process sums are
294.668132/232.424919 s. These sums do not establish an overall performance PASS.
Three cases, six individual rounds and one process exceed the unchanged
five-percent descriptive screen, leaving four case groups outside the combined
round/case/process screen. In particular, the In2O3/La(OH)3/TiO2 ternary case has
lower total elapsed but its zero-based round 3 rises from 4.546176 to 4.784189 s
(+5.24%); this failure is not hidden by its aggregate improvement. The other
failed groups are cpd-1a, cpd-1b and the In2O3/MnCO3 binary case.

The retained-template peak is 126.59 MiB, below 128 MiB; cohort maximum process
peaks are 94.26/217.54 MiB, including input data and worker temporaries. This
completes the specifically authorized remeasurement, not a claim that every
performance screen passes. Single adjacent pairs do not establish repeatability,
stable percentiles, statistical significance or a unique cause of earlier
cross-session differences; they also do not isolate R4 versus the wrapper repair.
No threshold is weakened and no extra pair or warmup is run.

`tests/unit/core/pfsm_test.cpp` uses mathematical IL expressions, a closed-form
integral, analytic NNLS solutions, independent prescribed mixtures and invariance
properties. Exact binary mean/contrast perturbations exercise both sides of the
numerical-rank threshold. A negative-tail single-phase problem independently
establishes zero and positive analytic projections at different shifts, verifying
that a later start can recover a scale that was initially inactive. These added
tests pass on Windows GCC/MSVC and Linux GCC without solver or tolerance changes;
the scoped Linux input delta is retained in `build/pfsm/linux-reentry-01/test-scope.json`.
The Qt-free `tests/architecture` build also provides `pfsm_probe`, a
text-only test transport. `tools/validation/pfsm_scipy_reference.py` compares that
transport against pinned SciPy 1.16.2 NNLS/TRF and independent dense-scan/Brent
single-phase solutions. It specifies cases and tolerances before running the
product, preserves each result immediately, and never generates expectations from
product output. SciPy is an optional validation dependency, not a product dependency.

The relative-coordinate cache repair retains the original narrow-width failure:
at a 30-degree center with H=1e-11, the old cached fit returned Q about 0.01686
on a prescribed noiseless profile. The repaired fit reaches about 1.18e-20.
The new bounded regression fails on the original implementation and checks
independent values, shift differences, fitted recovery, far tails, cancellation
and finite observations around an overflowing unobserved peak. Independent
100-digit calculations cover 1,170 samples, with maximum repaired relative error
2.29e-13. Review and original intermediate failures remain in
`build/pfsm/relative-cache-repair-02` and
`build/pfsm/relative-cache-independent-review-01`. The stronger exploratory
endpoint requirement of 2e-5 H still fails in 24 free-bound fits in each of the
direct and repaired modes; their original physical-shift, scale and Q checks
pass. That diagnostic failure is retained rather than treated as a cache fix.

The applied correction passes actual Windows GCC, Windows MSVC and Linux GCC
Qt-free suites (7/7 each), affected PFSM/strict product checks (5/5 each), and
the unchanged cached SciPy 1.16.2 comparison (24/24 each). Linux's scoped seal
is `build/pfsm/relative-cache-linux-product-01/batch-summary.json`; only the
expansion and its bounded regression changed among recorded scientific inputs.
These affected checks are separate from the earlier full normal checkpoints.

Independent sensitivity checks exercise the actual Application baseline, scan,
multistart and joint-fit path on four prescribed IL problems. Direct scan steps
H/4, H/8 and H/16, followed by repaired cache cell widths H/2, H/4 and H/8,
pass all 144 Q comparisons and 24 complete ranking comparisons against separately
sealed SciPy references. Eighteen raw parameter comparisons exchange two identical
template labels; their original failures and same-tolerance permutation checks
remain explicit. A separate exact affine active-set example retains 18/24 failed
single-start global-minimum assertions; all three frozen multistart runs recover
its analytic minimum. These are bounded sensitivity results, not global convergence
proofs or replacement real-data acceptance (`build/pfsm/sensitivity-run-01`).

```sh
python tools/validation/pfsm_scipy_reference.py --probe build/pfsm/core/pfsm_probe \
  --output build/pfsm/scipy-comparison.json
```

Add `--template-cache` to exercise the same independently defined cases and
tolerances through the optional approximation and direct final verification.

The retained Windows GCC checks are supplemented by current MSVC 14.40 and
Linux GCC 15.2 comparisons: each platform passes all 24 unchanged cases in both
direct and cached modes. The MSVC transport links the tested product Core library;
the Linux transport is the compiled Qt-free probe in the isolated SDK. A Windows
stdin/stdout bridge runs the Linux binary without performing any numerical work.
`build/pfsm/scipy-final-provenance-01.json` binds reports to the actual probes,
Core libraries, source files and SciPy 1.16.2. These numerical checks do not replace
the separately tracked full normal suite or real-library identification acceptance.

The reference algorithm is [SciPy 1.16.2 TRF](https://github.com/scipy/scipy/blob/v1.16.2/scipy/optimize/_lsq/trf.py).
[O'Leary and Rust](https://www.cs.umd.edu/~oleary/software/varpro/varpro.pdf)
describe variable projection, including both Jacobian terms; the present reduced
finite differences avoid dropping either term. The [Match! official description](https://crystalimpact.com/match/help/idh_searchmatch_PFSM_details.htm)
supports fixed background, scale/shift refinement and temporary joint candidate
fits; it does not establish that Match! uses this NNLS/TRF implementation.
