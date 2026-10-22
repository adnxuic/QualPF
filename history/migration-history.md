# Migration history and recovery

QualPF derives from IC-CNR's QualX3 implementation and retains its LGPL-3.0
license and attribution; see [LICENSE](../LICENSE), the README acknowledgments
and the original source/resource headers. Fortran-to-C++ migration completed as
Milestone F (`FORTRAN_MIGRATION_COMPLETE`) at implementation `f386bfa4`.
Windows GNU/MSVC and Linux GNU were accepted; macOS product releases are retired.
The subsequent architecture implementation was independently accepted as
`CPP_ARCHITECTURE_REFACTOR_COMPLETE` at `2610439f`. Original acceptance reports,
approvals, failures and compiler/source fingerprints are historical authority.

Current development starts with [architecture](architecture/README.md),
[numerical policy](numerical/policy.md), [building](development/building.md) and
[CHANGELOG.md](../CHANGELOG.md). Phase numbering ended with the refactoring;
this page is a historical lookup and recovery entry. Historical STOP records and
pending outcomes apply to their original snapshots, not current task status.
The normal suite is independent of migration execution facilities. Final
architecture reacceptance and subsequent strict-layer validation are recorded in
the [project baseline](development/project-baseline.json). Those acceptance
snapshots predate PFSM; current PFSM contracts and qualifications are maintained
in the [PFSM guide](numerical/pfsm.md).

## Retained reports

These documents retain their original scope, outcomes and evidence. They are not
templates or a task queue for new development. Routine change history belongs in
the changelog; keep current guides and contracts in their owning areas.

- [Independent architecture reacceptance](architecture/milestone-a/a01/independent-review.md),
  [gate](architecture/milestone-a/a01/gate.json) and [acceptance matrix](architecture/milestone-a/a01/acceptance-matrix.md)
- [Original blocked architecture review](architecture/milestone-a/independent-review.md)
- [Strict-layer refinement and validation](architecture/strict-layer-convergence.md)
  and [independent Linux closure](architecture/strict-layer-linux-closure.md)
- [Repository consolidation batches](development/repository-consolidation/batches.md),
  [closure](development/repository-consolidation/closure.md),
  [inventory](development/repository-consolidation/inventory.json) and
  [validation index](development/repository-consolidation/validation-index.json)
- [Agent context maintenance](development/agent-governance.md)
- [Unused-file audit](development/unused-file-audit-20261009.md)
- [Release refresh](development/release-refresh-20261009.md)
- [Main integration validation](development/main-integration-20261010.md)

## Archive locations

The protected local annotated tag `phase37e-entry-20261009` resolves to
`8fdf0a71d7eef8b6801e5336720c19b3c936e18f`. It retains the exact tracked entry tree,
including Phase37 audit/plan/DAG/decisions, implementation validation, original raw
attempts and receipt. `phase37e-regression-resources-20261009` protects the complete
current reference set, including 29 formerly ignored fixtures preserved by their
original manifest hashes. No Git history or existing tag was rewritten.

Complete older migration payloads are at `D:/QualPF-evidence-backup/20261004`.
The entry Git bundle `qualpf-history-cdb20666-20261007.bundle` retains the original
21 references. Its SHA-256 is
`130b03c7fe7f8eb9a57e7cf72d9269233ad0bbc18370d66649cb326f3c701396`.
`ENTRY-DISPOSITION-20261007.jsonl` maps original paths to hashes and actual ZIP
objects. `BUILD-DISPOSITION-20261007.json`, supplementary library and diagnostic
maps cover historical builds/data/raw attempts. The external `evidence/catalogs`
and `evidence/objects` retain original manifests and actual content, not pointers.
No remote availability is assumed. Do not prune unique Git/LFS objects.

Git stores some historical text as canonical LF while original recorded bytes
used CRLF. The inventory records each retired file's Git blob hash, original
hash/length and exact recovery rule. All 2,089 retired tracked originals were
actually recovered and matched: 2,062 direct, 26 LF-to-CRLF, and one storage guide
with its single recorded CRLF line. Apply only the recorded transformation and
verify the original SHA; ordinary `git archive` alone is not a byte-equality
claim for those text files. Original receipts, gates and hashes are unchanged.

[Recovery index](development/repository-consolidation/recovery-index.json) binds
six actual restored samples and their original hashes. [Inventory](development/repository-consolidation/inventory.json)
maps every retired tracked file to its protected tag or external payload, and
every moved reference to its stable current location. All 1,329 retired LFS
pointer payloads were verified against actual external ZIP length/SHA-256 and
catalog/member manifests before active copies were removed. No numerical values,
old SHA, qualification or FAIL was rewritten.

## Recovering historical authority

Use a separate directory. For tracked historical files, read/extract the protected
Git tree. For original raw members, locate their object/catalog using the external
maps, then restore with the byte-preserving tool in the protected entry tree.
The tool validates the containing object and member hashes; no historical program
execution is required for inspection. For example, restore the original tool:

```powershell
git archive --format=zip --output=build/history-tools.zip phase37e-entry-20261009 tools/evidence/store.py
Expand-Archive -LiteralPath build/history-tools.zip -DestinationPath build/history-tools
python -B build/history-tools/tools/evidence/store.py --root D:/QualPF-evidence-backup/20261004 restore D:/QualPF-evidence-backup/20261004/evidence/catalogs/phase31a-verification.json --destination build/history-view --path docs/refactoring/phase-31/complete-fortran-migration/zero-fortran-gate.json
```

