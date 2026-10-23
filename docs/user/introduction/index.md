# Workspace and analysis choices

The central plot shows the working diffraction signal and any visible analysis
layers. The surrounding docks expose different parts of the same project:

| Dock | Purpose |
| --- | --- |
| Peaks | Examine the measured peak list used by classical matching |
| Results List | Inspect and sort candidate reference cards |
| Card | Read the selected reference card and its reflections |
| Quantitative | Review accepted phases and available composition estimates |
| Compare | Work with comparison patterns |
| Report | Review the report for the current analysis |
| Workflow | Inspect calculation sources, parameters and session history |

Use the **View** menu to expose the panels you need. Zooming, panning and changing
plot styles help inspection; they do not rerun a scientific calculation.

## Two matching methods

![Measurement, matching, review and project saving](../images/analysis-route.png)

**CSM (Classical SM)** compares a measured peak list with reference reflections.
It can prepare missing background and peaks using the project's CSM settings.
It is useful when you want to inspect the peak evidence behind a match.

**PFSM** compares continuous observations with expanded reference profiles and
jointly fits selected phases. It requires the original measurement evidence and
does not need a formal peak list. Its settings, width preparation and scoring
are independent of CSM.

Both methods require interpretation. Use measured sample information to justify
[restraints](../search-match/restraints.md), then review alternative phases and
unexplained reflections before accepting a result.

## Keep a complete project

Use **File > Save Project As** to create a `.qpf` file. QPF retains analysis
parameters, source evidence, reference cards, selected phases and project plot
settings. Opening it restores the saved analysis without running a new search.
The database itself and session undo stack are not embedded. QXP projects are
not supported, including files renamed with a `.qpf` extension.

For a practical sequence, continue to [Getting started](getting-started.md).
