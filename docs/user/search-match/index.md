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
| **Experimental Representation** | Uses the discrete **Experimental Peak List** ($2\theta$, $d$, $I$, FWHM) and a real continuous spectrum with a calculation range. | Uses the continuous **working diffraction pattern**, **background curve**, and one common representative **FWHM** (automatic geometric, compatible manually measured Gaussian/geometric, or fixed). |
| **Automatic Prerequisite Execution** | Requires extracted peaks (`Background` → Gaussian `Initial Peaks`/`FWHM`, or geometric/fixed width → `Peak Search`; Range is explicit). | Automatically executes any missing prerequisite steps (`Background` → geometric `FWHM` with internal candidates, unless a compatible measured or fixed width is supplied) before running PFSM. It does not insert SG Initial Peak Search or formal Peak Search. |
| **Database Pre-Screening** | With `Check strongest peaks`, all first up to three stored strong-line d-spacings must pass the screen; active **Restraints** also apply. | Uses multi-channel **Candidate Recall** (standard v2 original/residual channels or bounded integral recall, retaining ties and protected candidates) plus active **Restraints** before formal profile fitting. |
| **Angular Shift Handling** | Matches reflections within **$\Delta 2\theta$** (manual or `Auto`); the strongest-line screen uses derived d-spacing windows. | Fits one constant additive angular shift per phase within **Shift bounds** (default `[-0.500, +0.500]` °$2\theta$). This is not an angle-dependent displacement correction. |
| **Multiphase Decomposition** | **Residual Searching**: attenuates matched experimental peak intensities as phases are accepted so subsequent searches target remaining unexplained peaks. | **Joint Profile Fitting**: simultaneously fits non-negative scale factors for all accepted phases plus the currently highlighted candidate (`Joint Trial`), updating the live residual profile. |
| **Scoring Metric** | Classical Figure of Merit combining position accuracy, intensity agreement and peak coverage. See the executed **Phases** formula rather than interpreting its endpoint labels as a phase count. | Relative FOM from improvement in the common baseline's net Rwp; scores share the same frozen observations and accepted-phase baseline. |

---

## When to Choose Each Method

### Choose **PFSM (`S&M > PFSM`)** when:
- Your sample is a **multi-phase mixture** with significant peak overlap where classical peak picking merges adjacent reflections or misplaces peak centroids.
- You want **one-click automated execution** directly from an imported pattern without manually tuning peak-picking thresholds first.
- A constant phase angular offset would cause fixed-window peak matching to miss reflections. The default bounds are $\pm 0.5^\circ\ 2\theta$ and can be changed; this is not a universal displacement-correction limit.
- You want to visually verify candidate phases using a **live calculated profile and residual difference curve** (`Joint Trial`) on the central plot.

### Choose **CSM (`S&M > CSM (Classical SM)`)** when:
- Your diffraction pattern consists of **well-resolved reflections** and you have already curated a clean peak list in the **Peaks** dock (including manual or inferred peak edits).
- You want control over position, intensity and coverage FOM weights. A curated or imported peak table can be used with its compatible continuous pattern; **a peak-list-only dataset cannot run CSM**.
- You want to perform iterative peak-table **Residual Searching** with customized $\Delta 2\theta$ tolerances.

---

## Combining Restraints with Search-Match

Before running either **Run CSM** or **Run PFSM**, you can restrict the candidate pool using **S&M > Restraints...**:

- **Chemical Composition**: Require, allow, or exclude specific elements on the Periodic Table (`And`, `Or`, `Not`, `Exact elements`, `Contains any`) and bound the number of distinct elements (`Minimum / Maximum number of species`).
- **Subfiles**: Restrict candidates to `Inorganic`, `Organic`, `Mineral`, `Alloy`, `Ceramic`, `Zeolite`, `Pharmaceutical`, and other indexed database subfiles.
- **Properties**: Filter by chemical/mineral name substrings, color tokens, or calculated/measured crystal density ($\text{g/cm}^3$).
- **Entries**: Query specific database card IDs directly.
- **Symmetry**: Filter by crystal system (including separate `Hexagonal/trigonal` and `Rhombohedral` choices), space-group symbol, or unit-cell dimensions. Selected space groups and crystal systems form a union; unit-cell restrictions intersect it.

You can also click **Search with Restraints** directly inside the **Restraints** dialog to launch the selected search-match engine (**SM: CSM** or **SM: PFSM**) immediately after configuring your filters.

---

## Detailed Guides in This Section

- [Executing Search-Match, Phase Acceptance, and Quantification](search-match.md) — Complete parameter tables and visual guides for **CSM Options**, **PFSM Options**, the **Results List** dock, **Compare** dock, **Quantification** panel, **Edit RIR** dialog, and **Report** generation.
- [Restraints](restraints.md) — Full reference for all five tabs of the **Restraints** dialog and direct database card browsing (`Load Cards` / `Load and Merge Cards`).
