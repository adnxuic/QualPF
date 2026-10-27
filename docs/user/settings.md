# Workflow Settings and Project History

QualPF separates **persistent default parameters for new projects** from **per-project runtime parameters and execution history**. Understanding how these three configuration surfaces interact ensures reproducible analyses across single patterns and large laboratory batches.

---

## The Three Configuration Surfaces in QualPF

| Surface | Where to Open | Scope | Purpose |
| :--- | :--- | :--- | :--- |
| **1. `Setting` Menu (`Setting — Defaults for New Projects`)** | **Setting > Preprocessing...**<br/>**Setting > CSM...**<br/>**Setting > PFSM...**<br/>**Setting > Peak Search...** | Persistent user preferences (applies to **future projects** created via **Import Pattern**) | Defines the baseline parameter profile that every newly imported pattern inherits. Does **not** overwrite or invalidate an already open project. |
| **2. Project `Options` Dialogs** | **Preprocessing > ... Options...**<br/>**Search-Match > CSM Options...**<br/>**Search-Match > PFSM Options...** | Active project only (saved inside `.qpf`) | Configures and executes steps on the **currently open pattern**. Changes take effect immediately for the current project without altering global defaults. |
| **3. `Workflow — Current Project` Dock** | **View > Workflow — Current Project** (dock panel) | Active project execution log (saved inside `.qpf`) | Records every manual and automatic workflow step executed on the current project, allows you to inspect exact runtime parameters, and supports non-destructive rollback (**Undo from Here**). |

---

## Configuring Defaults for New Projects (`Setting` Menu)

Choosing any item under the **Setting** menu opens the **Setting — Defaults for New Projects** dialog with that workflow selected in the left navigation list.

The left sidebar contains four workflow profiles:
1. **Preprocessing** — Default parameters when you run manual data-reduction actions from the **Preprocessing** menu.
2. **CSM** — Default prerequisite and search parameters for the Classical Peak-List Search-Match workflow.
3. **PFSM** — Default prerequisite, profile-fitting, and candidate-recall parameters for the Profile-Fitting Search-Match workflow.
4. **Peak Search** — Default prerequisite and extraction parameters for standalone Peak Search workflows.

> **Note:** > Why do **Preprocessing**, **CSM**, **PFSM**, and **Peak Search** each have their own tabs for modules like **Background**, **Initial Peaks**, and **FWHM**?
> Because different analysis routes benefit from different defaults! For example, **PFSM** defaults to **Geometric FWHM** ($Q = 6$) and does not require a classical peak table, whereas **CSM** and **Peak Search** default to **Gaussian fit (Classical)** FWHM and extract a discrete peak list.

### Example Views of `Setting — Defaults for New Projects`

![Setting — Defaults for New Projects dialog showing the CSM Background module tab](images/setting-csm-background.png)

![Setting — Defaults for New Projects dialog showing the PFSM Initial Peaks module tab](images/setting-pfsm-initial-peaks.png)

![Setting — Defaults for New Projects dialog showing the PFSM Profile Expansion (Template cache) module tab](images/setting-pfsm-template-cache.png)

---

## Complete Parameter Reference: `Setting — Defaults for New Projects`

### 1. Shared Data-Reduction Module Tabs (`Range`, `Background`, `Initial Peaks`, `FWHM`, `Smoothing`, `Kα2`)

These tabs appear in **Preprocessing** and (where applicable as automatic prerequisites) in **CSM**, **PFSM**, and **Peak Search**:

