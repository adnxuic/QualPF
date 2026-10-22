# Numerical policy and regression references

Preserve approved formulas, precision, rounding, ordered evaluation, tolerances,
error behavior and independent expected values. Do not regenerate a golden from
product output or widen a tolerance to obtain PASS. Preserve original failed
comparisons and approved bounded differences. Do not reproduce undefined Legacy
behavior; the current deterministic C++ contracts are the authority.

Current C++ Core producers retain their accepted numerical implementations and
per-source strict floating compilation. Independent analytic/property and exact
high-precision references coexist with frozen observations. Binary32 transport,
RNE primitives, formed-angle policies and qualified crystallography differences
retain their original approved scopes. Host transcendental bit identity is not
assumed. A substantive numerical correction requires independent mathematics,
boundary/error evidence and bounded downstream review before implementation.

The [retained product decisions](../architecture/adr/README.md) index the approved
wavelength, crystallography, Range and Kalpha2 contracts with phase-specific
historical locators. Current implementation ownership follows the architecture
guide; historical intermediate owners do not replace it.

## Current resources

[The manifest](../../../tests/reference/manifest.json) binds 169 independently frozen
reference/provenance inputs by SHA-256 and length, with `expected_from_product=false`.
`tests/data` supplies current numerical datasets, wires, original transcripts,
tolerance policies and CIF fixtures. `tests/regression/references` contains the
small required qualified-output, independent-proof and inert provenance set,
organized by algorithm rather than execution phase. Source fragments are `.txt`
and never compiled. Registry generators independently reproduce accepted tables.

The original logical path keys in manifests and reference checks are provenance
identities. `reference_paths.py` maps them to current resources; it neither restores
complete history nor runs a native oracle. Missing/corrupt inputs fail. The
unchanged approved values and important malformed/stale/duplicate/error assertions
remain in normal C++/Python regression. No historical evidence ZIP/LFS download
or Phase1-36 runner is a normal-build/test prerequisite.

```sh
python -B tools/validation/check_references.py
ctest --test-dir build/linux-gnu -L normal --output-on-failure
```

See [testing](../../../tests/README.md), [test-data provenance](../../../tests/data-guide.md)
and [historical recovery](../../../history/README.md). Historical gates bind their
original source/runner/reference/compiler snapshots, never mutable current
agent files. Existing numerical approvals are not reopened by file organization.

## Measurable peak candidates

The separately authorized [geometric FWHM kernel](geometric-fwhm.md) has its own
binary64 contract and independent analytic evidence. It does not replace the
Classical contracts below.

The [configured GUI FWHM policy](../architecture/fwhm-workflow.md) explicitly
selects fixed, Gaussian or geometric physical widths. Automatic GUI measurement
failure stops its downstream operation; it does not use the legacy wavelength
seed. The lower-level Classical contract and its existing CLI callers retain that
fallback. The physical width display precedes the unchanged binary32 `.9f` search
range conversion.

The built-in FWHM refinement and formal peak extraction reject a candidate when
its two local-baseline minimum indices coincide, before evaluating the slope.
They record the detection stage, ordered candidate index before compaction,
zero-based sample index in the full input pattern, binary32 angle and
`coincident_baseline_endpoints` reason. Already rejected candidates do not enter
later overlapping-baseline comparisons, FWHM aggregation, selection or the pool.
Valid candidates keep the original evaluation order, binary32 height correction,
raw trapezoidal area and fitted-width formulas. Other numerical failures remain
structured errors. All candidates rejected is an error with zero publication;
no detected candidates retains the existing empty-result semantics.

This is an authorized behavior correction, not equivalence to the undefined
QualX division or the former whole-run failure. The behavior was introduced with
initial/formal version 2. Current checked-input initial/FWHM/formal IDs use versions 5/3/3;
older contracts are refused, never silently substituted. Frozen nonfinite
observations, original failed comparisons and finite reference tolerances remain
unchanged. Independent triangle/plateau/valley/boundary/narrow-window tests check
height, area and absence of divide-by-zero; the retained kernel regression exercises the original example1 subtraction/SG
reproduction and its sample-343 rejection. Product tests exercise example1, CPD2
and MIXT_5 through current unsubtracted/background inputs and default SG smoothing.

## Initial candidates and spectrum-consuming FWHM

Initial detection now returns only ordered derivative-trigger sample indices and
actual detection parameters. Gaussian fitting and all width-dependent refinement
run in the FWHM stage using the same checked spectrum/background input; private analysis buffers retain their existing arithmetic. The numerical
functions remain in `legacy_peaks.cpp` with the original strict floating options;
formal extraction shares them. Fit radius retries, peak suppression, broad-width
fallback (including the historical denominator), baseline measurement, endpoint
rejection, filtering and strongest-three binary32 reduction retain their original
order and formulas. Initial diagnostics retain their stage and full-pattern indices.
An empty usable set still selects wavelength fallback; an all-unmeasurable set
still fails. No width, normalization or signal is manufactured to make a fit pass.

Core regressions use independent Gaussian widths with the existing width tolerance,
explicit retry/adjacency/broad-peak cases, and the retained analytic measurement
and reduction assertions. Analytic fixtures must sample log curvature over enough
fitting points for the preserved binary32 fitter; changing the kernel or widening
tolerance is not a fixture repair. Frozen observations and original failed
comparisons remain unchanged.
