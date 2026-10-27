# Executing Search-Match, Phase Acceptance, and Quantification

This chapter provides a complete parameter and workflow reference for configuring **CSM Options** and **PFSM Options**, evaluating candidate phases in the **Results List** and **Compare** docks, inspecting live **PFSM Joint Trial** profiles, accepting phases into the working solution, performing **Reference Intensity Ratio (RIR)** semi-quantitative analysis, and generating the final **Report**.

---

## Configuring Classical Search-Match (`Search-Match > CSM Options...`)

Select **Search-Match > CSM Options...** to configure the Figure of Merit (FOM) weights, angular matching window, and screening rules used when you run **Search-Match > Run CSM**.

![CSM Options dialog showing FOM Settings and Search Settings](../images/options-csm-native.png)

### Parameter Reference: `CSM Options` Dialog

| Group | Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- | :--- |
| **FOM Settings** | **Min. FOM** | `0.35` | `0.00` – `1.00` | Minimum Figure of Merit score required for a candidate card to be included in the **Results List** dock. Lower to `0.15`–`0.25` when searching for minor trace phases; raise to `0.50`+ to display only high-confidence matches. |
| **FOM Settings** | **2θ/d** | `0.50` | `0.00` – `1.00` (slider & spinbox) | Relative weight assigned to angular position agreement ($\Delta 2\theta / \Delta d$) between experimental peaks and reference reflections. Increase (e.g., to `0.75`) when peak positions are well calibrated but preferred orientation distorts intensities. |
| **FOM Settings** | **Intensity** | `0.50` | `0.00` – `1.00` (slider & spinbox) | Relative weight assigned to reflection intensity agreement. Decrease (e.g., to `0.20`) if strong texture/preferred orientation is present in the specimen. |
| **FOM Settings** | **Phases (`Single` ↔ `Multiple`)** | `0.50` | `0.00` (`Single`) – `1.00` (`Multiple`) | Adjusts the FOM penalty for unmatched experimental peaks vs. unmatched reference reflections. Move toward **Single (`0.00`)** when the sample is expected to be a single pure phase; move toward **Multiple (`1.00`)** when identifying multiple phases in a complex mixture. |
| **FOM Settings** | **Δ2θ (°)** & **Auto** | `0.08` (`Auto` checked) | `0.00` – `1.00`° | Maximum angular discrepancy allowed between an experimental peak and a reference reflection to count as a match. When **Auto** is checked, QualPF automatically derives $\Delta 2\theta$ from the pattern's experimental peak widths. Uncheck **Auto** to specify a fixed angular window (e.g., `0.12`° or `0.20`° when specimen displacement is present). |
| **Search Settings** | **Residual Searching** | Checked | Checked / Unchecked | When checked, accepting a phase (or running residual search) subtracts the matched intensity contributions from the working experimental peak list (`intensity`) while preserving the original intensities (`intensityOrig`), allowing minor phases to rank at the top. |
| **Search Settings** | **Check strongest peaks** | Checked | Checked / Unchecked | Requires at least one of the three strongest reflections of a reference card to match an experimental peak during fast database screening. Uncheck only if severe preferred orientation completely suppresses a phase's strongest reflection. |
| **Search Settings** | **Check deleted cards** | Unchecked | Checked / Unchecked | Includes historical cards flagged as deleted/replaced in ICDD PDF databases. |
| **Search Settings** | **Max. number of resulting entries** | `3000` | `50` – `10000` | Maximum number of candidate rows retained in the **Results List** dock after ranking. |

---

## Configuring Profile-Fitting Search-Match (`Search-Match > PFSM Options...`)

Select **Search-Match > PFSM Options...** to configure the profile-fitting search engine, specimen-shift bounds, observation weighting, parallel execution, and multi-channel **Candidate Recall** limits used by **Search-Match > Run PFSM**.

![PFSM Options dialog — General tab with default view](../images/pfsm-options.png)

