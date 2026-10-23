# K-alpha2 stripping

**Preprocessing > K-α2 Stripping** requests removal of the supported K-alpha2
contribution from the working pattern. Check the measurement's radiation and
earlier instrument processing before using this command. A pattern already
corrected elsewhere must not be treated as an untouched doublet measurement.

The operation checks radiation support and recorded processing history. Read any
reported error; a rejected operation does not supply a corrected signal. QualPF
records successful application so the correction is not repeatedly evaluated on
the same processed input.

When intensities change, dependent preparation and analysis are invalidated.
A successful correction can also record its processing provenance even if the
computed intensities are unchanged. In that case the project may become modified
without a visible difference in the curve.

Use **View > Workflow** to inspect the operation and its dependencies. To analyze
the uncorrected measurement again, use the explicit preprocessing restart or
reimport the original data. Turning off a plot layer does not reverse stripping.

The historical method reference is included in [Methods](../references.md#methods).
