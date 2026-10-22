# Action icons

These original QualPF icons complement the existing colorful 24px GUI artwork.
The SVG files are the editable masters. They use a 24-unit canvas, transparent
backgrounds, rounded outlines, blue spectra, green execution/output accents and
red removal accents. Shapes distinguish operations without relying on color.
Options use the corresponding operation with a gear. Existing icons are reused
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
resource URLs and control behavior when assigning icons. Menu-bar titles,
recent-file rows, visibility checkboxes and standard dialog buttons retain their
existing presentation. The large legacy `images/about.png` is retained only for
provenance and is not embedded in the product; `about.svg` is the small menu icon.
