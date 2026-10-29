# K-alpha2 Stripping

Standard laboratory X-ray diffractometers equipped with a $\beta$-filter or secondary graphite monochromator (rather than a primary incident-beam Johansson monochromator) illuminate the specimen with both components of the characteristic **$K\alpha_1 / K\alpha_2$ doublet**. At medium and high $2\theta$ angles, every Bragg reflection splits into a primary $K\alpha_1$ peak and a secondary $K\alpha_2$ peak shifted to a slightly higher angle with approximately half the intensity ($I(K\alpha_2)/I(K\alpha_1) \approx 0.5$).

The **Preprocessing > K-α2 Stripping** command applies the accepted fixed Cu
compatibility correction to the working diffraction pattern. It has no adjustable
deconvolution parameters and does not support arbitrary anodes or doublet ratios.

---

## Prerequisites for Running `K-alpha2 Stripping`

The operation requires known, not-yet-stripped Cu X-ray radiation and a valid
increasing sample grid. A second declared wavelength is not mandatory:

1. Import the original measurement. If wavelength metadata is absent, the radiation dialog appears during import; otherwise verify the source metadata before importing.
2. Confirm `X-ray` and the actual Cu wavelength. A single Cu-compatible declaration
   or a Cu Kalpha1/Kalpha2 doublet with secondary/primary ratio `0.5` is accepted.
3. If **Use second wavelength** is enabled, verify both wavelengths and **Ratio**.
   Other ratios are refused by stripping, even though the general radiation editor
   accepts a broader range.
4. Confirm import. Unknown stripping history remains unknown; reimport the
   original measurement rather than inferring its history from appearance.

The single-component admission interval is `1.5405`–`1.54195` Å. For a declared
doublet, the primary must be `1.5405`–`1.5407` Å, the secondary
`1.5443`–`1.5445` Å and the secondary/primary ratio within `0.000001` of `0.5`.
Synchrotron imports are refused by this operation. Samples must remain finite
and strictly increasing after binary32 conversion and inside the fixed model's
angular domain; a valid general import can still fail this narrower check.

![Set Wavelength Radiation dialog configured with Cu-Ka1 primary wavelength, Cu-Ka2 second wavelength, and 0.5 intensity ratio](../images/wavelength-dialog.png)

---

## Executing `K-alpha2 Stripping`

Once the radiation and processing history are valid:

1. Select **Preprocessing > K-alpha2 Stripping** from the menu bar. The **Setting > Preprocessing > Kα2** page is informational; stripping is always explicit.
2. QualPF strips the $K\alpha_2$ contribution across the active calculation range, updates the central diffraction plot, and records a `Kα2` step in the **Workflow — Current Project** dock.

When intensities change, dependent background, width, peak and search results
are invalidated. The radiation declaration itself is retained. In particular,
stripping a declared two-component input does not collapse it to one wavelength;
PFSM refuses a stripped input that still declares two components. For that PFSM
workflow, retain the original unstripped doublet or undo the stripping operation
while its checkpoint is available.

### Relationship to Bragg's Law Split

For any interplanar spacing $d$, the angular separation between the $K\alpha_1$ reflection at $2\theta_1$ and the $K\alpha_2$ reflection at $2\theta_2$ follows from Bragg's law:

$$\sin\theta_2 = \frac{\lambda_2}{\lambda_1} \sin\theta_1$$

The first-order separation
$\Delta(2\theta) \approx 2(\lambda_2-\lambda_1)\tan\theta_1/\lambda_1$
(in radians) increases with angle. Whether the doublet is resolved also depends
on the measured peak width; there is no universal angle at which it splits.

---

## When to Use (and When to Skip) `K-alpha2 Stripping`

- **Recommended before Peak Search and CSM**:
  Classical peak picking can treat resolved secondary reflections as independent
  peaks. Consider stripping only for measurements within this fixed Cu model's
  scope, then inspect the result; improved identification is not guaranteed.
- **Inspect correction artifacts**:
  If the fixed correction is unsuitable, undo it while a session snapshot is
  available or reimport the original source. Changing the doublet ratio is not a
  tuning control for this operation.

PFSM can retain a declared unstripped doublet in its profile model. Do not strip an already stripped or monochromatic input. Unknown source/stripping history is refused; reimport the original source rather than declaring it known from appearance.
