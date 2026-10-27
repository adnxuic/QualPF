# Search-Match Overview

Once your experimental diffraction pattern has been imported and prepared (or immediately after importing if you rely on automatic workflow prerequisites), **QualPF** identifies crystalline phases by comparing your data against all active reference databases enabled in **Database > Manage Databases**.

All phase-identification and database-filtering tools are organized under the **Search-Match** menu.

---

## Methods

QualPF provides two complementary search-match engines—**CSM (Classical Peak-List Search-Match)** and **PFSM (Profile-Fitting Search-Match)**—which share the same reference database catalog, restraint filters, candidate results table, phase acceptance model, and Reference Intensity Ratio (**RIR**) quantification panel.

### Detailed Comparison: CSM vs. PFSM

| Dimension | Classical Search-Match (`Run CSM`) | Profile-Fitting Search-Match (`Run PFSM`) |
| :--- | :--- | :--- |
| **Menu Command** | **Search-Match > Run CSM** | **Search-Match > Run PFSM** |
| **Options Dialog** | **Search-Match > CSM Options...** | **Search-Match > PFSM Options...** |
| **Experimental Representation** | Uses the discrete **Experimental Peak List** ($2\theta$, $d$, $I$, FWHM) shown in the **Peaks** dock. | Uses the continuous **working diffraction pattern**, **background curve**, and representative **FWHM** (`Gaussian`, `Geometric`, or `Fixed`). |
| **Automatic Prerequisite Execution** | Requires extracted peaks (`Range` → `Background` → `Initial Peaks` → `FWHM` → `Peak Search`). | Automatically executes any missing prerequisite steps (`Range` → `Background` → `Initial Peaks` → `FWHM`) before running PFSM. Skips `Initial Peaks` when `Use fixed Default FWHM` is active. |
| **Database Pre-Screening** | Screens cards by strong-line $d$-spacing coincidence (`Check strongest peaks`) and active **Restraints**. | Uses multi-channel **Candidate Recall** (integral screening, local window shortlists, and protected channel top-K) plus active **Restraints** before formal profile fitting. |
| **Angular Shift Handling** | Matches reflections within a symmetric tolerance window **$\Delta 2\theta$** (manual or `Auto`). | Optimizes specimen displacement / zero-point shift continuously within configurable **Shift bounds** (`[Lower, Upper]` °$2\theta$, default `[-0.500, +0.500]`). |
| **Multiphase Decomposition** | **Residual Searching**: attenuates matched experimental peak intensities as phases are accepted so subsequent searches target remaining unexplained peaks. | **Joint Profile Fitting**: simultaneously fits non-negative scale factors for all accepted phases plus the currently highlighted candidate (`Joint Trial`), updating the live residual profile. |
| **Scoring Metric** | Classical Figure of Merit combining **2θ/d** position accuracy, **Intensity** agreement, and **Phases** (`Single` vs. `Multiple`) bias. | Profile-fitting Figure of Merit derived from weighted profile variance reduction, reflection coverage, and non-negative multiphase scale refinement. |

---

## When to Choose Each Method

### Choose **PFSM (`Search-Match > Run PFSM`)** when:
- Your sample is a **multi-phase mixture** with significant peak overlap where classical peak picking merges adjacent reflections or misplaces peak centroids.
- You want **one-click automated execution** directly from an imported pattern without manually tuning peak-picking thresholds first.
- Your specimen exhibits noticeable **sample displacement or zero-point shift** (up to $\pm 0.5^\circ\ 2\theta$) that would cause fixed-window $\Delta 2\theta$ peak matching to miss reflections.
- You want to visually verify candidate phases using a **live calculated profile and residual difference curve** (`Joint Trial`) on the central plot.

### Choose **CSM (`Search-Match > Run CSM`)** when:
- Your diffraction pattern consists of **well-resolved reflections** and you have already curated a clean peak list in the **Peaks** dock (including manual or inferred peak edits).
- You are working with a **peak-list-only dataset** or want fine-grained control over position vs. intensity FOM weights (`2θ/d` slider, `Intensity` slider, `Phases` Single↔Multiple slider).
- You want to perform iterative peak-table **Residual Searching** with customized $\Delta 2\theta$ tolerances.

---

## Combining Restraints with Search-Match

Before running either **Run CSM** or **Run PFSM**, you can restrict the candidate pool using **Search-Match > Restraints...** (`Ctrl+R`):

- **Chemical Composition**: Require, allow, or exclude specific elements on the Periodic Table (`And`, `Or`, `Not`, `Exact elements`, `Contains any`) and bound the number of distinct elements (`Minimum / Maximum number of species`).
- **Subfiles**: Restrict candidates to `Inorganic`, `Organic`, `Mineral`, `Metal & Alloy`, `Ceramic`, `Zeolite`, `Pharmaceutical`, and other curated database subfiles.
- **Properties**: Filter by chemical/mineral name substrings, color tokens, or calculated/measured crystal density ($\text{g/cm}^3$).
- **Entries**: Query specific database card IDs directly.
- **Symmetry**: Filter by crystal system (`Cubic`, `Tetragonal`, `Orthorhombic`, `Hexagonal/Trigonal`, `Monoclinic`, `Triclinic`), space-group symbol, or unit-cell dimensions ($a, b, c, \alpha, \beta, \gamma$).

You can also click **Search with Restraints** directly inside the **Restraints** dialog to launch the selected search-match engine (**SM: CSM** or **SM: PFSM**) immediately after configuring your filters.

---

## Detailed Guides in This Section

- [Executing Search-Match, Phase Acceptance, and Quantification](search-match.md) — Complete parameter tables and visual guides for **CSM Options**, **PFSM Options**, the **Results List** dock, **Compare** dock, **Quantification** panel, **Edit RIR** dialog, and **Report** generation.
- [Restraints](restraints.md) — Full reference for all five tabs of the **Restraints** dialog and direct database card browsing (`Load Cards` / `Load and Merge Cards`).
