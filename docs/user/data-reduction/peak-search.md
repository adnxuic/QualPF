# FWHM Estimation and Peak Search

In **QualPF**, representative Full Width at Half Maximum (**FWHM**) estimation and formal **Peak Search** work together to characterize the reflections in your diffraction pattern:

- **FWHM Estimation (`Preprocessing > FWHM Estimation` / `FWHM Estimation Options...`)** determines a single representative peak width (in degrees $2\theta$) for the active pattern. This width is required both by **PFSM** (to expand reference reflection sticks into continuous fixed Intermediate Lorentzian profile templates) and by **Peak Search** (to scale peak-picking windows and assign fallback widths).
- **Peak Search (`Peak Search > Peak Search` / `Peak Search Options...`)** extracts a discrete table of experimental reflections ($2\theta$, $d$, Intensity, FWHM) displayed in the **Peaks** dock and consumed by **Classical Peak-List Search-Match (CSM)**.

---

## Representative FWHM Estimation (`Preprocessing > FWHM Estimation Options...`)

Choose **Preprocessing > FWHM Estimation Options...** to configure how QualPF determines the representative peak width of the working pattern.

![FWHM Estimation Options dialog with Gaussian fit (Classical) selected](../images/fwhm-gaussian.png)

![FWHM Estimation Options dialog with Geometric FWHM and Minimum sampling density Q = 6 selected](../images/fwhm-geometric.png)

![FWHM Estimation Options dialog with Use fixed Default FWHM enabled](../images/fwhm-fixed.png)

### Parameter Reference: `FWHM Estimation Options` Dialog

| Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- |
| **Algorithm** | `Gaussian fit (Classical)` (Preprocessing / CSM / Peak Search)<br/>`Geometric FWHM` (PFSM) | `Gaussian fit (Classical)`, `Geometric FWHM` | Selects the automatic FWHM estimation method executed when **Use fixed Default FWHM** is unchecked. |
| **Minimum sampling density Q** | `6` | `2` – `8` (integer) | Shown only when **Algorithm** is `Geometric FWHM`. Specifies the minimum number of data samples across a reflection required for that candidate peak to be included in the geometric width estimate. Lowering $Q$ (for example, to `3` or `4`) allows coarser step-size patterns to succeed without falling back to a fixed width. |
| **Use fixed Default FWHM** | Unchecked | Checked / Unchecked | When checked, disables automatic `Gaussian fit` and `Geometric FWHM` estimation and directly assigns **Default FWHM (°2θ)** as the representative pattern width. Fixed and automatic geometric PFSM widths both bypass the separate SG Initial Peak Search. |
| **Default FWHM (°2θ)** | `0.1` | `> 0.0` – `180.0` | Explicit fixed width in degrees $2\theta$. Automatic estimation failure stops the operation; it does not silently use this value. The field is read-only in automatic mode. |

### Comparing the Three FWHM Modes

1. **Gaussian fit (Classical)**:
   Selects isolated reflections from the **Initial Peak Search** candidate list and fits local Gaussian bell curves to determine the representative FWHM. Best suited for standard laboratory scans used with **Peak Search** and **CSM**.
