# Data Reduction

Before running a Search & Match, the raw diffraction pattern usually needs
to be processed. QualPF provides the following data reduction tools:

| Tool | Menu | Description |
|------|------|-------------|
| [Background](background.md) | Preprocessing → Generate Background | Estimate background immediately |
| Background Options | Preprocessing → Background Options | Configure and preview background estimation |
| [Initial Peak Search](initial-peak-search.md) | Preprocessing → Initial Peak Search | Inspect configurable derivative-trigger candidates |
| FWHM Estimation | Preprocessing → FWHM Estimation | Estimate physical peak width with Gaussian or geometric measurement |
| [Peak Search](peak-search.md) | Peak Search → Peak Search | Extract the formal experimental peak table |
| [Smoothing](smoothing.md) | Preprocessing → Smoothing | Reduce noise |
| [K-α2 Stripping](kalpha2-stripping.md) | Preprocessing → K-α2 Stripping | Remove the Kα2 contribution |

The **Preprocessing** menu separates range, background, initial peak search,
FWHM estimation, other pattern processing and restart operations with separators.
**Generate Background** estimates immediately; **Background Options** opens the
settings and preview dialog. **View → Subtract Background** changes only the display.

**Initial Peak Search** and **FWHM Estimation** have independent commands and
editable **Options** entries. Initial search provides a read-only candidate result
table. Gaussian FWHM preparation consumes the initial settings;
fixed and geometric width modes retain their own behavior.

The independent **Peak Search** menu groups formal extraction and its options,
manual peak editing, and peak-table loading/saving separately. Use
**Preprocessing → Restart preprocessing...** to restart analysis from the original
pattern under the existing recovery rules.
