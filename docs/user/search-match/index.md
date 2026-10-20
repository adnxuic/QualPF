# Search & Match

QualPF identifies crystalline phases through Classical peak matching (CSM) or
continuous Profile Fitting Search-Match (PFSM), ranking reference candidates by
the selected method's **Figure of Merit (FOM)**.

## Menu

| Action | Description |
|--------|-------------|
| **S&M → CSM (Classical SM)** | Run Classical Search-Match using current peaks and options |
| **S&M → CSM Options** | Configure FOM weights, Δ2θ, min. FOM |
| **S&M → PFSM** | Fit complete reference profiles to the continuous imported original |
| **S&M → PFSM Options** | General and Candidate Recall tabs: shifts, weighting, display threshold, workers, cache and candidate selection |
| **S&M → Restraints** | Restrict the search to specific elements, space groups, etc. |
| **S&M → Recalculate FOMs** | Recompute candidates using the current analysis method |
| **S&M → Manage Databases** | Add, remove or switch reference databases |

CSM and PFSM each have their own menu group. **Restraints** has separators above
and below it because the database restrictions are shared. **Recalculate FOMs**
and **Manage Databases** each occupy a separate group. The Restraints Composition
tab also selects CSM or PFSM immediately left of Min species. Both entry points
use the same search admission and checked method transition. PFSM Options saves
settings without starting a search or switching the selected analysis method.

## Results List

After a search, the **Results List** dock shows ranked candidates with columns:

| Column | Description |
|--------|-------------|
| (colour) | Unique colour assigned to the card |
| QM | Quality mark of the reference card |
| ID | Card identifier |
| Chemical Name | Compound name |
| Chemical Formula | Chemical formula |
| Peakpos. / Rwp(net) % | CSM peak-position component / PFSM joint trial net Rwp in percent |
| Intensity / Shift °2θ | CSM intensity component / PFSM candidate shift in degrees |
| Scale | Intensity scale factor; PFSM shows the joint trial coefficient, not mass fraction |
| FOM | CSM total FOM / PFSM improvement relative to the best candidate in this round |
| S-Quant. / ΔRwp pp | CSM RIR / PFSM baseline-to-trial improvement in percentage points |

Click a column header to sort the displayed table. This does not change the
stored candidate order, later equal-score rescoring, or candidate order in a
saved project. Deleting a selected row removes that candidate and preserves the
order of the remaining candidates. Accepting several selected rows processes
them in their displayed order.

The same compact table serves both methods, with independent column widths and
sort preferences. PFSM Rwp, Shift and Scale describe the selected phases plus
that candidate jointly fitted. Filtering and pagination never renormalize FOM.
The **Quantitative analysis · PFSM/RIR** panel shows wt.%, joint Scale, Shift and
effective RIR, with joint net Rwp and availability reasons. A pie chart appears
only when every selected phase is quantifiable. Select a phase and use **Edit RIR…**
to enter a finite positive Cu Kα RIR and its source; **Restore reference RIR**
removes the analysis-local revision. The reference database is unchanged. Revisions
survive removing/reselecting the same card and end with the analysis. Missing or
invalid RIR, unsupported radiation, zero templates or dependent templates make the
whole composition unavailable. These percentages normalize only the selected
crystalline phases; unidentified and amorphous material are excluded. Library RIR
provenance is explicitly unverified, and preferred orientation, microabsorption
and profile mismatch can bias the result. Reports and printing share the panel's
result and preserve revision sources. **Export CSV…** saves UTF-8 rows with full
precision, phase identity, wt.%, Scale/Shift, RIR/source, status reasons, joint Rwp
and snapshot identifiers. Unavailable compositions retain empty percentage fields
and their reasons; no target file is replaced if an atomic write fails. Its plot
shows the current working observations, background points, joint calculation and
residual in the working intensity units. **View → Subtract
Background** switches observations and calculation to their net display without
refitting or changing the residual. QPF 1.0 saves the full PFSM analysis, including RIR revisions, for verified continuation after reopening.

See [Search Match](search-match.md) and [Restraints](restraints.md) for details.
