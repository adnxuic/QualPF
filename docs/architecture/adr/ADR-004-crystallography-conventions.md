# ADR-004 — Exact symmetry, reciprocal indices and Friedel policy

Status: existing decisions indexed.

Symmetry uses integer rotations and translation numerators modulo 12 with fixed
denominator 12; this covers the retained table fractions, not arbitrary-origin
rational transformations. Fractional direct coordinates use column action
`r' = R*r + t`. Authority: [Phase 14 contract](../../migration-history.md#phase-14)
and [approved exact-source fractions](../../migration-history.md#phase-14).

Reciprocal indices use **row action `h' = h*R`**, equivalently `Rᵀ*h` on a column.
Do not replace it with `R*h`. Translation enters the stabilizer extinction phase
through `h·t`, not the equivalent hkl. The per-operation witness matters, even
when complete orbits happen to agree. Authority:
[Phase 15 contracts](../../migration-history.md#phase-15),
[native exact audit](../../migration-history.md#phase-15),
[acceptance](../../migration-history.md#phase-15).

Symmetry-only and Friedel-merged orbits are explicit separate policies. Inversion
may already belong to the symmetry group. Do not use Legacy's single-crystal flag
as a definition of the full acentric orbit or silently equate anomalous F(h) and
F(-h). Preserve declared policy through reflection, scattering, line and card
construction. Authority: [Phase 15](../../migration-history.md#phase-15),
[Phase 16](../../migration-history.md#phase-16),
[Phase 17](../../migration-history.md#phase-17).

## Provenance

Earlier index: `phase37e-entry-20261009:.agent/decisions/ADR-004-crystallography-conventions.md`.
Phase links identify original-path prefixes from the protected historical index.
The underlying external approval members are not independently reverified here;
use the phase locator and recovery maps to inspect their original bytes.
