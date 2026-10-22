# PFSM headless acceptance

`qualpf_pfsm_benchmark` composes the product Application `SearchEngine`,
`SearchCoordinator` and canonical Session with execution-thread read-only SQL.
The normal `qualpf.cli.pfsm_benchmark` test uses an independent analytic mixture
and a temporary SQLite fixture. Real COD data are not a normal-test dependency.

## Current candidate-recall entry points

`--search-diagnostics` enables runtime-only read and candidate-work diagnostics.
It is off by default and must be omitted from controlled speed comparisons.
`database_read_diagnostics` separates the row-count query, prepare/exec, cursor
fetch, metadata conversion, paired-peak decoding and materialization. Cursor fetch
includes SQLite and driver work; exec alone is not database execution time.
The row-query count excludes restraint planning, which remains included in the
outer `database_seconds`. Counts include decoded cards/peaks, peak-text bytes and
observed peak-buffer growths. Failed/cancelled reads remain failed transactions.

`search_diagnostics` separately reports correlation, initial and review worker
work: input construction, reflection expansion/sorting, correlation, template
preparation, physical sampling and NNLS. Worker sums overlap wall-clock stages;
template and sampling timers can contain work also counted by lower-level timers.
Buffer counts describe explicit monitored sites, not all heap allocations.
Diagnostics and timers never affect the scientific payload or persist in QPF.

The v2 scan retains recall-only expanded/stably sorted/merged lines in a separate
128 MiB analysis-local store. Original-index reservations include flat line and
slot storage, and all budget misses follow the uncached full-scan path. Identity,
numerical policy, rounding and denormal controls bind the store to the frozen
problem and universe. It is frozen after joined workers, committed only with the
successful analysis, and never serialized or passed to formal fitting. Accepting
or removing phases reuses static lines while rescoring the current residual.
Runtime `recall_cache_*` fields report hits, misses, budget fallbacks and bytes.
First-round allocation/build time is included in operation latency. Libraries
whose estimated expanded line table is smaller than 4 MiB retain no recall bank: the
long-spectrum controls exposed a small RSS regression for millisecond-scale scans.
This deterministic admission rule changes only retention; every card still follows
the original full scoring path. The 128 MiB cap applies to admitted banks.

The existing 128 MiB template store now survives until analysis end through
immutable entries and a private directory per operation. A matching frozen
problem/universe permits borrowing; complete expansion-input and floating-mode
checks still guard preparation. Reservations use line counts and frozen settings
without constructing or retaining unprepared line buffers. Preparation builds
one input per working candidate; review and formal stages borrow the prepared
template's owned input. Retained inputs and templates share the budget.
Unreserved candidates use the existing path, counted as
`template_budget_fallbacks`. Failed/cancelled drafts cannot change the committed
directory. Prepared input ownership also avoids rebuilding the same d/I input in
initial evaluation, review and formal fitting. NNLS remains cold at every solve.
The recall scorer computes a shared grid location for its three fields while
retaining the original interpolation expressions and channel summation order.

Strict PFSM paired reads use a forward-only SQL cursor and streaming QStringView
tokens with the same Qt UTF-8/numeric conversion. Capacity is bounded by actual
text, not declared peak counts. Classical decoding, row order, metadata/RIR and
malformed-record errors retain their contracts. The optional
`qualpf_candidate_reader_tests --full-library <database.sq>` audit compares every
ordered d/I binary64 value with the retained preoptimization Qt decoder; it is
not a normal-test dependency and writes no reference data.

The maintained benchmark uses `--pfsm-candidate-recall off|shadow|enabled`
and `--pfsm-candidate-recall-version v2`. The default is off with implementation
version v2; new GUI analyses separately default to enabled. `--fast-pfsm` and
`--fast-pfsm-version` are compatibility aliases. Matching old/new values are
accepted together; conflicting values and explicit v1 requests fail before
execution. `--full-round` still forces a complete-pool run for that round.
`--pfsm-resource-fallback bounded_integral|full_pool` selects resource-limit
behavior (default `bounded_integral`), while ordinary widths retain v2. The
integral backend is exactly the `3fa1123a` baseline, excluding A and A+B.

Each attempt names its actual `backend`, `switch_reason` and separate
`full_fallback`. Integral reports contain fixed budgets, bins, bank/cache sizes,
evaluation counts, timings, matching no-candidate controls and channel flags.
The exact formal comparator retains Top-5, survivor evidence, normalization and
baseline checks, and independently reconstructs the integral union. Frozen
physical-state replay also routes resource limits to the baseline integral
backend, with zero formal fits; v2 ablation stages apply only to v2 rounds.

Round reports expose `candidate_recall`, and performance fields use
`candidate_recall_*_seconds`. Run metadata uses `candidate_recall_mode` and
`candidate_recall_version`. Legacy fields are serialized from the same values.
The implementation version is separate from the existing report schema version;
schema identifiers and evidence names such as `fast-N.jsonl` remain unchanged.
Current readers accept historical fields alone and reject inconsistent aliases
when both forms exist. Local recall fits and gains remain diagnostic; ordinary
ranking files contain only checked formal fits and their fixed PFSM FOM.

Current tools use `pfsm_candidate_recall_compare.py`,
`pfsm_candidate_recall_validation.py`, `pfsm_candidate_recall_reference.py`,
`pfsm_candidate_recall_replay_inputs.py` and
`pfsm_candidate_recall_replay_compare.py`. Their prior script names remain thin
compatibility entry points. Current replay/probe targets are
`qualpf_pfsm_candidate_recall_replay` and `qualpf_pfsm_candidate_recall_probe`;
the standalone Qt-free probe is `pfsm_candidate_recall_probe`. The validation
launcher recognizes the old replay filename in frozen runtimes. The preserved
experiments below retain their original names, commands, versions and conclusions;
v1 execution requires its original Git snapshot.

## 2026-10-21 long-spectrum peak-RSS correction

The lifetime-only follow-up at `49cc8c7d` passes the unchanged controlled
performance gates, including both long-spectrum RSS bounds. The earlier failed
checkpoint below and all its raw comparisons remain preserved.

The initial/review peak came from overlapping temporary storage in anchored
steps. The copied dictionary now ends before NNLS workspace allocation, and the
linear matrix ends after prediction/shift recovery, before physical-trial
sampling. All arithmetic expressions, column order, cold NNLS calls, candidate
sets, protections, fallback rules and solver settings remain unchanged. No
observation reduction, scheduling change or cache-budget increase is involved.

Fresh evidence lives in `build/pfsm-rss-20261021/`: `acceptance-binding.json`,
`baseline-reverification.json`, `long-cache/comparison.json`,
`main-full/comparison.json`, `thread-modes/comparison.json`, `final-audit.json`
and `final-verification.json`. The original `551bbb18660d74a1a86ec07bdd8c9a76964bf1e4`
runtime, database/sidecars and input bindings were reverified. This uses the
same Windows GCC Release configuration, v2 enabled, 12 workers, cached templates
and algebra-cache settings as the preceding protocol: one warmup per variant,
five alternating fresh-process pairs per long spectrum and three per primary
full-library case. High-frequency diagnostics are off, and no task build/test
overlaps timing. OS caches are not controlled. Long spectra retain their original
chemistry constraints (461/468 cards); both primary cases scan all 88,312 cards
without chemistry, with the same frozen acceptances.

Values are operation medians [minimum, maximum] in seconds and maximum process
peak through each round in MiB, using the same latency/RSS definitions below.

| Case | Round | Before seconds | After seconds | Reduction | Peak RSS MiB, before / after |
| --- | ---: | ---: | ---: | ---: | ---: |
| Fe3Sb2O4Br4 | 0 | 2.970 [2.870, 2.974] | 2.903 [2.863, 2.943] | 2.26% | 231.316 / 220.457 |
| Sr3Fe2Cl2O5 | 0 | 0.792 [0.784, 0.802] | 0.783 [0.781, 0.788] | 1.20% | 166.902 / 164.812 |
| NIST_660c | 0 | 10.580 [10.553, 10.637] | 6.882 [6.840, 6.894] | 34.96% | 1612.152 / 836.055 |
| NIST_660c | 1 | 4.935 [4.927, 5.041] | 3.200 [3.172, 3.241] | 35.15% | 1612.152 / 886.441 |
| example1 | 0 | 7.841 [7.836, 7.926] | 4.752 [4.736, 4.799] | 39.40% | 1611.574 / 823.234 |
| example1 | 1 | 2.611 [2.601, 2.737] | 1.378 [1.365, 1.379] | 47.22% | 1611.574 / 870.355 |
| example1 | 2 | 3.125 [3.019, 3.152] | 1.516 [1.473, 1.542] | 51.48% | 1611.574 / 879.914 |
| example1 | 3 | 3.527 [3.358, 3.886] | 1.836 [1.825, 1.915] | 47.94% | 1611.574 / 887.656 |

Both long-spectrum peaks are also below the **previously recorded** baseline
bounds of 240,431,104 and 174,899,200 bytes, independently of variation in the
fresh control measurements. Every controlled round meets the unchanged 5%
maximum median-regression and strict RSS gates. Both full-library first rounds
exceed the 25% reduction target, and all subsequent rounds exceed 20%. The
2–5 second exploratory target remains unmet for NIST_660c's first operation.

All 24 A/B pairs preserve the complete scientific payload, including IDs/order,
recall scores/seeds, initial/review decisions, protections/fallbacks, NNLS counts,
formal candidate sets, full rankings, FOM, parameters and baseline curves.
The direct/cache matrix at 1/4/12/20 threads additionally preserves exact
within-mode scientific bytes; its single pairs are functional evidence only.
Their raw timing/RSS observations, including RSS increases in some configurations,
remain recorded and do not establish performance qualification for that matrix.
Subsequent rounds keep the original universe and zero full-library read time.
Output-only probes freshly compare 416 complete published calculated/residual
curve pairs and FOMs across 16 rounds in both modes. `curve-rss-audit.json` binds
the probe and unchanged ordinary executable to the preserved baseline outputs;
these probes provide neither timings nor replacement scientific references.

Fresh affected checks pass on Windows GCC (14 tests), Windows MSVC (13) and
Linux GCC (13), including Core/Application, CLI, QPF, settings, GUI, threaded
readers and configured architecture boundaries. The independent SciPy 1.16.2
reference passes 54/54; all 169 normal references remain byte-preserved and
source boundaries pass for 2,007 inputs. This small lifetime correction receives
affected delta checks, not a new full normal or hosted-CI run. The earlier normal
checkpoint retains its qualified scope below. The initial misspelled GNU build
target and the corrected build log are both retained; no assertions or timeouts
were changed.

## 2026-10-21 full-library engineering optimization (prior failed RSS checkpoint)

Implementation and equivalence checks are complete; **overall performance
qualification is not passed** because the two long-spectrum controls exceed the
strict peak-RSS bound. The full-library first-round and subsequent-round targets
pass. No tolerance, card pool, recall selection or scientific reference was changed
to make a gate pass.

The controlled comparison binds original revision `551bbb18660d74a1a86ec07bdd8c9a76964bf1e4`
to implementation `da2e40c7`, including the subsequent bounded storage corrections.
Raw evidence is under `build/full-library-optimization-20261021/`:
`baseline-binding.json`, `acceptance-extent-binding.json`,
`baseline-reverification.json`, and the `*-extent/comparison.json` groups.
The original frozen executable/runtime, database and its sidecars, manifest and
build configuration were rehashed without changes. The database snapshot contains
88,312 cards and 27,367,965 ordered d/I pairs; these are snapshot facts, not product
constants. Read-only query plans retain the original row scan and count-index
scan. No schema or index was modified.

Measurements use Windows x64 GCC 13.1, Qt 6.8.3, Release, v2 enabled, cached
templates, 12 candidate workers and fixed algebra-cache sizes
`49152,1310720,25165824`. Both primary cases explicitly disable chemistry;
example1 accepts `2104748`, `2104736`, then `1000032`. Each variant has one
warmup, followed by three alternating A/B pairs per full-library case and five
pairs per long spectrum. Every execution is a fresh process. No build or test
from this task overlaps these controlled runs. The warmup does not imply an
operating-system cold cache or reuse a previous process's card/cache objects.
High-frequency diagnostics are disabled.

Times below are operation-to-result medians and [minimum, maximum] in seconds;
headless report serialization and native GUI painting are excluded from latency.
RSS is the maximum process peak through that round, in MiB, before / after; it
is not incremental stage memory. The long spectra keep their original chemistry
constraints (461 and 468 candidates).

| Case | Round | Before seconds | After seconds | Reduction | Peak RSS MiB, before / after |
| --- | ---: | ---: | ---: | ---: | ---: |
| NIST_660c | 0 | 10.752 [10.692, 10.821] | 7.045 [6.955, 7.058] | 34.48% | 1612.105 / 836.562 |
| NIST_660c | 1 | 5.077 [5.029, 5.091] | 3.243 [3.225, 3.252] | 36.12% | 1612.105 / 888.113 |
| example1 | 0 | 7.867 [7.823, 8.012] | 4.730 [4.729, 4.889] | 39.88% | 1610.703 / 824.129 |
| example1 | 1 | 2.589 [2.583, 2.621] | 1.376 [1.374, 1.390] | 46.85% | 1610.703 / 871.176 |
| example1 | 2 | 2.745 [2.701, 2.990] | 1.539 [1.533, 1.546] | 43.92% | 1610.703 / 880.707 |
| example1 | 3 | 3.311 [3.106, 3.352] | 1.847 [1.838, 1.849] | 44.21% | 1610.703 / 890.898 |
| Fe3Sb2O4Br4 | 0 | 2.905 [2.893, 2.930] | 2.921 [2.905, 2.971] | -0.54% | 229.293 / 230.434 |
| Sr3Fe2Cl2O5 | 0 | 0.805 [0.796, 0.810] | 0.779 [0.775, 0.780] | 3.27% | 166.797 / 166.820 |

Both full-library first rounds exceed the 25% reduction target, and all four
subsequent rounds exceed 20%. Their RSS bounds pass. No controlled round exceeds
5% median slowdown: Fe3Sb2O4Br4 slows by 0.54%, while Sr3Fe2Cl2O5 improves by
3.27%. However, their peak RSS increases by **1,196,032 bytes** and **24,576
bytes**, respectively, so the strict RSS gate fails. NIST_660c still takes about
7.04 seconds on its first operation; the 2–5 second exploratory goal is not met
across the primary cases.

Original failed controls are preserved under `long-cache/` and
`long-cache-admission/`. Their failure led to zero recall retention below 4 MiB
and extent-only template reservation. Final results above use `long-cache-extent/`;
they do not replace those earlier comparisons. Output-only phase probes under
`memory-before/` and `memory-after/` locate the Fe peak in initial/review work,
before formal fitting and report serialization. They are descriptive single runs
with separate instrumented executables and cannot clear or replace the controlled
RSS failure. Further peak reduction remains unqualified; no scheduling or numerical
change was introduced to chase this small difference.

All 24 measured A/B pairs preserve complete scientific output: candidate count,
ID/order, correlation/seed evidence, initial/review selections, protection and
fallback reasons, formal sets, complete rankings, parameters, baseline diagnostics
and curves. The additional chemistry-constrained example1 matrix (849 cards)
uses direct/cache at 1/4/12/20 threads, one pair each without warmup. It verifies
exact A/B and within-mode thread identity, **not statistical speed qualification**.
All subsequent rounds retain the original universe and record zero full-library
read time. `extent-final-audit.json` records each check separately.

Output-only curve probes additionally compare **416 complete published candidate
calculated/residual curve pairs and FOMs across 16 rounds**, covering both primary
full-library cases and both long spectra in direct/cache modes. All are exact.
`curve-extent-audit.json` binds the final probe and unchanged ordinary executable;
unchanged baseline outputs retain their original executable, manifest and command
bindings. These probes are never used for speed claims or scientific goldens.

Instrumented causal counts (`work-count-extent-audit.json`) show peak-buffer
growths dropping from 1,587,314 to 176,622. The second NIST round hits 7,784
recall entries and still scans all 80,527 uncached remaining cards. Initial
template preparations fall from 500 to 381; the 1,500 cold NNLS solves and all
initial/review physical-sampling counts are unchanged. The 128 MiB recall cap and
existing 128 MiB template cap remain independent. Instrumented worker sums are
not used as controlled stage wall times.

Correctness scope is explicit:

- The strict full-database decoder audit preserves every ordered ID and binary64
  d/I pair against the retained preoptimization Qt parser. Edge fixtures cover
  zero/duplicate peaks, dual radiation, malformed values and counts, empty tokens,
  cancellation and source changes.
- Final storage corrections pass 14 affected Windows GCC tests and 13 each on
  Windows MSVC (Qt 6.8.3) and Linux GCC (Qt 6.10.2), including Core/Application,
  QPF, CLI, GUI, settings, threaded readers and architecture boundaries. Existing
  lifecycle coverage includes accept/remove, baseline/resource fallback, stale
  publication and close/drain. No hosted or full Linux/MSVC suite PASS is implied.
- The primary normal regression at implementation checkpoint `15836c96` passes
  120/122 tests in the sandbox. The UUID-owned registry migration fixture and
  localhost update transport test both pass unchanged in their native rerun.
  Both original failures remain in `gnu-normal.xml/.log`; the two passes are in
  `gnu-normal-native.xml/.log`. The later storage-only corrections receive the
  affected delta checks above, not another full normal run.
- The final fresh independent SciPy 1.16.2 numerical reference passes 54/54;
  all 169 preserved normal references remain byte-identical. Boundary checks
  cover 2,007 bound inputs and the configured strict target gate. No product
  output was promoted to a reference.

## Baseline integral resource-fallback verification

The 2026-10-21 port uses Core baseline `3fa1123a` and receiving product revision
`e10dde62`; A (`1250a174`) and A+B (`8c149f61`) are excluded. Source correspondence
is recorded in the [numerical contract](pfsm.md). Evidence is under
`build/pfsm-integral-fallback-20261021/`; frozen input/reference files are unchanged.

The focused NIST check covers both historical grid-limit rounds in each of
`NIST_674b_CeO2` and `NIST_676a`. All four switch successfully to integral recall.
CeO2's unions contain its entire 35/34-card pools; NIST_676a's unions contain
64/64 of 589/588 cards. Successful full-pool unions are not preparation failures.
Exact comparisons retain full-pool Top-5, every retained formal row, baseline
curves and normalization. Separate frozen physical-state replay reproduces the
unions with zero formal fits (`focused/comparison.json`). These checks were
allowed to overlap builds and establish correctness, not controlled performance.

The independent Dara replay compares all 837 candidate numerical rows against
the frozen baseline: numbers, shifts, scales, KKT evidence, evaluation counts and
selection match exactly. COD `7218520` is in the relaxed Top-32; its matched
no-candidate and candidate searches use the same two old coordinates. The maximum
is 165 NNLS evaluations per card. One metadata difference is retained explicitly:
old-bank ownership accounting is 7,229,552 bytes rather than the older standalone
diagnostic's 7,229,440 bytes (two bank objects and two old-shift doubles). The
initial all-fields comparison failure remains beside the qualified assessment in
`dara-baseline-replay/`; no numerical tolerance or frozen evidence was changed.

Windows GCC, Windows MSVC and Linux GCC affected Core/Application, QPF/protocol,
settings and native GUI checks pass. The integral baseline-rebuild replay and
historical-QPF load-then-rescore controls are included. Original independent
high-precision/SciPy integral checks pass 24/24 on both Windows compilers; original
v2 checks pass 54/54. Strict architecture and its negative controls pass. Logs
retain initial build/assertion failures and the sandbox temporary-directory access
failure; the QPF test passes with a workspace-owned temporary directory and
unchanged assertions.

The single full replay completes all 80 ready cases and 263 rounds. Against
hash-bound full-pool evidence, Top-5, retained formal rows, baseline curves and
normalization pass. All 259 ordinary-width v2 rounds also retain byte-identical
scientific files against the receiving branch. Four rounds use the integral
backend; the five other full-pool fallbacks remain four formal-underestimation
supplements and one v2 review-limit fallback. Identification remains 79/80 with
the original Bi2Mo3O12 Top-5 failure; all nine nonready classifications remain.
`cohort-audit.json` binds the outputs. The original cohort driver exits 1 because
it incorrectly expected exit 1 for `failed_top5`; both the frozen historical and
current native processes actually return 2. That original failure and the initial
audit field-name error remain preserved. Read-only correction verifies the
original exit and scientific evidence without rerunning any case.

Windows GCC full normal regression passes 122/122 with original timeouts
(`gnu-normal.xml`, actual terminal exit 0); this is separate from the three-platform
affected checks and the independent numerical comparisons above.

## Acceptance scope

Primary acceptance uses independently documented chemical-composition constraints.
Unconstrained full-library runs are supplemental and are reported separately.
This supersedes the original unconstrained-primary evaluation arrangement;
preserved earlier runs retain their original inputs and interpretation. Rwp
improvement measures spectral agreement and does not establish chemical plausibility.

## Performance-only measurements

### Baseline integral fallback: adjacent NIST_676a pair (2026-10-21)

The authorized single pair ran from 19:31:56 to 19:44:02 Asia/Shanghai after
all builds, tests and the correctness cohort exited. Both native processes and
the owning driver exit 0. Windows GCC Release, runtime, database, frozen inputs,
twelve threads, cached IL configuration and explicit algebra cache sizes are
shared; there are no warmups or repeated measurements. The predecessor binary
SHA-256 starts `47306481`, and the integral product binary starts `7a0cdb9c`.
`timing-adjacent/measurement.json` binds commands, complete hashes and raw reports;
`timing-assessment.json` records the actual terminal wait and derived totals.

| Scope | v2 resource-limit full pool | Baseline integral fallback |
| --- | ---: | ---: |
| Formal candidates, rounds 0 / 1 | 589 / 588 | 64 / 64 |
| Operation-to-result total | 564.141 s | 149.205 s |
| Case end-to-end | 565.021 s | 150.066 s |
| Process lifetime | 570.244 s | 154.271 s |
| Peak process RSS | 185.664 MiB | 182.316 MiB |

