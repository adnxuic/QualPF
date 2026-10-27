# Smoothing

Rapidly acquired powder diffraction scans or patterns collected from weakly scattering samples often exhibit high-frequency counting noise ($\sqrt{N}$ shot noise). The **Smoothing** tool (**Preprocessing > Smoothing...**) applies a digital smoothing filter to the working diffraction pattern to improve visual clarity and stabilize derivative-based peak picking.

![Smoothing dialog showing Savitzky-Golay filter configuration with Points of Window and Polynomial Order](../images/actionSmoothing-native.png)

---

## Smoothing Methods and Parameters (`Preprocessing > Smoothing...`)

Select **Preprocessing > Smoothing...** from the menu bar to open the **Smoothing** dialog.

### Parameter Reference: `Smoothing` Dialog

| Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- |
| **Method** | `Savitzky-Golay` | `Savitzky-Golay`, `Adjacent-Averaging` | Selects the digital filter algorithm applied to the working pattern intensities. |
| **Points of Window** | `4` | `2` – `50` (`Savitzky-Golay`)<br/>`1` – `50` (`Adjacent-Averaging`) | Half-window in sample points; a value $m$ uses $2m+1$ samples. Larger window widths remove more noise but increase the risk of broadening narrow reflections or clipping peak maxima. |
| **Polynomial Order** | `5` | `1` – `10` | Enabled only when **Method** is `Savitzky-Golay`. Specifies the degree of the local least-squares polynomial fitted within each moving window. The effective degree is capped at twice the half-window. Higher polynomial orders (`3`–`4`) preserve peak heights and widths much better than `Adjacent-Averaging`. |

---

## Comparing `Savitzky-Golay` and `Adjacent-Averaging`

1. **Savitzky-Golay (Recommended)**:
   Fits a local polynomial of degree $k$ (**Polynomial Order**) across $2m+1$ points, where $m$ is **Points of Window** by linear least squares. Unlike a simple moving average, a local polynomial can retain shape better than averaging, but smoothing can still alter peak heights, areas and widths. Inspect the preview before committing.
2. **Adjacent-Averaging**:
   Replaces each intensity value with the unweighted arithmetic mean of its neighboring samples inside **Points of Window**. While effective at suppressing extreme noise spikes, it attenuates sharp Bragg peak maxima and broadens reflection tails if the window exceeds a few samples.

---

## Scientific Best Practices and Effect on PFSM

> **Important:** > **How Smoothing Interacts with PFSM Weighting**:
> Smoothing modifies the **working pattern** intensities and correlates adjacent data points. When **S&M > PFSM** is executed:
> - `Auto from input` uses attested noise only when it remains compatible with the working signal. Smoothing makes that original-statistics route unavailable, so the current GUI uses equal weights.
> - Current text/CIF importers do not attest Poisson counts or variances merely from positive intensities. Inspect the **Resolved** readout.
> - Choose a modest window and inspect changes in weak and overlapping reflections before accepting the preview.

### Recommended Tuning Guidelines

- **For standard step scans ($\Delta 2\theta = 0.01^\circ\text{–}0.02^\circ$)**: Use `Savitzky-Golay` with **Points of Window** = `2` to `4` (full windows of `5` to `9` samples) and **Polynomial Order** = `3`.
- **If peak tops appear slightly clipped after smoothing**: Click **Undo Selected** on the `Smoothing` step in the **Workflow — Current Project** dock (or **Preprocessing > Restart Preprocessing**) and either reduce **Points of Window** or increase **Polynomial Order** from `2` to `3` or `4`.
