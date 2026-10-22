# Search & Match

## Classical matching

QualPF prepares any missing background and experimental peaks before searching.
A compatible manually prepared or imported peak table skips peak extraction.
Missing dependencies use the CSM profile, independently of other workflows.
Classical matching still uses the real diffraction pattern. The search then follows these steps:

1. **Strongest peaks filter**: for each database card, only the _N_ strongest
   reference peaks are compared against the experimental peak list (fast pre-filter).
2. **FOM calculation**: a weighted Figure of Merit combines peak-position
   matching and intensity matching.
3. **Results**: only cards above **Min. FOM** are retained. The table displays
   them by FOM by default; click headers to change the displayed order. This does
   not change saved candidate order or the order of later equal-score results.

## Classical Figure of Merit

At the end of the search–match step a list of plausible database crystalline
phases are ranked according to the decreasing values of a FOM. The FOM
includes four contributions, three of them suitably weighted, according to
the following formula:

$$
\mathrm{FOM}=\sqrt{\frac{\mathrm{FOM}_{db}\cdot\left(w_{\theta}\,\mathrm{FOM}_{\theta}
+w_{I}\,\mathrm{FOM}_{I}+w_{ph}\,\mathrm{FOM}_{ph}\right)}{w_{\theta}+w_{I}+w_{ph}}}
$$

where FOM<sub>θ</sub> takes into account the average difference between the
2θ values of the observed and matched database peaks; FOM<sub>I</sub> is
related to the average difference between the intensity values of the
observed and matched database peaks; FOM<sub>ph</sub> depends on the
percentage of the matched experimental peaks and on their intensity; and
FOM<sub>db</sub> depends on the percentage of the matched database peaks and
on their intensity.

FOM<sub>θ</sub> is the contribution coming from the 2θ differences between
the experimental and the associated database peaks:

$$
\mathrm{FOM}_{\theta} = 1 - \dfrac{\displaystyle\sum_{i}^{N_{db}^{ass}}
\left|2\theta_i^{\mathrm{exp}} - 2\theta_i^{db}\right|}{N_{db}^{ass}\cdot\Delta}
$$

where the summation is over the associated database peaks (a database peak
is considered associated if its 2θ distance from the experimental peak is
less than Δ), 2θ<sup>exp</sup> and 2θ<sup>db</sup> are the positions of the
experimental and database peaks, respectively.

FOM<sub>I</sub> is the contribution due to the differences between the
intensities of the experimental and the associated database peaks:

$$
\mathrm{FOM}_{I} = 1 - \dfrac{\displaystyle\sum_{i}^{N_{\mathrm{exp}}^{ass}}
\left|I_i^{\mathrm{exp}} - I_i^{db}\right|}{N_{\mathrm{exp}}^{ass}}
$$

where I<sup>exp</sup> and I<sup>db</sup> are the experimental and database
intensity respectively, the summation is over the associated experimental
peaks.

FOM<sub>ph</sub> is the contribution due to the intensities of the
associated experimental peaks and their percentage:

$$
\mathrm{FOM}_{ph} = \sqrt{\dfrac{\displaystyle\sum_{i=1}^{N_{\mathrm{exp}}^{ass}}
I_i^{\mathrm{exp}}}{\displaystyle\sum_{i=1}^{N_{\mathrm{exp}}} I_i^{\mathrm{exp}}}
\cdot\dfrac{N_{\mathrm{exp}}^{ass}}{N_{\mathrm{exp}}}}
$$

where N<sub>exp</sub> is the total number of experimental peaks.

FOM<sub>db</sub> is the contribution due to the intensities of the
associated database peaks and their percentage:

$$
\mathrm{FOM}_{db} = \sqrt{\dfrac{\displaystyle\sum_{i=1}^{N_{db}^{ass}}
I_i^{db}}{\displaystyle\sum_{i=1}^{N_{db}} I_i^{db}}
\cdot\dfrac{N_{db}^{ass}}{N_{db}}}
$$

where I<sub>db</sub> is the database intensity and the summation at the
numerator is over the associated database peaks; N<sub>db</sub> is the total
number of the database peaks.

The weighting factors w<sub>θ</sub>, w<sub>I</sub>, w<sub>ph</sub> and Δ
(default heuristic values are set for them) can be adjusted directly by the
user via the graphical interface. w<sub>ph</sub> is related to the number of
expected phases.

The default value of the weights w<sub>θ</sub>, w<sub>I</sub>, and
w<sub>ph</sub> is 0.5, but it can be changed by the dialogue window in
Figure 1 (via the "2θ", "Intensity" and "Phases" trackbars).

