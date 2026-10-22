# QualPF application icon

The central white diffraction stick and the surrounding blue/teal peak share
one position. Together they express PFSM's expansion of a discrete reference
line into a continuous profile. The navy rounded tile has a transparent exterior
and carries no lettering so the shape remains useful at desktop icon sizes.

`qualpf-master.png` is the unmodified original generated with the built-in
`image_gen` tool. The exact generation prompt is recorded in [prompt.txt](prompt.txt).
All derived files preserve the artwork and alpha; the export helper only resizes
and packages the master. It does not regenerate or redraw the design.

## Consumers and exports

- `qualpf.png`: 512px Qt window/application icon, embedded at `:/branding/qualpf.png`.
- `qualpf.ico`: Windows executable icon with 16, 24, 32, 48, 64, 128 and 256px PNG frames;
  `qualpf.rc` embeds it. CMake explicitly depends on the ICO when rebuilding the resource.
- `deploy/linux/icons/<size>x<size>/qualpf.png`: 16, 32, 48 and 256px launcher assets,
  installed under the existing QualPF desktop identity.
- `docs/user/images/qualpf.png` and `qualpf-favicon.png`: 256px documentation logo
  and 32px browser favicon.

From the repository root, use Node.js and sharp (current exports: sharp 0.35.5):

```sh
node tools/gui/render_app_icon.cjs
node tools/gui/render_app_icon.cjs --check --preview build/app-icon
```

If sharp is locally provisioned, set `NODE_PATH` to its `node_modules` directory.
Do not commit machine-specific paths. `--check` compares every exported file with
the master-derived output without rewriting it. `--preview` writes light/dark
size sheets in the chosen local directory. Normal builds and packages consume
the committed assets and require neither the generator nor Node.js.

Original upstream artwork remains in its original paths. This application mark
is separate from the colorful menu/action icon collection in `../icons`.
