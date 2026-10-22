# QPF 1.0 project contract

QPF is QualPF's native `.qpf` full-project document. QXP v1/v2/v3 are rejected,
regardless of filename. Identification uses file bytes and protocol identifiers;
renaming a QXP file does not make it a QPF project. This contract starts a new
format family, independently of the former JSON envelopes.

## Container

The container is a SQLite database with its standard 16-byte header,
`application_id = 0x51504650`, and `user_version = 1`. Its only application tables
are:

```sql
CREATE TABLE manifest (id INTEGER PRIMARY KEY CHECK(id=1), payload BLOB NOT NULL, sha256 TEXT NOT NULL)
CREATE TABLE objects (id TEXT PRIMARY KEY NOT NULL, kind TEXT NOT NULL, version INTEGER NOT NULL, payload BLOB NOT NULL, sha256 TEXT NOT NULL)
CREATE TABLE blocks (sha256 TEXT PRIMARY KEY NOT NULL, byte_length INTEGER NOT NULL, payload BLOB NOT NULL)
```

The manifest has exactly one row. CBOR payloads use definite-length arrays/maps,
UTF-8 text map keys in bytewise lexical order, native integers, binary64 floating
values, byte strings, booleans, null and undefined. Duplicate keys, trailing data,
unsupported simple values and malformed CBOR fail. A signed integer and a floating
value remain distinct, even if numerically equal. Unsigned IDs and metadata integers support all 64 bits.
Known scientific values must be finite; negative zero survives. Float32-valued
fields use their exact binary64 embedding and must narrow back without change.

Manifest fields are `format` (`qualpf.project`), `major` (1), `minor` (0 for new
files), `project_uuid` (lowercase UUID without braces), `created_utc`, `saved_utc`
(ISO 8601 UTC timestamps), `writer`, `required_features`, and `root` (object
reference). Required features are `typed_arrays`, `frozen_analysis`, `project_view`.
The major version and required features govern admission; an unknown minor version
alone does not. Save As retains the UUID and creation timestamp.

Objects have stable semantic kinds and version 1. The authoritative field/token
inventories are [qpf_schema.h](../../../src/infrastructure/serialization/qpf_schema.h)
and [qpf_tokens.h](../../../src/infrastructure/serialization/qpf_tokens.h). Literal wire
names and tokens must not change when C++ names or enumeration order change.
The root `project` object references `analysis`, `view` and `quantification`.
Project settings use named `preprocessing`, `csm`, `pfsm` and `peak_search` fields,
independently of the in-memory profile-array order. Literal application metadata
is wrapped as `{"$metadata": value}` so user keys named `$ref` or `$array` never
become protocol references.
References have exactly the map shape `{"$ref": "object-id"}`. Current object IDs
are lowercase SHA-256 strings derived from a semantic kind and document-local path;
readers treat IDs as opaque and must resolve them. Shared immutable input objects
are referenced once. Card and workflow-record collection identities use their
domain IDs rather than display order.

Manifest hashes cover its stored CBOR payload. An object hash covers the CBOR
array `[object-id, kind, version, fields]`. Hashes are lowercase hexadecimal SHA-256.
Hashes detect damaged content; this format does not provide authentication.

## Numerical arrays

Numeric arrays are represented by:

```text
{"$array": {"dtype": "f64", "shape": [N], "byte_order": "little",
            "unit": "intensity", "blocks": [sha256, ...]}}
```

Supported numeric encodings are IEEE `f32`/`f64` and fixed-width signed/unsigned
integers (`i8/i16/i32/i64`, `u8/u16/u32/u64`). The current model uses one-dimensional
arrays, including fixed arrays. Blocks contain raw little-endian values, at most
4 MiB each, with no compression. Empty arrays have no blocks. SHA-256 covers raw
block bytes; identical blocks are stored once regardless of the referring array.
Chunk order reconstructs the array exactly. Length arithmetic is checked before
allocation and chunk lengths must exactly match the declared element count/type.