2. **Geometric FWHM**:
   Implements the [IAEA F1 geometric half-maximum chord procedure](../references.md#f1-iaea-fwhm-procedure) using its own internal candidate detection and peaks that satisfy the **Minimum sampling density Q** criterion (`2`–`8` samples). Because it measures the half-height width directly from background-referenced profile geometry without assuming a pure Gaussian peak shape, **Geometric FWHM** is the recommended default for **PFSM**.
3. **Fixed Default FWHM**:
   Directly applies the user-supplied **Default FWHM (°2θ)** value. Ideal when analyzing patterns with very few or severely overlapped reflections where automatic single-peak isolation is not possible.

---

## Formal Peak Search Options

To extract or update the discrete experimental peak table in the **Peaks** dock, select **Peak Search > Peak Search** or open **Peak Search > Peak Search Options...**.

![Peak Search Options dialog showing Interval bounds, Append new peaks, Intensity threshold, Sensitivity, and Number of peaks](../images/actionPeak_Search_Conditions-native.png)

### Parameter Reference: `Peak Search Options` Dialog

| Group | Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- | :--- |
| **Interval** | **Append new peaks** | Unchecked | Checked / Unchecked | When unchecked, running Peak Search replaces the entire experimental peak list. When checked, existing peaks outside the $[\text{Min}, \text{Max}]$ search sub-interval are preserved and newly detected peaks inside $[\text{Min}, \text{Max}]$ are merged into the table. |
| **Interval** | **Min (°2θ)** | Pattern start angle | Within calculation range | Lower angular bound for peak extraction. Synchronized with the left vertical cursor on the central plot. |
| **Interval** | **Max (°2θ)** | Pattern end angle | Within calculation range | Upper angular bound for peak extraction. Synchronized with the right vertical cursor on the central plot. |
| **Settings** | **Intensity threshold (%)** | `1.50` | `0.00` – `100.00` | Minimum relative peak height (expressed as a percentage of the strongest peak's net intensity) required to keep a detected reflection. Increase to `2.0`–`5.0%` to ignore trace noise; decrease to `0.2`–`0.5%` to capture weak minor-phase reflections. |
| **Settings** | **Sensitivity** | Derived from the current extraction window | `0` – `100` (slider & spinbox) | Controls second/third-derivative curvature sensitivity for resolving closely spaced doublets and shoulders. Higher values split shoulders more aggressively; lower values retain only well-separated maxima. |
| **Settings** | **Number of peaks** | Current selected count | Up to the detected candidate count | Reselects the strongest $N$ candidates and updates the associated threshold. It is a count field, not an enable checkbox. |

> **Tip:** > Combining **Append new peaks** with a narrow **Min / Max (°2θ)** window lets you apply high sensitivity (`Sensitivity = 85`, `Intensity threshold = 0.3%`) in a specific high-angle region without flooding the low-angle region with noise peaks.

---

## Inspecting and Editing the `Peaks` Dock

All extracted and manually added experimental peaks are listed in the **Peaks** dock (**Window > Peaks**).

![Peaks dock displaying extracted experimental reflections with 2-theta, d-spacing, Intensity, and FWHM columns](../images/peaks-dock.png)

### `Peaks` Dock Columns and Context Actions

| Column / Action | Description |
| :--- | :--- |
| **2θ (°)** | Bragg angle of the experimental reflection in degrees $2\theta$. |
| **d (Å)** | Interplanar spacing $d = \lambda_1 / (2 \sin\theta)$ in Ångströms calculated from the primary wavelength. |
| **Intensity** | Peak intensity (net above background when a background curve is present). |
| **FWHM (°2θ)** | Full Width at Half Maximum of the individual reflection in degrees $2\theta$. |
| **Right-Click > Add Peak...** | Opens the **Add table peak** dialog to insert a reflection by typing its exact $2\theta$ or $d$-spacing coordinate, intensity, and width. |
| **Right-Click > Modify measured peak...** (or double-click row) | Opens the **Modify table peak** dialog to edit the selected peak's coordinates, intensity, or FWHM. |
| **Right-Click > Delete Peaks** | Removes the highlighted row(s) from the experimental peak list. |

---

## Adding and Modifying Peaks Manually

Even with optimal automatic settings, weak shoulders or partially overlapped reflections may benefit from manual refinement. QualPF provides both **tabular** and **graphical** manual peak tools.

### 1. Tabular Peak Addition and Modification (`Add / Modify table peak` Dialog)

Double-clicking a row in the **Peaks** dock (or choosing **Add Peak...** / **Modify Peak...** from its context menu) opens the **Modify table peak** (or **Add table peak**) dialog.

![Modify table peak dialog showing Coordinate representation, Position, Intensity, Infer width from neighboring peaks, and FWHM](../images/manual-peak-dialog.png)

| Parameter | Default | Options / Range | Role and Effect |
| :--- | :--- | :--- | :--- |
| **Coordinate representation** | `2 theta` | `2 theta`, `d spacing` | Chooses whether you enter the peak position in degrees $2\theta$ (`2 theta`) or in Ångströms (`d spacing`). QualPF automatically converts the complementary coordinate using the active primary wavelength $\lambda_1$. |
| **Position (°2θ)** / **d-spacing (Å)** | Selected peak coordinate | Valid diffraction domain | Exact peak position. Must lie within the pattern's valid angular domain $(0^\circ, 180^\circ)$ and satisfy $d > \lambda / 2$. |
| **Intensity** | Selected peak height | `>= 0` | Net peak intensity assigned to the reflection. |
| **Infer width from neighboring peaks** | Unchecked | Checked / Unchecked | When checked, QualPF automatically interpolates the peak's **FWHM** from compatible neighboring experimental reflections; insufficient neighbors cause a reported failure, disabling the manual **FWHM** input box. Uncheck to enter a custom **FWHM (°2θ)**. |
| **FWHM (°2θ)** | Enter or retain a measured width | Finite and positive | Explicit Full Width at Half Maximum in degrees $2\theta$ when **Infer width from neighboring peaks** is unchecked. |

### 2. Graphical Peak Addition and Deletion on the Central Plot

- **Add Peak on Graph (`Peak Search > Add Peak`)**:
  Activate **Add Peak** from the **Peak Search** menu or toolbar, then left-click on a peak maximum in the central diffraction plot. The measured-peak dialog requires explicit intensity and FWHM. **Add inferred peak** is a separate command; it does not silently replace a failed measured input.
- **Delete Peak on Graph (`Peak Search > Delete Peak`)**:
  Activate **Delete Peak** and left-click a peak marker (or drag a selection rectangle around multiple peak markers) on the central plot to remove them immediately.

When the peak snapshot includes integrated intensities, manual edits also require an explicit integrated intensity. Cancel preserves the prior peak table.
