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
| **Points of Window** | `5` | `2` – `50` (`Savitzky-Golay`)<br/>`1` – `50` (`Adjacent-Averaging`) | Total number of consecutive data samples included in the moving filter window. Larger window widths remove more noise but increase the risk of broadening narrow reflections or clipping peak maxima. |
| **Polynomial Order** | `3` | `1` – `10` | Shown only when **Method** is `Savitzky-Golay`. Specifies the degree of the local least-squares polynomial fitted within each moving window. Must be strictly less than **Points of Window**. Higher polynomial orders (`3`–`4`) preserve peak heights and widths much better than `Adjacent-Averaging`. |

---

## Comparing `Savitzky-Golay` and `Adjacent-Averaging`

1. **Savitzky-Golay (Recommended)**:
   Fits a local polynomial of degree $k$ (**Polynomial Order**) across **Points of Window** adjacent points by linear least squares. Unlike a simple moving average, a cubic or quartic Savitzky-Golay filter preserves the area, centroid, height, and FWHM of diffraction peaks when the window width is kept comparable to or smaller than the instrumental FWHM.
2. **Adjacent-Averaging**:
   Replaces each intensity value with the unweighted arithmetic mean of its neighboring samples inside **Points of Window**. While effective at suppressing extreme noise spikes, it attenuates sharp Bragg peak maxima and broadens reflection tails if the window exceeds a few samples.

---

## Scientific Best Practices and Effect on PFSM

> **Important:** > **How Smoothing Interacts with PFSM Weighting**:
> Smoothing modifies the **working pattern** intensities and correlates adjacent data points. When **Search-Match > Run PFSM** is executed:
> - If **PFSM Options > Weighting** is set to `Auto from input`, QualPF checks whether the unsmoothed original raw counts (`original_observed`) are attested and compatible with the current calculation range so Poisson counting weights can still be referenced cleanly, or automatically falls back to `Equal weights` when observations have been transformed by smoothing or stripping.
> - Avoid heavy smoothing before PFSM: because PFSM fits integrated multi-reflection profile templates across all data points simultaneously, it is naturally resilient to uncorrelated counting noise without pre-smoothing.

### Recommended Tuning Guidelines

- **For standard step scans ($\Delta 2\theta = 0.01^\circ\text{–}0.02^\circ$)**: Use `Savitzky-Golay` with **Points of Window** = `5` to `9` and **Polynomial Order** = `3`.
- **If peak tops appear slightly clipped after smoothing**: Click **Undo from Here** on the `Smoothing` step in the **Workflow — Current Project** dock (or **Preprocessing > Restart Preprocessing**) and either reduce **Points of Window** or increase **Polynomial Order** from `2` to `3` or `4`.
