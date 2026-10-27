# Search-Match Overview

Once your experimental diffraction pattern has been imported and prepared (or immediately after importing if you rely on automatic workflow prerequisites), **QualPF** identifies crystalline phases by comparing your data against the single selected reference database in **S&M > Manage Databases**.

All phase-identification and database-filtering tools are organized under the **S&M** menu.

---

## Methods

QualPF provides two complementary search-match engines—**CSM (Classical Peak-List Search-Match)** and **PFSM (Profile-Fitting Search-Match)**—which share the same reference database catalog, restraint filters, candidate results table, phase acceptance model, and Reference Intensity Ratio (**RIR**) quantification panel.

### Detailed Comparison: CSM vs. PFSM

| Dimension | Classical Search-Match (`Run CSM`) | Profile-Fitting Search-Match (`Run PFSM`) |
| :--- | :--- | :--- |
| **Menu Command** | **S&M > CSM (Classical SM)** | **S&M > PFSM** |
| **Options Dialog** | **S&M > CSM Options...** | **S&M > PFSM Options...** |
| **Experimental Representation** | Uses the discrete **Experimental Peak List** ($2\theta$, $d$, $I$, FWHM) shown in the **Peaks** dock. | Uses the continuous **working diffraction pattern**, **background curve**, and representative **FWHM** (`Gaussian`, `Geometric`, or `Fixed`). |
| **Automatic Prerequisite Execution** | Requires extracted peaks (`Background` → Gaussian `Initial Peaks`/`FWHM`, or geometric/fixed width → `Peak Search`; Range is explicit). | Automatically executes any missing prerequisite steps (`Background` → geometric `FWHM` with internal candidates, unless a compatible measured or fixed width is supplied) before running PFSM. It does not insert SG Initial Peak Search or formal Peak Search. |
| **Database Pre-Screening** | Screens cards by strong-line $d$-spacing coincidence (`Check strongest peaks`) and active **Restraints**. | Uses multi-channel **Candidate Recall** (standard v2 original/residual channels or bounded integral recall, retaining ties and protected candidates) plus active **Restraints** before formal profile fitting. |
| **Angular Shift Handling** | Matches reflections within a symmetric tolerance window **$\Delta 2\theta$** (manual or `Auto`). | Optimizes specimen displacement / zero-point shift continuously within configurable **Shift bounds** (`[Lower, Upper]` °$2\theta$, default `[-0.500, +0.500]`). |
| **Multiphase Decomposition** | **Residual Searching**: attenuates matched experimental peak intensities as phases are accepted so subsequent searches target remaining unexplained peaks. | **Joint Profile Fitting**: simultaneously fits non-negative scale factors for all accepted phases plus the currently highlighted candidate (`Joint Trial`), updating the live residual profile. |
| **Scoring Metric** | Classical Figure of Merit combining **2θ/d** position accuracy, **Intensity** agreement, and **Phases** (`Single` vs. `Multiple`) bias. | Relative FOM from improvement in the common baseline's net Rwp; scores share the same frozen observations and accepted-phase baseline. |

---

## When to Choose Each Method

### Choose **PFSM (`S&M > PFSM`)** when:
- Your sample is a **multi-phase mixture** with significant peak overlap where classical peak picking merges adjacent reflections or misplaces peak centroids.
- You want **one-click automated execution** directly from an imported pattern without manually tuning peak-picking thresholds first.
- Your specimen exhibits noticeable **sample displacement or zero-point shift** (up to $\pm 0.5^\circ\ 2\theta$) that would cause fixed-window $\Delta 2\theta$ peak matching to miss reflections.
- You want to visually verify candidate phases using a **live calculated profile and residual difference curve** (`Joint Trial`) on the central plot.

### Choose **CSM (`S&M > CSM (Classical SM)`)** when:
- Your diffraction pattern consists of **well-resolved reflections** and you have already curated a clean peak list in the **Peaks** dock (including manual or inferred peak edits).
- You are working with a **peak-list-only dataset** or want fine-grained control over position vs. intensity FOM weights (`2θ/d` slider, `Intensity` slider, `Phases` Single↔Multiple slider).
- You want to perform iterative peak-table **Residual Searching** with customized $\Delta 2\theta$ tolerances.

---

## Combining Restraints with Search-Match

Before running either **Run CSM** or **Run PFSM**, you can restrict the candidate pool using **S&M > Restraints...**:

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
