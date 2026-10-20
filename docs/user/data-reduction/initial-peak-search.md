# Initial Peak Search

**Preprocessing → Initial Peak Search** detects candidate sample positions on the
full current working pattern. It uses the Classical Savitzky–Golay third
derivative and the same unsubtracted working signal and independent background as
Gaussian FWHM estimation; subtraction is prepared inside the numerical adapter.
A missing background uses the Preprocessing profile and is published with the
initial candidates after successful calculation. Failed or cancelled preparation
does not replace existing results.

The command calculates fresh candidates every time. It opens the **Initial Peak
Candidates** window, a read-only table of
sample number (starting at 1), 2θ and observed intensity. The summary shows the
candidate count, actual half-window and effective SG degree. Candidates appear
only in this table and add no markers or legend entries to the plot. These are
derivative trigger positions before peak-top refinement. The formal Peaks
table, widths, areas and matching results remain available independently.

## Options

Open **Preprocessing → Initial Peak Search Options** to open the original independent dialog,
with **Settings** and **Initial Peak Candidates** tabs:

| Setting | Default | Meaning |
| --- | --- | --- |
| Derivative threshold | 0.1 | Normalized derivative trigger threshold from 0 to 200; lower values admit more candidates |
| Automatic window | Enabled | Resolve the window using the Classical wavelength and sampling-step rule |
| Manual half-window | 11 samples | Available with automatic window disabled; range 2–100, full window `2w + 1` samples |
| SG polynomial degree | 9 | Requested degree from 3 to 20; effective degree is capped at `2w` |

**OK** calculates candidates and saves the requested parameters to the current
project on success, then selects the read-only candidate tab. Failed or cancelled
calculations preserve the prior project configuration and results. **Restore
Defaults** changes the draft; Cancel discards it. **Setting → Preprocessing**
changes saved defaults for new projects in the separate workflow settings window.

Each workflow has independent initial-search parameters. Missing dependencies in
standalone Gaussian FWHM estimation use Preprocessing; automatic CSM and Peak
Search use their respective profiles. Compatible manual candidates may be reused.
Fixed and geometric FWHM paths bypass initial detection. PFSM automatic width is
geometric and does not insert an initial or formal peak search.

## Result lifetime and projects

Candidate detection is available with a loaded continuous pattern and wavelength
while no operation or editor is active. Changes to the signal, background, range,
wavelength or initial detection settings invalidate old candidates. Unrelated
session changes retain them. Changing initial settings makes an existing Gaussian
width and dependent extraction/search results invalid after confirmation.

Settings are local to the project and saved in QPF 1.0 together with candidates,
input snapshots and workflow records. Reopening validates this evidence before
reuse. Older v1/v2 files keep their historical settings and unverified results;
missing evidence must be recalculated. Undo history begins anew after reopening.
