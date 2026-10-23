# Initial peak candidates

Initial candidates identify sample positions for subsequent Gaussian width
estimation. They are not the formal measured peak list used by CSM and do not
provide fitted widths or integrated areas.

## Calculate candidates

Choose **Preprocessing > Initial Peak Search** with a continuous pattern loaded.
The command performs a fresh derivative-based detection over the current working
pattern. If background preparation is needed, it uses the Preprocessing profile;
the background and candidates are committed together only after success.

The **Initial Peak Candidates** window lists the sample number, angle and observed
intensity. Sample numbers in this table start at one. The summary reports the
actual half-window and SG polynomial degree. These positions precede peak-top
refinement, so they need not coincide with later measured peak maxima. The table
adds no candidate markers to the main plot.

## Tune detection

Open **Preprocessing > Initial Peak Search Options** for its Settings and
Initial Peak Candidates tabs.

| Control | Factory value and interpretation |
| --- | --- |
| Derivative threshold | 0.1; permitted range 0 to 200; reducing it admits more triggers |
| Automatic window | Enabled; derives the window from wavelength and sampling |
| Manual half-window | 11 samples; adjustable from 2 to 100 when automatic mode is off |
| SG polynomial degree | Requested value 9, adjustable from 3 to 20; effective degree cannot exceed twice the half-window |

The full window has `2w + 1` samples for half-window `w`. **OK** calculates and,
on success, stores the requested parameters and displays the candidate tab.
**Restore Defaults** changes the draft; Cancel abandons it. Invalid or failed
calculations do not replace committed parameters or results.

## Reuse and lifetime

Signal, background, wavelength, range and detection-setting changes can invalidate
these candidates and dependent Gaussian widths. The program checks compatibility
before reuse. Fixed and geometric width routes do not require this SG stage;
automatic PFSM preparation follows its geometric route.

QPF preserves candidates, their input evidence and workflow records. Session undo
is separate and starts empty when the file is reopened.
