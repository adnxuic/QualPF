# Executing Search-Match, Phase Acceptance, and Quantification

This chapter provides a complete parameter and workflow reference for configuring **CSM Options** and **PFSM Options**, evaluating candidate phases in the **Results List** and **Compare** docks, inspecting live **PFSM Joint Trial** profiles, accepting phases into the working solution, performing **Reference Intensity Ratio (RIR)** semi-quantitative analysis, and generating the final **Report**.

---

## Configuring Classical Search-Match (`S&M > CSM Options...`)

Select **S&M > CSM Options...** to configure the Figure of Merit (FOM) weights, angular matching window, and screening rules used when you run **S&M > CSM (Classical SM)**.

![CSM Options dialog showing FOM Settings and Search Settings](../images/options-csm-native.png)

### Parameter Reference: `CSM Options` Dialog

| Group | Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- | :--- |
| **FOM Settings** | **Min. FOM** | `0.35` | `0.00` – `1.00` | Initial search retains scores **strictly greater than** this value. Recalculation retains positive scores **greater than or equal to** it. Lower values retain weaker matches. FOM is not a probability or calibrated identification confidence. |
| **FOM Settings** | **2θ/d** | `0.50` | `0.00` – `1.00` (slider & spinbox) | Relative weight assigned to angular position agreement ($\Delta 2\theta / \Delta d$) between experimental peaks and reference reflections. Increase (e.g., to `0.75`) when peak positions are well calibrated but preferred orientation distorts intensities. |
| **FOM Settings** | **Intensity** | `0.50` | `0.00` – `1.00` (slider & spinbox) | Weight of intensity agreement when more than one experimental peak is associated. A single associated peak omits this term. This setting also controls the residual-removal cutoff described below. |
| **FOM Settings** | **Phases (`Single` ↔ `Multiple`)** | `0.50` | `0.00` – `1.00` | Weights experimental peak coverage and changes the expected coverage per card. The interface labels the ends `Single` and `Multiple`, but the executed formula is described below; these labels must not be read as an actual phase-count setting. |
| **FOM Settings** | **Δ2θ (°)** & **Auto** | `Auto` on; saved manual value `0.08`° | Manual editor: `0.01` – `0.50`° | Angular matching tolerance. Auto uses $\min(0.5,\ 1.3\,\overline{\mathrm{FWHM}})$ degrees from the current experimental peak widths, with `0.5`° for an empty table. The saved manual value is inactive while Auto is on. Strongest-line screening uses the corresponding per-peak d-spacing tolerances. |
| **Search Settings** | **Residual Searching** | Checked | Checked / Unchecked | On phase acceptance, subtracts matched contributions from the current experimental peak intensities, removes exhausted peaks and recalculates the remaining candidates. Original heights of surviving peaks are retained. It does not subtract a continuous profile or perform another database query. |
| **Search Settings** | **Check strongest peaks** | Checked | Checked / Unchecked | Requires **all** of the first up to three stored strongest-line d-spacings to pass the experimental-peak screen. See the rules below before using it with missing or out-of-range strong lines. |
| **Search Settings** | **Check deleted cards** | Checked | Checked / Unchecked | Excludes cards whose space-trimmed quality is `D`. On the SQL query route, a NULL quality also fails this predicate. Unchecking disables this exclusion. |
| **Search Settings** | **Max. number of resulting entries** | `3000` | Positive integer | Caps retained candidates on initial CSM search and recalculation. It does not cap restrained card browsing or set the number of rows per display page. |

The **Phases** value $p$ enters the calculation twice: it weights an experimental
coverage term, and sets $n=\operatorname{round}(10-9p)$, with half-integers rounded
up. The expected experimental peaks per phase are
$\max(1,\lfloor N_{\mathrm{exp}}/n\rfloor)$. Thus `p = 0` uses a divisor of `10`,
while `p = 1` uses `1`; increasing the value increases the expected coverage per
card as well as the coverage term's weight. Do not infer the opposite behavior
from the displayed `Single` / `Multiple` labels. None of the three FOM weights
has to complement another, and an effective zero weight denominator is an error.

### How `Check strongest peaks` actually screens cards

- The screen reads the database's stored strongest-line index in its stored
  order; it does not reconstruct or sort strong lines from the decoded card.
  It tests the first $k=\min(3,N_{\mathrm{stored}})$ d-spacings, **all** of which
  must pass. One matching line out of three is insufficient.
- There must be at least $k$ experimental peaks. Each tested line must satisfy
  $|d_{\mathrm{stored}}-d_{\mathrm{exp}}|\leq\Delta d_{\mathrm{exp}}$ in the
  ordered peak-list lookup. Experimental intensities are not compared here,
  and the same experimental peak may satisfy several stored lines.
