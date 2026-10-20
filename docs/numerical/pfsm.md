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
diagnostic, described in the [benchmark contract](pfsm-benchmark.md). It changes
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
remain in the benchmark contract.

## Candidate recall versus formal fitting

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

Only the v2 candidate-recall implementation is executable in the current product;
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

The [complete v2 replay and enabled assessment](pfsm-benchmark.md#completed-v2-replay-cohort-2026-10-14)
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
More than 131,072 grid nodes or 4,096 scan points requires a full-search fallback.

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
[benchmark evidence](pfsm-benchmark.md#experimental-fast-pfsm-modes) preserves
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
component order defines the direct sum order. The default uses no interpolation,
tail cutoff or uniform grid assumption. Lines outside the experimental window retain their
tails. The derivative returned by a template is with respect to the phase shift.

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
cache, justified by the retained full-library and 32-card profiling. The default
remains direct IL. Each interval is at most H/2 wide and represents the **complete**
IL sum with 32 Chebyshev coefficients computed from direct values. No reflection
or tail is removed. A maximum of 16,384 intervals bounds coefficient storage at
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
This opt-in computational approximation is an explicit extension to the first
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
points. The [complete assessment](pfsm-quantification-assessment.json) retains
every expected/actual fraction, signed phase error, card ID, effective RIR, Rwp
and runner/source identifiers. These are observations, not product-generated
test expectations or an accuracy PASS. The
[independent frozen inputs](../../tests/data/pfsm-quantification/real-inputs.json)
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
unchanged. The [assessment protocol](pfsm-benchmark.md#conditional-pfsmrir-quantification-assessment)
describes reproduction; raw fits, CSV and original failed runs remain under
`build/pfsm/quantification-01`.

## Evidence and reproducibility

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
