# Getting Started

This chapter describes the current menu routes, workspace, pattern import,
radiation selection and project/export commands.

## Workspace Overview

![QualPF workspace with diffraction plot, results, quantification, comparison and workflow panels](../images/workspace-overview.png)

| Region | Purpose |
| :--- | :--- |
| **Central plot** | Working and comparison patterns, background, peaks, reference sticks and PFSM joint trial/difference curves. |
| **Results List** | Candidate cards, scores, text filtering, display sorting and pagination. Selecting a card previews it; acceptance is a separate action. |
| **Quantitative analysis (PFSM/RIR when applicable)** | Accepted phases and available RIR percentages. PFSM also displays joint scales, shifts and effective RIR. |
| **Peaks, Compare, Card and Report** | Experimental peak table, reflection comparison, reference-card details and printable analysis. |
| **Workflow — Current Project** | Executed steps, actual parameters and rollback available during the current session. |

Panels can be resized, tabbed or floated. **Window** contains their visibility
actions and **Default Layout**. The workflow dock also has a **View** toggle.

## Complete Menu Bar Reference

Menu labels below follow the application. Only explicitly assigned shortcuts are listed.

### `File` Menu

| Command | Shortcut | Description |
| :--- | :--- | :--- |
| **New** | `Ctrl+N` | Clears the workspace after the unsaved-change check. |
| **Import Diffraction Pattern** | — | Imports supported pattern files, including ASCII/XY/DAT and powder CIF inputs. A source without wavelength information prompts for radiation. |
| **Open Project** | — | Restores a native `.qpf` project, including frozen analysis and display state. |
| **Save Project / Save Project As** | — | Writes the active project or a new destination atomically. |
| **Export Diffraction Pattern** | — | Exports active observations in the chosen supported format. |
| **Export Image** | — | Opens image settings and saves a plot image in the selected supported format. |
| **Print...** | `Ctrl+P` | Opens a content selector for Report, Result List, Peak List, Pattern, Card, Quantitative or Compare before the printer dialog. |
| **Load Recent Files** | — | Reopens recorded patterns or projects. |
| **Quit** | — | Handles unsaved work and drains any cancelled task before closing. |

### `View` and `Window` Menus

| Command | Description |
| :--- | :--- |
| **Reset Zoom** (`Ctrl+0`) / **Autoscale** | Restores the plot domain or scales the displayed intensity range. |
| **Legend** / **Subtract Background** | Toggles the legend or the background-subtracted display. |
| **Selection Mode / Horizontal Zoom / Rectangle Zoom / Pan** | Selects how mouse gestures interact with the plot. Choose a zoom mode before dragging a region. |
| **Plot Style** | Edits graph lines, markers, axes and background appearance. |
| **Window > panel name** | Shows or hides a dock. |

### `Preprocessing` Menu

| Command | Description |
| :--- | :--- |
| **Range** | Explicitly crops the working angular interval. |
| **Generate Background / Background Options** | Runs the project configuration or opens its private preview editor. |
| **Add Background Point / Delete Background Point / Export Background** | Edits supported background points or exports the curve. |
| **Initial Peak Search / Initial Peak Search Options** | Runs or configures SG third-derivative detection for Gaussian width preparation. |
| **FWHM Estimation / FWHM Estimation Options...** | Measures a representative Gaussian/geometric width or uses an explicit fixed width. |
| **Smoothing** | Previews Savitzky-Golay or adjacent averaging on the working observations. |
| **K-α2 Stripping** | Runs the fixed operation when source radiation and processing history permit it. |
| **Restart preprocessing...** | Restores stored original intensities within the current cropped domain after confirmation; reimport to recover discarded samples. |

### `Peak Search` Menu

| Command | Description |
| :--- | :--- |
| **Peak Search / Peak Search Options** | Extracts formal peaks or opens the selection/sensitivity editor. |
| **Add Peak / Add inferred peak / Delete Peak / Delete all peaks** | Edits experimental peaks. Measured and inferred tools have distinct inputs. |
| **Load Peaks / Save Peaks** | Reads or writes `2 theta` or `d` peak values; saving also offers a detailed peak list. DICVOL and McMaille exports are unavailable in this build. |

### `S&M` and `Entry` Menus

| Command | Description |
| :--- | :--- |
| **S&M > CSM (Classical SM) / CSM Options** | Runs or configures Classical search against the single selected database. |
| **S&M > PFSM / PFSM Options** | Runs or configures continuous-profile search, preparing missing prerequisites. |
| **S&M > Restraints** | Opens Composition, Subfiles, Properties, Entries and Symmetry filters, card browsing and restrained search. |
| **S&M > Test Database / Recalculate FOMs** | Invokes the existing database test or candidate rescoring route. |
| **S&M > Manage Databases** | Registers, selects, renames, creates or deletes database entries. |
| **Entry > Accept Selected Entries** (`Ctrl+Return`) | Accepts selected cards. PFSM publishes their joint fit and refreshed ranking together. |
| **Entry > Delete Selected Entries / Change Color** | Deletes selected candidate rows. Change Color edits the first selected card only. |
| **Entry > Load/Add Entry by ID...** (`Ctrl+E`) | Loads a known reference identifier. |