Find Milestone F in `docs/refactoring/phase-36/current-release-acceptance/` and its
`phase36d-current-release001.json` external catalog. Find Phase36E in the external
entry catalog under `docs/refactoring/phase-36/post-f-hygiene/closure-report.md`.
Find Phase37 acceptance in `docs/architecture/phase-37/receipt.json` at the entry
tag. The restored original files keep their original language, license, toolchain
identity and failure/approval meaning. To reproduce an old gate, also restore
its specified original runner/source/configuration in an isolated historical
environment. Historical Fortran is never reintroduced to the current product.

Nine small older deferred-investigation records remain conservatively under
`docs/refactoring/deferred-defects`; they are original, unreproduced historical
concerns and are not a current execution queue. The accepted current state and
tests govern development. No additional architecture repair is included here.

## Retained ADR phase locators

These original-path prefixes are verified against
`phase37e-entry-20261009:docs/maintenance/phase-history-cleanup/HISTORY.md`.
Look up members in `ENTRY-DISPOSITION-20261007.jsonl` and its containing catalog,
then follow the byte-preserving recovery procedure above. The external approval
members themselves are **NOT_VERIFIED** by the Agent governance maintenance.
A phase locator is a lookup location, not a new approval or a claim that every
record in that phase applies to a particular decision.

### Phase 8

Original prefix: `docs/refactoring/phase-8/`. Product ownership and validation;
see [ADR-002](architecture/adr/ADR-002-session-and-cutover.md).

### Phase 9.6

Original prefix: `docs/refactoring/phase-9.6/`. Preprocessing cutover and rejection
acceptance; see [ADR-002](architecture/adr/ADR-002-session-and-cutover.md) and
[ADR-003](architecture/adr/ADR-003-numerical-acceptance.md).

### Phase 10

Original prefix: `docs/refactoring/phase-10/`. Wavelength and undefined native
observations; see [ADR-001](architecture/adr/ADR-001-wavelength.md) and
[ADR-003](architecture/adr/ADR-003-numerical-acceptance.md).

### Phase 11

Original prefix: `docs/refactoring/phase-11/`. Owning Pattern input and private
preprocessing boundaries; see [ADR-001](architecture/adr/ADR-001-wavelength.md)
and [ADR-002](architecture/adr/ADR-002-session-and-cutover.md).

### Phase 12

Original prefix: `docs/refactoring/phase-12/`. Preprocessing and Kalpha2 numerical
contracts; see [ADR-002](architecture/adr/ADR-002-session-and-cutover.md) and
[ADR-007](architecture/adr/ADR-007-kalpha2-policy.md).

### Phase 13

Original prefix: `docs/refactoring/phase-13/`. Bounded numerical differences;
see [ADR-003](architecture/adr/ADR-003-numerical-acceptance.md).

### Phase 14

Original prefix: `docs/refactoring/phase-14/`. Exact symmetry fractions and bounded
numerical acceptance; see [ADR-003](architecture/adr/ADR-003-numerical-acceptance.md)
and [ADR-004](architecture/adr/ADR-004-crystallography-conventions.md).

### Phase 15

Original prefix: `docs/refactoring/phase-15/`. Reciprocal indices, native exact
audit and Friedel policy; see [ADR-003](architecture/adr/ADR-003-numerical-acceptance.md)
and [ADR-004](architecture/adr/ADR-004-crystallography-conventions.md).

### Phase 16

Original prefix: `docs/refactoring/phase-16/`. Scattering and scoped numerical
acceptance; see [ADR-003](architecture/adr/ADR-003-numerical-acceptance.md) and
[ADR-004](architecture/adr/ADR-004-crystallography-conventions.md).

### Phase 17

Original prefix: `docs/refactoring/phase-17/`. Wavelength alignment, reflection
and line/card contracts; see [ADR-001](architecture/adr/ADR-001-wavelength.md),
[ADR-003](architecture/adr/ADR-003-numerical-acceptance.md) and
[ADR-004](architecture/adr/ADR-004-crystallography-conventions.md).

### Phase 18

Original prefix: `docs/refactoring/phase-18/`. Complete-card calibration and scoped
qualification; see [ADR-001](architecture/adr/ADR-001-wavelength.md) and
[ADR-003](architecture/adr/ADR-003-numerical-acceptance.md).

### Phase 29

Original prefix: `docs/refactoring/phase-29/`. RP1, eight separately approved
Range contracts and Phase29B conformance; see
[ADR-005](architecture/adr/ADR-005-range-alignment.md) and
[ADR-006](architecture/adr/ADR-006-range-state.md).

### Phase 30

Original prefix: `docs/refactoring/phase-30/`. K1-P1/K3-P1 approval and Phase30B
conformance; see [ADR-007](architecture/adr/ADR-007-kalpha2-policy.md).

### Phase 31

Original prefix: `docs/refactoring/phase-31/`. Phase31B provider and transport
contracts; see [ADR-007](architecture/adr/ADR-007-kalpha2-policy.md).
