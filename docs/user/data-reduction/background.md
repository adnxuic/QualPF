# Background

A background estimate is stored alongside the working observations. It is not
defined by whether a background curve is visible on the plot.

## Estimate and inspect

Choose **Preprocessing > Generate Background** to calculate a new estimate over
the current pattern. Successful generation replaces the earlier background
points and background-filter history while retaining the current working signal's
range, smoothing and stripping. Failure leaves the prior data in place.

Open **Preprocessing > Background Options** to inspect a private preview, select
the method and adjust its coefficients. The editor also provides the Bruckner
filter controls. Accept a successful preview to apply it with its requested
parameters. Cancel discards the draft. If other results depend on the old
background, inspect the replacement confirmation before continuing.

The **Add Background Point** and **Delete Background Point** commands permit
point editing. Entering background editing restores the unsubtracted view so
you can place points against the observations.

## View a net signal

**View > Subtract Background** displays observations minus the aligned background.
It retains negative values and the working intensity units. The checkbox is
available only when a complete background is present and is initially off.
Background points are hidden in this display mode.

For PFSM, the displayed joint calculation uses the same subtraction. The fit,
residual, scores and composition are unchanged. Numerical exports retain their
scientific values; a printed plot or exported image follows the displayed view.
The checkbox itself does not mark the project as scientifically edited.

## When preparation cannot continue

Algorithms require a known subtraction history and aligned observations and
background. If that evidence is missing or already-subtracted input is refused,
restart preprocessing or reimport the measurement as instructed by the error.
Do not add an estimated background back to clipped data to manufacture an
unsubtracted measurement.