- A stored index with one or two lines tests all available lines. An existing
  index row with no stored spacings passes this screen; a card missing entirely
  from that index is not recalled by this route. Passing does not establish an
  acceptable FOM.
- Stored lines are not first restricted to the calculation range. Missing
  lines, including strong reflections outside the measured range, can reject
  a phase before scoring. The ordered lookup uses a bounded neighbor search;
  with nonuniform per-peak tolerances it is not an exhaustive overlap scan.
- Unchecking this option skips this stored-line screen. Active restraints,
  deleted-card filtering, FOM evaluation and the result limit still apply.
  Re-run CSM to retrieve previously screened-out cards. Recalculation and
  residual acceptance reuse the existing candidate collection without applying
  this screen again.

For residual acceptance, a matched height is reduced by the scaled reference
height and clipped at zero. If the remainder is at most
$0.2(1-w_I)$ times that peak's original height, it is also set to zero; $w_I$ is
the **Intensity** weight. Zero-height rows are removed. Removing an accepted CSM
phase does not restore these rows or heights; use an available workflow undo or
restart and search again.

---

## Configuring Profile-Fitting Search-Match (`S&M > PFSM Options...`)

Select **S&M > PFSM Options...** to configure the profile-fitting search engine, specimen-shift bounds, observation weighting, parallel execution, and multi-channel **Candidate Recall** limits used by **S&M > PFSM**.

![PFSM Options dialog — General tab with default view](../images/pfsm-options.png)

![PFSM Options dialog — General tab with Advanced section expanded](../images/pfsm-options-advanced.png)

![PFSM Options dialog — Candidate Recall tab](../images/pfsm-options-recall.png)

![PFSM Options dialog when accepted phases are present, showing frozen shift/weighting provenance](../images/pfsm-options-frozen.png)

### Parameter Reference: `PFSM Options` — `General` Tab

| Section | Parameter / Readout | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- | :--- |
| **Shift bounds** | **Lower (°2θ)** / **Upper (°2θ)** | `-0.500` / `+0.500` | Finite values with $\text{Lower} \le \text{Upper}$ | Bounds a constant additive $2\theta$ shift for each phase; positive shifts move its reflections to higher angles. Equal bounds fix the shift. This can approximate an offset but does not fit an angle-dependent displacement or transparency model. Wider intervals require more search work and do not guarantee a satisfactory correction. |
| **Provenance Readouts** | **FWHM** | Read-only status | Current project FWHM state | Displays the active representative peak width and its provenance (e.g., `0.200 deg 2theta (Geometric, Q = 6)` or `Will run prerequisite FWHM estimation`). |
| **Provenance Readouts** | **Signal** | Read-only status | Current working observations | Identifies the signal being fitted; the separate **Resolved** readout reports the selected weighting. |
| **Weighting** | **Weighting policy** | `Auto from input` | `Auto from input`, `Equal weights` | Determines the least-squares observation weights $w_i$ across the diffraction profile. `Auto from input` uses compatible attested noise, including inverse-variance or Poisson weights, when supplied. Current text/CIF imports do not attest those statistics, so ordinary GUI imports resolve to `Equal weights` ($w_i = 1$). Transformed observations do not inherit raw-count statistics. Choosing `Equal weights` forces uniform weighting regardless of input provenance. |
| **Weighting** | **Resolved** | Read-only status | Active weight formula | Reports equal weights or the available attested noise route, including supplied inverse variance, Poisson counts or count-rate variance. Ordinary current GUI imports resolve to equal weights. |
| **Filtering** | **Min. relative FOM** | `0.000` | `0.000` – `1.000` | Filters displayed candidates by relative FOM without discarding canonical fit evidence. |
| **Advanced** | **Workers (0 = Auto)** | `0` | Nonnegative integer | Number of parallel CPU worker threads used during PFSM candidate screening and profile fitting (`0` selects workers automatically). |
| **Advanced** | **Template cache** | Checked | Checked / Unchecked | Uses interpolated Intermediate Lorentzian templates during fitting iterations. Preparation consumes time and memory; small searches may not benefit. Final fits are verified by direct profile evaluation. Unchecked uses direct evaluation throughout fitting. |

> **Current-project scope:** FOM changes the display immediately; workers apply to
> the next run. Shift bounds, weighting, cache and recall inputs belong to a frozen
> analysis. Changing those project settings requires confirmation before dependent
> results are removed; the dialog also displays the current frozen configuration.

### Parameter Reference: `PFSM Options` — `Candidate Recall` Tab

When searching large databases (such as the full 500,000+ entry COD), PFSM uses a multi-stage, multi-channel **Candidate Recall** pipeline to identify promising candidates before running full nonlinear multiphase profile fitting:

