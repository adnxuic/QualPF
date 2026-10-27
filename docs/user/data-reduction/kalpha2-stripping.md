# K-alpha2 Stripping

Standard laboratory X-ray diffractometers equipped with a $\beta$-filter or secondary graphite monochromator (rather than a primary incident-beam Johansson monochromator) illuminate the specimen with both components of the characteristic **$K\alpha_1 / K\alpha_2$ doublet**. At medium and high $2\theta$ angles, every Bragg reflection splits into a primary $K\alpha_1$ peak and a secondary $K\alpha_2$ peak shifted to a slightly higher angle with approximately half the intensity ($I(K\alpha_2)/I(K\alpha_1) \approx 0.5$).

The **Preprocessing > K-alpha2 Stripping** command removes the $K\alpha_2$ component from the working diffraction pattern using a Rachinger-type deconvolution correction, leaving a monochromatic $K\alpha_1$ profile.

---

## Prerequisites for Running `K-alpha2 Stripping`

Before **Preprocessing > K-alpha2 Stripping** can be executed, the active project's radiation configuration must define a second wavelength ($\lambda_2$):

1. Open **Preprocessing > Wavelength...** (`Ctrl+W`).
2. Confirm that **Radiation** is set to `X-ray` and **Wavelength** is set to your tube anode (for example, `Cu-Ka1` at `1.54056` Å).
3. Check **Add second wavelength**. QualPF automatically populates the corresponding $K\alpha_2$ wavelength (`1.54439` Å for Cu) and sets **I(lambda2)/I(lambda1)** to `0.5` (you can adjust this ratio between `0.0` and `1.0` if your optics partially attenuate $K\alpha_2$).
4. Click **OK** to save the dual-wavelength configuration.

![Set Wavelength Radiation dialog configured with Cu-Ka1 primary wavelength, Cu-Ka2 second wavelength, and 0.5 intensity ratio](../images/wavelength-dialog.png)

---

## Executing `K-alpha2 Stripping`

Once a second wavelength is configured:

1. Select **Preprocessing > K-alpha2 Stripping** from the menu bar (or configure the **Kα2** tab in **Setting > Preprocessing...** / **Setting > CSM...** / **Setting > Peak Search...**).
2. QualPF strips the $K\alpha_2$ contribution across the active calculation range, updates the central diffraction plot, and records a `Kα2` step in the **Workflow — Current Project** dock.

### Relationship to Bragg's Law Split

For any interplanar spacing $d$, the angular separation between the $K\alpha_1$ reflection at $2\theta_1$ and the $K\alpha_2$ reflection at $2\theta_2$ follows from Bragg's law:

$$\sin\theta_2 = \frac{\lambda_2}{\lambda_1} \sin\theta_1$$

Because $\Delta(2\theta) = 2\theta_2 - 2\theta_1 \approx 2 \frac{\lambda_2 - \lambda_1}{\lambda_1} \tan\theta_1$ grows with $\tan\theta$, $K\alpha_2$ doublets are strongly overlapped below $30^\circ\ 2\theta$ and clearly split above $50^\circ\ 2\theta$.

---

## When to Use (and When to Skip) `K-alpha2 Stripping`

- **Recommended before Peak Search and CSM**:
  Classical peak picking (**Preprocessing > Peak Search**) can mistake resolved high-angle $K\alpha_2$ peaks for independent reflections of a second crystalline phase. Running **K-alpha2 Stripping** before **Peak Search** prevents false-positive $K\alpha_2$ entries in the **Peaks** dock and improves CSM Figure of Merit scores.
- **Keep `I(lambda2)/I(lambda1)` Accurate**:
  Setting **I(lambda2)/I(lambda1)** too high (`> 0.55`) can cause over-subtraction oscillations on the high-angle side of strong reflections. If over-subtraction ripples appear at high angles, undo the `Kα2` step in the **Workflow — Current Project** dock, reduce **I(lambda2)/I(lambda1)** slightly (e.g., to `0.48`), and re-run **K-alpha2 Stripping**.