| Round (zero-based) | Old operation | New operation | New shared preparation | New integral scan | New relaxation | Old formal wall | New formal wall |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 115.060 s | 43.061 s | 0.039 s | 19.781 s | 8.498 s | 114.748 s | 14.437 s |
| 1 | 449.081 s | 106.144 s | 0.233 s | 22.422 s | 8.994 s | 448.966 s | 74.377 s |

The new backend uses 2048 fine/1024 coarse bins, with 0/257 old-bank columns;
reported integral buffer peaks are 22,191,672/33,593,716 bytes. Per-card limits
remain independently checked. Shared preparation includes the rejected v2
preparation attempt. Relaxation is reported in the existing review/local timing
aliases and must be counted once. Formal wall excludes baseline refinement;
worker sums overlap, and RSS is a cumulative process high-water mark.

Both rounds preserve full-pool Top-5, every retained formal row, baseline curves
and normalization. The observed case end-to-end reduction is 73.44%. This single
adjacent observation establishes no repeated-run variance or general speedup;
host CPU snapshots before the processes are 18.3% and 6.0%. No A/A+B comparison
was performed, and earlier measurements and failures remain unchanged.

### Current enabled-only twelve-thread cohort timing

The owner-requested timing pass completed on 2026-10-18, from 20:45:58 to
21:45:14 Asia/Shanghai. It uses implementation `4469e12b`, Windows GCC 13.1
Release, the frozen manifest10/database, twelve candidate threads, cached IL
templates and fixed dense-algebra cache sizes `49152,1310720,25165824`.
Each of the eighty ready cases runs once in a separate process, sequentially,
with `--pfsm-candidate-recall enabled --pfsm-candidate-recall-version v2`.
There are no extra warmups, repeated scientific runs or off/shadow runs.
The original Top-5-assisted multiphase protocol supplies all 263 rounds; the nine
nonready cases retain their original classifications.

The native driver terminal returns zero. All eighty per-case receipts and 1,635
output-file hashes pass the read-only audit, including serial execution, actual
twelve-thread reports, canonical/legacy field consistency and runtime/input
bindings. The separate process observer records a completed wait but a null exit
code; it is preserved as such, and the native terminal receipt supplies the actual
zero exit. No completed case was rerun during closeout.

| Timing scope | Total | Per-case median | Per-case P95 | Per-case maximum |
| --- | ---: | ---: | ---: | ---: |
| Serial measurement loop, including receipt hashing and aggregate output | 3,549.954 s | — | — | — |
| Sum of benchmark process lifetimes | 3,108.671 s | 13.128 s | 89.382 s | 1,069.567 s |
| Sum of case end-to-end work | 2,740.845 s | 8.775 s | 84.091 s | 1,065.269 s |
| Sum of operation-to-result times across each case's rounds | 2,697.192 s | 8.324 s | 83.203 s | 1,064.102 s |

Across individual rounds, operation-to-result median/P95/maximum are
2.285/18.296/920.486 s. Total recall wall time is 716.137 s (26.55% of total
operation time): shared preparation 9.587 s, correlation 3.115 s, initial local
evaluation 644.882 s and review 58.554 s. Formal candidate wall time is
1,915.503 s (71.02%); SQL accounts for 28.978 s (1.07%). Formal candidate wall
time excludes accepted-phase baseline reconstruction. Expansion/solve worker
sums overlap and are not additional wall time.

| Slowest cases by summed operation-to-result time | Rounds | Time |
| --- | ---: | ---: |
| NIST_676a | 2 | 1,064.102 s |
| Dara_reaction_Fe2O3-2TiO2_1000C_60min | 3 | 222.422 s |
| Dara_reaction_2FeC2O4_H2O_2-Y2O3_200C_60min | 3 | 137.097 s |
| cpd-1d | 4 | 97.007 s |
| CPD3 | 4 | 83.203 s |

Nine rounds use full-pool fallback: four formal-underestimation supplements,
four grid-limit exclusions and one review-limit fallback. The longest round is
NIST_676a's second round: its grid-limit fallback fits all 588 remaining cards in
920.486 s, including 919.975 s formal candidate wall time. The run evaluates
11,456 formal candidates and reuses 138 completed fits. Identification remains
79/80; the original `Dara_reaction_Bi2O3-2MoO3_400C_60min` Top-5 failure is
preserved. These timings establish no before/after speed ratio. This is one
descriptive pass; CPU load before each process ranges from 1.9% to 25.9%
(median 12.2%), with no competing PFSM process observed in the receipts.

Raw evidence is under `build/candidate-recall-timing-20261018-01/`: `timing.json`,
`cases.csv`, `rounds.csv`, `audit.json`, `driver.log`,
`driver-terminal-receipt.json`, `cohort.json`, the frozen `runtime/` binding,
per-case receipts and original `runs/` outputs. The archived driver and audit
scripts reproduce the orchestration and read-only extraction respectively.

### Experimental Fast-PFSM modes

The isolated `codex/fast-pfsm` branch accepts `--fast-pfsm off|shadow|enabled`,
defaulting to off, and `--full-round` for an explicit complete-pool run. The fixed
budgets are 500 recall and 20 formal fits, plus ties and numerical protection.
No sample-specific budget tuning is allowed. The mathematical contract is in
[PFSM numerics](pfsm.md). GUI defaults and the formal solver remain unchanged.

`fast-N.jsonl` contains every card's channel scores, shifts, local gain, selection,
protection and NNLS counts for each attempt. Version 2 adds one-based stable
channel/local ranks and explicit reasons (channel, cutoff tie, local budget or
numerical protection); these diagnostic additions do not change the selections.
Round JSON contains generation,
baseline version, discarded/replayed attempts, selection identities, grid size,
normalization scope and shadow Top-5 retention at both cutoffs. Shared-field,
recall and local-refinement wall times are included in operation-to-result time.
Only formal fits appear in ordinary ranking files.

Admission requires a separately frozen unmodified build of
`3e304908da6f91d3ca44f3893dd506ed3ca34c3b`, equality with the new off mode, exact
off/shadow formal evidence, and exact Top-5 and surviving formal evidence in
enabled mode. Every ready case and round is required, with original nonready
classifications and failures preserved. Revealed holdouts are regression data.
A numerical-kernel PASS does not establish either identification or speed.

`pfsm_fast_freeze.py` freezes actual compiler input bytes, the committed archive,
build configuration, executable and an independently hash-verified Qt/GCC runtime.
LF/CRLF-only archive/checkout differences are explicitly inventoried while both
original byte streams are retained. Other source changes fail the binding guard.
`pfsm_fast_validation.py` verifies frozen bindings and defaults to shadow/enabled.
Shadow supplies complete formal rankings and independently checks both cutoffs.
Enabled runs even after a shadow miss so that the actual loss is retained.
Only a failed screening or formal comparison triggers baseline/off for that case
and configuration. It continues other cases,
preserves nonready manifest classifications, and records process IDs, exit codes,
timeouts, original logs and exact comparisons. It retains exact copies of the
runner and comparator. Launch-to-metadata time includes runtime loading, Qt/CLI
initialization and database verification, separately from per-case processing;
it is not an isolated operating-system loader measurement. `pfsm_fast_compare.py` checks every
surviving formal row, its order and signed zero, complete baseline curves, Top-5
at both cutoffs and consistency between declared selections and published rows.
Repeatable `--reuse <previous-validation-directory>` imports completed matching
runs, including failures, without launching them again or overwriting any raw
evidence. It checks frozen runtime and manifest bindings, executable hashes and
scientific command arguments, records the source report and evidence hashes,
and leaves interrupted runs incomplete. Completed historical baseline/off runs
are reused if diagnosis becomes necessary. Recomparison reads saved evidence;
it never refits a completed case. The older `--workflow four-way` remains an
explicit option, but is not used for the owner-requested reduced regression.

The default regression invocation uses 12 workers and cached formal templates:

```sh
python tools/validation/pfsm_fast_validation.py --baseline <frozen-baseline-exe> \
  --benchmark <frozen-fast-exe> --database <cod_inorg.sq> --manifest <manifest10> \
  --output <new-output-directory>
```

Controlled baseline timing requires explicit `--workflow four-way --truth-free
--timing`, five alternating baseline/enabled pairs,
and an idle host checked before every pair. Use `--require-twofold` for both slow
spectra at 12 workers in direct/cache modes and unrestricted NIST_660c at 12
workers with cache (`--full-library`). Example1 replays the frozen acceptance
sequence at 1/4/12/20 workers in direct/cache modes, allowing at most 5% median
regression per round. Correctness runs serve as warmups; concurrent builds and
correctness jobs never establish controlled speed. Operation-to-result includes
all search stages and publication; process, serialization and peak-memory costs
remain separate. Failure never triggers sample-specific budget expansion.
The reduced shadow/enabled workflow records descriptive operation times only.
It does not establish the controlled twofold baseline speed requirement.

The completed reduced-workflow cohort of 2026-10-14 binds the frozen C++ runtime
at `1ffc946b`, orchestration at `983c7625`, 12 workers and cached templates.
`build/fast-pfsm/cohort-03-exit.json` records actual exit 1; all 80 ready cases
completed. The saved-output assessment in `cohort-03-audit-02/assessment.json`
binds its copied report, assessor source and raw artifact hashes. The first
18-case checkpoint remains unchanged in `cohort-03-audit-01`. An initial
assessment-script failure assumed exit 0/1; the CLI correctly returns 2 for the
preserved identification failure. That script/log remain, and `assess-v2.py`
checks the actual CLI contract without changing product assertions.

The 500-card recall retains Top-5 in 263/263 rounds; local reranking to 20 retains
it in 256/263 rounds. Case preservation passes are 73/80: 51/58 development and
22/22 previously revealed holdouts. All 263 rounds preserve surviving candidates'
formal evidence, relative order and baseline curves. Every failed case has
byte-identical baseline/off/shadow complete rankings and curves. Each loss is
in the last saved round, after the assisted acceptances:

| Case | Round (zero-based) | Excluded COD card | Formal rank | Local rank |
| --- | ---: | --- | ---: | ---: |
| CPD-1c | 3 | 9011662 | 5 | 360 |
| CPD-1f | 3 | 9004180 / 9004178 | 2 / 3 | 52 / 53 |
| CPD-1h | 3 | 5000092 / 1000017 | 4 / 5 | 24 / 28 |
| Dara TiO2/ZnO, 2 min | 2 | 2300112 | 5 | 49 |
| Dara TiO2/ZnO, 8 min | 2 | 2300112 / 2300450 | 3 / 4 | 64 / 62 |
| Dara Bi2O3 precursor | 1 | 1526458 | 5 | 29 |
| Dara_reaction_2FeC2O4_H2O_2-Y2O3_200C_60min | 2 | 7218520 / 3000327 | 2 / 3 | 105 / 142 |

Enabled confirms every Top-5 identity/order loss. Baseline/off diagnosis was
reused or run only for failed cases. Identification is separately unchanged:
both modes pass 79/80 assisted cases and retain the historical
`Dara_reaction_Bi2O3-2MoO3_400C_60min` failure. Nine nonready entries retain their
original classifications: four `truth_incomplete`, four `uncovered_truth_phase`
and one `truth_conflict`. The fixed 500/20 configuration fails preservation
admission; later passes cannot waive these losses. Budgets remain fixed and the
experiment stays unadmitted.

The following example1 operation times are the already completed single pair,
reused without refitting. They include screening, full or selected formal fits,
SQL, sorting and publication. They were captured during verification activity
and do not establish controlled baseline speed or medians:

| Round (zero-based) | Full pool | Enabled fits | Shadow seconds | Enabled seconds |
| --- | ---: | ---: | ---: | ---: |
| 0 | 849 | 20 | 3.6641 | 1.1507 |
| 1 | 848 | 20 | 7.7439 | 0.9823 |
| 2 | 847 | 20 | 12.0101 | 1.6579 |
| 3 | 846 | 20 | 13.2740 | 1.8759 |

The assessment retains per-stage times, process/startup/output time and peak
memory for all saved rounds. The unrestricted 88,312-card NIST_660c search and
both long spectra in direct/cache modes also completed on 2026-10-14. All five
supplemental groups preserve Top-5 identities/order, every surviving formal row,
relative order, baseline parameters/diagnostics and complete baseline curves.
Their shadow screening checks pass; no baseline/off diagnosis was needed or run.
These truth-free initial searches do not add identification-acceptance cases.

The following are single shadow/enabled pairs using Windows GCC 13.1 Release,
12 workers and the frozen final runtime. Times cover operation start through
checked result publication, including SQL and screening. They are descriptive
observations, not controlled baseline speed ratios or medians:

| Input | Formal templates | Pool | Recall / enabled fits | Shadow seconds | Enabled seconds |
| --- | --- | ---: | ---: | ---: | ---: |
| Fe3Sb2O4Br4 | Direct | 461 | 461 / 20 | 164.5811 | 23.9289 |
| Fe3Sb2O4Br4 | Cache | 461 | 461 / 20 | 24.8277 | 2.1138 |
| Sr3Fe2Cl2O5 | Direct | 468 | 468 / 20 | 13.1259 | 1.5738 |
| Sr3Fe2Cl2O5 | Cache | 468 | 468 / 20 | 2.9438 | 0.5476 |
| NIST_660c, full library | Cache | 88,312 | 501 / 21 | 1258.1061 | 8.2772 |

The extra full-library card is COD `4124738` (original index 32835), with
`Zero fast candidate` protection at both cutoffs. It enters formal fitting in
addition to the 500 scored recalls and 20 local selections; the budget was not
expanded. No cutoff tie or full-search fallback occurred in these five groups.
The full-library enabled result exceeds the exploratory 2--5 second goal.
Its measured stages are SQL 5.4693 s, shared fields 0.0156 s, recall 2.2640 s,
local NNLS 0.3151 s, formal candidate fitting 0.1150 s, sorting 0.000057 s and
publication 0.0184 s. Stage timers need not sum to operation time because they
exclude some orchestration/preparation overhead; overlapping worker sums are
reported separately.

Enabled process and output costs are separate from operation-to-result time:

| Input / templates | Process seconds | Launch to metadata seconds | Output seconds | Peak resident MiB |
| --- | ---: | ---: | ---: | ---: |
| Fe3Sb2O4Br4 / direct | 28.6356 | 4.1615 | 0.0708 | 78.80 |
| Fe3Sb2O4Br4 / cache | 6.7900 | 4.1511 | 0.0819 | 74.80 |
| Sr3Fe2Cl2O5 / direct | 5.9030 | 4.1459 | 0.0234 | 59.11 |
| Sr3Fe2Cl2O5 / cache | 4.9226 | 4.1466 | 0.0238 | 59.05 |
| NIST_660c / cache | 13.4646 | 4.1716 | 0.7758 | 1609.22 |

Full-library memory is essentially unchanged: shadow peaks at 1,687,306,240 bytes
and enabled at 1,687,392,256 bytes. `full-library-03/validation.json` and
`slow-spectra-01/validation.json` preserve every run and comparison, with actual
zero-exit receipts beside those directories. The saved-output streaming audit
in `performance-assessment-01/assessment.json` records all ten runs' process,
startup, import, preparation, output, memory and per-stage values, input hashes,
protection reasons and exact formal comparisons. It launches no fit and verifies
every file bound by both frozen packages. The earlier repeated timing launcher
is superseded; the twofold baseline median and 1/4/12/20-thread regression gates
remain unverified under the owner's reduced execution protocol.

Final affected validation for C++ implementation `1ffc946b` has the following
actual scope. Numerical references use independent direct convolution and
pinned SciPy 1.16.2 NNLS/KKT, not product-generated expected results:

| Configuration | Affected product tests | Qt-free tests | Independent numerical cases |
| --- | ---: | ---: | ---: |
| Windows GCC | 5 / 5 | 10 / 10 | 24 / 24 |
| Windows MSVC | 5 / 5 | 10 / 10 | 24 / 24 |
| Linux GCC | 5 / 5 | 10 / 10 | 24 / 24 |

The GNU final normal checkpoint passed 109/110; `card.properties` exceeded its
unchanged 180-second subprocess limit. After all heavy jobs ended, only that
failed test was repeated at the original limit: all 361 fixtures and 136,029
checks passed (`gnu-final/card-properties-recheck-01.json`). The original timeout
remains in `gnu-final/normal.log`; this is a checkpoint plus an isolated recheck,
not a new full-suite run. The earlier Linux `12f83173` normal suite passed 110/110;
final guard changes have the affected checks above. Exact-comparator controls
passed 12/12, workflow/reuse controls passed 8/8, and the new workflow CTest
registration was verified separately (111 current registrations). No full
111-test run or final MSVC full-normal pass is claimed.

Source/target boundaries, Qt-free configured compilation and zero-Fortran checks
passed in their recorded scopes. Failed fresh-only capture invocations without
fresh-build markers remain preserved; subsequent configured incremental captures
are separate evidence. The initial MSVC OpenMP build failure, invalid zero-header-
dependency capture, line-ending freeze failure and assessment-script failure also
remain alongside their genuine repairs/checks. No original failure is erased.

Delivery uses branch `codex/fast-pfsm`, baseline source `3e304908`, final C++ source
`1ffc946b` and orchestration `983c7625`. Under `build/fast-pfsm/`,
`frozen-baseline-01` and `frozen-fast-03` retain actual source bytes, committed
archives, compiler configuration, executables and verified libraries. Executable
SHA-256 values are respectively
`a8a923a1cab63747e39cf34e798ad94590a511df0a27aafdeb0a85a56b7ea0be` and
`90d80ad8cea75f028e649f1989012945d817fad9a9c45f89549f1bcf6d62567b`.
`cohort-03-audit-02` and `performance-assessment-01` bind the final saved-output
assessments; stopped `cohort-02` and all historical evidence remain immutable.
These local artifacts are ignored build outputs, not published repository assets.
The fixed 500/20 design remains default-off and unadmitted: seven primary cases
fail exact Top-5 preservation, the full-library exploratory target is unmet,
and controlled speed acceptance is unverified. Final formal refinement cannot
recover a candidate removed by either earlier heuristic.

### Full-search performance mode

`--performance-only` runs the complete candidate universe without requiring
`truth` or `truth_source`. It always reports `not_acceptance` and
`performance_not_acceptance`, omits recall and false-positive statistics, and
does not add cases to development/holdout acceptance. The default is one initial
search. An optional ordered `performance_accept_ids` array replays recorded card
acceptances and reranks the retained universe after each acceptance. Missing or
duplicate replay identities are errors. This mode cannot be combined with
`--profile-candidates`, `--prepare-only` or `--quantify-selected`.

Input bytes, database hashes, explicit radiation/noise provenance and chemical
source validation remain mandatory. A filename-derived allowed-element union is
permitted when explicitly prescribed by the sample owner and recorded in hashed
evidence; it supplies neither phase truth nor count-statistics evidence.

Each case reports import/hash-validation, preflight preparation and end-to-end
elapsed seconds. Each round reports search, checked publication, serialization,
operation-to-result elapsed seconds and peak process residency, alongside the
existing Application timings. Worker elapsed sums overlap. `display_seconds` is
null: this headless runner cannot measure native GUI drawing. Preflight
preparation is reported separately; the initial product search also performs its
own preparation. Peak residency is a process high-water mark, not per-round
allocation. Use a fresh process per case for comparable memory measurements.

The native worker exposes owner-thread `lastTiming()` and `timingReady`, separating
queue, worker, queued delivery, checked publication and synchronous presentation
callback from controller admission to result availability. Enable
`QT_LOGGING_RULES=qualpf.search.timings.debug=true` for diagnostic records during
manual GUI comparisons. This includes preparation inside the search worker;
import is a separate operation. Native asynchronous painting/plot-preview latency
still requires an external screen measurement and is not reported as measured
display time. Cancellation, failed publication and drain also retain timing records.

Speed comparisons use uninstrumented Release binaries with identical compiler,
inputs and settings. Preserve the preoptimization executable/runtime and all
original failures. Warm up once, alternate before/after for five measured pairs
(three for full-library stress), and compare scientific payloads independently
of timing metadata. Report medians, ranges, memory and every round separately;
test direct/cache paths and 1/4/12/20 candidate threads. The two user-supplied
long spectra (34,001 and 8,701 original points) form an independent performance
cohort. Match! timings require the same machine, comparable reference-library
scope and recorded acceptance order before making an algorithm-speed claim.
`tools/validation/pfsm_performance.py` provides this alternating protocol,
preserves every run/log, compares complete ranking and curve bytes plus baseline
diagnostics, and stops on any failure or numerical difference. It never writes
reference expected values. Pass fresh `--output`, frozen `--before`/`--after`
runners, the same `--manifest` and `--database`; use `--full-library` for the
separate unconstrained stress measurement.

Recall reports are compared exactly except for `shared_seconds`,
`integral_seconds` and `relaxed_seconds` inside each attempt's `integral` object.
These elapsed times may differ or be absent without a scientific mismatch.
Selection, objectives, parameters, evaluation/resource counts and unknown fields
remain checked; the exclusion does not apply to similarly named fields elsewhere.
Comparison uses copies and preserves the original reports and timing records.

An optional `--linear-algebra-cache-sizes L1,L2,L3` freezes the private dense
backend's process-start blocking hints in bytes, with positive nondecreasing
values. `linear_algebra_cache` records the exact values in startup and final
metadata. Omission preserves automatic CPU detection. Configure before numerical
work or workers; Core refuses later conflicting explicit settings. This is
independent of the IL template cache, candidate threads and solver tolerances.
On hybrid processors, automatic cache detection can select different matrix
reduction blocks depending on the core executing first initialization. A byte
comparison requires identical blocking hints as well as identical compiler and
solver settings; preserve and diagnose earlier mismatches before declaring a new
controlled baseline. The original alternating driver is frozen with its original
experiments. A fresh local copy forwards the new option in subsequent experiments.

Startup metadata is saved as `run-start.json`. Intermediate case summaries use
`summary-through-round-N.json`, where N is the completed round. Each progress
file is written once; final `run.json` and per-case `summary.json` retain their
completed-report paths and are also saved once. This avoids replacing reports
that Windows readers may hold. Readers must use the progress filenames while a
run is active. A failed run still retains its startup and completed-round files.

### Long-spectrum performance measurements (2026-10-12--13, ended by owner)