| Parameter | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- |
| **Enable candidate recall** | Checked | Checked / Unchecked | Prescreens before formal fitting. Disabled evaluates the candidate pool directly, subject to resource checks. Approximate recall can omit a phase. |
| **On resource limit** | `Bounded integral recall` | Bounded integral or full-pool fitting | Selects the alternative when standard v2 recall hits its resource route. Bounded integral uses fixed channel budgets, ties and protection; the five standard-v2 counts below do not define its funnel. |
| **Recall candidates** | `500` | Positive integer | Target from alternating original/residual channels; cutoff ties and numerical protection are additional. |
| **Local shortlist** | `20` | Positive, at most recall target | Top-K before and after local review. This is not a count per angular window. |
| **Protected candidates per channel** | `5` | Positive, at most local shortlist | Protects channel leaders within the recalled set, including ties. |
| **Review set size** | `80` | At least local shortlist | Full-pool fallback threshold if the review set grows larger. |
| **Formal candidate set size** | `80` | At least local shortlist | Full-pool fallback threshold if the proposed formal set grows larger; not a fit-count cap. |

---

## Evaluating Candidates (`Results List`, Central Plot, and `Compare` Dock)

After running **S&M > CSM (Classical SM)** or **S&M > PFSM**, candidate phases populate the **Results List** dock (**Window > Results List**).

![Results List dock showing ranked PFSM candidate phases, FOM scores, chemical formulas, and pagination controls](../images/pfsm-candidates.png)

### 1. The `Results List` Dock

| Feature / Column | Role and Usage |
| :--- | :--- |
| **Phase acceptance** | Select scored rows and choose **Entry > Accept Selected Entries** (`Ctrl+Return`), or the corresponding context action. Unscored browsed cards and already accepted phases are skipped. PFSM runs a worker operation and publishes the accepted joint fit and refreshed candidate ranking together when it completes. |
| **ID** | Database entry identifier (e.g., `9000101`). |
| **FOM** | Overall Figure of Merit score (sorted descending by default). |
| **Scale** | Fitted phase scale factor relative to the experimental pattern. |
| **Chemical Name / Mineral Name / Formula** | Phase identity and empirical/structural formula. |
| **Quality (`Q`)** | Crystallographic quality mark (`S` = Star, `I` = Indexed, `C` = Calculated, `B` = Blank/Low precision, etc.). |
| **Filter Box & Pagination** | Type any text in the filter bar to filter candidate rows before pagination, and use **Previous Page** / **Next Page** (navigation buttons in the results toolbar) to browse large result sets. |

### 2. Live Joint Trial Preview on the Central Plot (PFSM)

When you highlight a candidate row after a PFSM search, the central plot displays
its stored **Joint Trial** fit once the background preview task completes.
Selection displays the fitted evidence; it does not accept the phase or optimize
a new fit:

- The calculated profile of all **already accepted phases plus the highlighted candidate phase** is drawn over the experimental pattern;
- The **difference curve** ($I_{\text{obs}} - I_{\text{calc}}$) updates at the bottom of the plot so you can see immediately whether the candidate explains the remaining residual peaks;
- Reference reflection stick bars for the candidate are displayed below the pattern.

![Central diffraction plot showing a PFSM Joint Trial overlay, accepted phase contributions, reflection sticks, and difference curve](../images/pfsm-joint-trial.png)

### 3. Reflection-by-Reflection Alignment (`Compare` Dock)

Open the **Compare** dock (**Window > Compare**) to inspect numerical reflection matching between the experimental peak table and the currently selected candidate (plus any accepted phases).

![Compare dock with experimental and reference angle/intensity pairs and association colors](../images/compare-dock.png)

The table has `2θ exp` / `I exp` columns and one `2θ [ID]` / `I [ID]` pair
for each accepted phase and the selected card. It has no d-spacing or
$\Delta 2\theta$ column. Experimental rows show the associated scaled reference
reflection where available, with a dash for a missing association. Additional
rows show unmatched reflections of the selected card, so not every row has an
experimental peak. Association colors reflect the existing angular/intensity
comparison; fully compensated experimental rows are gray. Selecting cells
highlights the corresponding points on the central plot.

This is a classical peak-table comparison, not the PFSM joint-fit diagnostic.
PFSM hides this dock after searching; inspect its joint profile, difference curve
and fit metrics instead. Opening Compare manually does not create a PFSM peak
table or apply the fitted phase shifts to this classical comparison.

---

## Quantification

Every phase you accept from the **Results List** is added to the **Quantification** panel (**Window > Quantitative analysis**).

![Quantification panel showing accepted phases, RIR values, normalized weight percentages (Wt.%), and phase-fraction pie chart](../images/quantification-panel.png)

![PFSM selected phases view in the Quantification dock](../images/pfsm-selected-phases.png)

![Edit RIR dialog allowing custom Reference Intensity Ratio override and RIR source attribution](../images/rir-dialog.png)

### How Reference Intensity Ratio (RIR) Quantification Works

