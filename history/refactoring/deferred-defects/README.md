# Deferred defects and investigation concerns

Discoveries outside a migration unit's causal contract do not block that unit.
[Phase31C recovery backlog](phase31c-recovery-20261004.json) preserves source
anchors, reproduction/evidence, severity, future owner and causal disposition.

The complete R1–R8 investigation is on `rescue/phase31c-overreach-20261004` at
`fab332541e27fe40141384e09564c4d0e89c3d53`. Its source edits and raw failures remain
an investigation archive. Use that Git view and its two evidence catalogs to
restore a referenced raw attempt; do not import the investigated production fix
into a new phase without causal proof.

Some observations concern unchanged native paths; others were observed only with
the expanded patch. The latter are explicitly distinguished from proved baseline
defects. A future owner must reproduce against its own entry baseline before
selecting a fix. No finding here authorizes an architecture change or waives a
numerical failure. Every listed item has blocks_current_phase=false for31C.
