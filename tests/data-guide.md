# Frozen test-data provenance

`data/reference-manifest.json` records F01-F17 capture tasks and hashes for the
existing three patterns and two reference tables. Its `expected` and `tolerances`
fields are null; these records are capture inventory rather than validated golden
expectations.

`data/numerical-reference-manifest.json` records S4 tasks N01-N08, scenarios and
per-dimension comparison goals. Its `expected` and `environment` fields remain
null. Proposed tolerances are not approved compatibility bounds, and
`legacy_tolerances_frozen` remains false. Analytical expectations in independent
C++ unit tests are separate from this inventory.

The original `data/README.md` is a frozen provenance input retained at its original
path and bytes, including its original language. This English guide is the current
description. Current accepted reference checks use
[reference/manifest.json](reference/manifest.json); see [test instructions](README.md).
