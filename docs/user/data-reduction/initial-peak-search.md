# Initial Peak Search

The **Initial Peak Search** stage detects candidate reflection positions directly from the Savitzky-Golay third derivative of the diffraction pattern. Rather than producing the final peak table immediately, it constructs a high-recall list of **Initial Peak Candidates** that serves two critical downstream roles in **QualPF**:

1. **Seeding Representative FWHM Estimation** — Both the **Gaussian fit (Classical)** and **Geometric FWHM** estimators sample isolated peaks from the initial candidate list to determine the pattern's representative Full Width at Half Maximum (FWHM).
2. **Seeding Formal Peak Extraction** — The formal **Peak Search** step refines and filters these candidates using the representative FWHM, intensity threshold, and sensitivity settings to build the final **Peaks** table used by CSM.

---

## Configuring Initial Peak Search (`Preprocessing > Initial Peak Search Options...`)

Select **Preprocessing > Initial Peak Search Options...** to open the two-tab **Initial Peak Search Options** dialog.

### Tab 1: `Settings`

![Initial Peak Search Options dialog showing the Settings tab with Derivative threshold, Automatic window, Manual half-window, and SG polynomial degree](../images/initial-peak-options.png)

### Parameter Reference: `Initial Peak Search Options` (`Settings` Tab)

| Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- |
| **Derivative threshold** | `0.1` | `0.0` – `200.0` (`> 0`) | Controls the minimum normalized Savitzky-Golay third-derivative zero-crossing slope required to register a peak candidate. **Lower values** (e.g., `0.03`–`0.08`) increase sensitivity to weak or broad reflections; **higher values** (e.g., `0.25`–`1.0`) suppress high-frequency counting noise. |
| **Automatic window** | Checked | Checked / Unchecked | When checked, QualPF automatically selects the Savitzky-Golay filter half-window $m$ based on the angular step size $\Delta(2\theta)$ of the pattern. Uncheck to unlock **Manual half-window (samples)**. |
| **Manual half-window (samples)** | `11` | `2` – `100` (integer) | Specifies the half-window width $m$ in data samples (giving a total moving window of $2m + 1$ points) when **Automatic window** is unchecked. Note that the total window $2m + 1$ must strictly exceed **SG polynomial degree**. Increase $m$ for heavily oversampled or noisy patterns; decrease $m$ for coarse step sizes. |
| **SG polynomial degree** | `9` | `3` – `20` (integer) | Polynomial order of the Savitzky-Golay filter used to evaluate smoothed derivatives. Higher degrees preserve narrow peak shapes better; lower degrees (`3`–`5`) apply stronger derivative smoothing. |

> **Important:** > **Validation Rule**: When **Automatic window** is unchecked, the total Savitzky-Golay window size $2 \times (\text{Manual half-window}) + 1$ must be strictly greater than **SG polynomial degree**. If you choose a small manual half-window (for example, `3`, giving $2(3)+1 = 7$ points), reduce **SG polynomial degree** to `5` or lower.

---

### Tab 2: `Initial Peak Candidates`

Clicking **OK** in **Initial Peak Search Options** (or running **Preprocessing > Initial Peak Search**) executes the third-derivative detector, overlays candidate markers on the central diffraction plot, and switches the dialog to the **Initial Peak Candidates** tab so you can inspect the detected positions immediately.

![Initial Peak Search Options dialog showing the Initial Peak Candidates table after execution](../images/initial-peak-candidates.png)

| Column | Description |
| :--- | :--- |
| **Sample** | Zero-based sample index in the working diffraction pattern where the third-derivative zero crossing was detected. |
| **2θ (°)** | Angular position of the initial peak candidate in degrees $2\theta$. |
| **Observed intensity** | Working pattern intensity at the candidate sample index. |

At the same time, the central diffraction plot highlights the detected initial peak candidates along the diffraction profile:

![Central diffraction plot displaying detected initial peak candidate markers](../images/initial-peak-plot.png)

---

## Practical Tuning Guidelines

- **When `FWHM Estimation` reports too few isolated peaks**:
  Open **Preprocessing > Initial Peak Search Options...**, lower **Derivative threshold** (for example, from `0.1` to `0.05`), and click **OK**. Check the **Initial Peak Candidates** tab to confirm that clean single reflections are now detected.
- **When noisy low-count scans produce dozens of false-positive noise spikes**:
  Increase **Derivative threshold** (for example, to `0.25`–`0.50`) or uncheck **Automatic window** and increase **Manual half-window (samples)** (e.g., `15`–`21`) while lowering **SG polynomial degree** to `5`.