![PFSM Options dialog — General tab with Advanced section expanded](../images/pfsm-options-advanced.png)

![PFSM Options dialog — Candidate Recall tab](../images/pfsm-options-recall.png)

![PFSM Options dialog when accepted phases are present, showing frozen shift/weighting provenance](../images/pfsm-options-frozen.png)

### Parameter Reference: `PFSM Options` — `General` Tab

| Section | Parameter / Readout | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- | :--- |
| **Shift bounds** | **Lower (°2θ)** / **Upper (°2θ)** | `-0.500` / `+0.500` | `-5.000` – `+5.000` (with $\text{Lower} \le \text{Upper}$) | Defines the allowed angular shift interval $[\Delta 2\theta_{\min}, \Delta 2\theta_{\max}]$ over which PFSM optimizes each candidate phase's position to compensate for specimen displacement, transparency, or zero-point offset. Narrowing to `[-0.15, +0.15]` speeds up search on well-aligned diffractometers; widening to `[-0.80, +0.80]` accommodates unaligned capillary or in-situ stages. |
| **Provenance Readouts** | **FWHM** | Read-only status | Current project FWHM state | Displays the active representative peak width and its provenance (e.g., `0.200 deg 2theta (Geometric, Q = 6)` or `Will run prerequisite FWHM estimation`). |
| **Provenance Readouts** | **Signal** | Read-only status | Current pattern provenance | Displays the working observation source, intensity units, and whether unclipped raw counts are attested for Poisson weighting. |
| **Weighting** | **Weighting policy** | `Auto from input` | `Auto from input`, `Equal weights` | Determines the least-squares observation weights $w_i$ across the diffraction profile. `Auto from input` uses Poisson counting weights ($w_i = 1/\max(y_i, 1)$) when unclipped raw counts are attested and automatically falls back to `Equal weights` ($w_i = 1$) if observations have been transformed or lack count provenance. Choosing `Equal weights` forces uniform weighting regardless of input provenance. |
| **Weighting** | **Resolved** | Read-only status | Active weight formula | Explicitly reports the resolved weighting mode that PFSM will execute (`Poisson counting weights` vs. `Equal weights`). |
| **Filtering** | **Min. relative FOM** | `0.000` | `0.000` – `1.000` | Discards candidate phases whose PFSM relative Figure of Merit is below the specified cutoff. |
| **Advanced** | **Workers (0 = Auto)** | `0` | `0` – `256` | Number of parallel CPU worker threads used during PFSM candidate screening and profile fitting (`0` uses all hardware threads automatically). |
| **Advanced** | **Template cache** | Checked | Checked / Unchecked | Caches expanded pseudo-Voigt/Lorentzian reference profile templates in RAM across searches and phase-acceptance steps on the active pattern. Keep checked for fast interactive phase acceptance. |

> **Important:** > **Frozen Parameters When Phases Are Accepted**:
> Once you have accepted one or more phases into a PFSM solution, **Shift bounds**, **Weighting**, and **Candidate Recall** settings are locked (frozen) in **PFSM Options** so that all accepted and trial phases remain on a mathematically consistent joint baseline. Clear the accepted phases in the **Quantification** panel if you wish to unlock and change those baseline parameters.

### Parameter Reference: `PFSM Options` — `Candidate Recall` Tab

When searching large databases (such as the full 500,000+ entry COD), PFSM uses a multi-stage, multi-channel **Candidate Recall** pipeline to identify promising candidates before running full nonlinear multiphase profile fitting:

| Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- |
| **Enable candidate recall** | Checked | Checked / Unchecked | Enables fast multi-channel integral and window recall prior to formal profile fitting. Unchecking evaluates the entire database pool directly with formal fitting (up to resource bounds). |
| **On resource limit** | `Bounded integral recall` | `Bounded integral recall`, `Full-pool formal fitting` | Controls behavior when the candidate pool is very large: `Bounded integral recall` enforces the recall funnel below to guarantee fast response and bounded memory; `Full-pool formal fitting` bypasses recall truncation when full exhaustive evaluation is desired. |
| **Recall candidates** | `500` | `1` – `100000` | Number of top candidates retained from global integral correlation screening across the database. |
| **Local shortlist** | `20` | `1` – `100000` | Number of top candidates shortlisted within each local angular peak window to ensure minor phases with localized reflections are captured. |
| **Protected candidates per channel** | `5` | `0` – `100000` | Minimum number of top-ranked candidates protected from each recall channel so a dominant major phase cannot crowd out a distinct minor-phase channel. |
| **Review set size** | `80` | `1` – `100000` | Number of candidates entering intermediate shift-and-scale review refinement. |
| **Formal candidate set size** | `80` | `1` – `100000` | Final number of candidates evaluated by full formal profile fitting and returned to the **Results List** dock. |

---

## Evaluating Candidates (`Results List`, Central Plot, and `Compare` Dock)

After running **Search-Match > Run CSM** or **Search-Match > Run PFSM**, candidate phases populate the **Results List** dock (**View > Results List**).

![Results List dock showing ranked PFSM candidate phases, FOM scores, chemical formulas, and pagination controls](../images/pfsm-candidates.png)

### 1. The `Results List` Dock

| Feature / Column | Role and Usage |
| :--- | :--- |
| **Checkbox / Double-Click Acceptance** | Check the box next to a candidate row (or double-click / use the accept action) to **accept** that phase into the **Quantification** panel. In PFSM mode, accepting a phase immediately recomputes the joint multiphase profile fit and updates candidate rankings against the residual profile. |
| **ID** | Database entry identifier (e.g., `9000101`). |
| **FOM** | Overall Figure of Merit score (sorted descending by default). |
| **Scale** | Fitted phase scale factor relative to the experimental pattern. |
| **Chemical Name / Mineral Name / Formula** | Phase identity and empirical/structural formula. |
| **Quality (`Q`)** | Crystallographic quality mark (`S` = Star, `I` = Indexed, `C` = Calculated, `B` = Blank/Low precision, etc.). |
| **Filter Box & Pagination** | Type any text in the filter bar to filter candidates on the current page, and use **Previous Page** / **Next Page** (`Search-Match > Previous Page` / `Next Page`) to browse large result sets. |

### 2. Live Joint Trial Preview on the Central Plot (PFSM)

When you highlight any candidate row in the **Results List** after a PFSM search, the central diffraction plot immediately renders a **live Joint Trial preview**:
- The calculated profile of all **already accepted phases plus the highlighted candidate phase** is drawn over the experimental pattern;
- The **difference curve** ($I_{\text{obs}} - I_{\text{calc}}$) updates at the bottom of the plot so you can see immediately whether the candidate explains the remaining residual peaks;
- Reference reflection stick bars for the candidate are displayed below the pattern.

![Central diffraction plot showing a PFSM Joint Trial overlay, accepted phase contributions, reflection sticks, and difference curve](../images/pfsm-joint-trial.png)

### 3. Reflection-by-Reflection Alignment (`Compare` Dock)

Open the **Compare** dock (**View > Compare**) to inspect numerical reflection matching between the experimental peak table and the currently selected candidate (plus any accepted phases).

![Compare dock showing experimental peaks aligned against reference card reflections and angular discrepancies](../images/compare-dock.png)

Each row in the **Compare** dock represents an experimental reflection ($2\theta$, $d$, $I$) alongside the closest matched reflection ($2\theta_{\text{ref}}$, $d_{\text{ref}}$, $I_{\text{ref}}$, $\Delta 2\theta$) from the highlighted candidate card and each accepted phase. Clicking rows in the **Compare** table highlights the corresponding reflection positions directly on the central plot.

---

## Quantification

Every phase you accept from the **Results List** is added to the **Quantification** panel (**View > Quantification**).

![Quantification panel showing accepted phases, RIR values, normalized weight percentages (Wt.%), and phase-fraction pie chart](../images/quantification-panel.png)

