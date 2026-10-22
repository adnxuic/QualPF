# ADR-002 — Product owner and preprocessing cutover

Status: existing decisions indexed.

## Retained contract and current ownership

The Application Workspace uniquely owns ProductSearchSession, publishing immutable
ProductHandle snapshots through validated source-bound transactions. GUI SessionView
borrows that owner. The older standalone Session library is not the product owner.
See [current ownership](../overview.md) and [strict layer convergence](../strict-layer-convergence.md).
The earlier AppState and Legacy workspace descriptions are historical implementation
context; no production AppState, native workspace or writable mirror remains.
Provenance: [Phase 8 ownership and acceptance](../../../../history/README.md#phase-8),
[product validation](../../../../history/README.md#phase-8).

Preprocessing cutover followed explicit manual signoff, fixes and product/fallback
regression in [Phase 9.6 cutover](../../../../history/README.md#phase-96)
and its [validation](../../../../history/README.md#phase-96).
Earlier “pending / Legacy default” statements are historical, not active policy.
Current provider and compatibility responsibilities follow the
[consumer allowlist](../compatibility-allowlist.json).

Owning input and private preprocessing drafts are extended by
[Phase 11](../../../../history/README.md#phase-11) and
[Phase 12](../../../../history/README.md#phase-12). Cancel/failure/stale Apply
must not publish partial numerical state. Earlier compatibility mirroring belongs
to the historical cutover; current GUI projections follow canonical publication.

## Provenance

Earlier index: `phase37e-entry-20261009:.agent/decisions/ADR-002-session-and-cutover.md`.
Phase links identify original-path prefixes from the protected historical index.
The underlying external approval members are not independently reverified here;
use the phase locator and recovery maps to inspect their original bytes.
