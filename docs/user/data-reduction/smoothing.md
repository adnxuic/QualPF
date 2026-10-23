# Smoothing

Smoothing changes the working observations. Use it for a justified noise treatment
and inspect whether narrow or weak features are altered before applying it.
It is an explicit preprocessing operation, not a prerequisite imposed by every
search command.

Open **Preprocessing > Smoothing**. Choose **Savitzky-Golay** or
**Adjacent-Averaging**, then adjust **Points of Window**. Savitzky-Golay also
uses **Polynomial Order**; that control is inactive for adjacent averaging.
The window parameter is a half-window in samples, so the full span is `2w + 1`.

The dialog evaluates a private preview as you change controls. Inspect the
preview over both narrow peaks and less structured regions. **OK** commits a
valid preview; Cancel restores the committed signal. Changing methods retains
their separate window drafts while the dialog remains open.

An applied signal change can invalidate background, widths, peaks or analysis
that relied on the earlier observations. Read the dependency confirmation and
inspect the resulting workflow record. Use session undo or an explicit
preprocessing restart when revisiting the decision; keep the original input.

The Savitzky-Golay publication is listed under [Methods](../references.md#methods).
