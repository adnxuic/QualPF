# Geometric FWHM

`estimate_geometric_fwhm` is a Qt-free, nonpublishing Core operation. It is
separate from the Classical Gaussian fit and strongest-three reduction. Its
identity is `qualpf.peaks.fwhm.geometric`, version 1; execution requires
`NumericalPolicy::explicit_float64`. Widths are in degrees two theta. This is
the geometric width of the supplied experimental envelope, not a deconvolved
component width or a fitted profile parameter.

## Inputs and measurement

Supply owned, equally sized angle, intensity and background arrays. Background
is required even when it is explicitly zero. Input coordinates must be finite
and strictly increasing. Invalid intensity/background samples, invalid-point
flags and explicit breaks separate continuous segments; samples are never
deleted to bridge a gap. Irregular steps alone do not imply a gap. No resampling,
smoothing, normalization or clipping occurs. Signal/radiation processing metadata
is retained as supplied, not inferred or used to transform the signal.

The kernel subtracts background once in binary64 and detects strict maxima and
plateaus. Prominence uses nearest strictly higher samples on each side and the
higher of the two interval minima; equal-height peaks do not bound that search.
A monotone stack and range-minimum tree avoid repeated full-spectrum scans.
Unknown-noise candidates require positive height and prominence, and a
prominence/sample-height ratio of at least 0.05 by default. This is an engineering
filter, not a statistical test. Significant rejected peaks remain overlap barriers.

For a non-plateau candidate, the parabola through its sample and two immediate
neighbors is computed in scaled local coordinates using their actual spacing.
Concavity must exceed 64 binary64 epsilons after local scaling. The apex must lie
between adjacent sample midpoints; half its height must lie strictly between
zero and the central sampled height. Only the apex height comes from the parabola.
The nearest half-height crossing on each side comes from linear interpolation
on the experimental net signal. A single exact crossing is accepted; a
half-height plateau is ambiguous and rejected. Missing crossings, significant
neighbors and saturated measurement samples reject the candidate.

Both half widths must be positive. The width divided by the largest sample
interval across both brackets and their interior must be at least 6 (preferred:
8). Optional absolute bounds are inclusive; no universal angular bounds are
assumed. Asymmetry is diagnostic, not an automatic rejection. The highest
interpolated eligible net peak supplies the default width. Exact height ties
use available smaller relative uncertainty, then larger prominence, then lower
sample index. No eligible peak returns an empty default and all diagnostics;
there is no hidden constant, wavelength seed or F1 fallback. Unknown noise is
not by itself a reason to reject an otherwise eligible peak.

## Noise and covariance contract

Noise is explicit: unknown, independent standard deviations, independent raw
counts, counts with acquisition times, or covariance of the net signal. Raw
counts use max(N,1); rates use max(N,1)/t^2 and require intensity=N/t. Scaled
intensities must instead supply matching standard deviations/covariance. No
Poisson model is inferred from Pattern weights, source names or intensity scale.

Known noise requires prominence >= 5 sigma and interpolated height >= 20 sigma
(preferred: 50 sigma). Sigma is the largest standard deviation at the apex and
its immediate neighbors; plateau prominence uses the plateau and its neighbors.
`noise_verified` denotes a supplied validated noise model, not a guarantee of
eligibility; the explicit rejection reason and `eligible` flag retain SNR failures.
Unknown noise retains the relative-prominence rule and is eligible without SNR.
An exactly zero local sigma is recorded as `zero_noise`, with no infinite SNR.

Covariance owns a diagonal array and unique upper-triangle entries, with omitted
entries interpreted as zero. Arbitrarily distant correlations are supported.
All dimensions, indices, finite/nonnegative diagonals, duplicate pairs and
Cauchy-Schwarz pair bounds are checked. Each used (at most 7 by 7) principal
submatrix is checked for positive semidefiniteness. This is not a global PSD
certificate for unused portions of a caller-supplied matrix. Singular PSD inputs
are accepted. PSD and negative propagated-variance roundoff tolerances are
64 binary64 epsilons times the scaled matrix infinity norm (and squared gradient
norm for the latter); only negative variance within that bound rounds to zero.

Nonuniform Lagrange apex derivatives and linear-crossing derivatives are combined
as gH=gR-gL before computing gH^T Sigma gH. Repeated sample indices are merged,
so shared peak-height and bracket correlations are retained. Background random
uncertainty can be included in supplied net-signal covariance. Background bias,
index switching, overlap and coordinate uncertainty are not included. Results
are explicitly conditional on fixed apex/bracket indices. Unknown noise leaves
uncertainty absent, never zero. No relative-uncertainty rejection cutoff is applied.

## Independent evidence

