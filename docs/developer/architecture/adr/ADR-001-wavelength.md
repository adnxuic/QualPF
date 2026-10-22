# ADR-001 — Canonical wavelength authority

Status: existing decision indexed.

Use the named constants in [radiation.h](../../../../src/core/include/qualpf/domain/radiation.h).
The authority is the existing [Phase 10 wavelength decision](../../../../history/README.md#phase-10).
Do not create another numeric constant table in `.agent/`. Product defaults,
historical binary32 ABI transport and algorithm literals are different concepts.
Preserve file/custom/saved values; do not overwrite them with canonical defaults.

[Phase 11](../../../../history/README.md#phase-11) establishes owning Pattern
input without REAL ABI loss. [Phase 17](../../../../history/README.md#phase-17)
requires explicit wavelength alignment across reflection/scattering options;
[Phase 18](../../../../history/README.md#phase-18) restricts complete-card
calibration. Those contracts do not authorize rewriting old references.

## Provenance

Earlier index: `phase37e-entry-20261009:.agent/decisions/ADR-001-wavelength.md`.
Phase links identify original-path prefixes from the protected historical index.
The underlying external approval members are not independently reverified here;
use the phase locator and recovery maps to inspect their original bytes.
