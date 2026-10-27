# References

These references identify upstream work, methods and database providers relevant
to the application. A scientific citation credits a contribution; it does not
replace the copyright notices or redistribution terms supplied with software
and data. Cite the methods and data actually used in your analysis, and identify
your QualPF version separately.

## QualX lineage

- Altomare, A., Cuocci, C., Giacovazzo, C., Moliterni, A. and Rizzi, R. (2008).
  QUALX: A computer program for qualitative analysis using powder diffraction
  data. *Journal of Applied Crystallography*, 41, 815-817.
  [Journal record](https://doi.org/10.1107/S0021889808016956).
- Altomare, A., Corriero, N., Cuocci, C., Falcicchio, A., Moliterni, A. and
  Rizzi, R. (2015). QUALX2.0: a qualitative phase analysis software using the
  freely available database POW_COD. *Journal of Applied Crystallography*,
  48, 598-603. [Journal record](https://doi.org/10.1107/S1600576715002319).
- Altomare, A., Corriero, N., Cuocci, C., Falcicchio, A., Moliterni, A. and
  Rizzi, R. (2017). Main features of QUALX2.0 software for qualitative phase
  analysis. *Powder Diffraction*.
  [Journal record](https://doi.org/10.1017/S0885715617000240).

## Methods

- Bruckner, S. (2000), estimation of the background in powder diffraction patterns
  through a robust smoothing procedure (see [Background](data-reduction/background.md)).
  [Journal record](https://doi.org/10.1107/S0021889800003617).
- Savitzky and Golay (1964), smoothing and differentiation by simplified
  least-squares procedures (see [Initial Peak Search](data-reduction/initial-peak-search.md)
  and [Smoothing](data-reduction/smoothing.md)).
  [Journal record](https://doi.org/10.1021/ac60214a047).
- Ladell et al. (1975), the method cited for K-alpha2 stripping (see
  [K-alpha2 Stripping](data-reduction/kalpha2-stripping.md)).
  [Journal record](https://doi.org/10.1107/S0021889875011132).
- ICDD, [Quantitative Analysis: RIR](https://www.icdd.com/assets/tutorials/Quantitative-Analysis-RIR.pdf),
  background on reference intensity ratios and their assumptions (see
  [Quantification](search-match/search-match.md#quantification)).

## Peak-width measurement

These sources explain the geometric width construction and its limitations (see
[FWHM Estimation and Peak Search](data-reduction/peak-search.md)).
Imaging studies provide methodological context, not independent validation of
QualPF on powder-diffraction samples. SciPy documentation describes comparison
definitions and numerical tools; it does not imply a SciPy runtime dependency.

### F1. IAEA FWHM procedure

International Atomic Energy Agency (2009). *Quality Assurance for PET and PET/CT
Systems*. IAEA Human Health Series No. 1, STI/PUB/1393. Section 5.1.1.5, p. 44.
ISBN 978-92-0-103609-4. [Official report](https://www-pub.iaea.org/mtcd/publications/pdf/pub1393_web.pdf).
Context: three-point peak-top interpolation and linear half-height crossings.

### F2. Comparison of width estimators

Rainio, O., Hällilä, J., Teuho, J. and Klén, R. (2025). Methods for estimating
full width at half maximum. *Signal, Image and Video Processing*, 19, article 289.
[Journal record](https://doi.org/10.1007/s11760-025-03820-6).
The reported real-data evaluation concerns PET imaging.

### F3. AAPM measurement guidance

Halama, J. R., Graham, D., Harkness, B. A., et al. (2019). *Acceptance Testing and
Annual Physics Survey Recommendations for Gamma Camera, SPECT, and SPECT/CT
Systems*. AAPM Report No. 177, p. 22. ISBN 978-1-936366-68-2.
[Official report](https://www.aapm.org/pubs/reports/RPT_177.pdf).
Context: peak interpolation in imaging-system resolution measurements.

### F4. Fitted and trace widths

Kisch, H. J. (2018). Full width at half maximum of low-angle basal phyllosilicate
X-ray diffraction reflections: fitted peaks vs. diffraction traces.
*Clay Minerals*, 53(3), 325-338. [Journal record](https://doi.org/10.1180/clm.2018.25).
The distinction between a geometric trace width and a fitted profile parameter
must be interpreted in the context of the studied reflections.

### F5. Diffraction data and uncertainty

McCusker, L. B., Von Dreele, R. B., Cox, D. E., Louër, D. and Scardi, P. (1999).
Rietveld refinement guidelines. *Journal of Applied Crystallography*, 32, 36-50.
[Journal record](https://doi.org/10.1107/S0021889898009856).
Background, sampling and uncertainty guidance is also relevant to interpreting
profile fits; this citation does not make QualPF a Rietveld refinement program.

### F6. Peak prominence

The SciPy Community. *scipy.signal.peak_prominences*.
[Official documentation](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.peak_prominences.html).
Defines prominence relative to a contour baseline for candidate screening.

### F7. Prominence-based widths

The SciPy Community. *scipy.signal.peak_widths*.
[Official documentation](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.peak_widths.html).
A width measured at half prominence is not automatically a width at half the
background-corrected peak height.

## PFSM and numerical fitting

The following references cover the public PFSM concept, profile shapes,
least-squares tools and interpretation of residuals (see
[Search-Match Overview](search-match/index.md#methods) and
[Executing Search-Match](search-match/search-match.md)). They are methodological
references, not claims that another program uses QualPF's solver or that every
algorithm discussed in a paper is implemented here.

### P1. Public PFSM description

Putz, H. (2026). *Match! - Phase Analysis using Powder Diffraction: Manual*,
version 4.3. Crystal Impact. Profile-fitting and PFSM sections, pp. 27 and 33-38,
as identified in the project's design notes. The accessible companion is
[Details about Profile-fitting Search-Match](https://www.crystalimpact.com/match/help/idh_searchmatch_PFSM_details.htm).
This is a source for the public scale/shift fitting and candidate-improvement
concept, not for Match!'s private implementation or QualPF's NNLS/TRF design.

### P2. Profile shapes

Young, R. A. and Wiles, D. B. (1982). Profile shape functions in Rietveld
refinements. *Journal of Applied Crystallography*, 15, 430-438.
[Journal record](https://doi.org/10.1107/S002188988201231X).
Provides context for intermediate-Lorentzian and other profile shapes; it does
not establish one universally optimal shape.

### P3. Interpreting R factors

Toby, B. H. (2006). R factors in Rietveld analysis: How good is good enough?
*Powder Diffraction*, 21(1), 67-70.
[Journal record](https://doi.org/10.1154/1.2179804).
Relevant to the distinction between numerical agreement and model validity.

### P4. Agreement-index conventions

Hill, R. J. and Fischer, R. X. (1990). Profile agreement indices in Rietveld and
pattern-fitting analysis. *Journal of Applied Crystallography*, 23, 462-468.
[Journal record](https://doi.org/10.1107/S0021889890006094).
Relevant to background and evaluation-interval effects on agreement indices.

### P5. Separable least squares

Golub, G. H. and Pereyra, V. (1973). The differentiation of pseudo-inverses and
nonlinear least squares problems whose variables separate. *SIAM Journal on
Numerical Analysis*, 10(2), 413-432.
[Journal record](https://doi.org/10.1137/0710036).

### P6. Linear constraints in separable fitting

Sima, D. M. and Van Huffel, S. (2007). Separable nonlinear least squares fitting
with linear bound constraints and its application in magnetic resonance
spectroscopy data quantification. *Journal of Computational and Applied
Mathematics*, 203(1), 264-278.
[Journal record](https://doi.org/10.1016/j.cam.2006.03.025).

### P7. Nonnegative least squares

Lawson, C. L. and Hanson, R. J. (1995). *Solving Least Squares Problems*.
Classics in Applied Mathematics, volume 15. SIAM; first edition 1974.
[Book record](https://doi.org/10.1137/1.9781611971217).
The NNLS reference is shared by the fitting and candidate-screening notes.

### P8. Variable projection in practice

O'Leary, D. P. and Rust, B. W. (2013). Variable projection for nonlinear least
squares problems. *Computational Optimization and Applications*, 54, 579-593.
[Journal record](https://doi.org/10.1007/s10589-012-9492-9).
Its example program is not itself the reference implementation of a nonnegative
inner solve; see P6 and P7 for the constrained problem.

### P9. Bound-constrained trust regions

Coleman, T. F. and Li, Y. (1996). An interior trust region approach for nonlinear
minimization subject to bounds. *SIAM Journal on Optimization*, 6(2), 418-445.
[Journal record](https://doi.org/10.1137/0806023).

### P10. Subspace bound-constrained minimization

Branch, M. A., Coleman, T. F. and Li, Y. (1999). A subspace, interior, and conjugate
gradient method for large-scale bound-constrained minimization problems.
*SIAM Journal on Scientific Computing*, 21(1), 1-23.
[Journal record](https://doi.org/10.1137/S1064827595289108).

### P11. Numerical comparison tools

SciPy Developers. Official documentation for
[nnls](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.nnls.html)
and [least_squares](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.least_squares.html),
and the [trust-region-reflective implementation](https://github.com/scipy/scipy/blob/v1.16.2/scipy/optimize/_lsq/trf.py).
The source link is pinned to the SciPy 1.16.2 version used in the project's
recorded comparisons. These are comparison resources, not production dependencies.

### P12. One-dimensional minimization

Brent, R. P. (1973). *Algorithms for Minimization without Derivatives*.
Prentice-Hall, chapter 5. [Author's book page](https://maths-people.anu.edu.au/brent/pub/pub011.html).
Relevant to independent single-phase numerical comparisons rather than a second
production fitting path.

### P13. Count weighting and statistical bias

Bergmann, J. and Monecke, T. (2011). Bayesian approach to the Rietveld refinement
of Poisson-distributed powder diffraction data. *Journal of Applied
Crystallography*, 44, 13-16. [Journal record](https://doi.org/10.1107/S0021889810047928).
This context accompanies F5 when considering the statistical interpretation of
weights; observed-intensity weighting is not exact Poisson maximum likelihood.

## Candidate screening and pattern comparison

These sources appear in the candidate-screening notes. They provide related
methods and context, not evidence that QualPF implements Rietveld search or a
particular published screening algorithm. P1 and P7 are shared references and
are listed once above.

### R1. Full-profile Rietveld search

Lutterotti, L., Pillière, H., Fontugne, C., Boullay, P. and Chateigner, D. (2019).
Full-profile search-match by the Rietveld method. *Journal of Applied
Crystallography*, 52, 587-598.
[Journal record](https://doi.org/10.1107/S160057671900342X).

### R2. Spectral similarity

de Gelder, R., Wehrens, R. and Hageman, J. A. (2001). A generalized expression
for the similarity of spectra: application to powder diffraction pattern
classification. *Journal of Computational Chemistry*, 22, 273-289.
[Journal record](https://doi.org/10.1002/1096-987X%28200102%2922%3A3%3C273%3A%3AAID-JCC1001%3E3.0.CO%3B2-0).

### R3. Powder-pattern matching

Ivanisevic, I., Bugay, D. E. and Bates, S. (2005). On Pattern Matching of X-Ray
Powder Diffraction Data. *The Journal of Physical Chemistry B*, 109(16), 7781-7787.
[Journal record](https://doi.org/10.1021/jp0455935).

## Database providers

- [Crystallography Open Database](https://www.crystallography.net/cod/):
  crystallographic structures and associated source records (see
  [Database Installation](database-installation.md)).
- [ICDD](https://www.icdd.com/): Powder Diffraction File products and their
  applicable access terms (see [Database Installation](database-installation.md)).

## Software acknowledgments

QualPF uses Qt, QCustomPlot, Eigen, Boost and other components identified in its
offline notices. Open **Help > About** or read `NOTICE.md`,
`THIRD_PARTY_NOTICES.md` and `LICENSES/` in the source distribution for the
preserved component attributions and license texts. This reference page does
not replace those materials.
