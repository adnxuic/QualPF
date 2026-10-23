# Data reduction

Preparation establishes the signal and evidence used by later calculations.
Treat it separately from plot controls: showing or hiding a curve does not change
the values used by an algorithm.

## Decide which operations are needed

| Operation | Role |
| --- | --- |
| Range | Restrict the working angular interval |
| Background | Estimate the slowly varying contribution beneath diffraction features |
| Smoothing | Apply a selected filter to the working signal |
| K-alpha2 stripping | Apply the supported radiation correction when appropriate |
| Initial Peak Search | Obtain candidate positions for the Gaussian width route |
| FWHM Estimation | Estimate a physical peak width or use an explicitly fixed width |
| Peak Search | Measure and select the formal peaks used by CSM |

Smoothing, cropping and stripping are explicit choices. Automatic matching
prepares missing dependencies, not an arbitrary sequence of every preprocessing
operation. Keep an unmodified measurement and inspect each applied correction.

## Width and peak preparation

The Gaussian width route uses initial derivative candidates. A fixed width
skips estimation; the geometric route measures widths using its own candidate
selection. The configured physical width is in degrees two-theta. An initial
display value is not evidence that a width has been measured.

**Preprocessing > FWHM Estimation Options...** selects the width policy.
**FWHM Estimation** performs the measurement. Failed automatic estimation stops
dependent preparation rather than silently reusing an old displayed value.
PFSM uses an explicitly configured fixed width, a compatible manual estimate,
or automatic geometric estimation. See [matching options](../search-match/search-match.md).

## Revisit a decision

Inspect [Workflow history](../settings.md#inspect-and-undo) before replacing
preparation: downstream peaks and searches may depend on it. **Preprocessing >
Restart preprocessing...** provides an explicit restart path. Read its
confirmation and preserve your raw file or a QPF checkpoint first.

The following pages explain [background](background.md),
[initial candidates](initial-peak-search.md), [formal peaks](peak-search.md),
[smoothing](smoothing.md) and [K-alpha2 stripping](kalpha2-stripping.md).
