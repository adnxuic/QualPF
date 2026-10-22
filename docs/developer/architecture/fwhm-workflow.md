# Configured physical FWHM

Application owns `FwhmConfiguration`, the last `FwhmUsage`, and separate usage
evidence for the existing peak snapshot. Configuration selects Classical Gaussian
fitting or the independent geometric method, a fixed-value mode, the displayed
physical width (initially 0.1 degrees two theta), and an integer geometric minimum
sampling density Q in [2, 8] (initially 6). Absence preserves legacy callers.

`evaluateConfiguredFwhm` takes an immutable source, an owned working pattern and
configuration, and an ExecutionContext. It never publishes. Fixed mode skips
initial detection and estimation. Automatic mode never treats an old displayed
value as a cache or supplies a wavelength seed after failed measurement. A normal
no-eligible result retains diagnostic evidence; compound peak preparation stops
without publishing any numerical draft.

Gaussian fitting retains the Classical analysis signal, binary32 arithmetic and
strongest-three reduction. A checked `AnalysisSpectrum` containing the unsubtracted working observations
and aligned background is accepted for compound operations. Only the numerical
adapter prepares the binary32 analysis buffer. Geometric measurement uses the working signal
and an explicitly aligned background in binary64, preserving negative net values.
Already-subtracted or unknown subtraction inputs are refused. A missing background is
prepared privately by the existing upstream estimator. Unknown subtraction history
or incomplete alignment fails. Noise remains explicitly unknown in this GUI route;
weights and normalized intensities are never reinterpreted as counting noise.

Both methods supply a physical width to formal detection. The existing Classical
conversion `float(width) * .9f` remains in the Core source with its original floating
options. Fixed and geometric sources have distinct calibration identities and never
manufacture Gaussian fitted-width evidence. Lower-level Classical fallback and
manual-range contracts remain unchanged.

Every configured redetection evaluates the selected policy again. Candidate
reselection retains the original detection evidence. Changes to intensity,
background, range or numerical producer invalidate automatic measurement currency;
Restart preserves the FWHM settings and displayed historical value. Editing FWHM
settings alone does not rerun the existing peak list.

## Project boundary

QXP preprocessing carries independent version-1 `fwhm`, `fwhm_usage` and
`peak_fwhm_usage` objects. They retain configuration, physical value, origin,
algorithm identity, input identity/revision, Q and contributing-width count.
Runtime candidate arrays, covariance and FWHM input-pattern snapshots are not
serialized. Measurement evidence owns the actual estimator input (including the
explicit aligned background and unsubtracted observations), independently of later pattern edits. Unknown preprocessing
and FWHM extension values keep integer/null types. Loaded automatic evidence is
historical and must be remeasured before redetection; a fixed value can be used
directly. Missing fields retain the GUI defaults without asserting a measurement.
Existing non-default AlgorithmSpec save restrictions are unchanged.

Runtime usage also records explicit manual triggering and the source pattern
before estimator preparation. These fields are never reconstructed from loaded
history. PFSM may reuse compatible manual physical widths without the Classical
parameter conversion; otherwise its automatic policy is geometric, regardless
of the Gaussian GUI default. Fixed configuration has priority. An automatic PFSM
measurement belongs to its frozen analysis and does not overwrite CSM settings.

## GUI behavior

Preprocessing keeps FWHM Estimation and FWHM Estimation Options together between
the Initial Peak Search group and Smoothing group. The options dialog has two
horizontal separators: one after algorithm-specific controls and one before the
standard Restore Defaults / Cancel / OK buttons. Geometric Q uses synchronized
integer slider/spin controls; Gaussian hides them. Fixed mode disables algorithm
controls and makes the physical width editable. Automatic mode makes it read-only.
The displayed decimal never replaces the stored binary64 value unless explicitly
edited; an initial or historical value is labeled as awaiting estimation.

Automatic OK evaluates every time, then saves configuration and width atomically
and closes. Fixed OK saves and uses the supplied value without a numerical estimator.
No-eligible or invalid input leaves the dialog open without committing its draft.
Cancel/Escape/window close cancels pending work and discards the draft after drain.
Restore Defaults changes only the draft to Gaussian/automatic/0.1/Q=6 and preserves
opaque extension fields. OK never reruns the existing peak list.

Options OK and the independent menu command use the existing single-worker task
controller with owned input, source/revision checks, cooperative cancellation and
queued publication. There is no SQL access for FWHM evaluation. Busy/edit/Closing
admission, action restoration and the existing nonblocking shutdown barrier remain
in force. Existing peak-editor previews retain their private transaction/scheduling
model; their redetection, the main Peak Search command and missing-peak Search/Match
preparation all pass the explicit configured FWHM policy. Pure reselection does not
estimate a new width. Fixed mode disables the independent estimation menu command.

`qualpf.application.configured_fwhm` uses independent triangular crossings and
injected stage counters to verify signal choice, fixed-mode skipping, physical
parameter mapping, atomic no-eligible failure and cancellation. Project boundary
tests verify new/old QXP, historical evidence, malformed Q and opaque extensions.
`qualpf.gui.fwhm_options` operates the real controls and task controller, checking
menu separators, precision-preserving display, failure retry, all three redetection
routes, cancellation and confirmed close before owner publication. It can save
native Qt captures with `--capture DIRECTORY`. Window lifecycle tests check the
admission and cancellation of editable Initial Peak Search and PFSM options.
Initial peak search has its own independent candidate command and GUI checks;
its owned options also flow into Gaussian width preparation.