| Module Tab | Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- | :--- |
| **Range** | **Full pattern range** | Checked | Checked / Unchecked | When checked, new projects use the entire imported $2\theta$ domain. Uncheck to enforce fixed default **Minimum 2θ** and **Maximum 2θ** bounds. |
| **Range** | **Minimum 2θ (°)** / **Maximum 2θ (°)** | `0.000` / `180.000` | `0.000` – `180.000` | Default lower and upper angular limits applied when **Full pattern range** is unchecked. |
| **Background** | **Function type** | `Chebyshev` | `Chebyshev`, `Polynomial`, `Cosine Fourier Series`, `Cubic Spline`, `Bezier Spline`, `Filter`, `None` | Selects the default background modeling algorithm for the workflow. |
| **Background** | **Automatic** | Checked | Checked / Unchecked | Automatically selects the optimal number of basis coefficients (`Chebyshev`, `Polynomial`, `Cosine Fourier`). Uncheck to fix **Coefficients**. |
| **Background** | **Coefficients** | `6` | `1` – `36` (`Chebyshev`) or `1` – `12` (`Polynomial` / `Fourier`) | Number of polynomial or series terms used when **Automatic** is unchecked. |
| **Background** | **Filter iterations** / **Filter window size** | `5` / `50` | `0` – `1000` / `1` – `1000` | Default Bruckner smoothing-minimum filter iterations and window half-width when **Function type** is `Filter`. |
| **Background** | **Use full pattern range for filter** | Checked | Checked / Unchecked | Applies the Bruckner filter across the entire pattern range unless custom **Filter min 2θ** / **Filter max 2θ** bounds are specified. |
| **Initial Peaks** | **Derivative threshold** | `0.1` | `0.0` – `200.0` | Minimum normalized Savitzky-Golay third-derivative excursion required to register an initial peak candidate. Lower values detect weaker peaks; higher values reject noise. |
| **Initial Peaks** | **Automatic window** | Checked | Checked / Unchecked | Automatically determines the Savitzky-Golay half-window from the pattern step size. Uncheck to specify **Manual half-window (samples)**. |
| **Initial Peaks** | **Manual half-window (samples)** | `11` | `2` – `100` | Savitzky-Golay half-window $m$ (total window $2m + 1$ samples) used when **Automatic window** is unchecked. |
| **Initial Peaks** | **SG polynomial degree** | `9` | `3` – `20` | Polynomial degree of the Savitzky-Golay third-derivative filter. |
| **FWHM** | **Algorithm** | `Gaussian fit (Classical)` (in Preprocessing / CSM / Peak Search)<br/>`Geometric FWHM` (in PFSM) | `Gaussian fit (Classical)`, `Geometric FWHM` | Selects how the representative pattern FWHM is estimated from initial peak candidates. |
| **FWHM** | **Minimum sampling density Q** | `6` | `2` – `8` | Minimum number of data points across a peak required by the **Geometric FWHM** estimator. |
| **FWHM** | **Use fixed Default FWHM** | Unchecked | Checked / Unchecked | Bypasses automatic FWHM estimation and uses **Default FWHM (°2θ)** directly. |
| **FWHM** | **Default FWHM (°2θ)** | `0.1` | `> 0.0` – `180.0` | Fallback or fixed representative peak width in degrees $2\theta$. |
| **Smoothing** | **Method** | `Savitzky-Golay` | `Savitzky-Golay`, `Adjacent-Averaging` | Default smoothing filter method (available in the **Preprocessing** profile). |
| **Smoothing** | **Points of window** / **Polynomial order** | `5` / `3` | `2` – `50` / `1` – `10` | Window width and polynomial order for Savitzky-Golay smoothing. |
| **Kα2** | **Enable Kα2 stripping** | Unchecked | Checked / Unchecked | Controls whether $K\alpha_2$ stripping is enabled by default when a dual-wavelength source is present. |

### 2. Peak Search & CSM Module Tabs (`Peak Extraction`, `Peak Selection`, `Candidate Screening`, `Matching`, `FOM`, `Residual`, `Results`)

| Workflow & Tab | Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- | :--- |
| **Peak Search / CSM — Peak Extraction** | **Append new peaks** | Unchecked | Checked / Unchecked | When checked, newly extracted peaks are merged with existing peaks outside the search sub-interval instead of replacing the table. |
| **Peak Search / CSM — Peak Extraction** | **Use full pattern range** | Checked | Checked / Unchecked | Searches the full calculation range unless custom **Minimum 2θ** / **Maximum 2θ** bounds are set. |
| **Peak Search / CSM — Peak Extraction** | **Intensity threshold (%)** | `1.00` | `0.00` – `100.00` | Minimum relative peak intensity (as a percentage of the strongest peak) retained by formal peak search. |
| **Peak Search / CSM — Peak Extraction** | **Sensitivity** | `50` | `0` – `100` | Peak-picking curvature sensitivity (`0` = strict/major peaks only; `100` = maximum sensitivity to weak shoulders). |
| **Peak Search / CSM — Peak Selection** | **Limit peak count** / **Maximum peaks** | Unchecked / `100` | `1` – `10000` | Caps the maximum number of strongest peaks retained in the experimental peak table. |
| **CSM — Candidate Screening** | **Check strongest peaks** | Checked | Checked / Unchecked | Requires the strongest reflections of a database card to match experimental peaks during pre-screening. |
| **CSM — Candidate Screening** | **Check deleted cards** | Unchecked | Checked / Unchecked | Includes historical cards marked as deleted/superseded in ICDD PDF databases. |
| **CSM — Matching** | **Automatic Δ2θ** / **Δ2θ (°)** | Checked / `0.08` | `0.00` – `1.00` | Angular window for matching reference reflections to experimental peaks. |
| **CSM — FOM** | **Minimum FOM** | `0.35` | `0.00` – `1.00` | Minimum Figure of Merit required for a candidate card to appear in the results list. |
| **CSM — FOM** | **2θ/d weight** / **Intensity weight** / **Phases bias** | `0.50` / `0.50` / `0.50` | `0.00` – `1.00` | Relative weights for peak position accuracy, intensity agreement, and single-vs-multiphase scoring. |
| **CSM — Residual** | **Residual searching** | Checked | Checked / Unchecked | Subtracts matched experimental peak intensities iteratively to highlight minor phases. |
| **CSM — Results** | **Max. number of resulting entries** | `3000` | `50` – `10000` | Maximum number of candidate cards retained in the **Results List** dock. |

### 3. PFSM Module Tabs (`Candidate Recall`, `Profile Expansion`, `Profile Fitting`, `Joint Baseline`, `Results`)