The owner ended testing on 2026-10-13 to start a new optimization attempt.
This attempt is closed by user cancellation, with incomplete performance
acceptance. The serial runner, comparison driver, active CLI and launcher have
all exited, and hourly follow-up `pfsm-2` is paused. Original inputs, frozen
runners, failed comparisons and every completed/partial output are retained.
`tail-variant-03/user-stop-20261013-01/closure.json`, process-exit receipts and
`launch-exit.json` record the actual termination. The frozen `matrix-state.json`
remains its last running checkpoint; it does not mean the workload is still live.
The remaining matrix, full fitted-candidate curve audit and final speed criterion
were not completed. Neither the original Fe cached-20 regression nor the noisy
repaired Fe direct-20 timing is waived. No comparable Match! measurement exists.

The sample owner specified the default database/radiation and filename-derived
allowed-element unions. The new explicit baseline uses the 88,312-card COD inorganic
database at `D:/QualX/QualXDB/cod_inorg/cod_inorg.sq`, Cu Kalpha1
1.54059307 angstrom with relative weight 1, automatic text-import equal weights
(count statistics unknown), default background and geometric width, and unchanged
shift bounds, tolerances and budgets. The unions are Fe/Sb/O/Br and Sr/Fe/Cl/O;
they do not supply phase truth. Original historical manual settings cannot be
fully recovered. Both exact byte copies and database hashes are retained locally.

| Input | Original points | Bytes | SHA-256 |
| --- | ---: | ---: | --- |
| Fe3Sb2O4Br4.dat | 34,001 | 642,171 | `b45f373bcedc96cbfe2ab5046c7e86d2d8a750e2ce1412fc946a449d320a4f18` |
| Sr3Fe2Cl2O5.dat | 8,701 | 163,789 | `97c49c2777fd679f01a9e7b4e2f849bda23885acb63f2388b7917b737e14ac6c` |

Measured on an i7-13650HX (20 logical processors), 32 GiB RAM, Windows x64,
MinGW GCC 13.1.0/Qt 6.8.3, using uninstrumented Release builds. The baseline
runner is frozen at `113810eb`; the optimized runner is additionally bound to
its complete source/runtime hashes. There is no concurrent build/test workload
during acceptance timing. Each input has its own fresh process, one warmup per
variant and five alternating measured pairs. Initial searches contain 461 and
468 candidates respectively; no accepted-card sequence was supplied for these
two samples, so only the initial round is measured. No points are removed.

| Input, 12 threads, cache enabled | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | --- | --- | ---: | ---: |
| Fe3Sb2O4Br4 | 32.534 [32.368, 33.781] | 25.982 [25.779, 26.220] | 20.14% | 73.48 / 66.81 |
| Sr3Fe2Cl2O5 | 3.754 [3.681, 4.286] | 2.806 [2.796, 2.914] | 25.24% | 58.95 / 58.90 |

These are operation-to-result medians, including checked publication. Complete
rankings, Q/Rwp, scales/shifts, termination, starts, logical solver counts and
baseline curve bytes match in all ten measured pairs. Both ranges are separated;
each sample improves independently. Neither controlled cached median is at minute
scale. The user's historical minute-scale observation remains separately qualified;
the default direct-mode results below still reach minutes for the Fe spectrum.
Case end-to-end medians and individual runs are retained in
`build/pfsm/performance-optimization-20261012/final-slow-cache12/comparison.json`.
Global runner/database validation is also included in the comparison driver's
process elapsed time. Redundant atomic report writes were removed from the CLI;
the table's operation-to-result interval ends before report serialization.

The subsequent five-pair 20-thread cached measurement completes with identical
scientific payloads in all ten comparisons, but it does **not** meet the speed
acceptance criteria for Fe3Sb2O4Br4:

| Input, 20 threads, cache enabled | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | --- | --- | ---: | ---: |
| Fe3Sb2O4Br4 | 27.858 [26.265, 93.553] | 38.913 [22.591, 86.351] | -39.68% | 102.62 / 91.23 |
| Sr3Fe2Cl2O5 | 2.867 [2.822, 2.912] | 2.252 [2.231, 2.289] | 21.47% | 58.98 / 58.98 |

The Fe median regression exceeds 5%, and its before/after ranges overlap widely.
Three pairs improve and two regress; this is a failed speed criterion with
unresolved variation, not evidence that can be discarded or averaged away by
the Sr or 12-thread improvements. All 255,881 logical residual evaluations and
the complete 461-card payload remain identical. The cause is not established;
no thread policy, numerical threshold or dispatch rule has been changed in
response. Original runs are retained in
`build/pfsm/performance-optimization-20261012/final-slow-cache20/comparison.json`.
The completed five-pair 12-thread direct measurement also preserves all ten
complete scientific comparisons:

| Input, 12 threads, cache disabled | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | --- | --- | ---: | ---: |
| Fe3Sb2O4Br4 | 179.678 [175.078, 780.466] | 165.364 [164.350, 614.074] | 7.97% | 66.51 / 62.89 |
| Sr3Fe2Cl2O5 | 13.845 [13.816, 13.880] | 12.952 [12.897, 12.993] | 6.45% | 59.04 / 58.92 |

All five Fe pairs improve, but the wide overlapping elapsed-time ranges do not
establish that the benefit exceeds timing variation. Its median remains at
2.76 minutes. The Sr ranges are separated. End-to-end medians are 180.149 to
165.832 seconds and 13.989 to 13.101 seconds respectively. Original measurements
are in `build/pfsm/performance-optimization-20261012/final-slow-direct12/comparison.json`.

The original serial matrix then stops during the **baseline** 20-thread direct
warmup. The candidate search finishes, but a second atomic replacement of
`round-0.json` fails. The CLI exits 2, its comparison driver exits 1, and the
owned serial session 65309 returns exit 1. There are no measured pairs in this
failed group and no numerical-comparison PASS. Original logs, files and failed
state remain in `final-slow-direct20` and `matrix-state.json`; no baseline or
optimized frozen binary is replaced.

A separate 1,000-replacement Qt diagnostic on a fresh path fails on replacement
138 with Windows access denied. The original baseline does not include the OS
error string; the OS-level cause and any holder are not identified. The CLI
repair above writes each output once. Independent
analytic CLI regressions pass for Windows GCC/MSVC and Linux GCC; the Windows
checks hold progress reports open without sharing delete access through process
completion. The same repaired CLI passes against an isolated archive of the
original preoptimization Core/Application. A new CMake configure attempt first
fails on backslash escaping; that directory/log is retained, and a fresh configure
with forward-slash paths succeeds. This is unrelated to numerical fitting.

Fresh variants under `io-variant-01` retain the original numerical source bytes
on each side, use the identical repaired CLI, and repeat affected scenarios in
new directories. Original timing results and the failed 20-thread cached Fe
criterion remain evidence. Full speed acceptance is still open; neither the
report repair nor fresh measurements waive that regression. Compilation and
independent checks end before the serial acceptance timing resumes.
The fresh serial matrix ran from `io-variant-01/matrix-run.py`; its
`freeze.json` binds commit `eeb00067`, both complete runtimes, the shared CLI,
unchanged numerical sources and the original evidence. Its `matrix-state.json`
and immutable `state-journal` snapshots record actual child exits. Completed
original scenarios also receive a same-variant scientific comparison across the
old and repaired CLI. No fresh group median is accepted before all five pairs
finish. This repetition later stops at the NIST numerical comparison described
below; neither stopped runner is restarted.

The repeated cached groups finish with actual comparison-driver exits of zero.
Each scenario below contains five alternating measured pairs after its own warmups:

| Input, cache enabled | Threads | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | ---: | --- | --- | ---: | ---: |
| Fe3Sb2O4Br4 | 12 | 32.456 [32.259, 33.555] | 26.266 [26.043, 26.649] | 19.07% | 74.13 / 66.47 |
| Sr3Fe2Cl2O5 | 12 | 3.713 [3.609, 3.730] | 2.801 [2.793, 2.818] | 24.56% | 58.93 / 58.89 |
| Fe3Sb2O4Br4 | 20 | 28.010 [26.150, 28.920] | 23.107 [22.134, 23.657] | 17.51% | 102.04 / 91.73 |
| Sr3Fe2Cl2O5 | 20 | 2.878 [2.803, 2.916] | 2.199 [2.185, 2.210] | 23.61% | 58.89 / 58.92 |

All four before/after ranges are separated, and all five pairs improve in each
scenario. Complete saved rankings and baseline scientific fields match in all
20 measured comparisons; eight same-variant comparisons also match the original
CLI's ranking, baseline curves and diagnostics. Original point/candidate counts remain 34,001/461 and
8,701/468. End-to-end medians are 32.917 to 26.728 and 3.850 to 2.936 seconds
at 12 threads, then 28.475 to 23.581 and 3.012 to 2.337 seconds at 20 threads,
in the same input order. Both controlled cached medians remain below one minute.

The source reports are `io-variant-01/final-slow-cache12/comparison.json` and
`io-variant-01/final-slow-cache20/comparison.json`. A lightweight check recomputes
every median, range, paired improvement and peak-memory value from individual
`run.json` files in `io-variant-01/stats-check-20261013-0058.json`. The independent
complete artifact audit remains deferred until all timing workloads finish,
including fitted candidate curves beyond the saved empty initial baseline.
The original 39.68% Fe cached-20 regression is not reproduced in this repetition;
its cause is still unresolved, and its original failed criterion is retained.
The report repair changes serialization after result availability and does not
establish a cause for the earlier elapsed-time variation.

The repeated direct groups also complete, with five alternating measured pairs
per scenario and actual driver exits of zero:

| Input, direct IL | Threads | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | ---: | --- | --- | ---: | ---: |
| Fe3Sb2O4Br4 | 12 | 186.565 [183.921, 187.213] | 172.390 [170.128, 181.919] | 7.60% | 66.47 / 63.07 |
| Sr3Fe2Cl2O5 | 12 | 14.193 [14.176, 14.279] | 13.000 [12.931, 13.027] | 8.41% | 58.97 / 59.10 |
| Fe3Sb2O4Br4 | 20 | 166.073 [164.643, 167.582] | 155.768 [154.993, 156.583] | 6.20% | 90.88 / 81.11 |
| Sr3Fe2Cl2O5 | 20 | 12.462 [12.438, 12.581] | 11.576 [11.518, 11.606] | 7.11% | 58.98 / 59.12 |

All four ranges separate and all five pairs improve. All 20 saved scientific
comparisons match; four direct-12 bridges also match the original CLI. There is
no original direct-20 bridge because that original warmup failed. End-to-end
medians are 187.033 to 172.861 and 14.327 to 13.142 seconds at 12 threads,
then 166.545 to 156.225 and 12.605 to 11.720 seconds at 20 threads. Fe remains
at minute scale in direct mode. The reports remain in
`io-variant-01/final-slow-direct12` and `final-slow-direct20`.

The next NIST direct-12 accepted-phase replay stops on measured pair 5:
`round-1.jsonl` differs. Both CLI processes exit zero; the comparison and serial
drivers actually exit one. All original artifacts remain. A read-only audit
finds 100 numeric field differences in 10 of 39 candidates, with identical
ranking order, logical solve counts, initial rankings and baseline curves.
Only that baseline repetition differs; the other eleven before/after/warmup
rankings share one byte hash. No completed NIST group is accepted.

Controlled original/current Core probes reproduce the exceptional and ordinary
top-candidate Q, scales, shifts, KKT and optimality **exactly** by changing only
Eigen's L1 blocking hint from 32 KiB to 48 KiB. Both versions agree for each
fixed configuration. The machine's per-logical-CPU inventory reports twelve P
logical CPUs with L1/L2 of 48 KiB/1.25 MiB and eight E CPUs with 32 KiB/2 MiB,
all with 24 MiB L3. Eigen 3.4.0 caches its first CPU query and uses these hints
in its matrix-product blocking heuristic; this accounts for different reduction
orders across processes. The source and controlled receipts are preserved in
`numerical-diagnostic-02`; this is observational equivalence, not a generated
golden. Floating-environment and heap-padding probes retain identical results.

Full NIST CLI reproduction then matches all five saved artifacts, including
every one of the 39 ranked rows and all 100 differing fields: both original and
optimized numerics reproduce the exceptional result with E-core hints and the
ordinary result with P-core hints. All four CLI processes actually exit zero.
This diagnoses the complete observed mismatch, rather than just its leading
candidate. The original comparison remains failed.

The new optional startup setting makes this previously implicit experimental
configuration explicit. Automatic product defaults remain unchanged. Windows
GCC/MSVC and Linux GCC pass independent analytic CLI, fixed-configuration byte
repetition and startup-contract checks; Windows also passes the held-reader
checks. All three pass the unchanged independent SciPy oracle's 24 direct and
24 cached cases with explicit P-core hints. The isolated original-math baseline
passes the same CLI regression. Original failed build/environment checks and a
corrected new test's wrong startup-exit expectation are retained, without a
full-suite PASS claim.

The next comparison uses identical 49152/1310720/25165824-byte hints on both
sides in `cache-variant-02`, with original numerical source bytes and the
identical new CLI/runtime configuration helper. It repeats the declared matrix
in fresh paths, starting with the affected NIST replay. The original alternating
driver stays immutable; a separately frozen copy only forwards the new setting
and records it. Earlier timings are separate evidence; no assertion or tolerance
is relaxed, and the original Fe cached-20 regression remains unresolved.

The `cache-variant-02/freeze.json` now binds implementation commit `9c4453db`,
both complete Release runtimes, original baseline source bytes, shared startup
helper, explicit settings, unchanged inputs and both preserved failed matrices.
Committed-CLI and original-math analytic checks finish before the new serial run
starts. The frozen driver also verifies the setting returned by each CLI. This
run used `cache-variant-02/matrix-run.py`; immutable state snapshots and
`launch-start.json`/`launch-exit.json` retain actual process receipts. Frozen Core
libraries and source snapshots support the pending supplementary fitted-candidate
curve audit after timing ends. Full numerical and speed acceptance remain open.

The fixed-configuration NIST common-thread job now completes with actual driver
exit zero. All 20 measured pairs retain identical ordered rankings, baseline
curves, diagnostics and logical counts; both direct-12 bridges match the earlier
controlled P-core reproduction. The first search and acceptance of recorded COD
2104747 are separate scenarios, each with five alternating pairs:

| NIST mode | Threads | Round | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | ---: | --- | --- | --- | ---: | ---: |
| Direct | 12 | First search | 6.6202 [6.5653, 7.1792] | 6.3777 [6.1727, 6.4087] | 3.66% | 58.04 / 58.15 |
| Direct | 12 | Accepted phase | 1.8766 [1.8327, 2.0138] | 1.7600 [1.7079, 1.7835] | 6.21% | 58.04 / 58.15 |
| Cache | 12 | First search | 0.6829 [0.6687, 0.6980] | 0.6035 [0.5947, 0.6285] | 11.62% | 58.20 / 58.21 |
| Cache | 12 | Accepted phase | 0.5235 [0.5102, 0.5365] | 0.3927 [0.3831, 0.4045] | 24.98% | 58.20 / 58.21 |
| Direct | 20 | First search | 5.8151 [5.7466, 5.9640] | 5.4427 [5.3747, 5.5990] | 6.40% | 58.10 / 58.25 |
| Direct | 20 | Accepted phase | 1.6293 [1.5553, 1.6716] | 1.4555 [1.4129, 1.4907] | 10.67% | 58.10 / 58.25 |
| Cache | 20 | First search | 0.5803 [0.5670, 0.6197] | 0.5241 [0.5214, 0.5389] | 9.68% | 58.17 / 58.18 |
| Cache | 20 | Accepted phase | 0.4066 [0.4028, 0.4079] | 0.3130 [0.3074, 0.3298] | 23.03% | 58.17 / 58.18 |

Every range separates and every paired time improves. Direct-12 first search
improves less than 5%; this is reported separately and does not establish the
declared overall representative threshold. End-to-end before/after medians
[ranges] are 8.6137 [8.5068, 9.1661] / 8.2046 [8.0645, 8.2797] seconds for
direct-12, 1.3133 [1.3089, 1.3210] / 1.1076 [1.1058, 1.1210] for cache-12,
7.5648 [7.4982, 7.7019] / 7.0182 [6.9248, 7.1851] for direct-20 and
1.0914 [1.0775, 1.1357] / 0.9527 [0.9401, 0.9612] for cache-20.

The fixed-configuration slow-spectrum cached-12 job also completes with actual
driver exit zero, all ten scientific pairs and four prior-variant initial-search
bridges identical. Full observation counts remain 34,001 and 8,701:

| Input, cached IL | Threads | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | ---: | --- | --- | ---: | ---: |
| Fe3Sb2O4Br4 | 12 | 32.2832 [31.9717, 32.5547] | 25.8785 [25.7639, 25.9209] | 19.84% | 73.96 / 66.86 |
| Sr3Fe2Cl2O5 | 12 | 3.7471 [3.7293, 3.7528] | 2.8253 [2.8093, 2.8765] | 24.60% | 59.12 / 59.14 |

Both have five positive pairs and separated ranges. End-to-end before/after
medians [ranges] are 32.7531 [32.4417, 33.0237] / 26.3461 [26.2336, 26.3905]
seconds for Fe and 3.8914 [3.8728, 3.8972] / 2.9673 [2.9524, 3.0133] for Sr;
neither cached scenario is minute-scale. `completed-stats-audit-01.json` in this
variant independently recomputes every reported median/range, paired improvement
and peak memory, and rechecks all 130 saved artifact pairs from these two jobs.
The next cached-20 and direct-12 slow-spectrum jobs also complete with actual
driver exits zero. All 20 measured scientific pairs and eight prior-variant
initial-search bridges match. Each of the following scenarios retains five
alternating pairs, positive paired improvements and separated ranges:

| Input | Mode | Threads | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | --- | ---: | --- | --- | ---: | ---: |
| Fe3Sb2O4Br4 | Cache | 20 | 27.6715 [26.4602, 29.3569] | 23.1941 [22.0817, 24.9780] | 16.18% | 102.26 / 91.88 |
| Sr3Fe2Cl2O5 | Cache | 20 | 2.8992 [2.8596, 2.9276] | 2.2368 [2.2255, 2.2564] | 22.85% | 59.07 / 59.16 |
| Fe3Sb2O4Br4 | Direct | 12 | 184.3944 [184.1586, 186.7844] | 170.4377 [169.8223, 172.0628] | 7.57% | 66.56 / 63.27 |
| Sr3Fe2Cl2O5 | Direct | 12 | 14.0230 [13.9833, 14.1022] | 12.8327 [12.7729, 12.9242] | 8.49% | 59.09 / 59.22 |

End-to-end before/after medians [ranges] are 28.1419 [26.9287, 29.8374] /
23.6663 [22.5484, 25.4612] seconds for Fe cached-20, 3.0407 [3.0024, 3.0690] /
2.3763 [2.3661, 2.3929] for Sr cached-20, 184.8552 [184.6203, 187.2652] /
170.9056 [170.2908, 172.5278] for Fe direct-12 and 14.1597 [14.1236, 14.2444] /
12.9880 [12.9111, 13.0695] for Sr direct-12. Fe direct-12 remains minute-scale
at approximately 2.84 minutes. `completed-stats-audit-02.json` independently
recomputes every statistic and checks all 60 saved artifact pairs for these four
scenarios, retaining both full observation grids.

The direct-20 slow-spectrum job also completes with actual driver exit zero,
all ten measured scientific pairs and four prior-variant bridges identical:

| Input, direct IL | Threads | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | ---: | --- | --- | ---: | ---: |
| Fe3Sb2O4Br4 | 20 | 173.0396 [165.7823, 176.8635] | 155.9602 [155.6961, 158.8466] | 9.87% | 90.59 / 81.26 |
| Sr3Fe2Cl2O5 | 20 | 12.5022 [12.4388, 12.5906] | 11.5497 [11.5173, 11.5811] | 7.62% | 59.03 / 59.09 |

Every pair improves and both ranges separate. End-to-end medians [ranges] are
173.5056 [166.2433, 177.3348] / 156.4302 [156.1781, 159.3188] seconds for Fe and
12.6458 [12.5811, 12.7339] / 11.6917 [11.6571, 11.7244] for Sr. Fe remains
minute-scale at approximately 2.60 minutes. `completed-stats-audit-03.json`
independently recomputes every statistic and rechecks all 30 saved artifact pairs.

This serial matrix then actually exits one on the first measured example1
direct-12 pair. Both CLIs exit zero; 83 floating fields differ across nine
candidate occurrences in accepted rounds 1--3 (4/848, 3/847 and 2/846).
The first 849-candidate ranking, all rank orders, top candidates, baseline curves
and logical counts agree. Each variant repeats its own complete ranking bytes
between warmup and pair zero. Explicit startup cache hints agree on both sides,
so the previously diagnosed NIST configuration difference does not explain this
failure. All original reports, runners and actual process receipts remain intact;
example1 has no accepted five-pair timing group.

Targeted probes in `numerical-diagnostic-03` reproduce every changed candidate.
Ordered scan Q and starts agree; restoring original NNLS still reproduces the
failure, while SSE2 restores each original complete-fit digest. The first
template difference is the odd final sample: the original baseline-compiled
code calls the Windows runtime `sqrt`, while the AVX2 translation unit inlines
hardware `SQRTSD`. For the same binary64 argument their roots differ by one ULP,
accounting for the observed template and downstream fit differences. A bounded
repair sends complete groups of four to AVX2 and retains the original evaluator
for the remainder. All nine original fit digests return; every template-call
byte for the first affected candidate also agrees. The new provider regression
passes against original Core, fails against the preserved faulty AVX2 Core and
passes after repair. Independent Windows GCC/MSVC and Linux GCC affected/CLI
checks, Linux SSE2/Qt-free checks and all unchanged 24 direct plus 24 cached
SciPy cases per platform pass. Original diagnostic build and false-negative
fixture attempts are retained, with no weakened assertions or new goldens.
The repaired implementation requires a fresh frozen matrix; these earlier
timings remain evidence for their own frozen implementation.
The repair is locally committed as `22806665`. Committed-CLI analytic/held-reader
checks and the current strict boundary gate pass before freezing
`tail-variant-03/freeze.json`. Its original baseline runner, shared CLI/helper,
input bytes, startup hints and comparison driver remain unchanged; the optimized
runner contains the verified remainder repair. A single owned serial run started
repeating the declared matrix in fresh directories, beginning with example1.
Immutable journal snapshots and launcher receipts retain its actual state; the
representative median, remaining thread counts, stress runs and complete fitted
candidate-curve audit were not completed before the owner stop.