QualPF computes semi-quantitative crystalline weight fractions ($W_k$, in $\text{Wt.\%}$) for all accepted phases $k = 1, \dots, M$ using the normalized **Reference Intensity Ratio ($I/I_c$)** method:

$$W_k = \frac{A_k}{\sum_j A_j}\times 100\%, \qquad
A_k = \frac{s_k M_k}{\mathrm{RIR}_k}$$

Here $s_k$ is the accepted joint PFSM scale and $M_k$ is the maximum intensity
of the **full frozen reference card**, before window restriction, expansion or
sampling. Omitting $M_k$ gives incorrect fractions for differently normalized
cards. In CSM, the corresponding amount is
$A_k=\max_j(I_{kj}s_k)/\mathrm{RIR}_k$ from the accepted card intensities and
its retained classical scale. All accepted CSM phases must have calculated,
compatible scores and positive RIR values, and the total corrected amount must
be positive.

PFSM/RIR currently requires Cu Kalpha-compatible radiation, a finite positive RIR
and reference maximum for every accepted phase, verified joint-fit diagnostics
and identifiable nonzero templates. A positive RIR alone is insufficient.
Percentages are normalized over selected crystalline phases; unidentified and
amorphous material are excluded. Reference-library RIR source conditions are
unverified and preferred orientation, microabsorption and model mismatch can bias
these semi-quantitative estimates. FOM is not a confidence probability.

> **RIR completeness:**
> **Every accepted phase must have a finite positive RIR**, including a PFSM
> phase whose fitted scale is zero. One invalid phase makes all percentages
> unavailable. The panel shows `Quantification unavailable`, phase-specific
> diagnostics and dashes instead of percentages, and hides the pie chart.
> In PFSM you can supply a calibrated value through **Edit RIR...**; Classical
> mode does not expose RIR overrides. A zero PFSM scale may give `0%` when all
> other requirements hold and the total corrected amount is positive.

### `Quantification` Panel Controls and Columns

| Control / Column | Role and Behavior |
| :--- | :--- |
| **Accepted Phase Table** | Lists `ID`, `Name` and `wt.%`; PFSM additionally shows `Scale`, `Shift °2θ` and `RIR`. RIR provenance is available in its tooltip and export/report. Unavailable values show a dash. |
| **Edit RIR...** | PFSM only. Requires a finite positive **Effective RIR** and a nonempty **Source / calibration** note. Applicable radiation is Cu Kα. RIR edits change quantification, not fitted scales, shifts or candidate ranking. |
| **Restore reference RIR** | Reverts a user-modified (`Revised`) RIR back to the original database `Reference RIR` value. |
| **Export CSV** | Exports method, phase IDs/names, available percentages, scales, shifts, effective/reference RIR, source notes, status/reasons and fit provenance/diagnostics. There is no chemical-formula column. Export is available with selected phases even when quantification is unavailable; percentage fields are then empty. |
| **Right-Click > Remove Phase** | Removes the selected accepted phase. PFSM jointly refits and reranks; Classical removal does not restore intensities already subtracted from its residual. |
| **Pie Chart Tab / View** | Displays a color-coded graphical breakdown of the normalized phase weight fractions (`Wt.%`). |

PFSM RIR revisions remain available after removal and reselection of the same
card within its frozen analysis. Ending that analysis clears the revisions.

---

## Generating and Printing the Analysis Report (`Report` Dock)

Switch to the **Report** dock (**Window > Report**) to view a comprehensive, publication-ready summary of the current session.

![Report dock displaying pattern summary, preprocessing settings, active restraints, accepted phases, RIR quantification table, and experimental peak table](../images/report-dock.png)

The **Report** document updates automatically as you accept phases or refine RIR values and includes:

1. **Pattern & Radiation Summary** — Source filename, angular calculation range, radiation kind, and $\lambda_1 / \lambda_2$ wavelengths.
2. **Peak Search Readout & Restraints** — The peak-search dialog's interval, threshold and sensitivity readouts, plus the current restraints summary. For the actual parameters of an automatic CSM/PFSM run, select its record in **Workflow — Current Project** and click **Details**.
3. **Search Results & Quantitative Breakdown** — Up to 100 cards from the canonical result collection, followed by accepted-phase quantification and RIR provenance. This is not just the currently filtered display page or a guarantee of its displayed ordering.
4. **Experimental Peak List** — Extracted experimental $2\theta$ positions and intensities. Use the **Peaks** table or peak export for $d$-spacing and FWHM details, and **Compare** for reflection comparisons.

Source paths, card metadata and restraint text retain their literal characters
in the report and printed PDF, including angle brackets and ampersands.

To print the report or save it as a PDF file, select **File > Print...** (`Ctrl+P`), choose **Report** in the content dialog, then choose the printer/PDF destination.
