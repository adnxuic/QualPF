# ADR-005: Deterministic Range/Crop aligned-state construction

Status: **APPROVED_DETERMINISTIC_CPP_ALIGNMENT**, bounded RP1 user decision.
Authority: [approval message](../../../../history/README.md#phase-29),
[separate decision](../../../../history/README.md#phase-29),
[audit](../../../../history/README.md#phase-29).

Use the existing finite ordered bounds, binary32 closed membership and at least
two selected samples. Slice canonical binary64 sample values directly by source
IDs. x/observed/optional original/corrected-x/weights share the canonical slice.
Sampled signals retain their explicit SampleWindow intersection and deterministically
rebase; no overlap is unavailable and unknown alignment rejects. No padding,
guessed offset, Legacy stale lengths or prefix-by-length substitution.

Original observed stays on the cropped canonical domain. No full pre-crop source
snapshot, undo, Restore Full Range or native datac project-history owner is added;
existing explicit source/file reload remains full-source recovery authority.

All 565 raw FAILs,139 bound initialized curve differences and 4 undefined field
observations are unchanged. Undefined values are excluded as compatibility,
golden or qualification targets. No numerical qualification/PASS/tolerance or
general waiver is created. Any new field/operation/input/window/state/source
fingerprint/behavior difference requires evidence/review.

RP1 itself grants no R1–R8 authority, including R3 beyond this construction. The
eight exact contracts are subsequently approved under separate
[Phase29A.1 decisions](../../../../history/README.md#phase-29)
and [ADR-006](ADR-006-range-state.md). Phase29B product integration is separately authorized and
verified by [its gate](../../../../history/README.md#phase-29).
RP1 authority remains exact and unchanged. The Phase29B record reports one product
switch and zero new numerical qualifications. Its phase boundary describes that
historical task. Subsequent implementation ownership follows [current architecture](../overview.md);
this record grants no authority for another behavior change.

## Provenance

Earlier index: `phase37e-entry-20261009:.agent/decisions/ADR-005-range-alignment.md`.
Phase links identify original-path prefixes from the protected historical index.
The underlying external approval members are not independently reverified here;
use the phase locator and recovery maps to inspect their original bytes.
