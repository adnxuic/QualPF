# Restraints and Database Card Browsing

In phase identification of minerals, ceramics, alloys, and synthetic materials, you often possess prior chemical or crystallographic knowledge about the specimen—such as EDS/XRF elemental composition, material class, density, or unit-cell symmetry. The **Restraints** dialog (**S&M > Restraints...**) lets you translate that prior knowledge into fast SQL database filters applied during **CSM** or **PFSM**, or browse matching reference cards directly without running a search.

---

## Overview of the `Restraints` Dialog

Open **S&M > Restraints...** to access the five-tab **Restraints** window:
1. **Composition** — Periodic Table elemental logic and chemical species count bounds.
2. **Subfiles** — Database subfile category filters (Inorganic, Mineral, Alloy, Ceramic, etc.).
3. **Properties** — Chemical/mineral name substring, color, and crystal density filters.
4. **Entries** — Direct lookup by space-separated database card IDs.
5. **Symmetry** — Crystal system, space-group symbol, and unit-cell parameter ($a, b, c, \alpha, \beta, \gamma$) restraints.

> **Note:** Restraint categories intersect: a composition restriction and
> `Inorganic` must both be satisfied. Within Symmetry, however, selected
> **space groups and crystal systems combine with OR**; adding a crystal system
> can broaden a space-group selection. Populated unit-cell bounds still
> intersect that union.

---

## Tab 1: `Composition` (Periodic Table & Species Count)

![Restraints dialog showing the Composition tab with Periodic Table, Boolean logic operators, SM method selector, and species count limits](../images/restraints-pfsm.png)

### How to Build Chemical Restraints on the Periodic Table

You can specify elemental restraints either by clicking element buttons on the interactive Periodic Table (left-click cycles selection state) or by selecting an operator mode (`And`, `Or`, `Not`, `Exact elements`, `Contains any`) and clicking elements to build an expression in the **Formula / Restraint Expression** bar at the top.

### Parameter Reference: `Composition` Tab

| Control / Parameter | Default | Options / Range | Role and Effect |
| :--- | :--- | :--- | :--- |
| **Formula / Restraint Expression Bar** | Empty | Boolean element expression | Displays the active elemental expression. Edit it directly or build it with Periodic Table selections and the operator buttons. **Clear** resets the composition controls. |
| **And** (Must Be Present) | Mode button | Element group | Requires all selected elements in the group to be present in the candidate phase's chemical formula. |
| **Or** (Possible) | Mode button | Element group | Allows any of the selected elements in the group to be present. |
| **Not** (Excluded) | Mode button | Element group | Excludes any database phase containing one or more of the selected elements. |
| **Exact elements** | Mode button | Element set | Restricts results to phases composed **exclusively** of the exact set of selected elements (no additional elements permitted). |
| **Contains any** | Mode button | Element set | Allows any nonempty subset of the selected elements and excludes elements outside that set. In contrast, the **Or** mode can also admit unselected elements. |
| **Period / Group / Series Buttons** | Unselected | Selection toggles | `P1`–`P7`, groups `1`–`18`, `La *` and `Ac **` select or deselect the corresponding elements. The active composition operator determines their filtering meaning. |
| **Clear** (Composition button) | Button | Action | Clears element selections and the expression, returns to `And`, and resets both species bounds to `any`. Other tabs are unchanged. |
| **SM (`CSM` / `PFSM`)** | `CSM` (or active workflow) | `CSM`, `PFSM` | Selects which search-match engine will be launched if you click **Search with Restraints** at the bottom of the dialog. This selection stays synchronized across all five tabs. |
| **Minimum number of species** | `any` (`0`) | `0` (`any`) – `99` | Minimum number of distinct chemical elements (species) that a candidate phase must contain. For example, setting `Min = 2` and `Max = 2` restricts results to binary compounds. |
| **Maximum number of species** | `any` (`0`) | `0` (`any`) – `99` | Maximum number of distinct chemical elements that a candidate phase may contain (`0` means `any`, i.e., no upper bound). |