Units are `degree` for angular coordinates, widths and shifts, `angstrom` for
d-spacing, `intensity` for signals/deviations, `weight` for observation weights,
`squared_intensity` for covariance diagonals, `count`/`second` for count/rate
evidence and `percent` for quantitative percentages. Generic values without a
standalone unit use `contextual`; indices, flags and ratios use `dimensionless`.
The owning measurement/provenance declares intensity units and normalization. Source precision
is evidence, never an instruction to round stored binary64 values to binary32.

## Scientific and presentation state

The document preserves all project profiles, original/current/comparison signals,
preprocessing inputs and evidence, peaks, algorithms, workflow records and their
dependencies, full reference cards and canonical candidate/phase order. PFSM also
preserves frozen observations/problem/universe, fit and recall evidence, selected
parameters, joint baseline/objective, normalization, quantitative references and
RIR revisions. Quantification is checked against that frozen evidence on read.
Executable templates are reconstructed; no fitting/search is performed on open.
Inspecting stored peaks and reselecting their frozen pool do not prepare upstream
workflow steps. Applied count/append/threshold controls are retained when reopening
the editor; scientific search keeps its provenance and database admission checks.

Existing powder-CIF source artifacts and their original bytes are scientific
provenance and remain intact, including non-UTF-8 source bytes. No additional raw
file attachments or reference databases are packaged. Session undo, tasks, weak
sources, thread state, runtime database-selection versions and caches are omitted.

Application Workspace owns a separate immutable ProjectViewState and display
revision. It stores axis/scale/ranges, graph and marker styles, visibility, offsets,
selected cards, card colors and result display order. Colors use RGBA8
`0xRRGGBBAA`; pen, marker, layer and visibility kinds use semantic strings. Window/dock geometry and
global preferences remain local. Rendering uses GUI projections; a visual edit
does not replace the scientific snapshot or make a running calculation stale.

Missing databases do not prevent restoration, inspection/export or operations on
frozen evidence. Fetching new references retains the existing database identity
guard. Application binds frozen-data tasks to the saved analysis identity, while
new reference reads keep the selected provider identity. A mismatched database
requires restarting the analysis before retrieving new references.

## Extensions, limits and transactions

Unknown optional object fields, optional objects and their referenced blocks
survive open/save. Reserved `$ref`/`$array` references also apply to extensions.
Known fields win when rewriting modified objects. Unknown required features, known
objects with unsupported versions, dangling references and cycles fail admission.
Unknown extension fields are never used as scientific evidence.

ProjectReadOptions defaults to a 2 GiB logical decode budget and maximum depth 64.
The budget accounts for CBOR structure, stored blocks and materialized arrays,
including repeated logical materialization. Callers may change the limits.
Exhaustion, overflow and malformed dimensions are structured failures.

File operations reject Busy, active edits and Closing. All SQLite connections and
queries are created, used and released in their execution thread. Readers use
read-only/query-only access, an exact fixed schema, fixed queries, integrity checks
and per-record hashes. No SQL from the document is executed by application code.

Writers build a private temporary database with DELETE journaling and FULL
synchronization, commit and close it, then validate it through the native reader.
They stream the closed file through binary QSaveFile with direct-write fallback
disabled. The completed artifact has no WAL/journal dependency. Failed writes
preserve the old destination and current project path/save marker.

Opening prepares and validates owned scientific/presentation values first.
Workspace rechecks source and view revision, prepares all allocations, installs
both values, and only then notifies scientific observers. Successful open clears
session undo. Failure leaves the existing workspace, identity and undo intact.

Historical QXP codecs live only in tests/support/legacy_qxp. Production file ports
accept QPF exclusively; test-only historical controls do not imply compatibility.

### PFSM resource-limit extensions

PFSM settings and frozen recall configuration optionally encode the resource-limit
policy, defaulting to bounded integral when absent. Recall reports optionally
encode the actual backend, switch reason and bounded integral/old-shift evidence;
absence means historical v2 diagnostics. Other existing fields remain mandatory.
Unknown extension fields survive the normal archive round trip. Integral caches,
old-phase column banks and worker workspaces are not serialized. Loading does not
rerun recall; a later search/re-score uses the restored frozen problem and current
resource routing. Backend-specific validation admits integral union sizes above
the v2 limits while checking candidate identity, baseline and evidence consistency.
