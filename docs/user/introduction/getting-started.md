# Getting started

Use a measurement you are entitled to analyze, together with its radiation and
acquisition information. Keep the original file so you can start again without
depending on an edited export.

## 1. Load and inspect

Choose **File > Import Diffraction Pattern**. The chooser includes XY columns,
ASCII profiles, GSAS, XYE and XRDML formats, among others. Select the format that
matches the contents; changing an extension does not convert a file.

Inspect the plotted angular range, intensity scale and radiation information.
If the import fails, resolve the reported format or data problem before
proceeding. Register your reference database through
**S&M > Manage Databases** if it is not already available.

## 2. Choose preparation deliberately

For a first inspection, **Preprocessing > Generate Background** estimates a
background. Open **Background Options** if the curve needs examination or
adjustment. **View > Subtract Background** changes the displayed signal only.

Range selection, smoothing and K-alpha2 stripping are explicit operations.
Apply them only when appropriate to the measurement. They are not mandatory
steps for every pattern. See [Data reduction](../data-reduction/index.md).

## 3. Run a matching method

For peak-based matching, choose **S&M > CSM (Classical SM)**. You can first run
**Peak Search > Peak Search** to examine a peak list; otherwise CSM prepares
missing dependencies with its own settings.

For continuous-profile fitting, inspect **S&M > PFSM Options**, then choose
**S&M > PFSM**. PFSM prepares its required background and width evidence without
running a formal peak search. If automatic width estimation fails, read the
diagnostic and resolve the preparation problem instead of interpreting an absent
result as a negative phase identification.

## 4. Evaluate and accept

Select a candidate to inspect its card and overlay. Check several reflections
and inspect signal that the candidate leaves unexplained. Select the entries
you want to retain and use **Entry > Accept Selected Entries**. Accepting a phase
can update the remaining ranking; read the method-specific explanation in
[Search and match](../search-match/search-match.md).

The Quantitative panel may report percentages or explain why they are unavailable.
Neither a leading score nor a displayed percentage independently verifies the
sample composition.

## 5. Preserve the analysis

Save with **File > Save Project As** and choose a `.qpf` destination. Record the
measurement source and database version in your accompanying experiment notes.
Use the [worked procedure](examples.md) as a review checklist. Reopen the saved
project to inspect retained results; its session undo history starts empty.
