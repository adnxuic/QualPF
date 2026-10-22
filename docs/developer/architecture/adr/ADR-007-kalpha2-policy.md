# ADR-007 — Exact Kα2 routing and flag-only state policy

Status: two explicitly approved semantic contracts, separately implemented and
verified in Phase30B. No numerical acceptance is created or transferred.
Authority: [verbatim human approval](../../../../history/README.md#phase-30),
[decisions](../../../../history/README.md#phase-30),
[Phase30B product conformance](../../../../history/README.md#phase-30).

K1-P1 binds original proposal SHA-256
9e190c260c8913dcc33915b2ff6e5a5c905f9a534c4f999b7d22108f45416fca:
exact dedicated cpp/legacy overrides global; unset/empty inherits only exact global
legacy, all other global tokens select C++; unsupported nonempty dedicated rejects.
Selection fixes before operation; no cross-backend error retry.

K3-P1 binds original proposal SHA-256
efbca50433cb0c3a99a61523c2f6bfa057055e9ce4e1884b9cb314ff02498715:
owning binary64 result exact ordinary finite == opening committed y, no epsilon
or narrowing, signed zero equal. Exact no-op preserves valid numeric/editor/Search
payload and original representation, while current provenance true publishes one
new revision, dirty project and existing QXPv2 flag. True changes retain complete
old-signal invalidation/delta0.5. Unknown current history still rejects, repeat does
not evaluate/publish, and old in-flight SearchHandle remains stale.

K2/K4/K5/K6 are preservation obligations. Phase12 math/native compatibility,
radiation supported domain, Search/residual and QXP schema are unchanged. Old
pending review gates, raw semantic FAILs and original numerical fingerprints remain
immutable. Native/Fortran/runtime deletion was outside that original decision's scope.

Phase31B changes only the selected compatibility implementation provider to the
existing Phase12 C++ owner. Exact K1/K3 contracts and original approval hashes
above are unchanged. The native private value definition was a temporary oracle
for that historical consumer migration. Authority:
[31B report](../../../../history/README.md#phase-31),
[transport contract](../../../../history/README.md#phase-31).
No new semantic or numerical decision is created.

Current Kalpha2 selection and size/error mapping belong to Qt-free Application
use cases. No production native oracle or src/compat target remains; see the
[compatibility allowlist](../compatibility-allowlist.json). Frozen observations
remain test evidence, with the exact K1/K3 contracts unchanged.

## Provenance

Earlier index: `phase37e-entry-20261009:.agent/decisions/ADR-007-kalpha2-policy.md`.
Phase links identify original-path prefixes from the protected historical index.
The underlying external approval members are not independently reverified here;
use the phase locator and recovery maps to inspect their original bytes.
