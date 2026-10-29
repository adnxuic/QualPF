# Background Estimation and Editing

An accurate background model is essential both for **Classical Peak-List Search-Match (CSM)**—where background curvature affects peak-base placement and integrated/net peak intensities—and for **Profile-Fitting Search-Match (PFSM)**, which fits phase templates against the net observations relative to a fixed background curve.

In **QualPF**, background estimation is controlled via **Preprocessing > Generate Background**, **Preprocessing > Background Options...**, and interactive background control-point tools.

---

## Background Estimation Methods (`Preprocessing > Background Options...`)

Choose **Preprocessing > Background Options...** to open the **Background** configuration dialog.

![Background dialog showing Chebyshev polynomial configuration with Automatic coefficient selection](../images/background-options.png)

When **Function Type** is switched to **Filter**, the dialog displays the iterative Bruckner smoothing-minimum filter controls:

![Background dialog showing Bruckner Filter parameters (Iterations, Window size, Min/Max range)](../images/background-filter-options.png)

### Supported Background Function Types

| Function Type | Mathematical Model | Best Suited For |
| :--- | :--- | :--- |
| **Chebyshev** (Default) | $c_0/2+\sum_{k=1}^{N-1}c_kT_k(u)$ fitted to base points, with $u$ scaled to `[-1, 1]` | A series background model with `1`–`36` manually selected coefficients; inspect the fitted curve rather than assuming a high order is appropriate. |
| **Polynomial** | Standard power polynomial $\sum_{k=0}^{N-1} a_k x^k$ fitted to background base points | Smooth, gently varying backgrounds requiring low-to-moderate polynomial degree (`1`–`12` coefficients). |
| **Cosine Fourier Series** | Trigonometric cosine series fitted to background base points | Periodic or undulating background baselines (`1`–`12` coefficients). |
| **Cubic Spline** | Cubic interpolation through automatically selected background points | An alternative to the fitted series. Current manual point editing is refused for this method. |
| **Bezier Spline** | Sampled Bezier curve from automatically selected background points | A smooth alternative background construction. Current manual point editing is refused for this method; there is no tension control. |
| **Filter** (Bruckner) | Iterative moving-average minimum filter operating directly on pattern intensities | Complex, rapidly varying backgrounds or patterns where automatic base-point selection struggles; requires no control points. |
| **None** | Zero / flat disabled background | Pre-subtracted patterns imported from external software where background removal has already been performed. |

---

## Complete Parameter Reference: `Background` Dialog

### 1. Parametric Series Models (`Chebyshev`, `Polynomial`, `Cosine Fourier Series`)

| Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- |
| **Function Type** | `Chebyshev` | `Chebyshev`, `Polynomial`, `Cosine Fourier Series`, `Cubic Spline`, `Bezier Spline`, `Filter`, `None` | Selects the active background estimation algorithm. |
| **Automatic** | Checked | Checked / Unchecked | Tests coefficient counts from `1` to a limit set by `70%` of the selected point count, rounded to the nearest integer, and capped at `20` for Chebyshev or `8` for Polynomial/Cosine. Selects the smallest residual among those trials; it does not explore the entire manual editor range. |
| **No. of coefficients** | Saved manual value `6`; resolved count in Auto | `1` – `36` (`Chebyshev`)<br/>`1` – `12` (`Polynomial`, `Cosine Fourier Series`) | Number of basis terms when **Automatic** is off. In Auto the displayed count is the estimator's result. Excessive orders can follow peaks rather than the baseline. |

### 2. Iterative Filter Model (`Filter`)

| Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- |
| **No. of iterations** | `5` | `1` – `1000` | Number of Bruckner filter passes applied to erode Bragg peaks down to the underlying baseline. Increase if broad peaks remain partially inside the background curve. |
| **Window size** | `50` | `1` – `1000` (samples) | Half-window width (in pattern data points) of the moving average in each filter iteration. Should be wider than the broadest Bragg reflection you want to preserve. |
| **Min (°2θ)** / **Max (°2θ)** | Active calculation range | Within pattern bounds | Interval for an additional filter pass on the existing background curve. Fresh background estimation uses the full current pattern. |
| **Apply** | Button | Live preview | Adds a filter pass to the background preview, retaining the curve outside the selected interval. Repeated applications accumulate. |
| **Reset** | Button | Fresh full estimate | Recomputes the full background with the current model settings and clears accumulated filter passes. **Cancel** discards the dialog's draft. |

---

## Interactive Background Control-Point Editing

For **Chebyshev**, **Polynomial** and **Cosine Fourier Series**, you can edit the
base points used by the least-squares fit. These are fitting observations, so the
result is not required to pass exactly through every point. The current Spline,
Bezier and Filter routes reject manual point edits.

Use the central plot to edit supported points:

1. **Add Background Points (`Preprocessing > Add Background Point`)**:
   Activate this mode and left-click anywhere along the baseline of the diffraction pattern (for example, between overlapping peak clusters or across an amorphous halo) to insert a new anchor point. The background curve updates immediately.
2. **Delete Background Points (`Preprocessing > Delete Background Point`)**:
   Activate this mode and click near a background point to delete the nearest
   point by angular coordinate. At least two points must remain. This mode does
   not delete a rectangle of points. There is no Clear Background Points command.

Changing a supported point refits the background. If experimental peak markers
are visible, the same operation also requests peak redetection; otherwise
dependent peak/search results are invalidated. Review the dependency confirmation.

---

## Inspecting the Background-Subtracted Pattern (`View > Subtract Background`)

Once a background curve has been calculated, you can toggle between the raw/working view and the net background-subtracted view at any time using **View > Subtract Background** or the corresponding toolbar icon.

| Raw Pattern with Overlaid Background (`Subtract Background` Off) | Net Background-Subtracted Pattern (`Subtract Background` On) |
| :---: | :---: |
| ![Diffraction pattern with modeled background curve overlaid](../images/background-default.png) | ![Net diffraction pattern after subtracting the modeled background](../images/background-net.png) |

> **Tip:** > Toggling **View > Subtract Background** changes how the pattern is displayed and how classical peak overlays are inspected, while preserving the scientific observations and background vector. This display toggle does not establish Poisson counting provenance.
