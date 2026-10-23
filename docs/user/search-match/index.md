# Search and match

Select the method according to the evidence you want to examine. Both methods
read reference cards, but their preparation and scores have different meanings.

| Method | Input to comparison | Main result to inspect |
| --- | --- | --- |
| CSM | Measured experimental peaks and reference reflections | Position, intensity and peak-coverage agreement |
| PFSM | Continuous working observations and expanded reference profiles | Joint fitted profile, residual and candidate improvement |

The commands are **S&M > CSM (Classical SM)** and **S&M > PFSM**. Their adjacent
Options commands configure the current project's method. **S&M > Restraints**
provides chemical and crystallographic restrictions shared by the search entry.

## A practical review loop

Run a search, inspect a candidate's card and reflection overlay, then decide
whether it explains reproducible features of the measurement. Accept justified
phases and inspect the updated ranking and unexplained signal. Record the sample
information supporting your decision instead of treating ranking as proof.

Sorting a result table changes its presentation. PFSM filtering and pagination
also leave the round's score normalization intact. Accepting or removing a phase
can change the analysis and therefore the ranking itself.

Use [Matching and quantification](search-match.md) for method options and score
interpretation, and [Restraints](restraints.md) for restricting a search. Save a
QPF project to preserve the selected cards and analysis evidence for later review.
