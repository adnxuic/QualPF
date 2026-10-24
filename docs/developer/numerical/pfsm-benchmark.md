# PFSM benchmark guide

`qualpf_pfsm_benchmark` composes the Application SearchEngine, SearchCoordinator
and canonical Session with execution-thread read-only SQL. Its normal
`qualpf.cli.pfsm_benchmark` test uses an independent analytic mixture and temporary
SQLite fixture; real COD data are not a normal-test dependency.

This page describes the developer tool. The [numerical contract](pfsm.md) owns
scientific behavior and limitations. The [original benchmark record](../../../history/README.md#pfsm-benchmark-evidence)
preserves every recorded run, failed comparison, qualification and acceptance scope.

Retired local `build/` inputs and observations are available through the
[build-record recovery index](../../../history/README.md#retired-local-build-records).
Restore a historical input when inspecting an old campaign; ordinary tests use
their independent checked-in fixtures and do not need that archive.

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
launcher recognizes the old replay filename in frozen runtimes. The experiments
in the original record retain their names, commands, versions and conclusions;
v1 execution requires its original Git snapshot.

## Freeze and run

Build `qualpf_pfsm_benchmark` in the selected supported configuration. Use its
`--help` for the compiled option set. Replace the database path below with a
separately installed database and supply the required independent source data.
These commands are examples, not an instruction to start a reserved assessment.

```powershell
python tools/validation/pfsm_prepare_benchmark.py `
  --database /path/to/cod_inorg.sq `
  --recipes tests/data/pfsm-real/recipes.json --output build/pfsm/frozen-inputs

build/windows-gnu/bin/qualpf_pfsm_benchmark.exe `
  --database /path/to/cod_inorg.sq `
  --manifest build/pfsm/frozen-inputs/manifest.json `
  --split development --chemistry evidence --threads 4 --output build/pfsm/development-run
```

Both commands require new output directories. The preparer hashes database
companions, raw observations and independent evidence, and freezes structural
mapping rules before fitting. Chemical formula alone does not establish polymorph
identity. Preserve raw intensities, coordinate grids, uncertainty units and the
recorded radiation sources; do not infer counts or silently normalize inputs.

Use the development split for iteration. Freeze code/options before any separately
authorized holdout assessment. `--split all` is the tool default, so specify the
intended split explicitly. Split labels select datasets, not fitting constraints.
Never reclassify a holdout failure as development evidence. Consult the original
record for the frozen split and unresolved source qualifications.

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

## Reading evidence

Treat wall time, summed worker time and peak resident memory as distinct metrics.
Worker sums overlap; process high-water RSS is not per-candidate allocation.
Controlled comparisons must bind implementation, inputs, references, compiler,
options and environment. Retain all failed runs and use independent scientific
comparisons; faster execution alone is not numerical or identification acceptance.

The [original record](../../../history/README.md#pfsm-benchmark-evidence) contains dated
performance cohorts, development and holdout assessments, original failures and
unresolved inputs. [Conditional mass assessment](../../../history/numerical/pfsm-quantification-assessment.json)
is separate from identification acceptance and establishes no new accuracy tolerance.