### `Setting` and `Help` Menus

**Setting > Preprocessing / CSM / PFSM / Peak Search** edits defaults for future
projects. Current-project dialogs do not overwrite these defaults.

**Help > User Manual (HTML)** opens the current online guide;
**Documentation (PDF)** opens a matching installed PDF when present.
**Check for Updates** and **About** provide update status and product/legal details.

## Radiation and Wavelength Configuration

When an imported pattern lacks wavelength metadata, the **Set wavelength radiation**
dialog appears during import. There is no separate Preprocessing wavelength command.
Check these values before confirming import; to correct a missing-metadata source,
reimport it with the correct radiation.

![Radiation selection during pattern import](../images/wavelength-dialog.png)

| Parameter | Default / options | Meaning |
| :--- | :--- | :--- |
| **Radiation** | `X-ray`, `Synchrotron`, `Neutron` | Import classification. Electron scattering exists in crystallographic calculations but is not an option in this dialog. |
| **Wavelength** | Cu-Ka1 `1.54059307` Å; Cr `2.28973`, Fe `1.93604`, Co `1.78900`, Mo `0.70932`; `User defined` | Primary wavelength for angle/spacing conversion. Custom wavelength editors admit values up to `5` Å; product import requires a positive wavelength. The dialog remembers the saved wavelength choice. |
| **Use second wavelength** | Off | Enables the second radiation component. |
| **Wavelength 2** | Corresponding anode value, or a supplied value | Secondary wavelength; confirm it against the measurement. |
| **Ratio** | `0.5` when enabled with an empty ratio | Relative secondary intensity. The editor accepts `0`–`1`; downstream operations also validate radiation compatibility. |

Imported file metadata and QPF projects retain their own radiation. A positive
signal alone does not establish raw counting statistics.

For ordinary XY/ASCII input, the first two numeric columns are $2\theta$ in
degrees and observed intensity. Additional columns are not selected as a
different signal or interpreted as uncertainty. Use the source format's
wavelength metadata, or verify the import radiation dialog when it is absent.

## Interactive Central Plot Controls

Choose **Horizontal Zoom**, **Rectangle Zoom** or **Pan** before using the matching
mouse gesture. **Reset Zoom** (`Ctrl+0`) restores the plot domain. The cursor
readout reports coordinates in the current plot representation.

Selecting a candidate displays reference sticks. PFSM additionally previews the
stored joint trial of that candidate with the accepted phases; selection itself
does not accept a phase.

### Customizing Visual Appearance (`View > Plot Style`)

![Plot Style editor](../images/plot-style-dialog.png)

Select a graph item to edit its line color/style/width and scatter shape/size.
The dialog also provides plot-background and axis controls. Plot presentation is
saved separately from scientific analysis, so a visual edit does not recompute a fit.

## Saving Projects and Exporting Data

### Saving and Reopening Projects (`.qpf`)

**File > Save Project / Save Project As** saves original/current/comparison data,
preprocessing evidence, peaks, settings and workflow records, complete candidates
and accepted phases, PFSM frozen evidence, RIR revisions and project display state.
Reference databases remain separate.

**File > Open Project** accepts QPF 1.0 only. QXP v1/v2/v3 are rejected even if the
filename is changed. Stored evidence can be inspected/exported without its
database; fetching new references still checks database identity.
Workflow records survive saving, but session undo snapshots do not.

### Exporting Patterns, Images, CSVs, and PDF Reports

1. **File > Export Diffraction Pattern** saves the active pattern in the chosen
   supported format. It exports working observations, not a PFSM joint trial or
   the Subtract Background display. The powder-CIF exporter records the primary
   wavelength only; it is not a complete radiation/project round trip.
2. **File > Export Image** opens image-size/resolution settings and an output-format
   selector.
3. **Peak Search > Save Peaks** exports experimental peaks. **Export CSV** in the
   quantification panel writes accepted phases, RIR provenance and available percentages.
4. **File > Print...** opens its content selector. Choose **Report**, then use the
   available printer/PDF destination. The active dock does not choose print content.

**Load Peaks** imports coordinates, then measures their heights, areas and widths
from the current continuous pattern and aligned background. It replaces the
table, ignores coordinates outside or exactly on the calculation boundaries,
and can fail when a measurement is undefined. It does not restore a saved set of
measured intensities and widths. The simple **Save Peaks** formats contain
positions only; the detailed listing additionally reports width and intensity
normalized to `100`, rather than a complete editable project state. Use QPF to
preserve measured peak values and provenance.
