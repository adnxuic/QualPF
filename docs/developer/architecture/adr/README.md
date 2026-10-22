# Retained product decisions

These bounded decisions explain current wavelength, precision, crystallography,
Range and Kalpha2 contracts. Their original approvals/failures remain in the
[protected archive](../../../../history/README.md); this index grants no new
numerical approval. Current ownership follows [architecture](../overview.md).

| Decision | Contract |
| --- | --- |
| [001](ADR-001-wavelength.md) | Canonical wavelength |
| [002](ADR-002-session-and-cutover.md) | Session/preprocessing boundaries |
| [003](ADR-003-numerical-acceptance.md) | Qualified differences and undefined behavior |
| [004](ADR-004-crystallography-conventions.md) | Translations, reciprocal hkl and Friedel conventions |
| [005](ADR-005-range-alignment.md) | Deterministic aligned Range values |
| [006](ADR-006-range-state.md) | Range transaction semantics |
| [007](ADR-007-kalpha2-policy.md) | Kalpha2 product state policy |

New substantive decisions need new independent evidence and bounded review;
never rewrite an old approval or relabel an original FAIL.

Each record separates its retained contract from historical implementation
context. Phase-specific recovery links locate the original prefixes in the
protected historical index. Original approval members have not been independently
reverified by this documentation maintenance. Preserve their proposal identities
and recover original bytes before relying on a particular approval.