---

## Tab 2: `Subfiles`

Switch to the **Subfiles** tab to filter candidate cards by their curated database classification.

![Restraints dialog showing the Subfiles tab with Main Subfiles and Additional Subfiles checkboxes](../images/restraints-subfiles.png)

### Parameter Reference: `Subfiles` Tab

| Group | Available Checkboxes | Role and Effect |
| :--- | :--- | :--- |
| **Main Subfiles** | `Inorganic`, `Organic`, `Mineral` | Multiple checked subfiles intersect: a card must belong to **all** selected subfiles. |
| **Additional Subfiles** | `Alloy`, `Battery material`, `Cement`, `Ceramic`, `Corrosion`, `Coordination Polymer`, `Detected`, `Educational`, `Experimental`, `Forensic`, `Ionic Conductor`, `NBS`, `Pharmaceutical`, `Pigment`, `Polymer`, `Superconductor`, `Zeolite` | Uses the selected database's recorded subfile codes. These checkboxes also intersect; a checkbox does not establish that the active database contains that category. |

---

## Tab 3: `Properties` (Chemical Name, Color, Density)

Switch to the **Properties** tab to filter phases by name, physical color, or crystal density.

![Restraints dialog showing the Properties tab with Chemical name, Color, and Calculated/Measured Density fields](../images/restraints-properties.png)

### Parameter Reference: `Properties` Tab

| Parameter / Control | Default | Format / Range | Role and Effect |
| :--- | :--- | :--- | :--- |
| **Chemical name** | Empty | Space-separated words (e.g., `silicon oxide`) | Requires every word to match within the chemical name, or every word within the mineral name, using SQLite LIKE. |
| **Color** | Empty | Semicolon-separated color tokens (e.g., `gray ; colorless`) | Matches any selected recorded color with SQLite LIKE. Tokens have no implicit surrounding wildcard; `%` and `_` retain their LIKE meanings. |
| **Available colors...** | Button | Interactive picker dialog | Opens a list of standardized color descriptors present in the database; selected colors are inserted into the **Color** field separated by `;`. |
| **Density — Calculated** | Empty | Positive number ($\text{g/cm}^3$) | Target X-ray/crystallographic calculated density $D_x$. Matches cards within $[\text{Calculated} - \text{Tolerance}, \text{Calculated} + \text{Tolerance}]$. |
| **Density — Measured** | Empty | Positive number ($\text{g/cm}^3$) | Target experimentally measured density $D_m$. Matches cards within $[\text{Measured} - \text{Tolerance}, \text{Measured} + \text{Tolerance}]$. |
| **Density — Tolerance** | `0.1` | Nonnegative number ($\text{g/cm}^3$) | Half-width $\pm \Delta D$; zero requests an exact value. Invalid or negative text uses `0.1`. Calculated and measured bounds, when both active, intersect. |

---

## Tab 4: `Entries` (Direct Card ID Lookup)

Switch to the **Entries** tab when you know the specific database card number(s) (for example, COD seven-digit IDs such as `9000101` or ICDD PDF card numbers) that you want to inspect or test.

![Restraints dialog showing the Entries tab with space-separated entry IDs](../images/restraints-entries.png)

### Parameter Reference: `Entries` Tab

| Parameter | Default | Format | Role and Effect |
| :--- | :--- | :--- | :--- |
| **Space-separated entry IDs** | Empty | Whitespace-separated card IDs (e.g., `9000101 9000202 9000303`) | Restricts database queries strictly to the listed entry IDs. Particularly useful with **Load Cards** or **Load and Merge Cards** to pull specific reference cards directly into the **Results List** dock for overlay on the plot. |

---

## Tab 5: `Symmetry` (Crystal System, Space Group, Unit Cell)

Switch to the **Symmetry** tab to constrain candidates by crystallographic symmetry or lattice dimensions.

![Restraints dialog showing the Symmetry tab with Crystal system checkboxes, Space group filter, and Unit Cell Parameters](../images/restraints-symmetry.png)

