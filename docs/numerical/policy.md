# Numerical policy and regression references

Preserve approved formulas, precision, rounding, ordered evaluation, tolerances,
error behavior and independent expected values. Do not regenerate a golden from
product output or widen a tolerance to obtain PASS. Preserve original failed
comparisons and approved bounded differences. Do not reproduce undefined Legacy
behavior; the current deterministic C++ contracts are the authority.

Current C++ Core producers retain their accepted numerical implementations and
per-source strict floating compilation. Independent analytic/property and exact
high-precision references coexist with frozen observations. Binary32 transport,
RNE primitives, formed-angle policies and qualified crystallography differences
retain their original approved scopes. Host transcendental bit identity is not
assumed. A substantive numerical correction requires independent mathematics,
boundary/error evidence and bounded downstream review before implementation.

The [retained product decisions](../architecture/adr/README.md) index the approved
wavelength, crystallography, Range and Kalpha2 contracts with phase-specific
historical locators. Current implementation ownership follows the architecture
guide; historical intermediate owners do not replace it.

## Current resources

[The manifest](../../tests/reference/manifest.json) binds 169 independently frozen
reference/provenance inputs by SHA-256 and length, with `expected_from_product=false`.
`tests/data` supplies current numerical datasets, wires, original transcripts,
tolerance policies and CIF fixtures. `tests/regression/references` contains the
small required qualified-output, independent-proof and inert provenance set,
organized by algorithm rather than execution phase. Source fragments are `.txt`
and never compiled. Registry generators independently reproduce accepted tables.

The original logical path keys in manifests and reference checks are provenance
identities. `reference_paths.py` maps them to current resources; it neither restores
complete history nor runs a native oracle. Missing/corrupt inputs fail. The
unchanged approved values and important malformed/stale/duplicate/error assertions
remain in normal C++/Python regression. No historical evidence ZIP/LFS download
or Phase1-36 runner is a normal-build/test prerequisite.

```sh
python -B tools/validation/check_references.py
ctest --test-dir build/linux-gnu -L normal --output-on-failure
```

See [testing](../../tests/README.md), [test-data provenance](../../tests/data-guide.md)
and [historical recovery](../migration-history.md). Historical gates bind their
original source/runner/reference/compiler snapshots, never mutable current
agent files. Existing numerical approvals are not reopened by file organization.
