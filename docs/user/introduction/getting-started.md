# Getting Started

## Step 1 — Install a reference database

Before running the first search, at least one crystallographic reference
database must be installed. QualPF automatically detects databases placed in the
**QualXDB** folder inside your home directory.

For download links, installation instructions and advanced configuration see
the [Database Installation](../database-installation.md) page.

## Step 2 — Import a diffraction pattern

1. **File → Import Diffraction Pattern** (or drag-and-drop a file onto the graphic area)
2. Supported formats: `.xy`, `.dat`, `.xrdml`, `.gda`, `.xye`, `.rtv`
3. The pattern appears in the graphic area

## Step 3 — Background

1. **Preprocessing → Generate Background**
2. Adjust parameters with **Preprocessing → Background Options** if needed
3. Optionally view the net signal with **View → Subtract Background**

## Step 4 — Peak Search

1. **Peak Search → Peak Search**
2. The detected peaks appear as vertical markers in the graphic area
3. Fine-tune with **Peak Search → Peak Search Options**

## Step 5 — Search & Match

1. **S&M → CSM (Classical SM)**
2. QualPF queries the database and shows ranked results in the **Results List**
3. Select a card to view its reflections overlaid on the experimental pattern

If Step 3 and/or Step 4 are skipped and **CSM (Classical SM)** is run directly,
QualPF automatically performs the missing steps (Background and/or Peak Search)
before running the search.

## Step 6 — Accept a phase

1. Select a card in the Results List
2. Click **Accept** (check-mark button) — the phase moves to the **Quantitative** panel
3. If residual search is enabled in the **CSM Options** dialog,
   QualPF automatically performs a residual search on remaining peaks — see the
   [Search & Match](../search-match/search-match.md) page for details

## Step 7 — Save the project

**File → Save Project As** saves a `.qpf` file containing the pattern data,
peaks, results and accepted phases.

The native `.qpf` format also saves workflow evidence, comparison patterns and
project graph settings. QXP v1/v2/v3 projects cannot be opened. Opening an existing
QPF project uses its own settings and does not require the original reference
database for inspecting the saved analysis.
