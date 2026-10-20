# Background

## Generate and configure

**Preprocessing → Generate Background** immediately estimates the background over
the full current pattern using the current background options. It replaces previous
background points and background-filter history. A failed estimate leaves the
previous data intact. Range, smoothing and Kα2 processing remain part of the
current working signal.

**Preprocessing → Background Options** opens the original independent background
editor. Choose a method, adjust its coefficients or apply/reset the Bruckner
filter while previewing the curve. OK applies the preview and its requested
parameters to the current project; Cancel discards them. Dependent results
require confirmation before replacement. **Setting → Preprocessing** edits
defaults for new projects in the separate workflow settings window.

## Background points

The main plot normally shows the working observations and background points,
with the background curve hidden. Add or delete points with the corresponding
Preprocessing actions. Background Options and point editing restore the
unsubtracted display before editing. Algorithms use the background independently
of its plot visibility.

## Display subtraction

**View → Subtract Background** is a display-only checkbox, initially off. It is
available when a complete aligned background exists. The checked display shows
`Y - B`, preserving negative values and the current intensity units without
clipping or renormalization. Background points are hidden until the checkbox is
cleared. PFSM displays its joint calculation minus the same background; its
residual, fitted parameters, FOM and quantification do not change.

Switching the checkbox never edits or dirties the project and never changes
numerical exports. Image export and plot printing reflect the displayed view.
All algorithm inputs contain the unsubtracted working signal and its background;
the algorithm prepares its own analysis buffer. Background changes invalidate
dependent numerical evidence under the normal source rules.

Historical projects retain their stored values. An already-subtracted or unknown
subtraction state requires an explicit preprocessing restart or reimport before
new calculations; no background is added back to clipped historical data.