## CSM Options

Open **S&M → CSM Options** to use the original weight sliders and search controls.
OK saves these options to the current project; run CSM from its menu command.
Setting → CSM edits defaults for new projects in a separate window.
The separate Setting dialog's module tabs include independent background, initial
peaks and FWHM defaults.

| Option | Description |
|--------|-------------|
| **Min. FOM** | Discard cards below this threshold |
| **2θ / d weight** | Contribution of peak-position agreement to FOM |
| **Intensity weight** | Contribution of intensity agreement to FOM |
| **Phases weight** | Bias towards single-phase vs. multi-phase solutions |
| **Δ2θ** | Peak matching tolerance in degrees (Auto = calculated from peak widths) |
| **Max. entries** | Maximum number of results to display |
| **Check strongest peaks** | Use only the N strongest peaks for the pre-filter |
| **Check deleted cards** | Include cards marked as deleted in the database |
| **Residual searching** | After accepting a phase, automatically re-search on residual peaks |

## Accepting a CSM Phase

1. Select a card in the Results List.
2. Click **Accept** (check-mark icon). The card is moved to the **Quantitative** dock.
3. If **Residual searching** is enabled, QualPF subtracts the accepted phase
   contribution and rescores the existing candidates on the remaining peaks.
   It does not query the database again. A new Search & Match command performs
   a new database search.

## PFSM Options and width selection

**S&M → PFSM Options** opens the original General and Candidate Recall tabs. OK validates and
stores the current project's settings. Run PFSM from its menu command. Cancel
discards drafts. Restore Defaults restores the original dialog defaults. Setting →
PFSM edits defaults for new projects. Errors appear beside the relevant fields.
CSM's score, peak prefilter and deleted-card options do not filter the PFSM pool;
the explicit restraints define its candidate universe.

| Module option | Default and effect |
| --- | --- |
| Shift bounds | −0.500 to +0.500 °2θ; finite ordered bounds, equal values fix the shift |
| FWHM | Read-only current source; PFSM owns its defaults and compatible manual width can be reused |
| Signal | Current working observations, with preserved units and processing evidence |
| Weighting | Auto from explicitly supplied statistics; unknown noise uses labelled equal weights; equal weights can also be chosen |
| Min. relative FOM | 0, in [0,1]; immediate display filter only |
| General → Advanced → Workers | Auto (internal 0); applies to the next run |
| General → Advanced → Template cache | On; interpolated fitting iterations always have direct final verification |

Template caching can speed up searches by reducing repeated profile calculations
during fitting iterations. Preparing and storing the cache costs time and memory,
so small searches may not benefit. Disabling it evaluates profiles directly
throughout fitting, avoiding interpolation and its cache overhead, but repeated
calculations can be slower. Both modes verify final fits by direct evaluation.
Saved off choices and existing project settings are retained. Restore Defaults
enables caching; Setting → PFSM → Profile Expansion controls defaults for new
projects.

| Candidate Recall option | Default and effect |
| --- | --- |
| Enable candidate recall | On for first use or missing settings; an explicit off choice is remembered |
| On resource limit | Bounded integral recall; choose Full-pool formal fitting to fit every candidate when the v2 grid or shift scan is too large |
| Recall candidates | 500; total target from alternating original/residual channels |
| Local shortlist | 20; Top-K for both initial selection and local review |
| Protected candidates per channel | 5; protect each channel's top candidates within the recalled set |
| Review set size | 80; fall back to the full pool if the review set exceeds this threshold |
| Formal candidate set size | 80; fall back to the full pool if the proposed formal set exceeds this threshold |

The five counts apply to normal v2 recall. Bounded integral recall has fixed
channel budgets and keeps ties and numerical protection without a final cap.
The resource-limit choice applies to the next new analysis, including saved
defaults for new projects. Missing settings use bounded integral recall.

Counts are integers from 1 to 2147483647. Channel protection must not exceed the
local shortlist, which must not exceed the recall target. Both fallback thresholds
must be at least the local shortlist. Ties and numerical protection can add
candidates; a threshold is not a truncation cap. Equality does not trigger fallback.
Disabling recall fits the full pool and disables recall controls while retaining
their values. Internal solver accuracy, starts, step counts and risk coefficients
remain fixed. Shadow mode is available only in the benchmark CLI, whose default
remains off.

Execution reads only this project's profile. Defaults are copied at project
creation; opening/cancelling Options never writes
preferences. Invalid persisted settings are rejected before publication.

