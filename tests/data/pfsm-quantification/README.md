# Independent PFSM/RIR real inputs

`real-inputs.json` freezes 11 complete reference-card rows, their original RIR and
normalization, database/source hashes, 28 development observations and independent
mass inputs before the conditional quantification run. These are input evidence,
not product-generated golden outputs. The selection policy is fixed independently
of fit quality or quantitative error. IUCr organizer weights and nominal author
sample-label masses are kept separate from the runner manifest. See the
[assessment protocol](../../../history/numerical/pfsm-benchmark.md#conditional-pfsmrir-quantification-assessment).

Library RIR provenance remains unverified. The Dara sample labels are not a
separately retained weighing log. Raw fit outputs and original preparation
failures remain in the ignored validation directory; current numerical
documentation reports all cases and scientific limitations.

The frozen input file is byte-preserved by `.gitattributes`, including its original
CRLF transport; its SHA-256 must match the assessment on every checkout platform.