The worked example has apex height 70225/7, apex position 42003/1400,
left crossing 167781/5600, right crossing 1683239/56000, and width 5429/56000.
The test supplies the omitted monotone interior samples explicitly; they do not
alter these apex or crossing brackets. Analytic Gaussian, Lorentzian,
intermediate-Lorentzian and split-Gaussian fixtures have known widths. Their
grid-refinement and invariance checks use analytic widths, never product output
as a golden. Engineering cutoffs are not accuracy guarantees or NEMA clauses.

The example's independent Poisson variance is
17311769493/12984608000000000, giving uH=0.0011546658166378617 degrees.
In units of the 0.01-degree step, the apex offset is 3/14 and its Lagrange
weights are (-33/392, 187/196, 51/392). Crossing fractions are 5/56 and 439/560.
The two slopes are 130000 and -100000 intensity units per degree, so the shared
apex contribution to gH is -23/2600000 times those three weights. Including the
brackets, the nonzero gH entries at indices (2,3,5,6,7,11,12) are
(51/7280000, 1/1456000, 759/1019200000, -4301/509600000,
-1173/1019200000, 121/56000000, 439/56000000). Summing their squared values
times the raw counts (5100,6400,9200,10200,9800,6000,5000) gives the variance
above, independently of the implementation.
For the symmetric triangular fixture with slopes +/-1, gH=e4-e8+e12: unit
independent variances give variance 3; fully correlated unit noise gives 1.
For the three-sample spike with relaxed sampling admission, shared indices
cancel the apex term and gH=(e0+e2)/16. These exact identities check covariance
and repeated-index behavior. Fixed-seed low-noise simulations and nonuniform-grid
central differences independently check the local linearization.

A first validation incorrectly required every grid phase's error to decrease
monotonically upon refinement. Gaussian offset 0.47 produced errors 3.95802e-5
at step 0.04 and 7.71898e-5 at step 0.02, both within the unchanged analytic bound.
The original failure is retained in ignored validation logs. The corrected check
requires the phase-envelope error to decrease and every fixture to obey the
same curvature/slope bound; no implementation-derived expected widths are used.

## Application use

Include `qualpf/application/geometric_fwhm.h` and pass a `ProductHandle`, an owned
`GeometricFwhmRequest` and an explicit-float64 `ExecutionContext` to
`evaluateGeometricFwhm`. The result retains the opening source and measurement.
Dataset mismatch fails with `stale_snapshot`; cancellation and invalid inputs
produce structured errors, without publishing anything. A private draft may
supply different samples under the same dataset identity. The caller must supply
the intended intensity/background pair and metadata; no fields of the source
Pattern, including original intensity or weights, are silently substituted.

The standalone Qt-free Application example exercises this API alongside the
existing Classical workflow. Normal tests need no untracked example collection.
For real-data diagnostics, run the Application test executable with
`--real-data tests/real_example_data` (or an absolute collection directory).
This explicitly reads example1, CPD2 and MIXT_5, evaluates the existing default
background estimator, and records selected peaks and rejection counts on the
unsmoothed net signal. A missing requested input fails the diagnostic run.
These spectra have no independent FWHM ground truth and their results are not
accuracy goldens.

The Windows GNU/MSVC and Linux GNU diagnostic runs on the existing collection produced
the following observations (zero-based input indices; rounded displayed widths).
All selected peaks had unknown noise. Full reason counts are written by the
diagnostic command; these observations are not test expectations.

| Input | Candidates | Eligible | Selected index | FWHM (degrees) |
| --- | ---: | ---: | ---: | ---: |
| example1 | 836 | 3 | 3275 | 0.180428 |
| CPD2 | 2092 | 41 | 680 | 0.151727 |
| MIXT_5 | 776 | 12 | 3274 | 0.182261 |

## References

- [IAEA, Quality Assurance for PET and PET/CT Systems, section 5.1.1.5](https://www-pub.iaea.org/mtcd/publications/pdf/pub1393_web.pdf).
- [Rainio et al., Methods for estimating full width at half maximum, F6](https://doi.org/10.1007/s11760-025-03820-6).
- [AAPM TG-177, spatial resolution analysis](https://www.aapm.org/pubs/reports/RPT_177.pdf).
- [SciPy prominence definition](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.peak_prominences.html); [half-prominence width is a different definition](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.peak_widths.html).
- [IUCr Rietveld refinement guidelines](https://doi.org/10.1107/S0021889898009856).
- [Kisch, fitted peaks versus diffraction traces](https://doi.org/10.1180/clm.2018.25).

PET comparisons do not establish XRD-wide accuracy. Unresolved overlap and
background bias remain limitations, even when all geometric checks pass.