| Module Tab | Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- | :--- |
| **Candidate Recall** | **Enable candidate recall** | Checked | Checked / Unchecked | Enables multi-channel candidate recall before full-profile nonlinear fitting so large databases can be screened rapidly without missing weak or shifted phases. |
| **Candidate Recall** | **On resource limit** | `Bounded integral recall` | `Bounded integral recall`, `Full-pool formal fitting` | Controls fallback behavior if the database pool exceeds memory/resource bounds (`Bounded integral recall` keeps memory bounded; `Full-pool formal fitting` forces full evaluation). |
| **Candidate Recall** | **Recall candidates** | `500` | `1` – `100000` | Size of the initial recalled candidate pool passed from fast integral screening to local scoring. |
| **Candidate Recall** | **Local shortlist** | `20` | `1` – `100000` | Number of top candidates per local angular window prioritized during multi-channel recall. |
| **Candidate Recall** | **Protected candidates per channel** | `5` | `0` – `100000` | Minimum number of top candidates guaranteed from each independent recall channel. |
| **Candidate Recall** | **Review set size** / **Formal candidate set size** | `80` / `80` | `1` – `100000` | Number of candidates entering refinement review and final formal profile-fitting ranking. |
| **Profile Expansion** | **Template cache** | Checked | Checked / Unchecked | Caches expanded reference profile templates in memory across PFSM runs and phase acceptance steps, dramatically accelerating repeated searches on the same pattern. |
| **Profile Fitting** | **Shift lower bound (°2θ)** / **Shift upper bound (°2θ)** | `-0.500` / `+0.500` | `-5.000` – `+5.000` | Allowed specimen-displacement / zero-shift interval $[\Delta 2\theta_{\min}, \Delta 2\theta_{\max}]$ searched for each candidate phase. |
| **Profile Fitting** | **Workers (0 = Auto)** | `0` | `0` – `256` | Number of CPU worker threads used for parallel PFSM evaluation (`0` automatically uses all available hardware threads). |
| **Joint Baseline** | **Weighting** | `Auto from input` | `Auto from input`, `Equal weights` | Selects Poisson counting-statistics weights (`Auto from input`, $w_i = 1/\max(y_i, 1)$ when unclipped raw counts are attested) or uniform weights (`Equal weights`, $w_i = 1$). |
| **Results** | **Min. relative FOM** | `0.000` | `0.000` – `1.000` | Filters out PFSM candidates whose relative Figure of Merit falls below the specified threshold. |

### Dialog Action Buttons (`Setting — Defaults for New Projects`)

- **Restore Page Defaults**: Resets **only the currently visible workflow/tab page** back to factory defaults without discarding changes made on other tabs.
- **Cancel**: Closes the dialog and discards any unsaved edits.
- **Save Defaults**: Persists all modified workflow defaults to user preferences for future projects.

---

## Inspect and Undo

While you work on an open diffraction pattern, the **Workflow — Current Project** dock records an ordered provenance trail of every executed step—whether triggered manually by you (e.g., **Preprocessing > Background**) or executed automatically as a prerequisite of **Run PFSM** or **Run CSM**.

![Workflow — Current Project dock showing completed Background, FWHM, and Search steps with View Details and Undo from Here buttons](images/workflow-dock.png)

### Columns in the `Workflow — Current Project` Table

| Column | Meaning |
| :--- | :--- |
| **Step** | Name of the workflow stage (`Range`, `Background`, `Initial Peaks`, `FWHM`, `Smoothing`, `Kα2`, `Peak Search`, `Search`). |
| **Workflow** | The workflow context under which the step ran (`Preprocessing`, `CSM`, `PFSM`, or `Peak Search`). |
| **Origin** | `Manual` (invoked directly by the user from a menu/dialog) or `Auto` (invoked automatically as a missing prerequisite of a higher-level command such as **Run PFSM**). |
| **Status** | Execution state (`Completed`, `Running`, `Failed`, or `Invalidated`). |
| **Detail** | Concise summary of the algorithm outcome (for example, `Geometric FWHM estimated on working observations: 0.20 deg 2theta (Q = 6)`). |

### Viewing Step Details (`View Details`)

Select any row in the **Workflow — Current Project** table and click **View Details** (or double-click the row) to open the **Workflow step details** dialog.

![Workflow step details dialog showing exact runtime parameters, dependencies, and status for a completed step](images/workflow-step-details.png)

This read-only inspector displays:
- The step identifier, parent workflow, origin (`Manual` vs. `Auto`), and completion status;
- Upstream step IDs (`Dependencies`) whose outputs fed into this step;
- The exact **Actual Parameters** dictionary (`method`, `q`, `fwhm_deg`, `shift_lower_deg`, `template_cache`, etc.) recorded at execution time.

### Rolling Back Project Steps (`Undo from Here`)

If you want to revert a step (for example, to try a different background model or undo a search-match run together with its prerequisite steps):

1. Highlight the step you wish to roll back in the **Workflow — Current Project** table.
2. Click **Undo from Here**.
3. QualPF atomically restores the project state to the snapshot immediately preceding that step and removes the selected step and all downstream steps that depended on it.