The repaired variant completes the example1 and NIST common-thread jobs and
both cached long-spectrum jobs, with actual comparison-driver exits zero.
Each scenario has one warmup per variant and five alternating measured pairs.
All 120 measured CLIs exit zero; all 60 complete saved scientific comparisons
and eighteen recorded prior-variant bridges match. The single serial runner
was subsequently stopped at the owner's request, as recorded above.

Example1 retains 3,373 points, 849 initial candidates and the frozen
three-card acceptance sequence. Its four rounds at each thread/mode setting:

| Threads | Mode | Round | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| ---: | --- | ---: | --- | --- | ---: | ---: |
| 12 | Direct | 0 | 22.2619 [21.8207, 22.3213] | 20.2765 [20.0692, 20.5251] | 8.92% | 57.13 / 57.34 |
| 12 | Direct | 1 | 27.9272 [27.6610, 28.0459] | 25.5133 [25.2028, 25.5594] | 8.64% | 57.13 / 57.34 |
| 12 | Direct | 2 | 29.9631 [29.6111, 30.0635] | 26.6331 [26.3967, 26.7374] | 11.11% | 57.13 / 57.34 |
| 12 | Direct | 3 | 26.2051 [25.7840, 26.2903] | 22.2179 [22.1657, 22.2524] | 15.22% | 57.13 / 57.34 |
| 12 | Cache | 0 | 3.6460 [3.6074, 3.6659] | 3.1404 [3.1250, 3.5592] | 13.87% | 57.29 / 57.34 |
| 12 | Cache | 1 | 7.8370 [7.7217, 8.1058] | 6.1789 [6.1690, 7.5116] | 21.16% | 57.29 / 57.34 |
| 12 | Cache | 2 | 13.5576 [13.2386, 13.8708] | 10.0998 [9.8958, 10.8587] | 25.50% | 57.29 / 57.34 |
| 12 | Cache | 3 | 14.4292 [14.1753, 14.7239] | 10.2760 [10.0900, 10.5253] | 28.78% | 57.29 / 57.34 |
| 20 | Direct | 0 | 20.4936 [19.8887, 21.1563] | 18.8375 [18.7517, 18.9721] | 8.08% | 57.13 / 57.34 |
| 20 | Direct | 1 | 24.7702 [24.4043, 25.6363] | 22.9371 [22.7634, 23.1921] | 7.40% | 57.13 / 57.34 |
| 20 | Direct | 2 | 26.3995 [25.8497, 26.5322] | 23.4307 [23.3091, 23.7183] | 11.25% | 57.13 / 57.34 |
| 20 | Direct | 3 | 23.0349 [22.7357, 23.3892] | 19.2511 [19.1280, 19.4918] | 16.43% | 57.13 / 57.34 |
| 20 | Cache | 0 | 2.9108 [2.8561, 2.9308] | 2.5950 [2.5503, 2.6208] | 10.85% | 57.23 / 57.33 |
| 20 | Cache | 1 | 8.0248 [7.7867, 8.1920] | 6.1663 [5.8029, 6.6605] | 23.16% | 57.23 / 57.33 |
| 20 | Cache | 2 | 15.7872 [15.3274, 16.1100] | 12.4652 [12.1187, 13.1297] | 21.04% | 57.23 / 57.33 |
| 20 | Cache | 3 | 16.6929 [16.5364, 17.6122] | 12.1779 [11.2333, 12.4361] | 27.05% | 57.23 / 57.33 |

NIST 660c retains its initial search and recorded accepted-phase replay:

| Threads | Mode | Round | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| ---: | --- | ---: | --- | --- | ---: | ---: |
| 12 | Direct | 0 | 6.4465 [6.3678, 6.6519] | 6.2021 [6.0093, 6.2400] | 3.79% | 58.20 / 58.12 |
| 12 | Direct | 1 | 1.9089 [1.8539, 1.9263] | 1.7252 [1.6957, 1.7499] | 9.62% | 58.20 / 58.12 |
| 12 | Cache | 0 | 0.6833 [0.6714, 0.7106] | 0.6011 [0.5927, 0.6097] | 12.04% | 58.15 / 58.24 |
| 12 | Cache | 1 | 0.5202 [0.5024, 0.5288] | 0.3973 [0.3898, 0.3983] | 23.63% | 58.15 / 58.24 |
| 20 | Direct | 0 | 5.7518 [5.7248, 5.8346] | 5.4558 [5.4071, 5.5254] | 5.15% | 58.14 / 58.11 |
| 20 | Direct | 1 | 1.5831 [1.5739, 1.6468] | 1.4551 [1.4379, 1.5547] | 8.08% | 58.14 / 58.11 |
| 20 | Cache | 0 | 0.5960 [0.5852, 0.6061] | 0.5277 [0.5211, 0.5354] | 11.46% | 58.23 / 58.18 |
| 20 | Cache | 1 | 0.4016 [0.3964, 0.4134] | 0.3086 [0.3055, 0.3240] | 23.16% | 58.23 / 58.18 |

The two slow spectra retain all 34,001/8,701 points and 461/468 candidates.
Their repaired cached initial searches are independently measured:

| Input | Threads | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | ---: | --- | --- | ---: | ---: |
| Fe3Sb2O4Br4 | 12 | 31.8997 [31.8045, 32.0211] | 25.6323 [25.3538, 25.7386] | 19.65% | 73.86 / 66.61 |
| Sr3Fe2Cl2O5 | 12 | 3.6243 [3.6197, 3.6532] | 2.7953 [2.7862, 2.7984] | 22.87% | 59.12 / 59.06 |
| Fe3Sb2O4Br4 | 20 | 25.4887 [24.8903, 25.6643] | 21.3260 [21.1739, 21.4570] | 16.33% | 102.90 / 91.56 |
| Sr3Fe2Cl2O5 | 20 | 2.7971 [2.7763, 2.8491] | 2.1879 [2.1800, 2.2089] | 21.78% | 59.23 / 59.11 |

Every paired round improves and all 28 round ranges separate. No median in
this completed subset regresses; NIST direct-12 initial search improves only
3.79%, below 5% individually. The overall predeclared representative criterion
remains unverified. Each slow spectrum improves at both common cached thread
counts, with medians below one minute. The direct-12 measurements are reported
separately below; manual GUI timings remain unmeasured.

Case end-to-end medians include all recorded acceptance rounds:

| Input | Threads | Mode | Before median [range], s | After median [range], s |
| --- | ---: | --- | --- | --- |
| example1 | 12 | Direct | 106.6769 [105.0991, 106.7232] | 94.9262 [94.0636, 95.0694] |
| example1 | 12 | Cache | 39.5256 [39.0753, 40.5263] | 29.8426 [29.5440, 32.6128] |
| example1 | 20 | Direct | 94.6757 [93.6880, 96.2762] | 84.6831 [84.2616, 85.5177] |
| example1 | 20 | Cache | 43.8476 [43.1344, 44.8426] | 33.2818 [32.8801, 34.2667] |
| NIST_660c | 12 | Direct | 8.4658 [8.3295, 8.6700] | 8.0232 [7.8713, 8.0685] |
| NIST_660c | 12 | Cache | 1.3098 [1.2902, 1.3458] | 1.1004 [1.0975, 1.1130] |
| NIST_660c | 20 | Direct | 7.4397 [7.4064, 7.5586] | 7.0173 [6.9523, 7.1901] |
| NIST_660c | 20 | Cache | 1.1120 [1.0942, 1.1178] | 0.9456 [0.9363, 0.9692] |
| Fe3Sb2O4Br4 | 12 | Cache | 32.3661 [32.2717, 32.4905] | 26.0988 [25.8187, 26.2033] |
| Sr3Fe2Cl2O5 | 12 | Cache | 3.7666 [3.7604, 3.7955] | 2.9333 [2.9254, 2.9355] |
| Fe3Sb2O4Br4 | 20 | Cache | 25.9574 [25.3581, 26.1292] | 21.7901 [21.6518, 21.9303] |
| Sr3Fe2Cl2O5 | 20 | Cache | 2.9390 [2.9162, 2.9863] | 2.3284 [2.3173, 2.3458] |

The initial partial-group audit is retained in
`tail-variant-03/completed-groups-audit-01.json`. The fresh completed-job audit
`tail-variant-03/completed-stats-audit-01.json` independently recomputes every
median, range, paired benefit, end-to-end time and peak-memory value from
individual final reports, and rechecks all 340 saved artifact pairs. Rankings,
baseline parameters/diagnostics and baseline curve bytes agree throughout.
This lightweight audit takes 0.56 seconds and performs no fits alongside
the active timing workload. Complete fitted-candidate curves, remaining
thread/mode/replay/stress measurements and final speed acceptance remain open.

The repaired direct-12 slow-spectrum job then completes with actual driver
exit zero, all ten measured scientific comparisons and four prior-variant
bridges identical. Both complete observation grids and candidate sets remain:

| Input, direct IL | Threads | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | ---: | --- | --- | ---: | ---: |
| Fe3Sb2O4Br4 | 12 | 185.1853 [182.4109, 186.4185] | 169.9757 [169.7609, 174.5905] | 8.21% | 66.78 / 63.25 |
| Sr3Fe2Cl2O5 | 12 | 14.0791 [14.0473, 14.0958] | 12.7832 [12.6748, 12.8331] | 9.20% | 59.14 / 59.14 |

Every pair improves and both ranges separate. Fe remains minute-scale at
approximately 2.83 minutes. Case end-to-end before/after medians [ranges] are
185.6539 [182.8893, 186.8994] /
170.4461 [170.2293, 175.0625] seconds for Fe and
14.2180 [14.1860, 14.2351] /
12.9254 [12.8175, 12.9742] for Sr.
`tail-variant-03/completed-stats-audit-02.json` independently recomputes every
statistic and rechecks all 30 saved artifact pairs in 0.09 seconds, without
refitting or overlapping a heavy check with timing. Complete candidate-curve
and overall speed acceptance remain open.

The repaired direct-20 slow-spectrum job also completes with actual driver
exit zero, all ten measured scientific comparisons and four prior-variant
bridges identical, retaining both full observation grids:

| Input, direct IL | Threads | Before median [range], s | After median [range], s | Improvement | Peak process MiB, before/after |
| --- | ---: | --- | --- | ---: | ---: |
| Fe3Sb2O4Br4 | 20 | 167.7157 [157.1635, 460.9952] | 151.8602 [146.6267, 167.8156] | 9.45% | 92.06 / 81.39 |
| Sr3Fe2Cl2O5 | 20 | 12.2163 [12.1390, 12.3470] | 11.4955 [11.4455, 11.5612] | 5.90% | 59.14 / 59.09 |

All five pairs improve for each sample. The Fe ranges overlap, however, with
large variation including a 460.9952-second baseline search. Its median benefit
does not establish improvement beyond timing variation; that speed criterion
remains unverified. Fe still takes approximately 2.53 minutes at the optimized
median. The Sr ranges separate. Case end-to-end before/after medians [ranges] are
168.1812 [157.6342, 461.4616] /
152.3404 [147.0920, 168.2835] seconds for Fe and
12.3530 [12.2777, 12.4827] /
11.6381 [11.5849, 11.6999] for Sr.
`tail-variant-03/completed-stats-audit-03.json` independently recomputes every
statistic and checks all 30 saved artifact pairs in 0.74 seconds. Numerical
and arithmetic PASS does not qualify the noisy Fe timing as a speed PASS.
At the owner stop, cpd-1c direct-12 had two complete scientific pairs with
identical saved outputs; its third after-variant measurement was interrupted.
There is no five-pair cpd-1c timing median. Remaining multiphase, 1/4-thread and
full-library measurements were not completed.

The original Fe cached-20 39.68% regression remains a failed criterion with
unresolved cause. These new measurements and the separately diagnosed NIST
configuration difference do not waive it. Full fitted-candidate curve and speed
acceptance remain open.

Current isolated instrumentation samples the median and greatest logical-solve
count candidate from each frozen initial ranking, with three repetitions in
each mode. Its scientific payload matches uninstrumented current Core. In these
single-phase fits, template evaluation accounts for 78--97% of direct fit time;
cached templates account for 59--60% and NNLS for 23--24%. Scan time is separate.
These attributed timings are not Release acceptance or the historical multi-phase
49% NNLS/39% cache split. Full-grid cancellation during fitting returns in
15--19 ms in four direct/cache checks. Windows GCC/MSVC and Linux GCC affected
checks, native PFSM GUI integration, unchanged direct/cache SciPy cases (24 each
per platform) and baseline SSE2 checks pass; original restricted-write failures,
missing dependency invocation and overlapping-audit timeouts remain retained.
An isolated unchanged card-properties rerun passes. No full normal-suite PASS
is claimed from these affected checks.

The owner first requested delivery of both slow samples at common thread counts
while the complete matrix continued after verified repairs, then ended testing
to start another optimization attempt. The remaining 1/4/12/20 direct/cache,
representative accepted-phase replay and unconstrained full-library results are
unverified, not accepted by extrapolation. Existing development/holdout statistics
are unchanged. No comparable same-machine Match! measurement is available, so
neither parity nor a remaining numerical speed gap is established.

## Authorized holdout assessment: 2026-10-12

The human subsequently authorized evaluation of all 22 reserved holdout cases.
This is one frozen assessment, without outcome-driven tuning, model changes or
additional development-case experiments. The earlier development disposition
below remains a historical closeout, including its retained failure.

`build/pfsm/holdout-assessment-01/plan.json` binds manifest10, executable revision
`29962e2358f74980bc33bdae356956973dccfe2a`, the runtime, source-supported chemistry
and the complete ordered cohort: NIST 676a, CPD4 and twenty Dara ternary scans.
There are 64 truth-phase occurrences. The initial chemical universes contain
453--1,930 cards per case, totaling 15,193 cards before subsequent rounds.
Truth remains confined to post-ranking scoring and simulated Top-5 selection.

Execution uses 12 candidate OpenMP threads, single-threaded numerical libraries,
the development-validated optional template cache and direct full-IL final
verification. The product's fixed common Geometric H0, relative lines, radiation,
weights, NNLS/TRF tolerances, shift limits and absolute Delta-Rwp are unchanged.
Every round retains the entire chemical universe except accepted card identities;
successful identification includes the final remaining-improvement rerank.

All 22 current-executable prepare-only checks and the affected CLI analytic
regression pass before ranking. The metadata refresh leaves all 26 static
libraries and executable `.text` bytes unchanged; `.rdata` changes only the
revision string. Concurrent GUI and presentation-test edits are outside this
headless executable's dependency closure and are not validated by this evidence.
The frozen driver never retries a case or replaces its output. Both the original
unified driver session 34002 and read-only audit session 36133 terminate with
actual exit code zero; their tool receipts are preserved separately from JSON
completion markers. No holdout outcome was used for tuning or a replacement run.

The completed assessment identifies every frozen phase in every case:

| Holdout group | Complete Top-5 assisted cases | Identified phase occurrences | Predeclared weak-phase occurrences |
| --- | --- | --- | --- |
| NIST 676a and CPD4 | 2/2 | 4/4 | None declared |
| Ten Dara ternary 2-minute scans | 10/10 | 30/30 | 3/3 |
| Ten Dara ternary 8-minute scans | 10/10 | 30/30 | 3/3 |
| Total | 22/22 | 64/64 | 6/6 |

All ten physical Dara mixtures pass at both acquisition times. There are 86
complete rounds and 59,467 candidate evaluations, including each case's final
post-acceptance rerank. The read-only audit verifies 346 case artifacts, exact
candidate membership/order, source/manifest/database hashes, fixed observations,
background and weights, score arithmetic and assisted acceptance sequence.
All 59,467 returned scores have direct full-IL final verification. NNLS KKT is
checked for 59,408 rows, including 127 independently certified feasible fallbacks;
the other 59 rows explicitly retain the literal baseline-plus-zero fallback and
do not claim KKT certification. No baseline rebuild occurs.

Every one of the 430 Top-5 results terminates on the original cost criterion and
passes independent direct-IL/SVD active-subset NNLS, residual and KKT checks at its
saved shifts, with the existing controlled tolerances. These checks do not prove
a global nonlinear optimum or independently reproduce background estimation.
One lower-ranked candidate retains `budget` termination: card 7211032, rank 271
in round zero of the last V2O5/In2O3/LaOH3 8-minute scan. Its score is directly
verified and KKT checked; it is never a Top-5 choice. All 186 feasible fallback
terminations and this budget limit remain in the original full rankings.

Under the correctly assisted path, the 64 chosen cards rank first 51 times,
second eleven times, third once and fifth once. The fifth-place choice is V2O5
card 2020756 in round two of the V2O5/LaOH3/MnCO3 2-minute scan. Before complete
identification, 113 of 320 Top-5 card occurrences lie outside the frozen
structural truth mapping. After complete identification, this count is 101 of
110 and is reported separately. These conditional results establish the agreed
Top-5 assistance criterion, not autonomous first-card selection.

Final Rwp_net ranges from 20.391905% to 54.876467%, and remaining maximum absolute
Delta-Rwp_net ranges from 0.002234074 to 0.029683615. Substantial model residuals
and positive remaining improvement are retained alongside correct identification.
The remainder can include accepted-phase aliases and is not a stopping rule;
reference scales remain unrelated to phase mass fractions.

The sum of 22 process wall durations is 3,194.289070 seconds (53.238151 minutes),
with maximum observed process peak residency 178,479,104 bytes (170.210938 MiB).
Per-round database, template-worker, solve-worker, sorting and candidate-loop
timings remain in the machine-readable output. Worker elapsed sums overlap and
are not CPU time. Concurrent work and the absence of a heldout single-thread
control prevent an isolated speedup claim. This is a Windows GCC assessment;
other-platform numerical regressions retain their earlier independent scope.

`build/pfsm/holdout-assessment-01/results.json` and `results.tsv` contain the
complete 22-case results, accepted identities, per-round ranks, Rwp, weak recall,
performance and qualifications. `integrity-metrics.json`, `audit-snapshot-19`,
per-case protocol/direct-reference files and actual terminal receipts preserve
verification. Manifest10, all four COD files and the frozen numerical/runtime
inputs remain unchanged. The fixed product model and earlier development
Bi2Mo3O12 failure are retained; no failed-case or holdout tuning follows this run.
The development disposition below is not a fresh rerun of 58 cases on this binary.

## Development acceptance disposition: 2026-10-12

The human ended further testing of the Bi2Mo3O12 failure case and requested
acceptance closeout using the retained evidence. This closes the current work
with a known exception; it does not change the frozen Top-5 criterion or certify
complete identification across the original development and holdout inventory.

| Frozen population | Recorded disposition |
| --- | --- |
| 58 ready development cases | All terminal: 57 `passed_top5_assisted`, one `failed_top5` |
| Nine unresolved development cases | Four `truth_incomplete`, four `uncovered_truth_phase`, one `truth_conflict`; separate nonpasses |
| 22 holdout cases at development closeout | Frozen and unranked then; see the subsequent authorized assessment above |

The primary result is 57/58 (98.28%) complete, sequential Top-5 assisted
identifications among ready development cases under source-supported chemical
constraints. Repeated scans and diagnostic rerankings do not add cases. The
distinct aggregate is `build/pfsm/current-development-aggregate-10/snapshot-19`;
its performance extraction retains 177 rounds and 118,709 candidate evaluations
in `build/pfsm/current-performance-summary-58-10`. Unrestricted full-COD results
remain separate supplements, with their original incomplete-case qualifications.
Truth only guides scoring and simulated acceptance after ranking.
These results and timings bind their original frozen benchmark revisions, not a
fresh rerun of all 58 cases on the later GUI/Application checkpoint. In particular,
the later `pfsm_workflow.cpp` adds physical-width provenance, phase removal and
relative-FOM presentation. Its current implementation has separate platform and
transaction regressions; the historical real-data performance is not promoted
to a measurement of that newer revision.

For `Dara_reaction_Bi2O3-2MoO3_400C_60min`, the product accepts Bi2O3 and MoO3,
but the first mapped Bi2Mo3O12 card is tenth in the next round. The isolated
phase-width experiment also fails, placing card 2107214 sixteenth of 464 despite
its improved Rwp. Neither result is reclassified as a pass. The human reports
that Match! 4 also could not identify this sample; its settings, database and raw
output were not supplied or independently checked. That report supplies context,
not an independent numerical reference or a reason to waive the PFSM failure.

The prepared selected-Mo orientation proposal is closed without authorization,
implementation or execution. No further failure-case experiment or fit is queued.
Original proposals, failures, diagnostics and actual terminal receipts are
preserved. The existing PFSM follow-up automation remains off. The original NiO
supplemental session's later terminal receipt remains unavailable; absence of a
matching live process does not establish how that session ended.

The fixed IL expander, NNLS/TRF fitter, Application transactions, candidate-level
OpenMP and headless benchmark are implemented. Their numerical and architecture
evidence is separate from real-data identification. Product defaults retain the
fixed common-width model; the benchmark retains Geometric H0, fixed relative
intensities and positions, explicit radiation and the original Delta-Rwp score.
No diagnostic width, dilation or texture model is promoted. At that identification checkpoint, RIR interpretation
and QXP persistence of PFSM state remain unsupported.

Existing implementation regression evidence is reused only with matching source,
reference, runner and build bindings. Windows MSVC and Linux GCC each have an
actual 104/104 full normal run; Windows GCC has 102/102 plus two separately bound
Core-property passes, not one 104/104 invocation. The three Qt-free suites each
pass 7/7. Original failures, unchanged-limit retries and independent-reference
limitations remain attached to their original records. Closeout adds document,
reference and boundary verification, without rerunning scientific fits.
The read-only closeout audit in `build/pfsm/acceptance-closeout-01` verifies 763
case artifacts, 139 raw/source bindings and all four database hashes. The
2,257-file implementation snapshot differs only in five uncompiled document or
handoff files. Current Windows GCC/MSVC configured boundary gates and the 169
byte-preserved normal references pass; these are fresh closeout checks, distinct
from the reused scientific and full-suite results.

