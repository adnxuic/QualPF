# Formal peak search

The formal peak list contains measured positions, intensities and widths. CSM
uses this evidence to compare reference reflections. PFSM instead uses the
continuous signal and does not require this list.

## Detect and inspect

Choose **Peak Search > Peak Search**. The operation prepares missing dependencies
with the Peak Search profile and publishes the resulting peaks together. Inspect
the **Peaks** dock and the corresponding plot markers before using them in an
interpretation.

**Peak Search > Peak Search Options** opens the peak editor. Its Interval and
Settings controls cover the angular bounds, append choice, intensity threshold,
sensitivity and requested number of peaks. Inspect the preview and accept it
only when it represents the features you intend to measure. **Setting > Peak
Search** is the separate editor for defaults used by future projects.

Peak selection from an existing candidate pool is distinct from a fresh detection.
Reselecting peaks preserves that pool's original detection evidence. To change
the measurement conditions, rerun detection with the intended preparation.

## Handle rejected candidates

A candidate whose local-baseline endpoints coincide cannot be measured. The
program can skip such candidates and still return valid peaks. **Candidate
details...** identifies the stage, candidate, input sample, angle and rejection
reason. These diagnostic indices start at zero, unlike the initial-candidate
table's displayed sample numbers.

If no measurable candidate remains, the operation fails without replacing the
previous result. Inspect the signal and preparation, adjust suitable options or
use manual editing. A failed calculation is not an empty, successful peak set.

## Edit or exchange peaks

The Peak Search menu offers **Add Peak**, **Delete Peak**, **Add inferred peak**,
**Delete all peaks**, **Load Peaks** and **Save Peaks**. Review manually added or
imported coordinates against the measurement; they do not substitute for a
continuous experimental signal. Dependent analyses may need to be invalidated
when you change their peak evidence.