![PFSM selected phases view in the Quantification dock](../images/pfsm-selected-phases.png)

![Edit RIR dialog allowing custom Reference Intensity Ratio override and RIR source attribution](../images/rir-dialog.png)

### How Reference Intensity Ratio (RIR) Quantification Works

QualPF computes semi-quantitative crystalline weight fractions ($W_k$, in $\text{Wt.\%}$) for all accepted phases $k = 1, \dots, M$ using the normalized **Reference Intensity Ratio ($I/I_c$)** method:

$$W_k = \frac{S_k / \text{RIR}_k}{\sum_{j=1}^{M} \left(S_j / \text{RIR}_j\right)} \times 100\%$$

where:
- $S_k$ is the integrated/profile scale factor (in PFSM) or scaled reflection intensity measure (in CSM) of accepted phase $k$;
- $\text{RIR}_k$ is the Reference Intensity Ratio of phase $k$ relative to corundum ($\alpha\text{-Al}_2\text{O}_3$), taken either from the database card (`Reference RIR`) or from a user override (`Revised RIR`).

> **Important:** > **RIR Completeness Rule**:
> Because normalized RIR quantification requires dividing every accepted phase's scale factor by its $\text{RIR}_k$, **every accepted phase must have a valid positive RIR value ($\text{RIR}_k > 0$)** for weight percentages (`Wt.%`) and the pie chart to be computed. If any accepted phase is missing an RIR value in the database, the **Quantification** panel clearly indicates the `Missing RIR` status until you enter a value via **Edit RIR...**.

### `Quantification` Panel Controls and Columns

| Control / Column | Role and Behavior |
| :--- | :--- |
| **Accepted Phase Table (`ID`, `Name`, `Formula`, `RIR`, `RIR Source`, `Wt.%`)** | Lists all accepted phases, their active $\text{RIR}$ value, provenance badge (`Database` vs. `Revised` vs. `Missing`), and calculated weight percentage (`Wt.%`). |
| **Edit RIR...** | Opens the **Edit RIR** dialog for the selected phase so you can enter an experimentally calibrated or literature $\text{RIR}$ value (`> 0`) and record an optional **RIR source** note (for example, `Internal standard calibration (Cu Kα)`). |
| **Restore RIR** | Reverts a user-modified (`Revised`) RIR back to the original database `Reference RIR` value. |
| **Export CSV** | Exports the accepted phase quantification table (including phase IDs, names, formulas, scale factors, RIR values, RIR sources, and `Wt.%`) to a `.csv` file. |
| **Right-Click > Remove Phase** / **Clear All Phases** | Removes the selected phase (or all accepted phases) from the working solution and updates the plot and residual state. |
| **Pie Chart Tab / View** | Displays a color-coded graphical breakdown of the normalized phase weight fractions (`Wt.%`). |

---

## Generating and Printing the Analysis Report (`Report` Dock)

Switch to the **Report** dock (**View > Report**) to view a comprehensive, publication-ready summary of the current session.

![Report dock displaying pattern summary, preprocessing settings, active restraints, accepted phases, RIR quantification table, and experimental peak table](../images/report-dock.png)

The **Report** document updates automatically as you accept phases or refine RIR values and includes:
1. **Pattern & Radiation Summary** — Source filename, angular calculation range, radiation kind, and $\lambda_1 / \lambda_2$ wavelengths.
2. **Data-Reduction & Peak Search Parameters** — Active angular interval, intensity threshold, sensitivity, and restraints summary.
3. **Accepted Phases & Quantitative Breakdown** — Card IDs, chemical/mineral names, formulas, FOM scores, active RIR values, RIR provenance, and normalized `Wt.%`.
4. **Experimental Peak List & Reflection Assignments** — Complete tabular listing of experimental reflections ($2\theta$, $d$, relative intensity, FWHM) and matched phase reflections.

To print the report or save it as a PDF file, select **File > Print...** (`Ctrl+P`) while the **Report** dock is active.