Earlier entries below describe their historical snapshots and pending states at
the time. They do not reopen this closed experimental queue. New model work,
unresolved-truth repair or holdout identification requires a new scoped task.

## Freeze and run

```powershell
python tools/validation/pfsm_prepare_benchmark.py `
  --database D:/QualX/QualXDB/cod_inorg/cod_inorg.sq `
  --recipes tests/data/pfsm-real/recipes.json --output build/pfsm/frozen-inputs

build/pfsm/product-gnu/bin/qualpf_pfsm_benchmark.exe `
  --database D:/QualX/QualXDB/cod_inorg/cod_inorg.sq `
  --manifest build/pfsm/frozen-inputs/manifest.json `
  --split development --chemistry evidence --threads 4 --output build/pfsm/chemical-run
```

Both commands require a new output directory and preserve earlier runs. The
preparer hashes `.sq`, `.sq.info`, `.sq.infostat`, `.sq.search`, raw observations
and local evidence. It freezes structural mapping rules and acceptable card IDs
before fitting. Composition, space group and conventional cell must agree;
chemical formula alone never establishes polymorph identity. Unrecognized
equivalent settings are not silently accepted after seeing rankings.

`pfsm-holdout-v1` freezes 67 development cases and 22 holdout cases before any
full COD ranking was inspected. The holdout contains NIST 676a, CPD4 and every
Dara ternary mixture, keeping each physical mixture's 2-minute and 8-minute scans
together. Use `--split development` for iteration, freeze the code/options, then
use `--split holdout` for the reserved assessment. `--split all` (the default)
selects every case; split labels select datasets only and never enter fitting or
candidate queries. The preparer rejects sample families split across both groups.
Input-format/FWHM readiness was checked for all cases; no holdout PFSM rankings
have been used for optimization. A holdout failure remains attached to its frozen
version and cannot silently be reclassified as development evidence.

Decimal intensity/error tokens from CSV, NIST measured CIF loops and columns are
preserved. Coordinates are constructed only from explicit packed start/step/end
headers or XRDML positions. No normalization, clipping, subtraction, resampling
or inference of counts from two columns occurs. CSV sigma stays in supplied units;
CIF least-squares weights become diagonal variances. XRDML requires explicit
counts and records radiation components. Each recipe states radiation sources
and centroid approximations.

All twenty Dara reaction recipes use their original, same-acquisition XRDML
counts channel and native coordinate grid. The source inventory
[`reaction-count-evidence.json`](../../tests/data/pfsm-real/reaction-count-evidence.json)
binds both original XRDML and exported XY files: all 166,020 exported intensity
values equal the original integer counts, while XY coordinates round the native
grid to five decimal places. Counting time is recorded but is not divided into
these counts. The existing raw-count rule supplies `w = 1/max(n,1)`; it is a
variance model, not supplied empirical variance or covariance. Same-acquisition
`usedWavelength` values supply both wavelengths and their intensity ratio.
Instrument metadata does not establish an independent spectral calibration or
exclude prior instrument processing; no matching independent background was found.
The XRDML `intended` attribute does not override its explicit second wavelength
and ratio. Original XY files remain provenance evidence.

This source correction applies to every reaction recipe, including unresolved
cases, independently of truth or fitted rank. Manifest10 changes fifteen generated
reaction observations and twenty source/noise records; the other 69 cases,
chemistry, structural mapping and splits are unchanged. Earlier equal-weight
reaction results remain tied to their original manifests and are not relabeled.
Fresh prepare-only execution yields 80 prepared cases and the original nine
unresolved cases, with no candidate queries. All eleven ready reaction cases
select the same Geometric peak indices as before; their maximum H0 change is
8.79e-6 degrees. Eligible-peak counts change in ten cases, so the coordinate and
noise correction is not merely an output-format change. Its effect on rankings
requires new complete runs.

The source-corrected reaction rerun on `883f7bba`/manifest10 completes all eleven
cases in `current-reaction-counts-audit-10/snapshot-10`: ten pass and
Bi2O3-2MoO3 retains `failed_top5`. After Bi2O3 and MoO3 acceptance, the first
mapped Bi2Mo3O12 card ranks ten rather than the earlier equal-weight rank
eighteen. This remains a failure under the frozen criterion; the two objectives
are different and their Q/Rwp values are not compared directly. The accepted
MoO3 card also changes from 9014282 to 9015401, so this is not an isolated
weighting comparison. The eleven cases
retain all 23,099 candidate rows, each with direct final verification; 23,093
have checked KKT diagnostics and six retain explicit unchecked baseline-plus-zero
fallbacks. The root-owned driver exits one with that failure retained. The
distinct current-development aggregate in
`current-development-aggregate-10/snapshot-19` combines these eleven cases,
all 34 completed remaining-development cases and thirteen qualified reused cases:
58 completed, 57 passed and one retained Bi2Mo3O12 failure among the 58 ready
development cases. The original remaining-development driver exits zero. Its
corrected independent audit retains all 75,394 candidate rows, with direct final
verification throughout, 75,373 checked KKT diagnostics and 134 feasible fallbacks.
The original snapshot 18 audit failure remains unchanged: it incorrectly required
literal baseline-plus-zero scales for checked Core initial solutions as well as
unchecked Application fallbacks. Independent direct-IL/SVD active-subset NNLS
verification certifies the two affected rows under the original Q, scale and KKT
criteria. The corrected audit separately retains the exact literal assertion for
unchecked Application fallbacks. Control-binding and JSON-serialization setup
failures are also preserved; no product fit or numerical tolerance was changed. The nine unresolved
development cases remain separate
nonpasses; holdout rankings remain untouched.
Repeated runs do not increase distinct-case coverage.

The independent source-count Bi2Mo3O12 oracle was interrupted after four of its
twenty starts, with no terminal exit receipt or known cause. That incomplete
attempt is preserved. The separate reviewed recovery completes the entire
unchanged 33-cubed grid and all twenty starts, with seven repeating the best
minimum, then completes the thirteen-call conditioning check. No partial fit is
reused and no tolerance changes. Q and all scales pass, but the target physical
shift error is 3.81188e-5 degrees against the unchanged 2e-5-degree limit. Stable
local shift rank does not waive this failure. Independent best Q is
171,466.8348511, still 4,910.14779 above the current fifth card, 1528796; the
product Q error is only 0.00079104. The recovered objective therefore does not
bridge the Top-5 gap. This finite multistart search is not a global-optimum proof.
Reference and conditioning stages exit zero; comparison and its recorded wrapper
exit two. The outer tool shell reports one and is qualified separately. The
complete source-bound audit is retained in
`sourcecount-bi2mo3o12-reference-recovery-02/independent-completion-audit.json`.

The paired-HKL cell diagnostic in `bi2mo3o12-cell-geometry-decomposition-01`
checks the retained 500 SQL reflections and all 1,970 independently enumerated
members against the original source cell, then compares their geometry with the
author's Rietveld-refined cell. It uses fixed reference intensities and radiation
weights, with no fit to observed intensities. In the experimental window, the
reference-intensity-weighted doublet RMS after a constant shift is 0.13302 degrees
for the retained set and 0.17465 degrees for the member set. Adding a tan(theta)
column reduces these to 0.12692 and 0.15997 degrees, respectively: 0.704 and
0.887 of the unchanged Geometric H0. The remaining geometry includes anisotropic
and nonlinear effects. The refined cell comes from the same experiment, so this
is descriptive evidence, not independent truth or proof of the failure's cause.
No cell, profile, width, solver or ranking change follows from this diagnostic.

The read-only source preflight in `bi2mo3o12-cell-model-proposal-01` checks all
466 frozen chemical candidate identities without using labels or fitted ranks.
Every card has paired numeric HKL, d/I and a positive-definite source-cell metric;
462 have a unique exact space-group alias in the pinned symmetry resource.
These are descriptive coverage checks, not physical model eligibility. In
particular, a representative HKL and multiplicity do not provide separately
conserved intensities for accidentally merged reflections. The preflight proves
no individual merged-member intensity allocation. Moving a whole stored line by
its representative HKL would therefore be a phenomenological correction, distinct
from a member-conserving physical cell-deformation model. No HKL cell-deformation
fit or product model change is authorized.

The user separately approved the bounded stored-d isotropic diagnostic in
`bi2mo3o12-isotropic-proposal-01/proposal.txt` (SHA-256
`1bcc8799cd7f2ab24d0938494ae6d15ba3ba787d323ffd7239e3a2ece4506705`).
Authorization covers an isolated prototype and synthetic verification first,
then one conditional Bi2Mo3O12 three-phase diagnostic using 9012546, 9015401 and
2107214. Each phase has d'=d*exp(e), e within plus/minus log(1.005), shifts within
plus/minus 0.5 degrees, thirteen neutral starts and the original tolerances and
300 outer evaluations per start. IL, common Geometric H0, relative intensities,
radiation, weights and scoring stay fixed. Product-default integration, full-pool
reranking and holdout testing are excluded. Synthetic and exact zero-e nesting
gates must pass before the real diagnostic; approval does not establish Top-5
success. Prototype preparation is in `bi2mo3o12-isotropic-diagnostic-01`; its
initial standalone and reference arms both use SciPy TRF and are not independent
optimizer implementations. High-precision mathematics and the current Core direct
evaluator provide separate verification.

The first synthetic execution compiles the bridge and generates the five
high-precision fixtures successfully, then fails while serializing NumPy boolean
control results; no nonlinear fit runs in that attempt. An additive serialization
repair reuses the sealed bridge and generated data. Its exact zero-e profile/Core
Q nesting, derivatives and conditioning controls pass. Both fitting arms execute
all thirteen neutral starts for every fixture, but the frozen synthetic gate fails:
four cases recover the prescribed best solution while retaining budget-exhausted
starts, and the e-boundary case misses the unchanged Q, scale, shift and e limits.
Its prototype best Q is 8.17389e-7 versus the reference's 5.41126e-12. SciPy validly
reports scaled-gradient convergence; that flag does not establish physical
parameter accuracy. The independently recorded 432 seals remain unchanged.
The additive analytic experiment validates both variable-projection derivative
terms, scale-aware eligibility, bounded finite differences, independent 70-digit
Bragg/IL derivatives, exact-face retention/release and shared evaluation counts.
The actual quadratic loop and callback controls pass before any new IL fit.
Its ordinary-then-face trials compare actual costs using the full Coleman-Li
prediction and the same 300 budget; exact ties retain the ordinary trial.
No restart, new initial vector or post-fit parameter snapping is introduced.

All 65 analytic-face prototype starts now converge within that budget, and the
five best-result Q, scale, shift, e, prescribed-parameter and KKT checks pass.
The boundary best Q is 1.90318e-16. However, four corresponding analytic reference
TRF starts exhaust their unchanged 1,000 budget, so that synthetic comparison
fails. The original and additive failed comparisons remain separate; 429 input,
source and runtime bindings are unchanged. A separately named Dogbox reference
variant keeps the independently coded model/derivatives, thirteen neutral starts,
1,000 evaluations and 1e-13 tolerances; its known exact quadratic/rank/switch
controls pass. Its five fixtures now complete all 65 reference starts successfully,
with a maximum of 120 evaluations. All unchanged synthetic best-result and
all-start gates pass against the reused analytic-face prototype outputs. The
receipt-bound artifact audit verifies 468 unchanged bindings, all 195 retained
attempts across the two prototype variants and Dogbox reference, and the comparison
arithmetic. The comparison SHA-256 is
`19d09efc645f3e7edde641729ab71d7ae738e6622932e9afbc0669f15be19b42`.
Dogbox changes the verification optimizer, not the prototype or its acceptance
limits. The implementations still share NumPy/SciPy NNLS and linear algebra;
this is not full software independence. The original finite-difference and
analytic-TRF reference failures remain failures.

The one authorized real three-phase pilot then completes with actual root and
child exits zero. Exact zero-e Core Q nesting passes before fitting. All thirteen
neutral starts stop on the unchanged `ftol` criterion within 300 evaluations;
the maximum is 47, with 342 outer evaluations in total. Twelve reach the better
basin; one stops at Q=197640.362519 with the third shift at -0.5 degrees. A
successful termination flag does not imply a global minimum or `gtol` satisfaction.
The best conditional fit has no bound hits:

| Quantity | Original fixed-d three-phase row | Conditional d'=d*exp(e) pilot |
| --- | ---: | ---: |
| Q | 171466.835642101 | 163338.384345073 |
| Rwp_net | 0.310934959909 | 0.303475486105 |

Q decreases by 4.74054% and Rwp by 0.745947 percentage points. The best per-phase
parameters are:

| Card | Scale | Shift, degrees | 100*(exp(e)-1), percent |
| --- | ---: | ---: | ---: |
| 9012546 | 8.72835086241 | 0.299200632696 | 0.066711361697 |
| 9015401 | 5.29786110408 | 0.298267939374 | 0.031871076481 |
| 2107214 | 1.64107401079 | 0.326258069537 | 0.391949163863 |

These fitted d changes are phenomenological parameters, not independently measured
strain or cell uncertainties. The stable Delta Rwp against the original canonical
two-phase baseline is 0.0376349809948. That baseline retains the fixed-d model;
this diagnostic improvement is not a fair new-model candidate ranking. Every
candidate and a common selected-only baseline would need the same model freedoms
before comparing their improvements.

The fixed-parameter independent audit directly decodes the three SQL cards, checks
all 8,301 original observations and weights, evaluates complete IL/Bragg profiles,
and enumerates all eight active subsets using unit-column SVD instead of SciPy
NNLS. It reproduces Q=163338.384345073 and the scales within the original limits;
normalized KKT is 9.813e-16 against 4.719e-10. All 444 input/output bindings remain
unchanged. The complete two-term reduced Jacobian has local rank six and condition
34.6451 in shift/H0 and e/log(1.005) coordinates; this says nothing about global
uniqueness or physical validity. The audit SHA-256 is
`8c8823d669e1e3d2bd3ec67a81b90497a5ad1e92afff03ec86e3d4958a2b4578`.
No additional real outer optimization is used for this verification.

`bi2mo3o12-isotropic-pilot-01/execution-01` retains the complete attempts, counters,
seals and terminal receipt. `visualization/output-02` in the same pilot directory
contains independently evaluated full-range plots, curves and parameter changes,
with original Q checks before publication. Neither the better conditional fit nor
the local Jacobian establishes Top-5 success. Current product acceptance remains
57 passes and the retained Bi2Mo3O12 failure among 58 ready development cases,
with nine unresolved cases and 22 untouched holdouts. The pilot authorization did
not include product-default integration, full-pool reranking or holdout tests.

The user subsequently approved the separate conditional-ranking proposal in
`bi2mo3o12-isotropic-reranking-proposal-01/proposal-v2.txt` (SHA-256
`9bf59c65daa0ea857daa17e32435552eff43beff614669bf0f28f67614a7dd04`). It covers only
this same development pattern's 464 remaining chemical candidates, with identical
stored-d freedoms for both selected phases and every candidate. A common two-phase
baseline uses nine neutral starts and at most 32 explicitly counted confirmation
fits; candidates use thirteen neutral starts. Every start retains a 300 evaluation
cap and the same tolerances. The total outer cap is 1,821,900, with at most eight
single-threaded candidate worker processes. Baseline instability stops the
experiment; no pool restart or mixed-baseline ranking is permitted. Prototype
dimension/control validation precedes real fits. This authorization does not cover
product defaults, other development patterns, holdouts or sequential acceptance.
The generalized two-/three-phase prototype passes independent 70-digit fixture
checks, all 83 synthetic starts under the unchanged 300-evaluation cap, 27 bounded
finite-difference checks and exact owned-tuple bridge comparisons. Mock driver
controls verify failed baseline starts, cooperative cancellation, the exact
32-confirmation cap, SQL closure, joined workers and canonical zero-improvement
fallback scoring. The original Windows test-directory case collision remains a
failed test setup artifact; the repaired control passes. Saved direct-Q baseline
transitions permit an independent reconstruction of the 128-epsilon stability
rule without another optimization. The root composite preflight verifies 516
bindings, including all 490 final plan pins, in
`bi2mo3o12-isotropic-reranking-controls-01/generalized-gate-01.json`.

The approved `reranking-01/execution-01` has terminated after processing all 464
remaining candidates. Its common two-phase baseline
uses 191 outer evaluations across nine successful neutral starts and eight more
across four successful confirmation fits. The last Q improvement is
`1.8335413187742233e-9`, below the unchanged `5.0407042731843735e-8` stability
limit. Baseline Q is `199381.9332366103`, with Rwp_net `0.3352913808021217`.
Independent explicit-profile, four-subset SVD verification reproduces Q as
`199381.93323661038`, passes the original scale/KKT checks and verifies the exact
confirmation transitions and 530 immutable bindings. The early audit's original
NumPy-boolean JSON serialization failure remains preserved; only the output
converter changed before the passing audit. Evidence is in
`bi2mo3o12-isotropic-reranking-audit-01/baseline-audit-02`. This verifies the shared
baseline at fixed nonlinear parameters; it does not certify the later candidate
round.

The original unified execution returned 1, its numerical driver returned 3 and
the wrapper recorded intended exit 4. These are separate recorded statuses, not
inferred equivalents. There are 460 complete candidate records and four
`numerically_unresolved` records: 1534421, 1566716, 2003106 and 9017498. Their ten
failed starts each exhausted the unchanged 300-evaluation cap. Every candidate
has a saved numerical result, but the full round fails its all-start completion
requirement. The final source guard also fails: concurrent GUI development
changed 21 monitored Application/Infrastructure working files. Neither failure
is waived. No fits were restarted, and the successful-run `audit-v5.py` and
`report-01/run-v3.py` remain NOT_RUN because their prerequisites failed.

Artifact-only failure diagnosis is retained in
`bi2mo3o12-isotropic-reranking-failure-01/execution-02`. Its actual exit is zero.
It checks the original numerical seals, all 464 candidate identities, neutral
starts, counts, fallback scores and stable sorting, and verifies 502 remaining
frozen bindings. The 21 pre-run source files are recovered into inert evidence
copies with exactly their original pinned hashes; current GUI files are untouched.
One file needs its original CRLF checkout representation. The first diagnostic's
Git-blob line-ending assertion failure remains in `execution-01`; the additive
repair restores only that byte representation and does not change a numerical
assertion or the original source-guard result. Explicit full IL/Bragg evaluation
and exhaustive SVD NNLS independently reproduce Q, scales and KKT at the saved
nonlinear parameters for the common baseline and 13 candidate winners, including
every card above the target and all four unresolved winners. This narrow check
does not replace the unexecuted full success audit or establish global minima.

The saved conditional ranking is provisional:

| Saved rank | COD card | Formula | Q | Delta Rwp_net |
| ---: | --- | --- | ---: | ---: |
| 1 | 9011351 | Bi2 Mo O6 | 157536.289093 | 0.0372546482065 |
| 2 | 1530868 | Bi2 Mo O6 | 158230.461371 | 0.0365987313083 |
| 3 | 2106472 | Bi2 Mo O6 | 158892.648916 | 0.0359743760290 |
| 4 | 1528796 | Bi2 Mo O6 | 158955.406136 | 0.0359152718199 |
| 5 | 1537010 | Bi2 Mo O6 | 159262.736811 | 0.0356259987645 |
| 6 | 1529758 | Bi2 Mo2 O9 | 160001.089167 | 0.0349321679432 |
| 7 | 2104407 | Bi14 Mo5 | 162244.407534 | 0.0328338823206 |
| 8 | 2107214 | Bi2 Mo3 O12 (target) | 163338.384345 | 0.0318158946970 |

All seven better cards and the target completed all thirteen starts. Thus the
saved complete rows already put the target outside Top-5; unresolved candidates
cannot remove those seven better saved scores. The target Rwp_net is
0.303475486105. The same-formula leading cards are not merged to rescue card-level
Top-5, and neither formula equality nor element compatibility proves a common
phase. These formulas are metadata from the frozen original round, inspected
after ranking; no truth-based chemical exclusion was added.

Independent residual curves retain every original point and fixed weight.
Against leading card 9011351, the target has 8010.113823 more Q in 25-30 degrees
and 4339.996301 more in 45-50 degrees, but 2014.267321 less in 10-15 degrees and
3261.657809 less in 15-20 degrees. The full-range Q difference is 5802.095252.
These fixed five-degree partitions describe the saved joint fits; they are not
new fitting windows or scores, nor attribution to candidate intensity alone.
Both leading and target winners have no boundary hits. The target d change is
0.391949% and shift 0.326258 degrees; the leading candidate has -0.115750% and
0.033430 degrees. These remain phenomenological fit parameters. Full-range and
key-band plots in `failure-01/visualization-01` pass their original Q and partition
checks and visual inspection. Plotting uses the compatible installed NumPy 1.26.3
and Matplotlib 3.8.0; the numerical audit retains pinned NumPy 2.4.6. The earlier
Matplotlib/NumPy ABI availability-probe failure is separately recorded.

Observed wall time is 22493.375 seconds with eight candidate processes; aggregate
worker time is 174383.9 seconds and driver peak RSS is 643039232 bytes. Concurrent
GUI validation and supplemental work prevent a controlled speedup claim, and
driver RSS is not aggregate pool memory. This diagnostic has not achieved Top-5
and does not promote stored-d scaling into the product. Frozen development
acceptance remains 57/58, with the original Bi failure, nine unresolved nonpasses
and 22 untouched holdouts. Further model changes require a concrete,
source-supported proposal under the existing approval boundary.

An additional analytical stress diagnostic keeps H, d/I, shifts/e and scales at
these saved solutions. It evaluates no altered width or intensity and performs
no outer fit. Independent 70-digit differentiation checks 18 generic IL center
and log-width derivatives. Existing Q and unit-column NNLS orthogonality checks
pass. At the target joint solution, d(Q/Dnet)/d(log H) is +0.0746988151 for
selected Bi2O3, +0.0137639333 for selected MoO3 and -0.00807133685 for the target;
the leading-card joint solution has the same respective signs. Positive values
favor locally narrower widths, negative values locally broader widths. Opposing
sensitivities motivate testing the shared-width assumption, but do not establish
physical phase widths, a new optimum or improved identification. Individual
fixed-line intensity sensitivities also remain nonzero. The original Geometric
width comes from sample 1637 at 27.6917955 degrees. Six nonzero components from
the two selected phases and radiation doublets lie within its H0/2 neighborhood,
spanning 0.134825672 degrees in the saved target fit. This is descriptive
crowding evidence, not proof of a width-estimation bias or a line assignment.

Evidence is in `bi2mo3o12-fixed-model-stress-01/execution-02` and
`bi2mo3o12-fixed-model-stress-01/source-width-context.json`; actual diagnostic
exit is zero. The original NumPy-boolean serialization failure remains in
`execution-01`, and the additive
JSON-only repair repeats all three output summaries exactly. The subsequent bounded
experiment in `bi2mo3o12-phase-width-proposal-01/proposal.txt` (SHA-256
`0b5a90cd48be1ea31993ba463dfc3db56c35cf58d13e46c6de0a8677cbfb7d7a`)
retains Geometric H0 as an anchor but fits each phase's constant
H_j=H0*exp(h_j), h_j within plus/minus log(2). This explicitly changes the
common fixed-H0 contract. The user authorized it on 2026-10-12, with the sole
execution override of twelve candidate processes and one native numerical thread
per process. The approval covers independent synthetic gates and, only after
they pass, one uniform 464-card conditional ranking under a frozen 2,658,300
outer-evaluation cap. Product adoption, other patterns, sequential acceptance
and holdouts remain excluded; the product default still shares one fixed width.

The isolated prototype in `build/pfsm/bi2mo3o12-phase-width-01` compiles a
read-only, byte-identical copy of the current Core direct IL expander, with no
GUI or Application dependency. Its declared 4,012-file closure binds the actual
compiler dependencies, bridge, numerical scripts, Python/native runtime,
observations and original four database files. This new declaration does not
waive the earlier failed working-source guard. Width-zero and dilation/width-zero
profiles match the original frozen bridge bit-for-bit. Independent 70-digit
fixtures and conditioning-derived width-recovery limits precede all fits;
all 172 neutral starts across ten two-/three-phase fixtures pass the original
Q, scale and physical-shift limits. Actual one-/twelve-process results are
bit-identical, with all twelve spawned workers initialized. The full reduced
bounded three-point fallback passes 288 rational checks across shift, dilation
and width axes; native block cancellation and exact shared solver budgets pass.
The original Windows dependency-path parser and mock decimal-seed expectation
failures remain preserved separately, with no numerical tolerance change.

The real experiment uses thirteen neutral two-phase baseline starts, at most
32 own-result confirmations, and nineteen neutral starts for every candidate.
Each start retains 300 outer evaluations, NNLS maxiter 270 and all three
tolerances at 1e-8. Both earlier fixed-d and fixed-width isotropic baselines
remain feasible comparisons, never learned starts.

The single authorized real round has now terminated with genuine unified root,
driver and all twelve joined worker exits zero. All 464 cards complete their
nineteen neutral starts: 8,816 successful candidate starts, with no exhausted
budget or missing evaluation counter. The thirteen baseline starts and three
own-result confirmations bring the total to 8,832 starts. Reported outer work
is 238,265 against the unchanged 2,658,300 cap. Successful stopping does not
prove global minima or nonlinear stationarity.

The independent terminal audit passes all 4,507 source/output bindings, all
8,301 original observations, the 466 ordered SQL cards and five original
raw/evidence/Geometric-width hashes. Explicit IL/Bragg plus exhaustive SVD
active-subset checks pass at eleven saved parameter sets: the common baseline,
seven leaders, the target and two feasible baseline-plus-zero fallbacks. The
fallback checks certify their saved feasible curves without claiming joint KKT.
No real outer reference fit is added. Truth is read only after this audit.

The complete conditional result is `FAIL_TOP5`: the sole frozen missing-phase
card, COD 2107214 (Bi2Mo3O12), ranks 16 of 464. The common new-model baseline has
Q=182464.819593943 and Rwp_net=0.320751768097. Its target joint fit has
Q=128115.417270900 and Rwp_net=0.268769625220, giving Delta Rwp_net=0.0519821428771.

| Target comparison | Saved card rank | Target Rwp_net |
| --- | ---: | ---: |
| Original fixed-d/fixed-width conditional row | 10 | 0.310934959909 |
| Earlier fixed-width stored-d diagnostic | 8 | 0.303475486105 |
| Complete phase-width diagnostic | 16 | 0.268769625220 |

The earlier stored-d run still has a nonzero terminal, incomplete starts and its
failed working-source guard; its saved rank and independently checked curve are
qualified comparisons, not a successful experiment. The new target Q decreases
21.5644% and Rwp decreases 3.470586 percentage points relative to that saved fit,
but competing cards improve enough to move the target further down the ranking.
The leading card, COD 9011351 (Bi2MoO6, Pca2_1), has Q=117210.373112996 and
Rwp_net=0.257076579142. The first five cards are 9011351, 1530868, 2106472,
1537010 and 1528796, all annotated Bi2MoO6 in SQL. They remain distinct cards;
formula annotations neither group polymorphs nor establish true phase identity.
The same source-supported Bi/C/H/Mo/O/Zr union remains in force.

The target joint widths, in selected Bi2O3 / selected MoO3 / candidate order,
are 0.123114826920, 0.165752187903 and 0.360496647301 degrees. The candidate is
at the exact approved upper bound, twice Geometric H0. The first sixteen
candidates and 401 of 464 saved candidate winners also hit that exact width
bound; 462 winners have a positive candidate scale. These are fitted model
parameters, not measured widths. This result does not justify extending the
bounds or adopting free phase widths as the product default. In the fixed
five-degree partition, the largest target-minus-leader Q gaps are 25--30,
45--50 and 30--35 degrees; the full signed sum is 10905.0441579. Every native
point remains in the objective, including bands favoring the target.

The numerical driver takes 3428.453 seconds (about 57 minutes 8 seconds),
including 3388.297 seconds in the candidate pool, 13.547 seconds for baseline
work and 0.109 seconds for SQL. Sampled simultaneous RSS of the driver and its
own descendants peaks at 1,165,873,152 bytes, about 1.09 GiB. This is one
twelve-process run, not a speedup comparison or an exact allocation peak;
synthetic verification and terminal auditing are outside that driver time.

Evidence is retained in `bi2mo3o12-phase-width-01/execution-01`, `audit-03`,
`report-03`, `result-diagnosis-01` and `visualization-01`. Independent native-point
curves preserve Q and complete fixed-band partitions; both scientific PNGs were
visually inspected. Original audit failures remain in `audit-01` (mistaken six-file
expectation) and `audit-02` (receipt/card-order variable shadowing). The original
extra-source negative control was outside the raw-source tree and remains in
`audit-controls-03`; corrected location controls and the final auditor pass.
These additive tool repairs preserve all fit outputs, numerical functions,
tolerances and earlier failures. Product acceptance remains 57 passes / 58 ready
development cases, nine unresolved nonpasses and 22 untouched holdouts. This
completed conditional experiment supplies no sequential or end-to-end case pass.

The subsequent saved-parameter analytical postmortem adds no fit or changed
experimental profile. All nineteen target starts stop by `ftol` with Q between
128115.417270900 and 128115.417505291, a spread of 0.0002343911; every target
width reaches the same upper bound. At the saved target, the full scale/shift/
dilation/width tangent restricted to the active bound face has rank eleven and
column-normalized condition 8.05016. Its linearized residual projection is only
8.68356e-6 in Q units, compared with the 10905.0441579 target-minus-leader gap.
The leader's corresponding projection is 0.000244810. These are local linear
diagnostics, not achieved improvements, nonlinear error bounds or global-optimum
certificates. They do not support explaining the gap by local endpoint accuracy.

An exact signed symmetric decomposition, using the midpoint of the two saved
weighted residuals, allocates that Q gap as follows:

| Saved component change, target to leader | Signed contribution to Q_target - Q_leader |
| --- | ---: |
| Selected Bi2O3 component | +2.691550 |
| Selected MoO3 component | -12.368755 |
| Third template component | +10914.721363 |

The identity includes all native points and their actual joint parameter changes.
It is algebraic attribution, not a causal experiment replacing one component.
Both solutions also retain a large underprediction at 39.27542054 degrees.
For the target, the observed count is 4116, fixed background 739.3341064453125,
and saved Bi2O3/MoO3/target contributions are approximately 1.10/1011.62/47.90.
The closest retained MoO3 K-alpha1 centre is 39.23348872 degrees. A nearest
reference line is only a geometric annotation, not an experimental assignment.
This shared absolute residual must be distinguished from the signed ranking gap
before attributing failure to peak positions or relative intensities.

At the target and leader, d(Q/Dnet)/d(h/log(2)) for the third width is respectively
-0.00389922 and -0.00813094: both local gradients point outside the approved box.
They neither validate physical widths nor justify a larger bound. Evidence in
`bi2mo3o12-postwidth-diagnosis-01/execution-02` passes 162 independent 70-digit
scalar derivative checks, prescribed projection/identity controls, bitwise saved
component comparisons and 4,516 unchanged bindings. The initial old-record
adapter failure remains in `execution-01`; the additive repair explicitly uses
the old model's fixed h=0 without changing numerical functions or any fitted
parameter. No new optimizer, reranking, product model or holdout is involved.

A read-only review of the original manual-refinement workbook supplies a
specific relative-intensity hypothesis. In
[the author workbook](../../tests/real_example_data/02_multiphase_benchmarks/06_real_world/pairwise_reactions_manual_rietveld_summary.xlsx),
`Notes on fits!C4` reports that preferred orientation along (040) improves the
MoO3 fit somewhat. The numeric orientation parameter, correction function and
original TOPAS input are not supplied here. This is case-informed author
evidence, not a blind accuracy reference or proof that correcting MoO3 will
improve target ranking. The author also refines lattice parameters, sample
displacement and Lorentzian size broadening, with an independently calibrated
instrument profile and sample-specific polynomial background. Its Rwp 8.20%
and Rwp-dash 18.70% therefore are not PFSM acceptance targets. The workbook's
`CrySize_L` header does not state units; no width conversion or initialization
is inferred from those values.

Artifact Tool import and an independent raw-XML reader check 186 target-scoped
cells. Values/formulas match exactly in 173; thirteen stored empty strings
import as null and remain explicitly recorded representation differences.
Five parser controls and rejection of a forged numeric cell pass. The original
strict comparison failure at a blank cell and initial dependency-access failure
are preserved. Neither original workbook is edited or exported. The XML audit
also rechecks all four frozen COD database files.

The selected MoO3 card 9015401 and its
[original COD CIF](https://www.crystallography.net/cod/9015401.cif) use the same
Pbnm cell/axis setting as the workbook. A source-only enumeration independently
matches all 243 retained unsigned allowed HKL classes one-to-one; all recorded
multiplicities equal source-symmetry orbit sizes and all eight symmetry
operations preserve each family's squared angle to b*. Six extinction controls
pass. At already saved target parameters, the nearest Mo K-alpha1 centres to
the two annotated residual points are (040) at 25.96907071 degrees and (060) at
39.23348872 degrees. They share the author-mentioned direction, but nearest
centres do not establish experimental assignments or the cause of the signed
target-minus-leader gap.

The largest stored-d/source-metric discrepancy is 1.04453212e-6 Angstrom and
the smallest other allowed-direction separation is 1.46779671e-5 Angstrom.
Descriptive coincidence bins do not establish the historical SQL merge
tolerance or unknown cross-family intensity allocations. This narrow Mo
family analysis does not remove the earlier whole-pool physical-deformation
limitation. Evidence in `bi2mo3o12-refinement-source-review-01/xml-audit-01`
and `Mo-geometry-02` has actual terminal exit zero and unchanged source hashes;
the original download and importer failures remain retained.

One subsequent proposal is prepared only: a single shared selected-Mo
orientation-contrast parameter, with the
[March-Dollase functional form](https://onlinelibrary.wiley.com/iucr/itc/Ha/ch3o2v0001/sec3o2o3o5/),
a fixed b* axis and r in [0.5,2], followed by one fair 464-card twelve-process
conditional round after independent synthetic gates. Every candidate would
receive the same selected-component freedom; its own relative intensities and
the preceding width/dilation/shift bounds would remain unchanged. The proposal
explicitly changes selected-Mo relative intensities and needs new human
authorization before any future execution. The human subsequently ended related
testing; this proposal is closed without approval or execution. No corrected
profile, prototype, fit or ranking has run.
Product defaults, the retained failure, 57/58 development passes, nine unresolved
nonpasses and 22 untouched holdouts remain unchanged.

The four APS CSV inputs declare 34,500 acquisition steps but contain 47,998
tabulated rows. The adapter records both values and uses every explicit coordinate
and its supplied intensity/sigma; it never truncates the table or regenerates a
uniform grid from the header. APS documents calibration and merging of its twelve
detectors, including angle/wavelength/intensity corrections
([beamline calibration](https://wiki-ext.aps.anl.gov/ug11bm/index.php/Calibration)).
This explains why acquisition metadata need not describe the final table; the
exact origin of these particular header counts is not independently established.
Their normalization factors are also retained as supplied metadata. The signal and
sigma tokens are already in the same supplied units; the adapter applies no second
normalization. Unknown scaling provenance remains unknown in the CLI. A declared
scaled signal may use supplied sigma but cannot masquerade as raw Poisson counts.

`--prepare-only` saves Geometric FWHM and rejection diagnostics without searching.
`--case ID` selects an input. `--profile-candidates N` chooses the first N database
IDs in SQL ID order independently of truth for one profiling round. Its status
is always `profiling_not_acceptance`. No peak-table or truth-card prefilter is
used. Missing width is a failed prerequisite, never a
constant-width fallback.

`--chemistry evidence` is the default primary mode. Each supported case supplies
`chemical_constraints` with `kind: allowed_elements`, a unique element list, an
independent composition basis and source-file evidence with hashes and locators.
The preparer freezes these explicit fields separately from structural truth;
it never derives elements from accepted card IDs or fitted phase assignments.
Missing or partial composition information is `chemical_evidence_pending`, not
permission to silently run an unconstrained primary case. `--prepare-only`
remains an observation/FWHM check without a candidate query.

The existing Application element-subset query admits a card with at least one
listed element and no element outside the allowed union. A multiphase sample
does not require every individual card to contain every sample element. Known
reagents, documented impurities or possible preparation residues must be treated
according to their evidence; nominal recipes are distinguished from elemental
assays. Positive detection of some elements alone does not justify excluding all
others. Uncertainty and unresolved source evidence remain explicit.

The chemistry freeze covers all 89 existing recipes before constrained rankings.
It preserves all observation bytes, structural truth rules, readiness states and
development/holdout assignments. Allowed unions come from the QualX original
sample descriptions, NIST certificates, IUCr weighed starting materials and
preparation notes, and the Dara author supplement and original reagent workbooks.
They describe nominal major materials with documented exposures, not exhaustive
trace-element assays. In particular, documented ethanol/zirconia milling adds
C/H/O/Zr to the Dara mixtures and reaction inputs; precursor scans do not inherit
unestablished milling histories. CPD petroleum and glass constituents and the
NIST 676a manufacturing precursor are retained conservatively. Unknown ceramic,
alloy, impurity or processing constituents remain explicit qualifications; these
runs cannot establish trace-phase completeness outside the declared scope.

Each recipe pins the chemical source SHA-256 and a page/table/workbook locator.
The preparer refuses changed pinned sources and freezes paths relative to the
manifest; the benchmark verifies them before selecting candidate cards. Local
QualX manual/HTML snapshots are exact copies, while the IUCr source capture is
explicitly a text extraction rather than original server PDF bytes. These local
sources live at the paths listed in the recipes and must be restored with matching
hashes to reproduce this real-data freeze; they are not normal-test dependencies.
Missing sources or unsupported chemistry cannot silently become unconstrained
primary runs. The nine pre-existing unresolved cases remain unresolved even when
their starting-material chemistry is supported.

Use `--chemistry none` with a new output directory for supplemental unrestricted
searches. Report their candidate counts, per-round ranks, remaining improvements
and timings separately from primary constrained results; never pool the two
success rates. Chemistry changes the candidate universe only. Background,
weights, Geometric width, radiation, NNLS/TRF and Rwp scoring stay the same.

An allowed element union does not establish stoichiometry or crystal structure.
Different compounds containing the same allowed elements can still improve Rwp.
Their structural identities must be assessed separately under the frozen truth
mapping; chemical filtering alone cannot turn a spectral match into a phase proof.

## Output and success

The run records manifest/database hashes, database version, compiled revision,
executable hash, mode and threads. The revision is refreshed during incremental
builds; source archives without Git metadata report `unavailable`. Git is not
invoked by the benchmark at runtime. Every case saves candidate order and its hash.
`round-N.jsonl` retains all ranked cards, temporary joint parameters, Q, Rwp,
improvements, terminations and diagnostics. `round-N.json` adds baseline values,
true-phase card ranks, known-mapping phase ranks, false cards in the first five,
timing and memory. Phase rank collapses only known mapped phases; each unmapped
card remains separate. It is not a full database phase classification.
`baseline-N.tsv` retains aligned observations, background, weights, calculated
spectrum and residual. Peak resident memory is a cumulative process high-water
mark, not an isolated case or per-candidate allocation.

The artifact-only `current-performance-summary-58-10/summary.json` preserves all
177 rounds and 118,709 ranked/evaluated rows from the 58 terminal development
cases in aggregate snapshot 19, with source hashes, candidate counts and actual
threads. Its 37-case/119-round predecessor remains unchanged, and those original
values match exactly. Root verification checks all 122 source bindings and raw
round metrics. Database, sorting and candidate-loop
times are wall durations; template time is summed worker elapsed time, and solve
time includes baseline refinement and summed candidate-worker elapsed time.
Worker sums overlap across threads and are not CPU time. CLI elapsed time resets
each round. Concurrent workloads and resource controls varied, so these records
do not establish isolated speedup or a performance threshold. The retained
Bi2Mo3O12 failure is included; pending and unresolved cases are excluded.

After product ranking, the evaluator chooses the highest ranked card in the
first five belonging to an unaccepted true phase and submits ordinary checked
acceptance. The product jointly refines and reranks. Success requires all true
phases; the final post-acceptance ranking reports remaining improvement. Truth
card identities and structural phase assignments never enter constraints,
initialization or fitting; independently supplied composition evidence does.
Weak-phase recall is only
reported when the input supplies a weak-phase set. This is assisted identification,
not automatic stopping. Uncovered truth, missing evidence, failed width, solver
errors, cancellation and Top-5 failures cannot count as success.

Long runs print periodic work counts. During candidate fitting, only the OpenMP
primary thread invokes the progress callback on the search execution thread;
other workers touch private result slots. Progress carries no candidate publication,
and callback errors or cancellation still join all workers before returning failure.

## Current evidence

The chemistry integration passes the affected CLI/input/strict-architecture
checks on Windows GCC, Windows MSVC and Linux GCC. The analytic fixture includes
a chemically disallowed reference whose overlapping peaks yield a better Q,
and checks its exclusion plus retention of the complete allowed pool across
rounds. Independent freeze07-to-freeze08 comparison verifies all 89 identities,
statuses, splits and truth mappings, all 84 serialized observations and structural
mapping bytes unchanged. All 254 chemical evidence entries across 86 source files
match their pinned hashes. Fresh preparation succeeds for all 80 ready cases;
the nine unresolved cases retain their reasons. These checks establish input and
query readiness, not completed primary real-data identification.

The frozen `ff971c63` primary executable subsequently completed the five
development NIST examples, all ten Dara precursors, and example1: all sixteen
passed complete Top-5 assisted identification including the final post-acceptance
ranking. NIST 660c used 40 initial candidates and example1 used 849. Example1
accepted Silicon, LaB6 and Corundum in that order, each ranked first; its final
baseline Rwp_net is 0.35670028849827556 and remaining maximum improvement is
0.000471738393221147. Its four rounds retain all 849/848/847/846 cards, with no
baseline rebuild or feasible fallback. This is a completed development subset,
not a pass for the remaining mixture/reaction cases or any holdout case.

The subsequent frozen `primary-protocol-audit-02/snapshot-07` plus the separate
four-NIST audit cover 34 completed development cases: 33 pass and one fails the
original Top-5 criterion. In the V2O5/ZnO 8-minute case, the accepted V2O5 mapping
first appears at rank six after Zincite acceptance. The original manifest08 and
failed output remain unchanged. Source-based investigation identifies a missing
a/b Pmmn axis setting for rank-four COD 3000429; its original CIF refines noninteger
occupancies, while the local database represents its formula as O5 V2. An exact
axis/origin symmetry proof supports a separately versioned mapping correction,
with existing formula and cell tolerances preserved. The correction is frozen
in manifest09, which adds only COD 3000429 to this phase across 23 cases
(13 development and 10 holdout). All 89 case identities, readiness states and
splits, 84 serialized observations and 254 chemical evidence entries remain
unchanged. The 12-card causal regression retains the old failure and checks
the exact formula, existing settings, unchanged 3%/1-degree limits and rejected
unproved settings. A new real run is required; manifest08's failed result is
never relabeled (`build/pfsm/v2o5-mapping-batch-01`).

The subsequent frozen snapshot08 plus the separate four-NIST audit cover 44
completed ready development cases: 42 pass and two fail Top-5. The second failure
is the Bi2O3-2MoO3 reaction sample: after two accepted phases, the first frozen
Bi2Mo3O12 card ranks 18. Snapshot08 independently checks every retained pool and
row, including 1,395 rows for that failure. One additional completed driver entry
is the previously frozen Bi2O3-Nb2O5 truth-incomplete case: no query ran, so it is
reported separately from the 44 ready cases. Fourteen ready development cases
remain at that snapshot. All holdout rankings remain unrun.

Snapshot09 of the original executable, together with the separate four-NIST
audit, reaches 50 completed ready development cases: 48 pass and the same two
Top-5 failures remain. Five completed unresolved entries are recorded separately
and execute no query. These counts belong to the original executable/manifest08.

Snapshot10 completes the original Dara driver: 29 passes, two Top-5 failures
and nine unresolved inputs, each rejected before querying. Combined with the
eight completed CPD-1 cases and sixteen earlier NIST/precursor/example1 cases,
the original protocol has 55 completed ready development cases: 53 pass and
two fail. CPD2, CPD3 and MIXT_5 remain pending at this snapshot. The nine unresolved
inputs are separate nonpasses, not part of the 55 ready-case denominator.
Snapshot11 then adds a complete CPD2 pass: 56 of 58 ready development cases
are complete, with 54 passes and the same two original failures; CPD3 and MIXT_5
remain pending. The original snapshots keep their own frozen counts and hashes.
Snapshot12 adds CPD3: 57 of 58 ready development cases are complete, with 55
passes and the same two failures; only MIXT_5 remains pending at that snapshot.
Its 11,426 rows retain all four candidate pools and include four feasible
fallbacks, two without checked KKT. CPD3 identifies its three crystalline phases;
the recorded glass fraction is outside that success claim.
Snapshot13 completes MIXT_5 and the original development cohort: all 58 ready
cases have terminal results, with 56 passes and the same two original Top-5
failures. The nine unresolved cases remain separate nonpasses within the 67-case
development split. MIXT_5 retains all six candidate pools and 11,331 rows, including
39 unchecked feasible fallbacks; its five phases are accepted and the final
remaining maximum improvement is 0.005875017255190561. These original-version
results do not substitute for affected-input retests or the 22 untouched holdouts.

Fresh runs on revision `37ef053c` and manifest09 pass NIST 660c, example1 and
the V2O5/ZnO 8-minute case. The latter accepts Zincite first and the independently
proved V2O5 card at rank four, then completes its final reranking; remaining
maximum improvement is 0.004119205875434874. Independent checks verify all 4,891
rows and retained pools across these three cases. Pinned independent NIST fits
and the V2O5 conditional/accepted joint fits pass the original tolerances after
exact fixed-input checks. The original manifest08 failure is preserved; repeated
cases do not increase the distinct-case total. The other eleven ready development
inputs affected by the mapping correction also complete and pass in their separate
driver. Its final snapshot06 checks all 17,267 rows and retained pools, including
146 feasible fallbacks, of which 68 have unchecked KKT diagnostics; no baseline
rebuild occurs. Thus all twelve ready development cases affected by the mapping
change have fresh passing results. These runs cover the named retested cases;
acceptance of the remaining development and holdout cases is still open.

The repaired cache can change raw solver diagnostics: example1 retains every
round's Top-5 ID order but has eleven unchecked baseline-plus-zero fallbacks
where the original run had none. These remain explicit feasible solutions, not
KKT certifications. No overall bitwise-equivalence claim is made
(`build/pfsm/current-development-audit-09/summary-addendum-01.json`).

The Bi2Mo3O12 failure also exposes an input limitation. COD 2107214 reports 1,970
generated reflections but stores 500 paired lines, whose last K-alpha centres
are 76.07/76.30 degrees; the observation extends to 100.09 degrees. All four
database schemas were checked: no additional paired intensities are available.
The QualX source distinguishes generated count from retained output after its
500-slot selection; PFSM consumes every stored pair and adds no further cap.
IL tails still contribute above the last centre. This is a reference-coverage
limitation, not proof of the failure's cause or authorization to replace the
frozen reference. Independent nonlinear-fit and source-cell investigations remain
separate (`build/pfsm/bi2mo3o12-coverage-independent-01/summary.json`).

The original COD 2107214 CIF was subsequently retrieved and hashed. Regeneration
with the existing importer produces 1,970 reflections, 1,967 merged physical
lines within its existing generation range from 1 to 150 degrees, and the retained
500-line presentation. Independent reciprocal geometry,
symmetry, ionic structure factors and powder-intensity calculations agree;
all 500 stored HKL families match the regenerated presentation. Their numerical
values are close but not identical. The CIF supplies zero isotropic displacement
parameters, so this reconstruction explicitly uses the accepted importer defaults,
not measured displacement parameters. There are 480 additional K-alpha1 centres
inside the observation window. These source checks do not establish their effect
on identification (`build/pfsm/bi2mo3o12-full-reference-01/summary.json`).

A bounded diagnostic compares the original SQL 500 lines, regenerated 500 lines
and full 1,967 lines at the same explicitly reused first-start independent
three-phase shifts, resolving all three nonnegative scales for each variant.
Q is respectively 517,455,633.42, 517,454,436.01 and 519,635,751.69. Thus completing
the regenerated reference increases Q by 0.4215% at these shifts, principally
above 76.55 degrees; it does not support a simple missing-lines remedy. Every
generated reachable line contributes its full IL tail, including centres above the data
window. The shifts are provisional, not the final result of the independent
multi-start calculation. This is a fixed-shift input diagnostic, not a nonlinear
refit, ranking result or proof of the original failure's cause. No product input,
database or accepted 500-slot import contract changed
(`build/pfsm/bi2mo3o12-fixed-shift-01/summary.json`).

The current `37ef053c`/manifest09 Bi2Mo3O12 rerun remains `failed_top5`: Bi2O3
is accepted first, MoO3 second, and COD 2107214 still ranks 18. All three Top-5
ID sequences match the original; every one of its 1,395 rows has direct final
verification and checked NNLS KKT diagnostics. Raw solver diagnostics are not
identical, and neither result replaces the other.

The original independent 33-cubed grid and 30-start SciPy calculation completes
with all refinements reporting successful optimizer termination. Its strict
product comparison nevertheless fails: Q error 1.09036 and all scale errors
satisfy the original tolerances, but the target shift error is 2.05241e-5 degrees,
above the unchanged 2e-5-degree limit. This failure is preserved separately from
the Top-5 identification failure. A `cost` termination need not meet the gradient
stopping condition or independently guarantee a physical-parameter tolerance
(`build/pfsm/failed-bi2mo3o12-joint-01/2107214/comparison.json`).

The independently sealed comparison against the current executable has the same
target-shift failure after exact observation, background, weight, width, radiation
and phase-order checks. Eight starts reproduce the best basin, and a reduced
residual Jacobian check at two step sizes gives a stable local shift rank with
condition number 5.39. The mismatch cannot be dismissed as template ambiguity.
The best independent Q found is still 24.79 million above the current fifth card;
the measured 1.09 objective discrepancy does not explain the Top-5 failure. These
finite scans and refinements do not prove a global optimum. The original Jacobian
diagnostic's NumPy-boolean serialization failure is retained alongside its narrow
serialization-only repair (`verified-summary.json` and `current-comparison.json`
in the same directory).

An isolated replay then reproduces the current candidate's Q, all parameters and
all evaluation counts exactly; instrumentation leaves its output byte-identical.
At evaluation counter 23 of the winning original start, after 22 trial evaluations,
normalized cost decreases by
1.20026e-10, below `ftol * cost = 2.93376e-10`, with actual/predicted ratio 1.58307.
The step condition is false and the 300-evaluation budget is not exhausted.
Thus the original cost stopping predicate explains this endpoint; the independent
physical-shift comparison remains failed. No new starts, tolerance, model or
shared executable changes enter this diagnostic
(`build/pfsm/bi2mo3o12-trf-replay-01/summary.json`).

The separate source-count replay in `bi2mo3o12-sourcecount-replay-01` compiles
and runs successfully with the bound current libraries, but fails its required
exact comparison with the frozen candidate row. Q differs by 5.82e-10 and the
largest shift difference is 4.19e-12 degrees; all evaluation counts and the cost
termination agree. These small differences do not waive exact replay or the
separate failed physical-shift comparison. The original replay failure remains,
and input/operation-order investigation precedes any new trace. The old
equal-weight trace does not establish the source-count termination cause.
The subsequent capture-only `bi2mo3o12-input-capture-01/execution-01` compares
the actual public Qt/Application preparation and strict paired SQL path with the
existing istream transport. All 234,932 common finite binary64 input/template
values agree bitwise, including direct/cached values and derivatives at zero and
baseline shifts. Stage-labelled rounding/MXCSR, options and cache-mode records
also agree; all 343 sealed inputs remain unchanged. Compilation and both routes
exit zero, and independent artifact verification agrees. Only the transport has
its independently calculated baseline denominator. This rules out a difference
between these current assembly routes; it does not reproduce historical process
memory or waive the original replay failure. The subsequent scan capture in
`execution-03` agrees on 235,066 common finite binary64 values, all 25 scan
evaluations and all five ordered starts; 350 sealed inputs remain unchanged.
Both routes stop before nonlinear fitting. The original execution-02 compile
failure and its namespace-only repair are preserved.

The same-executable startup A/B in `bi2mo3o12-startup-ab-01` then executes the
unchanged five-start fit with and without QCoreApplication construction. Both
native processes exit zero; all thirty captured numerical bit patterns, starts,
results, diagnostics and stage-labelled floating-point states agree. This excludes
that constructor branch as the observed cause in the current controlled run;
both modes share DLL loader startup and do not reproduce historical process
memory. The original parser path-separator failure is retained, and artifact-only
recovery parses the already sealed outputs without rerunning either fit. The
historical exact-replay and physical-shift failures remain unwaived.

Independently prescribed synthetic inputs in
`fixed-model-confirmation-synthetic-02` freeze twelve IL cases before product
evaluation, with residuals orthogonal to the scale and shift tangent columns.
High-precision derivatives and positive reduced Hessians establish strict local
minima at the prescribed points; they do not prove global uniqueness or an
existing product failure. Phase permutation and observation/weight scaling
controls are explicit. Binary64 input-rounding analysis remains separate from
product arithmetic and an optimized reference. The original fixture setup
failure is preserved in its sibling directory.

The separate `fixed-model-confirmation-reference-02` executes all fifteen
synthetic/control cases and all 120 prescribed independent starts with SciPy
1.16.2. All starts terminate without exhausting their budgets; all 17,185
residual calls and 120 additional final NNLS evaluations are retained. Each
case's best observed result agrees with its prescribed local point within
1.34e-9 degrees and 2.62e-10 in scale. Poorer converged basins remain recorded;
these finite starts do not establish global optimality or product accuracy.
An independent arithmetic audit reproduces every recorded Q within 2.85e-14.
The first reference attempt rejected an unexpected SciPy version before any
fit; its original failure remains separate from the successful local-runtime
repair. The unmodified current GNU Core subsequently passes all fifteen cases
in both direct and cached modes from one frozen zero start, retaining all thirty
outputs. Maximum errors against the independent best results are 2.27e-14 in Q,
6.54e-9 in scale and 9.74e-9 degrees in shift, within the unchanged tolerances.
All runs retain checked KKT and direct final verification. This is scoped Core
evidence; it does not establish Application search equivalence or explain the
real source-count failure.

Independent full-bound two-phase scans and multi-start SciPy refinements verify
the V2O5/ZnO failure's rank-one, rank-four and rank-six conditional fits against the
unchanged tolerances: all 30 refinements converge, and the smallest Q gap is
800.63 versus a maximum product Q error of 8.65e-8. An independent 33-cubed scan
and 17 refinements also verify the accepted CPD-1a three-phase baseline, with Q
error 1.10e-10. These checks supply the fixed background, weights, radiation and
Geometric width; they do not independently validate preprocessing or prove global
optimality. Evidence is in `failed-v2o5-zno-joint-01` and
`real-cpd-three-phase-reference-01` under the local validation directory.

The chemical-integration normal checkpoints finish at Windows GCC 100/103,
Windows MSVC 101/103 and Linux GCC 102/103. Original failures are retained:
GCC/Linux card-property timeouts pass scoped retries at the unchanged 180-second
limit (139.15/90.53 seconds); both Windows settings-migration and network-update
permission failures pass under the required system permissions with unchanged
programs and assertions. Qt-free tests pass 7/7 on all three platforms. Strict
boundaries, reference protection and actual build/zero-Fortran inspection also
pass. These are full-run results plus scoped retries, not uninterrupted 103/103
claims (`chemical-windows-gates-01/summary.json`,
`linux-chemical-normal-01/summary.json`, `linux-card-retry-chemical-01/summary.json`).
Later cache repairs require their own affected verification.

The later source-corrected checkpoint in `current-normal-10` completes Windows
GCC at 102/103, retaining the original 180-second card-property timeout, and
Windows MSVC at 103/103 without retries. Both Qt-free suites pass 7/7; source
boundaries, unchanged scientific inputs, reference protection and zero-Fortran
checks pass. MSVC card/powder properties take 151.88/1,459.63 seconds under their
original limits. Its compiled benchmark revision is `3decd4fe`; the summary's
later documentary HEAD does not identify the compiled revision. Linux's complete
run in `current-normal-linux-10` exits zero with 103/103 normal and 7/7 Qt-free
tests. Its sixteen build/check/test stages pass, all 1,894 recorded source inputs
and eight key artifacts remain unchanged, and card/powder properties take
102.02/905.67 seconds under the original limits. This reuses an existing
configured build and does not claim a fresh configure. The separately reviewed
GNU card retry passes in 147.67 seconds at the unchanged 180-second limit, with
the same native program, scientific inputs, command and test properties. This
is full-run evidence plus one scoped retry, not an uninterrupted three-platform
full-pass claim. Benchmark affinities are then restored to all twenty processors,
followed by the original primary Normal priorities; supplemental runs remain
BelowNormal. The first restoration attempt safely rejects automatically parsed
JSON dates before any mutation. The separate date-string repair preserves every
identity guard, and its successful restoration receipts retain that failure.

Complete NIST 660c runs with 1/2/4/20 threads have identical scientific rows,
parameters, curves, termination diagnostics and evaluation counts. Their timing
and process-memory fields differ and are reported as observations under concurrent
load. The primary first fit, accepted baseline and final top joint candidate also
pass the unchanged independent SciPy/Brent tolerances after verifying identical
observations, background, weights, Geometric width, radiation and reference input
hashes. The oracle results are reused independent computations, not new optimizer
runs. The remaining improvement is 0.010910818546357254 from another LaB6 card;
this does not establish an additional phase or automatic stopping.

Independent static chemistry enumeration agrees with all 89 frozen universes,
which contain 28 to 2,858 cards. A separate subsequent coverage check retains all
1,984 accepted-ID occurrences across the 80 ready cases; no truth information
was used to enumerate or modify those universes. The completed precursor audit
retains all 1,976 ranked rows, including six explicit baseline-plus-zero-candidate
fallbacks for ZrO2. All rows have direct final verification; fallback rows do not
claim KKT convergence. Evidence is under `chemical-universe-probe-02`,
`primary-protocol-audit-01`, `primary-nist-development-t4-01`,
`primary-NIST660c-thread-audit-01.json` and
`primary-NIST660c-reference-comparison-01.json`. Original audit expectation errors
are retained: covariance weights follow the documented reciprocal round trip,
and saved residual identities use a binary64 operation-order bound. No solver or
independent-reference tolerance was changed.

The product checkpoint before bounded baseline refinement passes all 103 normal
tests in Windows GCC and Linux GCC, with the separate Linux Qt-free suite passing
7/7. The full MSVC run
passes 101/103; its card and powder timeouts are preserved. Both affected retries
pass with their original 180/1800-second limits, at 160.57/932.55 seconds, after
reserving CPU capacity. The powder retry verifies 153,844 properties with the
original binary and input hashes (`final-msvc-powder-pass-01/evidence.json`).
This combined evidence is not a new uninterrupted 103/103 MSVC run. Benchmark
affinities/priorities were restored afterwards (`benchmark-affinity-restored-01.json`).
The Linux archive-capture failure and collector-only repair are retained separately;
strict boundaries, Qt-free compilation and zero-Fortran checks pass on all targets.
Subsequent test-only rank/reentry additions pass on all three compilers without
changing numerical implementations. After the Application baseline refinement
extension, Windows GCC/MSVC each pass the seven Qt-free tests and 20 affected
product/architecture tests. Linux GCC also passes the seven Qt-free tests, four
PFSM product tests and strict architecture test. Its captured input comparison
confirms exactly the four intended implementation/test files changed
(`linux-baseline-polish-01/change-scope.json`). These are affected regressions,
not a new full-suite run. Full real-library acceptance remains open.
The same affected test sets also pass after deferring full-matrix rank diagnostics
(`deferred-rank-*` and `linux-deferred-rank-01` evidence).

The local COD version 2026-07-02 contains 88,312 cards, including one explicit
`nd=0` reference. Its legal zero template cannot improve a fit. The expanded
recipes record 89 cases: six NIST material examples, example1, CPD-1a through
CPD-1h, CPD2-4, MIXT_5, ten Dara precursors, forty Dara mixture scans and twenty
reaction products. The fourth input freeze has 80 ready inputs and nine unresolved
cases. Input readiness is not identification success. Four reaction samples have
unidentified phases; CoO-ZnO has conflicting manual assignments; four others have
no card under their frozen structure rules. They cannot count as passes.

A subsequent read-only coverage audit leaves these rules unchanged. COD 1541318
is orthorhombic Cmcm Nb12O29, whereas the manual seed is monoclinic Am; these are
distinct polymorphs, consistent with the indexed primary text of the
[Nb12O29 structure study](https://www.sciencedirect.com/science/article/pii/S0925838819347735)
(full-page retrieval was unavailable). COD 9012339 Na2HPO4 and 1537547 Zn2V2O7
also have different space groups/cells from the manual seeds. Stoichiometric
FeNbO4 card 2002235 has a plausible alternate-axis cell for the nonstoichiometric
iron niobate, but identity/occupancy equivalence is not established; it is not
silently admitted. The chromium-modified vanadium oxide has no exact composition
candidate. The retained `coverage-recheck-01.json` records all candidate metadata
and the unchanged conclusions without fitting or using ranking labels.

`primary-input-evidence.json` records the author supplement (2026-01-12), its hash,
original workbook rows and primary COD structure metadata. Table 2/S5/S6 identifies
the precursor phases; TiO2 is **anatase**, correcting the unrelated local YAML's
rutile description without editing it. The source XRDML supplies counts and explicit
doublet wavelengths/ratio, consistent with the supplement's Ni beta-filter optics.
Reaction XY exports supplied no variance in the earlier freezes and therefore
used explicit equal weights. The later original-XRDML audit described above
corrects that omitted count provenance, without changing the fixed IL model.
The manual reaction refinement is independent of QualPF but sometimes informed by
Dara suggestions; it is not a blinded comparison to Dara.

Structural variants explicitly record equivalent axis settings and hexagonal versus
rhombohedral cells before ranking. Space groups and cells still have to agree;
no polymorph is accepted by formula alone. A hydrogen-omission exception is bound
only to COD 4031381, whose original CIF explicitly identifies H3 La O3 although the
local database says La O3. The heated iron oxalate case retains both its original
seed and independently refined cell from workbook row 9, with the original 3%
length/1-degree angle criteria unchanged. CPD3's success scope is crystalline phase
identification; amorphous glass is recorded separately, not represented by a card.
Weak-phase reporting for Dara mixtures uses author filename masses at <=11 wt%
to capture nominal 10 wt% components. These are evaluation labels only.

The initial full NIST 660c direct diagnostic was stopped after 43 minutes to release
CPU for the verified cache run; it produced no completed ranking and is explicitly
`interrupted_by_agent_not_acceptance`. Its immutable executable, input/run metadata,
log and interruption resource record remain under `full-NIST660c-direct-01`.
Its subsequent cached run completed the initial full-library round: the first
five cards all map to LaB6, with COD 2104747 first (`Rwp_net=0.3800062049`).
The example1 initial full-library round likewise has five Silicon cards first,
led by COD 2104748 (`Rwp_net=0.7750374344`). LaB6 and Corundum have initial card
ranks 75 and 907 in example1; their identification still requires later rounds.
Both runs evaluate all 88,312 cards without truth-dependent query constraints.
They are **initial-round successes, not completed case acceptance**: subsequent
joint acceptance/re-ranking was initially pending. NIST 660c has now completed
both rounds on its preserved original executable. The original unconstrained
example1 run has now exited two with `pfsm.baseline: Baseline remained unstable
after two rebuilds`. Only round zero was saved; no accepted-ID sequence or final
recall is inferred from its first ranked true card. The saved 88,312 rows pass
artifact consistency checks in `supplemental-example1-audit-01/audit-02.json`,
with all 22 bindings unchanged; the overall case remains incomplete and failed.
An absent historical performance field is reported as null, and the original
audit schema failure is preserved. This supplemental outcome does not change
the separately completed chemistry-constrained primary result.
The historical freeze07/revision05c292 unconstrained Bi2O3 run also completes
both full-pool rounds: it accepts 9012546 at rank one, retains 88,311 candidates
after exclusion, and reports remaining improvement 0.00264047763. All 176,623
saved rows and the complete assisted-selection protocol pass artifact checks in
`supplemental-bi2o3-audit-01/audit-03.json`. One original residual-identity check
fails because subtraction order differs near cancellation; an additive check
requires exact binary64 witnesses for both source operations and a rational local
roundoff bound, without widening its tolerance. The failed audit remains intact.
These saved-artifact/protocol checks are supplemented by an independent fixed-IL
2,049-point grid/Brent comparison in
`supplemental-Bi2O3-single-phase-reference-01`. The initial leading card passes
the original Q/scale/shift limits. Its same one-phase problem also verifies the
final selected-only baseline: native coordinates, observations, background,
weights, radiation, H0 and accepted SQL identity remain unchanged. The initial
and final-baseline shift errors are 3.89e-7 and 4.52e-9 degrees respectively.
The final top five two-phase trials also pass independent direct-IL/exhaustive
SVD NNLS checks at their saved shifts, with unchanged Q/scale/KKT criteria.
Positive-scale and forged-Q/wrong-scale controls verify the generalized trial
checks; the original initial-fallback shift assertion retains its separate scope.
The actual reference and verification tool exits are zero, with 2,613 original
input/runtime bindings and nine additional verification inputs unchanged. This
does not independently search the outer shifts of the final two-phase trials,
establish global minima or promote historical results to current primary
acceptance.
Initial candidate wall times are 5,362.40 seconds (10 workers,
NIST 660c) and 2,612.88 seconds (4 workers, example1), with peak process working
sets about 1.69 GB. Concurrent builds/regressions and recorded priority/affinity
changes qualify these timings; they do not measure isolated speedup. Original
rankings and resource adjustments remain in `full-NIST660c-cache-01` and
`full-example1-reuse-01` under the local validation directory.

`tools/validation/pfsm_real_single_phase_reference.py` independently checks a
completed initial-round candidate using the frozen SQL d/I, IL power-law formula,
a 2,049-point shift grid and SciPy 1.16.2 bounded Brent refinements. It verifies
manifest/database/observation identity and never seeds optimization with product
parameters or truth labels. Background, weights and H0 are fixed inputs to this
conditional fitter check; it does not independently validate background/width
estimation. The pre-existing SciPy tolerances remain unchanged: Q within
`2e-8*max(1,Qreference)`, scale within `2e-5`, shift within `2e-5` degrees. Both
real candidates pass: example1 Q/shift differences are 0.02510 and 7.22e-6 degrees;
NIST 660c differences are 7.55e-5 and 4.50e-7 degrees. No product output becomes a
regression golden (`real-reference-*-02.json`).

The subsequent NiO precursor initial round also evaluates all 88,312 cards and
ranks the correct rocksalt phase first (COD 4329323, Rwp_net 0.2655555294).
Two of its first five cards meet the frozen NiO identity rule; the other three
remain false cards. Its independent grid/Brent comparison passes the unchanged
tolerances (Q error 6.14e-5, shift error 4.92e-7 degrees), retained in
`real-reference-NiO-01.json`. The post-acceptance round is still running, so this
is another initial-round result rather than completed case acceptance.
The separate read-only check in
`supplemental-NiO-initial-audit-01/execution-01` verifies all 88,312 saved rows,
8,282 native observations, score arithmetic, full-pool identity coverage,
stable ordering and 18 unchanged input/runtime bindings. Its actual tool exit
is zero. It preserves 32 budget terminations and the still-incomplete case;
it performs no fit, confirms no accepted-ID sequence and does not promote the
historical supplemental result to current primary acceptance. Original unified
session 43539 was directly observed live after this check; it was not restarted.

Across these three completed initial rounds, all 264,936 candidate results retain
direct final verification and checked NNLS KKT diagnostics. Final selected-fit
termination is `budget` for 25 NIST, one example1 and 32 NiO cards; none is in its
round's first 100 cards. These remain budget terminations, not convergence claims.
The read-only distribution report `full-initial-solver-diagnostics-01.json` binds
the original ranking hashes and records all evaluation/start counts without
changing settings or using truth labels.

The completed NIST 660c run passes the frozen Top-5 assisted criterion: COD
2104747 identifies LaB6, then the final round retains all 88,311 other identities.
Its old executable performs two baseline rebuilds (264,933 final-operation
candidate evaluations). Final baseline Rwp_net is 0.3800062048033462; the remaining
maximum improvement is 0.01091081839863116 from another LaB6 card, COD 1525698.
Only the accepted card identity is excluded, so this remainder is reported without
claiming automatic stopping. Of the final rows, 14,738 are KKT-checked; 73,573
explicit feasible fallbacks retain `kkt_checked=false` and exactly equal the
baseline plus zero candidate scale. All retain direct final verification. The
baseline passes the existing independent Brent comparison without changing
tolerances. `full-NIST660c-{completed,fallback,baseline}-audit-01.json` binds the
original rankings, observations, executable and reference. This is one complete
unconstrained supplemental case, not primary chemical-constrained acceptance,
acceptance of the whole cohort or validation of later implementation changes;
all 22 holdout cases remain unranked.

Repeated-fit diagnostics exposed a mismatch between ordinary cost termination
and the finer common-baseline consistency check. In independently prescribed
overlapping IL observations, adding a zero reference led to two whole-library
rebuilds and `unstable_pfsm_baseline`; a zero template cannot improve a converged
selected-phase objective. The original executable, analytic input and failure
hashes remain in `baseline-analytic-before-evidence-01.json`. Bounded baseline
confirmation now resolves that case without any library rebuild, agrees with
independent dense/golden search, and retains the 32-confirmation failure and
cancellation paths. Solver tolerances and the two-rebuild rule are unchanged.

The still-running original example1 acceptance operation subsequently completed
one full 88,311-candidate pass and restarted its progress count for a baseline
rebuild, without publishing round 1. The immutable progress prefix and hash in
`example1-baseline-rebuild-observed-01.json` preserve this actual full-library
recomputation separately from any completed ranking.

Two-card Application diagnostics (real selected reference plus zero template)
then refine example1, NIST 660c and NiO in 14, 5 and 4 total fits respectively,
without rebuilding. All three preserve Geometric H0 and pass their existing
independent Brent comparisons: Q errors are 5.13e-8, 5.83e-11 and 9.90e-10.
`baseline-real-comparison-01.json` binds diagnostic inputs and reference hashes.
These are conditional fitter/baseline checks, not substitute full-library runs;
original ongoing executables are preserved and have not acquired the refinement.

Conditional two-column profiling on these three observations and the first four
SQL-order candidates measured 650 NNLS calls. Full-matrix rank SVD consumed
0.0458 of 0.4649 fit seconds (9.85%); free-set solves consumed another 0.0931
seconds. Only the separate reporting diagnostic is now deferred to returned
fits. Before/after serialized Q, Rwp, KKT, termination, parameters, rank and
evaluation counts match for all 12 inputs
(`nnls-stage-profile-01/comparison-01.json`). The original instrumented source,
payloads and executable are retained. These conditional fits do not establish
full-library acceptance or an isolated speedup; their before/after timing passes
overlapped compilation and other long benchmarks.

Initial example1 profiling used 32 cards/four workers and full IL: 8.72 seconds
candidate wall time, 33.75 aggregate solver seconds, 0.003 SQL seconds, 5,516
residual evaluations and 21.25 MB peak working set. A build ran concurrently, so
these locate the bottleneck but do not establish controlled speedup or full-library
runtime. Solver/template work dominates. Original parser/integration failures
and profiling logs remain under `build/pfsm/`. Full COD Top-5 acceptance is open.

Controlled 32-card example1 runs then measured the same 5,516 residual calls:

| Kernel | 1 thread | 2 threads | 4 threads | 20 threads |
| --- | ---: | ---: | ---: | ---: |
| Full IL using hypot | 30.955 s | 15.952 s | 8.793 s | 4.501 s |
| Full IL using guarded square root | 16.554 s | 8.489 s | 4.454 s | 2.193 s |
| Same expression, coordinate SSE2 | 8.258 s | 4.321 s | 2.223 s | 1.313 s |

For this dataset, serialized ranking, parameters and diagnostics were identical
across 1/2/4/20 threads, and Q/shift differences against the original hypot run
were zero. The kernel change passed the existing independent SciPy/Brent 24-case
comparison, Qt-free seven-test suite, and MSVC PFSM Core/Application/CLI tests.
These measurements justify exact arithmetic reuse/vectorization, not changing IL,
fixed width, intensity constraints or the objective. Raw `profile-before-*`,
`profile-sqrt-*` and `profile-simd-*` runs are retained for comparison.

The optional `--template-cache` flag freezes Application's `template_cache=true`
for the analysis. Default runs remain direct. The bounded Chebyshev approximation
is described in the [numerical contract](pfsm.md); final candidate/accepted Q and
scales are always checked with full direct IL. In the 32-card example1 comparison,
ranking stayed unchanged, maximum Q difference was 1.85e-7, maximum Rwp difference
8.89e-15, and maximum shift difference 4.38e-11 degrees. All serialized results
were identical across 1/2/4/20 threads. Independent SciPy/Brent checks passed all
24 original cases without changing cases or tolerances. These runs overlapped
the full NIST 660c direct run, so their 4.10/1.97/1.33/0.66-second wall times are
observations under load, not controlled speedup claims. Full-library acceptance
still requires actual per-round evidence.

A subsequent NIST 660c 32-card check reused unweighted template columns when
returning curves. All serialized scientific results, diagnostics and 11,687
residual evaluations stayed identical to the retained pre-reuse run and across
1/2/4/20 threads. Observed candidate times were 6.49/3.49/2.29/0.86 seconds;
concurrent regression/full-library workloads prevent a controlled speedup claim.
The independent direct and cached SciPy/Brent comparisons each passed all 24
unchanged cases. Raw evidence is retained in `reuse-comparison-01.json` and
`scipy-reuse-*-01.json` under the local validation directory.

Paired NIST 660c cache profiling before/after coordinate SIMD subsequently measured
4.99/4.02 seconds with one thread, 2.82/2.23 with two, 1.90/1.47 with four and
0.76/0.64 with twenty. All 32 serialized results and diagnostics remained identical
across variants and thread counts; the unchanged cached SciPy/Brent cases passed
24/24. These runs also overlapped other work, so they are qualified observations,
not isolated speedup measurements (`cache-simd-comparison-01.json`).

Primary evidence: [QualX examples](https://ccorrado71.github.io/qualx3/introduction/examples.html),
[IUCr data kit](https://www.iucr.org/__data/iucr/powder/QARR/data-kit.htm),
[weighing results](https://www.iucr.org/__data/iucr/powder/QARR/results.htm),
[structures](https://www.iucr.org/__data/iucr/powder/QARR/letter/strucs.txt)
and original NIST files identified and hashed in the manifest. The older QualX2
manual explicitly labels example1 CuKa1; the newer overview says CuKa. The recipe
uses the more specific description. Explicit line centroids approximate known
Cu spectra, rather than reproducing the complete published emission spectrum;
this radiation representation is an engineering choice of the approved model.

## Conditional PFSM/RIR quantification assessment

`--quantify-selected` is a separate diagnostic mode. It requires exactly one
frozen `quantification_card_ids` entry per structurally mapped phase, queries
only those identities and accepts them in that specified order through the
ordinary Application joint-fit transactions. It reports `not_acceptance`, an
explicit truth-filtered conditional scope and specified-phase counts, without
identification/recall claims. Normal full-pool Top-5 behavior is unchanged.
The mode exports the final shared Application quantification result through the
atomic CSV port; it never reads expected mass fractions.

`tools/validation/pfsm_quantification_assessment.py freeze` hashes existing
prepared observations, original source evidence and the database; freezes full
reference rows, intensity maxima and original RIR; and selects the lowest numeric
non-deleted structurally mapped card per phase before any fits. Its separate mass
truth uses the IUCr organizer's weighed rows and author sample-label mass tokens
for the twenty development binary scans. CPD 1c/1e published sums of 99.99 and
all author mass tokens are explicitly normalized before comparison. The twenty-two
held-out cases are not selected. `evaluate` runs the conditional mode, reads its
CSV, verifies the complete-card maximum and reports every signed phase error,
mean absolute error and maximum absolute error in percentage points. No target
fraction is used to derive or adjust RIR, width or coefficients.

The frozen independent inputs live in
[real-inputs.json](../../tests/data/pfsm-quantification/real-inputs.json).
Raw observations, fit curves, per-round diagnostics and original failed runs stay
under `build/pfsm/quantification-01`; quantitative accuracy is assessed separately
from identification acceptance. See the
[measured errors and qualifications](pfsm.md#real-mixture-semi-quantification-assessment-2026-10-12)
and [complete per-phase assessment](pfsm-quantification-assessment.json).

## Experimental v2 invocation and evidence

`--fast-pfsm-version v2` selects the protected anchored algorithm; omission keeps
v1. `--fast-pfsm off` remains the default. V2 emits schema version 3 with an
independent `algorithm_version`, initial/review Top-20, channel/risk protections,
proposed/actual sets, seed/step traces, NNLS calls and initial/review wall times.
Approximate parameters remain diagnostic and never initialize a formal fit.

The formal underestimation check reads only simulated selected cards in shadow
mode. Enabled mode supplements a fallback with missing candidates and reuses
completed results only with matching candidate identity, immutable problem/data/
algorithm/configuration, source, generation, database and baseline version and
parameters. Baseline escape invalidates this cache and screening evidence before
full replay. `formal_reused` counts actual saved candidate fits.

Validation continues to reuse frozen complete v1 formal evidence. New v2
shadow runs are reserved for missing or mismatched replay bindings; baseline/off
are conditional diagnosis only. Single shadow/enabled operation times describe
the measured configuration and do not establish controlled twofold acceleration.

The validation-only `qualpf_pfsm_fast_replay --input <binding.json> --output
<new-directory> --stage abc --threads 12` recomputes A/B/C fast evidence with
zero formal fits. `pfsm_fast_replay_inputs.py` derives selected identities from
complete successive candidate-pool differences and binds original observations,
card order, database, width, radiation and formal old-phase parameters. It puts
formal ranks in a separate comparison binding. The kernel receives no truth or
formal candidate objective. `pfsm_fast_replay_compare.py` independently measures
pure A/initial B/reviewed B retention, protections and full fallback, and simulates
the underestimation gate using only the proposed formal set. A skipped review is
reported as unexecuted, not a local-algorithm repair. This comparison is screening
evidence; only a new enabled run can establish new formal preservation.

`pfsm_fast_v2_validation.py` runs one serial replay phase followed by new enabled
runs, with the seven historical failures first. Completed success and failure
receipts are hash-verified and reused with `--resume`; unfinished receipts require
process-ownership diagnosis. It normally refuses an already running PFSM workload.
For descriptive validation, `--allow-unrelated-workloads` permits only positively
identified different executable hashes outside this cohort and records concurrent
processes before/after each run. Identical binaries (including copies), owned
outputs and unknown ownership still prevent launch. It never starts baseline/off
itself. Workflow/comparator revisions are recorded
separately from the frozen C++ runtime. Original identification status and the
nine nonready classifications remain separate from screening preservation.

A frozen replay process that exits at the fixed grid-node limit is preserved as
an original failed invocation. The launcher independently checks that limit
against the frozen coordinates and width and records `fast-replay-unavailable`,
with null A/B/C retention and no fabricated native `run.json`. Only a new enabled
run that actually publishes the complete candidate pool with exact formal
comparisons can discharge this coverage requirement. Unknown errors still stop
the launcher. This case is never counted as a successful local reranker replay.

### Completed v2 replay cohort (2026-10-14)

The diagnostic-complete C++ runtime `ddbc7d26` in `frozen-v2-02` processed all
80 frozen ready cases and 263 round bindings. The read-only audit in
`build/fast-pfsm-v2/replay-assessment-02` rechecks receipts, frozen comparisons,
all candidate call limits and initial/trial diagnostics. Its first invocation
in `replay-assessment-01` used the seed-coordinate list instead of the seed-state
list and remains preserved; the corrected audit changes only that field mapping.

| Fast-stage measurement | Complete Top-5 retention |
| --- | ---: |
| Actual fast-layer rounds | 259 |
| Recall within those rounds | 259/259 |
| A physical-backprojection Top-20 | 255/259 |
| B initial Top-20 | 249/259 |
| B reviewed Top-20, where review executed | 256/258 |
| C final screening, including simulated underestimation fallback | 259/259 |

One additional available round, `Dara_reaction_Fe2O3-2TiO2_1000C_60min` round 2,
has more than 80 review candidates and skips review for full-pool fallback. Its
initial Top-5 is retained, but it is not a reviewed-reranker success. The four
rounds of `NIST_674b_CeO2` and `NIST_676a` exceed the unchanged grid-node limit;
their A/B/C retention is unavailable. New enabled runs now verify the complete
formal pool exactly in all four rounds, without counting them as local successes.
They are excluded from the local denominators above.

The eleven formerly lost card occurrences in seven v1 rounds have these primary
recovery sources. All applicable secondary reasons remain in the raw evidence.

| Case and zero-based round | Cards | Reviewed local ranks | Primary recovery |
| --- | --- | --- | --- |
| CPD 1c, 3 | 9011662 | 26 | Channel protection; also distortion risk |
| CPD 1f, 3 | 9004180 / 9004178 | 2 / 3 | Local Top-20 |
| CPD 1h, 3 | 5000092 / 1000017 | 5 / 6 | Local Top-20 |
| TiO2/ZnO, 2-minute scan, 2 | 2300112 | 9 | Local Top-20 |
| TiO2/ZnO, 8-minute scan, 2 | 2300112 / 2300450 | 9 / 10 | Local Top-20 |
| Bi2O3 precursor, 1 | 1526458 | 7 | Local Top-20 |
| Fe-oxalate/Y2O3 reaction at 200 C, 2 | 7218520 / 3000327 | 33 / 69 | Risk protection; also full fallback |

Thus five of the seven original failing rounds recover through pure reviewed
Top-20; eight card occurrences recover locally, one primarily through channel
protection and two through risk protection. Four available rounds simulate the
formal-underestimation fallback using only proposed-card formal results; the
review-limit fallback is separate. The Fe/Y protected cards are already in the
proposed set, so their primary recovery remains risk protection even when the
underestimation check additionally selects the full pool.

The audit checks 113,219 recalled candidate states and 394,435 bounded NNLS calls.
It retains 7,207 negative initial gains and 4,728 negative physical trial gains;
30,575 rejected trials include 27,458 indistinguishable trials. These are recorded
computations, not a reason to enlarge budgets or relax the physical acceptance
rule. The nine nonready cases retain four incomplete-truth, four uncovered-phase
and one conflicting-truth classifications. Replay performs zero formal fits.

The new enabled and supplemental formal results are reported below. Replay
alone does not establish formal preservation, speed acceptance or a no-miss
guarantee.

### New v2 enabled preservation and descriptive measurements

`build/fast-pfsm-v2/cohort-v2-02` completed all 80 new enabled cases and 263 rounds;
`supplemental-v2-02` completed five additional new enabled configurations. The
single serial launcher genuinely exited zero. The read-only audit in
`final-assessment-01/assessment.json` independently rechecks frozen runtime and
input bindings, receipt files, exact formal comparisons and replay/Application
stage decisions. It finds no mismatch in initial/review Top-20, channel/risk and
proposed sets or the fixed `g_star`. No old formal fitting, baseline/off diagnosis
or repeat timing was needed.

All 263 cohort rounds preserve complete Top-5 identities/order, every surviving
formal value/diagnostic, survivor order, baseline parameters and baseline TSV
bytes. Full fallback rounds also preserve complete ranking bytes. The historical
assisted identification outcome remains 79/80, including the unchanged
`Dara_reaction_Bi2O3-2MoO3_400C_60min` failure; screening preservation does not
turn that case into a successful identification. The nine nonready classifications
remain outside the ready cohort.

| Cohort measurement | Actual result |
| --- | ---: |
| New enabled cases / rounds | 80 / 263 |
| Exact formal preservation | 80/80 cases, 263/263 rounds |
| Published formal candidates | 11,456 |
| Complete reference-pool candidates | 178,176 |
| Formal candidate count, median / range | 25 / 20–1,057 |
| Full-pool fallback rounds | 9/263 |
| Formal-underestimation / grid-limit / review-limit fallback | 4 / 4 / 1 |
| Fits reused during formal-underestimation supplement | 138 |
| Baseline rebuilds in this real-data cohort | 0 |

The four underestimation supplements reuse 38 fits in CPD 1d round 3, 48 in CPD 1g
round 3, 22 in the Li2CO3 precursor round 1 and 30 in the Fe/Y reaction round 2.
Each final pool card is formally fitted once, with same-baseline cached results
reused during supplementation. Baseline escape/full replay, cancellation, failed
publication and thread consistency are exercised by deterministic Application
contracts; the real cohort did not trigger baseline escape. The original formal
fitter/baseline and formal direct/cached evaluation bodies also remain identical
to `ebc648b0`, separately recorded in `formal-body-invariance.json`.

All five supplemental scenarios preserve exact formal results and fit 21 cards
each, with no full fallback. These are single Windows GCC Release, 12-thread
observations. Receipts record concurrent workloads and CPU load; the recorded
operation time runs from operation start to results being available and includes
SQL, fast stages, formal fitting, sorting and publication. Process lifetime,
import/preparation, output time and measured process peak memory are separate
fields in the raw audit. No controlled speed ratio or statistical timing
acceptance is inferred.

| Scenario | Pool / formal fits | Initial / review (s) | Operation to result (s) | Peak process MiB |
| --- | ---: | ---: | ---: | ---: |
| Fe3Sb2O4Br4 direct | 461 / 21 | 3.833 / 1.983 | 39.489 | 91.84 |
| Fe3Sb2O4Br4 cache | 461 / 21 | 1.587 / 0.257 | 4.317 | 94.06 |
| Sr3Fe2Cl2O5 direct | 468 / 21 | 0.939 / 0.276 | 2.946 | 60.48 |
| Sr3Fe2Cl2O5 cache | 468 / 21 | 0.561 / 0.048 | 1.044 | 60.32 |
| NIST_660c full library, cache | 88,312 / 21 | 3.056 / 0.052 | 12.332 | 1,610.08 |

The full-library operation separately records 5.910 s SQL, 3.030 s recall,
0.016 s shared-field preparation, 3.109 s combined initial/review and 0.158 s
formal candidate wall time. Its observed 12.332 s does not meet the exploratory
2–5 s target. The fixed correctness protocol does not establish controlled
twofold acceleration or the former cross-thread timing-regression criterion.

Three-platform affected checks, Qt-free 10/10 consumers and independent pinned
SciPy 54 anchored/derivative plus 24 backprojection references passed. Linux's
original missing-telemetry-package unit failure and the successful isolated repair
remain distinct evidence. Final Windows GNU normal regression passed 111/111
with zero failures or skips in 1,442.29 s. The single guarded run started after
competing workloads drained, retained the original test limits and genuinely
exited zero at 2026-10-14 23:18:48 Asia/Shanghai. `card.properties` passed in
118.39 s within its unchanged 180 s subprocess limit; no failed-test rerun was
needed. `normal-launch-01.json` binds the original log/JUnit bytes, and
`final-checkpoint-01.json` binds normal, scientific, independent-reference and
original-failure evidence without replacing earlier failures.

Implementation and required validation are complete. The experiment remains
opt-in with off/v1 defaults, and speed acceptance is unestablished. Current formal
preservation is established only for these data/configurations; neither stage
acquires a no-miss guarantee.