### Parameter Reference: `Symmetry` Tab

| Group | Parameter / Control | Default | Range / Options | Role and Effect |
| :--- | :--- | :--- | :--- | :--- |
| **Crystal system** | `Cubic`, `Hexagonal/trigonal`, `Tetragonal`, `Orthorhombic`, `Monoclinic`, `Triclinic`, `Rhombohedral` | All unchecked (no filter) | Checkboxes | Selected systems form a union. `Hexagonal/trigonal` selects hexagonal and trigonal entries with hexagonal axes; the separate `Rhombohedral` checkbox selects trigonal entries with rhombohedral axes. There is no `Unknown` checkbox. |
| **Space group(s)** | **Space group text box** | Empty | Semicolon-separated Hermann-Mauguin symbols (e.g., `F d -3 m ; R -3 c`) | Matches stored trimmed symbols exactly. Multiple symbols and any checked crystal systems form one union. |
| **Space group(s)** | **Symbol List...** | Button | Interactive picker dialog | Opens a searchable list of standard crystallographic space-group symbols and inserts chosen symbols into the text box separated by `;`. |
| **Unit Cell Parameters** | **a (Å)**, **b (Å)**, **c (Å)** | Empty | Positive numeric text | Restricts each active length to $[L-\Delta L,L+\Delta L]$. The text fields do not impose a `100` Å upper limit. Blank, nonnumeric and nonpositive values are ignored. |
| **Unit Cell Parameters** | **Alpha (°)**, **Beta (°)**, **Gamma (°)** | Empty | Positive numeric text | Restricts each active angle to $[\theta-\Delta\theta,\theta+\Delta\theta]$. These text fields do not enforce a `180`° limit. Blank, nonnumeric and nonpositive values are ignored. |
| **Unit Cell Parameters** | **Length tolerance (Å)** | `1.5` | Nonnegative spinbox value | Allowed $\pm$ tolerance for active lengths; zero requests an exact value. |
| **Unit Cell Parameters** | **Angle tolerance (°)** | `2.0` | Nonnegative spinbox value | Allowed $\pm$ tolerance for active angles; zero requests an exact value. |

Use physically meaningful numeric values. The current query builder omits a
density or unit-cell interval if either endpoint is at most `-1`; an excessively
large tolerance can therefore leave that restriction unapplied.

---

## Dialog Action Buttons (Bottom Bar of `Restraints`)

The bottom toolbar of the **Restraints** dialog provides five workflow actions:

| Button | When Enabled | Role and Behavior |
| :--- | :--- | :--- |
| **Load Cards** | Classical mode, idle | Requires an active restraint to perform a query. With experimental peaks present, computes classical FOM using the continuous spectrum and keeps every defined score, including zero, without the normal minimum-FOM/result cap. With no peaks, loads unscored cards. Replaces the Results List. |
| **Load and Merge Cards** | Classical mode, idle, with existing result cards | Uses the same scored/unscored browse rules and appends IDs not already present. Existing IDs retain their current card. An active restraint is required; this route is disabled in PFSM. |
| **Search with Restraints** | At least three continuous-spectrum samples, no active edit/task, and an active restraint | Starts the selected CSM or PFSM engine with these restraints. Missing prerequisites, including CSM peaks, are prepared automatically. The dialog can remain open while its controls are disabled during the task. |
| **Cancel All Restraints** | Always | Clears every field and checkbox across all five tabs (`Composition`, `Subfiles`, `Properties`, `Entries`, and `Symmetry`). Use this dialog action; there is no separate S&M menu command with this label. |
| **Close** | Always | Closes the **Restraints** dialog while keeping the configured restraints active in the session for subsequent **Run CSM** or **Run PFSM** commands. |

Unscored reference cards can be inspected but cannot be accepted as phases.
Import a compatible continuous pattern and run a scored search before accepting
them. Neither browse action applies `Check strongest peaks`; active restraints
and deleted-card checks still apply. In PFSM, use **Search with Restraints** to
start a new candidate universe, confirming the end of an existing analysis.
