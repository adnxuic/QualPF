# Introduction to Phase Identification in QualPF

Qualitative phase identification from powder diffraction data determines which crystalline phases are present in an unknown sample by comparing its measured diffraction pattern against a reference database of known substances. Once the constituent phases have been identified and accepted, semi-quantitative analysis estimates their relative weight fractions.

**QualPF** provides an integrated scientific environment that supports both classical peak-list matching (**CSM**) and direct profile-fitting search-match (**PFSM**).

---

## End-to-End Analysis Architecture

![QualPF end-to-end phase identification route from pattern import through data reduction, CSM/PFSM search-match, and quantification](../images/analysis-route.png)

---

## The Two Stages of Powder Diffraction Analysis

### Stage 1: Data Reduction (`Preprocessing` Menu)

Experimental powder diffraction patterns contain instrumental and sample artifacts alongside Bragg reflections:
- **Low- or high-angle noise** from beam spill, air scatter, or detector edge cutoffs (**Preprocessing > Range**).
- **Continuous background intensity** from fluorescence, incoherent scattering, sample holders, or amorphous content (**Preprocessing > Background Options**).
- **Peak broadening** governed by diffractometer optics and crystallite size/strain (**Preprocessing > FWHM Estimation Options**).
- **$K\alpha_1 / K\alpha_2$ doublet splitting** in laboratory X-ray instruments without primary monochromators (**Preprocessing > K-alpha2 Stripping**).
- **Counting noise** in rapid scans (**Preprocessing > Smoothing**).

In QualPF, data reduction transforms raw experimental data into a clean **working pattern** and/or **experimental peak list**. Every data-reduction operation is tracked in the project's workflow provenance (**Workflow — Current Project** dock) and can be inspected, re-parameterized, or undone at any time.

### Stage 2: Search-Match & Phase Acceptance (`Search-Match` Menu)

During search-match, QualPF queries all enabled reference databases (filtered by any active **Restraints**) and ranks candidate phases by a **Figure of Merit (FOM)**. You then review the top candidates using synchronized views:
1. **Results List** dock — sortable candidate table showing FOM, scale factor, peak-position score, intensity score, chemical formula, and quality mark.
2. **Central Diffraction Plot** — overlays candidate stick reflections ($d$/$2\theta$ bars) and, in PFSM mode, the live **Joint Trial** profile and difference curve.
3. **Compare** dock — line-by-line tabular alignment of experimental peaks against candidate and accepted phase reflections.
4. **Quantification** panel — tracks accepted phases, calculates **Reference Intensity Ratio (RIR)** weight percentages, and displays a live composition pie chart.

---

## Choosing Between CSM and PFSM

QualPF offers two distinct search-match engines. Understanding how they consume experimental data helps you select the best route for your sample:

| Characteristic | CSM (`Search-Match > Run CSM`) | PFSM (`Search-Match > Run PFSM`) |
| :--- | :--- | :--- |
| **Primary Experimental Input** | Discrete **Experimental Peak List** ($2\theta$, $d$, $I$, FWHM) extracted via **Preprocessing > Peak Search**. | Continuous **working diffraction profile** plus estimated **background** and representative **FWHM**. |
| **Prerequisite Steps** | Requires a non-empty peak table in the **Peaks** dock. Background estimation and $K\alpha_2$ stripping are recommended prior to peak search. | Requires a valid pattern, background curve, and representative FWHM. Automatically executes missing prerequisite steps (`Range`, `Background`, `Initial Peaks`, `FWHM`) according to the active PFSM workflow profile. |
| **Handling of Peak Overlap** | Relies on accurate peak picking; severe peak overlap in low-symmetry or multi-phase mixtures can hide weak shoulders or shift centroids. | Fits complete multi-reflection phase templates directly against the profile, naturally resolving overlapping peaks and multiphase intensity contributions. |
| **Specimen Displacement / Zero Shift** | Controlled via angular match tolerance **$\Delta 2\theta$** (manual or automatic). | Explicitly optimizes phase angular shift within configurable **Shift bounds** (default `-0.500` to `+0.500` °$2\theta$). |
| **Multi-Phase Identification** | Uses **Residual Searching** to attenuate matched experimental peaks as phases are accepted or iterated. | Uses **Joint Multiphase Profile Fitting**: selecting a candidate previews a joint fit of all accepted phases plus the trial candidate, and accepting a phase updates the joint profile and residual curve. |
| **Best Suited For** | Clean patterns with well-resolved peaks; imported peak-only datasets; rapid classical Hanawalt/Frevel-style screening. | Complex multi-phase mixtures, patterns with severe peak overlap, broad or weak reflections, and quantitative profile-guided phase verification. |

---

## Next Steps

- Take a guided tour of the QualPF interface in [Getting Started](getting-started.md).
- Learn how default workflow parameters and project step provenance work in [Workflow Settings and Project History](../settings.md).
- Walk through complete step-by-step analyses in [Worked Examples](examples.md).
