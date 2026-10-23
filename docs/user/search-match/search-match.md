# Matching and quantification

Inspect method settings before starting an analysis. **CSM Options** and
**PFSM Options** edit the current project; the **Setting** window defines defaults
for future projects. Changing one method's profile does not configure the other.

## Classical matching

CSM prepares missing background and formal peaks, or uses compatible manually
prepared peak evidence. It prescreens reference cards, evaluates reflection
matches and ranks the retained cards by figure of merit (FOM). Position agreement,
intensity agreement and the coverage of experimental and database peaks all
contribute. FOM is a ranking statistic, not an identification probability.

In **S&M > CSM Options**, review the minimum FOM, position/intensity/phase
weights, angular tolerance, strongest-peak screening, deleted-card choice and
result limit. Automatic tolerance depends on peak-width preparation. These
controls affect classical matching rather than PFSM's candidate universe.

Select entries and use **Entry > Accept Selected Entries** to retain phases.
When residual searching is enabled, accepting a phase updates the residual peak
evidence and rescores existing candidates. It does not query the database again.
Start another search when a new database query is intended. Multiple selected
entries are processed in their displayed order.

## PFSM preparation and options

PFSM fits reference profiles to continuous observations. It needs the original
source and processing evidence; reimport the measurement if the required evidence
is missing. It uses a fixed intermediate-Lorentzian profile shape and joint
fitting of selected phases.

Width selection first honors an explicitly configured valid fixed width, then a
compatible manually estimated width. Otherwise PFSM measures geometric FWHM
automatically. The initial value shown in an editor is not a measurement, and
automatic estimation failure stops preparation. Geometric minimum sampling
density defaults to 6. The classical width conversion is not applied to PFSM.

**S&M > PFSM Options** has General and Candidate Recall tabs. Shift bounds
default to -0.500 and +0.500 degrees two-theta; equal bounds fix the shift.
Automatic weighting uses supplied statistical evidence. Unknown noise is reported
as equal weighting rather than interpreted as measured counting statistics.

The minimum relative FOM is a display filter in the range 0 to 1. Worker selection
applies to the next run. Template caching is enabled by default: fitting iterations
may use interpolation, with final fits verified by direct evaluation. Turning it
off avoids cache preparation and uses direct evaluation throughout.

### Candidate recall

Recall is enabled by default and selects candidates for formal fitting. Disabling
it fits the full pool. The normal recall controls have these defaults:

| Control | Default |
| --- | --- |
| Recall candidates | 500 |
| Local shortlist | 20 |
| Protected candidates per channel | 5 |
| Review set size | 80 |
| Formal candidate set size | 80 |

Counts must be positive integers. Protection cannot exceed the local shortlist,
which cannot exceed the recall target. Both fallback thresholds must be at least
the shortlist size. Ties and numerical protection can enlarge a set: these
thresholds trigger full-pool fallback when exceeded, rather than clipping entries.

**On resource limit** defaults to bounded integral recall when the normal grid or
shift scan is too large. Its fixed channel budgets differ from the five controls
above. Choose **Full-pool formal fitting** to use that alternative resource-limit
route. The choice takes effect in the next new analysis.

Scientific preparation and recall settings freeze when an analysis begins.
Accepting, removing or rescoring phases uses the same frozen candidate universe
and configuration. Changes to scientific settings require confirmation before
dependent results are invalidated; display filtering does not refit the analysis.

## Read PFSM results

Each formally fitted candidate is tried with the selected phases against a common
baseline. Its improvement is the reduction in net Rwp from that baseline. The
displayed FOM divides this improvement by the largest improvement among the
formally fitted candidates in that round. If all improvements are zero, FOM is
zero and the interface reports **Current candidates show no improvement**.

FOM therefore cannot be compared directly across rounds. Hiding rows, changing
page, filtering or sorting does not change its denominator. Candidate Rwp, Shift
and Scale describe the joint trial containing that candidate, not an independently
confirmed phase composition.

Accepting or removing a phase refits the selected set and reevaluates remaining
candidates under the frozen recall configuration. A removed phase becomes a
candidate again without another database query. The plot shows observations,
background, joint calculation and residual. **View > Subtract Background** changes
the view of observations and calculation without changing the fit or residual.

## Quantification

The **Quantitative analysis · PFSM/RIR** panel uses the selected-phase joint
scales, each full reference card's intensity maximum and its effective RIR.
For phase i, its unnormalized contribution is scale times reference maximum
divided by RIR. Percentages normalize these contributions over the selected set.
Candidate-preview scales and maxima of a clipped plotted curve are not used.

Every selected phase needs a finite positive RIR and a supported Cu K-alpha
reference convention. Missing or invalid RIR, unsupported radiation, zero or
dependent templates, or unverified fitting can make the whole composition
unavailable. Read the reasons in the panel or report; unavailable percentages
are not zero. A pie chart appears only for a fully quantifiable selected set.

Select a phase and choose **Edit RIR...** to record a positive replacement RIR
and its source or calibration. **Restore reference RIR** removes that revision.
The database value remains unchanged. Revisions survive removal and reselection
of the same card within the analysis and are saved in QPF. Changing RIR does not
refit the profiles or reorder the candidate ranking.

Percentages cover only selected crystalline phases. They exclude unidentified
and amorphous material. Library RIR provenance is unverified; preferred
orientation, microabsorption, reference choice and profile mismatch can bias the
estimate. Treat the result as semi-quantitative and retain those qualifications
when reporting it. Display rounding may leave a total slightly different from
100 percent; stored values are not adjusted to force the rounded sum.

## Export and continuation

Reports and printing use the same selected-phase result as the panel.
**Export CSV...** writes phase identities, full-precision values, RIR sources,
diagnostics and availability reasons. Unavailable percentages are empty fields.
The `joint_rwp_net` CSV value is a dimensionless fraction. If atomic writing
fails, the previous destination is retained.

QPF retains the frozen analysis and RIR revisions for continuation. Opening a
saved project does not search or fit again. Frozen results can be inspected
offline; retrieving new references after a database identity change requires
restarting the analysis.