A new analysis first uses an explicitly configured valid fixed width, then a
manually estimated Gaussian or geometric width compatible with the current input
and processing history. Otherwise it estimates geometric FWHM automatically,
using configured sampling density Q (default 6). A historical estimate or initial
display value 0.1 is not a measurement; PFSM does not apply CSM's 0.9 conversion.
An automatic estimation failure reports its reason and stops the search.

Width, weights, shift bounds, cache and all candidate-recall settings freeze when
the analysis starts. Workflow Details records the actual parameters. Saving changed
scientific parameters confirms and invalidates dependent results; the relative
display threshold applies immediately and
workers apply to the next run. Solver tolerances, budgets and IL shape are fixed.
If original-source evidence is missing, reimport the measurement before PFSM.

## PFSM candidates and selected phases

PFSM fits selected phases jointly, then formally tries the shortlisted candidates,
including ties and protected entries, against that common baseline. It fits the
full pool when recall is disabled or falls back. With the final baseline fixed, define

$$
d_j = R_{\mathrm{wp,baseline}}-R_{\mathrm{wp,trial},j},\qquad
\mathrm{FOM}_j =
\begin{cases}
d_j/\max_{k\in C_g} d_k,&\max_{k\in C_g} d_k>0,\\
0,&\max_{k\in C_g} d_k=0.
\end{cases}
$$

Here $C_g$ is the formally fitted set for this round: the selected subset when
screening succeeds, or the complete unselected universe for a full-pool run. Filtering,
pagination, hidden/deleted display rows and header sorting do not change the
denominator. If every improvement is zero, the status reports **Current candidates
show no improvement**. FOM is not an identification probability and cannot be
compared directly between rounds. CLI raw-improvement output stays unchanged.

Accepting or removing a phase refits the selected phases jointly and reevaluates
the full frozen universe through the same frozen recall configuration. Rescoring
also retains that configuration. A removed card participates again, without a new
database query. The **Quantitative analysis · PFSM/RIR** panel shows wt.%, joint
Scale, Shift, effective RIR and baseline Rwp. QPF 1.0 preserves the full PFSM analysis for continued work after reopening.
A mismatched reference database requires restarting the analysis before new retrieval.

## PFSM/RIR semi-quantitative analysis

For selected-phase joint coefficients $s_i$, complete frozen reference-card
intensity maxima $M_i$ and effective RIR values, the mass percentages are

$$
a_i=s_i M_i/RIR_i,\qquad w_i=100a_i/\sum_j a_j.
$$

The common fixed IL width factor cancels. Candidate-preview coefficients and
clipped/sample-grid maxima do not determine percentages. Each selected phase
needs a finite positive RIR and a supported Cu Kα reference convention. Missing
or invalid data, unsupported radiation, zero templates, dependent templates or
unverified joint fitting make the entire composition unavailable; the panel and
report show reasons and no pie chart. An otherwise valid zero-scale phase is 0%.

Select a phase and choose **Edit RIR…**. Enter a finite positive RIR, the source
or calibration description and its Cu Kα applicability. The original library value
remains visible; **Restore reference RIR** removes the revision. Busy, editing,
Closing and stale-snapshot checks prevent partial changes. Cancel and owner-window
close discard the private draft. Revisions bind the frozen library/card ID, survive
removal/reselection, end with the analysis and never refit, reorder candidates or
write to the database.

Reports and printing share the panel's Application result. **Export CSV…** writes
UTF-8 with full round-trip precision, identities, wt.%, Scale/Shift, original and
effective RIR/source, fit diagnostics, state reasons and revision/generation/database
identifiers. CSV `joint_rwp_net` is the dimensionless net Rwp fraction, matching the
panel/report diagnostic. The display rounds percentages to two decimals; internal
values are not adjusted to force a rounded total of 100%.

These percentages describe only the selected crystalline phases. Unidentified and
amorphous material are excluded; library RIR provenance is explicitly unverified.
Preferred orientation, microabsorption, reference choice and fixed-profile mismatch
can produce substantial errors. A frozen 28-scan evaluation gave mean absolute
phase errors of 13.55 percentage points for IUCr CPD 1a–1h and 7.70 for the binary
development scans. CPD uses organizer weighing results; the binary values use
nominal author sample-label masses, with no separately retained weighing log.
These measured errors qualify mass estimates separately from phase identification.
See the
[ICDD RIR explanation](https://www.icdd.com/assets/tutorials/Quantitative-Analysis-RIR.pdf)
for the method and its assumptions.
