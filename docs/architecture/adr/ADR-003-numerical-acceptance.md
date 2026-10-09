# ADR-003 — Numerical acceptance and undefined Legacy behavior

Status: existing decisions indexed.

Raw comparisons remain raw evidence. Independent proof plus explicit user approval
can qualify an exact difference; it cannot make the original comparison PASS or
change its golden value/tolerance. Qualifications are bounded to their records,
compiler fingerprints and evidence, never inherited by a later phase.

Authority: [Phase 13 decision](../../migration-history.md#phase-13),
[Phase 14 decision](../../migration-history.md#phase-14),
[Phase 15 decision](../../migration-history.md#phase-15),
[Phase 16 decision](../../migration-history.md#phase-16),
[Phase 17 decision](../../migration-history.md#phase-17),
[Phase 18 decision](../../migration-history.md#phase-18).
These decisions have different scopes; this index does not enlarge any of them.

Undefined native FOM/preprocessing observations are preserved while C++ uses
documented deterministic contracts. Never synthesize NaN/stale memory or silently
filter invalid candidates to call a workflow compatible. Authority:
[Phase 10 undefined contracts](../../migration-history.md#phase-10),
[Phase 9.6 rejection acceptance](../../migration-history.md#phase-96).
Current working rules: [numerical policy](../../numerical/policy.md).

## Provenance

Earlier index: `phase37e-entry-20261009:.agent/decisions/ADR-003-numerical-acceptance.md`.
Phase links identify original-path prefixes from the protected historical index.
The underlying external approval members are not independently reverified here;
use the phase locator and recovery maps to inspect their original bytes.
