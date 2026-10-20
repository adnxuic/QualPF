# Peak Search

## Overview

QualPF locates peaks in the diffraction pattern and records their 2θ position,
intensity and FWHM. CSM uses these peaks; PFSM fits continuous observations and
does not require a peak table.

## Running Peak Search

**Peak Search → Peak Search** runs the automatic formal peak search with the
current conditions. Detected peaks appear as vertical markers in the graphic area
and are listed in the **Peaks** dock.

After background estimation or smoothing, some detected locations may have
coincident local-baseline endpoints and cannot be measured. QualPF skips these
candidates and continues with valid peaks. The options dialog displays
**Skipped N unmeasurable candidates**; **Candidate details...** lists the initial
or formal stage, candidate index, original input sample index, angle and reason.
Indices start at zero. Valid previews can still be applied. Automatic peak search
and Search preparation use the same rule and display the skipped count.

If every candidate is unmeasurable, or another numerical error occurs, the
operation fails and applies no changes. Adjust the conditions, cancel, restart
preprocessing or add measured peaks manually. Candidate diagnostics follow the
current detection and are cleared by input changes; QPF 1.0 preserves them.

## Peak Search Options

**Peak Search → Peak Search Options** opens the current-project Peak Search
workflow. Its Background, Initial Peaks, FWHM, Peak Extraction and Peak Selection
pages are independent of CSM and Preprocessing. Save changes parameters; Save &
Run then calculates. Setting → Peak Search edits defaults for new projects.

| Parameter | Description |
|-----------|-------------|
| **Min. 2θ / Max. 2θ** | Search range |
| **Threshold** | Minimum peak height relative to the pattern maximum |
| **Sensitivity** | Controls smoothing before peak detection |
| **Max. peaks** | Maximum number of peaks to detect |
| **Append** | Add new peaks to the existing list instead of replacing it |

## Manual editing

- **Add peak**: View → Selection Mode, then right-click on the pattern
- **Delete peak**: select a peak marker and press Delete
- **Add inferred peak** and **Delete all peaks** are also available in the
  **Peak Search** menu's editing group.
- **Load / Save**: Peak Search → Load Peaks / Save Peaks

> **Tip:**
> If too many or too few peaks are found, adjust **Threshold** and
> **Sensitivity** in the Peak Search Options dialog.
