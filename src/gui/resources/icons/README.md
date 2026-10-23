# Action icons

These original QualPF icons supply the colorful 24px GUI action artwork.
The SVG files are the editable masters. They use a 24-unit canvas, transparent
backgrounds, rounded outlines, blue spectra, green execution/output accents and
red removal accents. Shapes distinguish operations without relying on color.
Options use the corresponding operation with a gear. Original icons are reused
for the same operations, including CSM settings, peak editing and phase removal.

The application consumes the committed 24px PNGs and 48px `@2x` PNGs through
`:/icons/<name>.png`. Both sizes are explicitly registered in `../resource.qrc`.
Qt selects the high-density companion automatically. SVG masters are authoring
inputs only; normal builds and packages require neither Node.js nor Qt SVG.

## Offline regeneration

From the repository root, use Node.js with `sharp` available in its module search
path (the current exports use sharp 0.35.5):

```sh
node tools/gui/render_icons.cjs
node tools/gui/render_icons.cjs --check
node tools/gui/render_icons.cjs --check --preview build/icon-design
```

The first command regenerates both PNG sizes. `--check` verifies the masters,
image dimensions, alpha channels, visible artwork and resource registrations
without rewriting assets. `--preview` writes light/dark contact sheets displaying
each icon at 24, 36 and 48 pixels. Preview files belong in an ignored build tree.
When using a locally provisioned sharp installation, set `NODE_PATH` to its
`node_modules` directory; do not commit a machine-specific runtime path.

Register both PNG sizes when adding an icon. Keep SVGs self-contained, without
text, external images, scripts or font dependencies. Preserve the existing
control behavior when assigning icons. Menu-bar titles,
recent-file rows, visibility checkboxes and standard dialog buttons retain their
existing presentation. The large legacy `images/about.png` is retained only for
provenance and is not embedded in the product; `about.svg` is the small menu icon.

## Replacement of inherited action images

On 23 October 2026, all 29 inherited PNG/XPM action images were removed from the
current source and replaced by independently constructed SVG geometry, following
operation semantics without tracing or importing the original artwork. These
masters and their PNG exports are QualPF additions under GPL-3.0-only, to the
extent applicable rights exist. No third-party icon pack, fonts or external
image inputs are used. The existing application mark and other original icons
are unchanged.

`deploy/rights-review.json` preserves every old SHA-256, upstream blob/path,
the last retained QualPF revision and its replacement SVG path. That revision
and path recover the removed bytes from Git; they are not shipped in source or
runtime packages. XPM records also identify the original CRLF checkout conversion
needed to reproduce their unchanged review hashes from Git's LF blobs.
Original grant findings remain unresolved, not relabeled as
QualPF ownership. The old `:/images/images/` action URLs have been removed;
consumers use `:/icons/<name>.png` with the corresponding `@2x` companion.
`create-database` is distinct from `database-add` (add an existing database).
The legend action retains its original state mapping: Off uses `legend-show`,
On uses `legend-hide`. The preferences asset remains available without adding
a new action or changing the settings workflow.
