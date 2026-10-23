# Worked procedure with your own data

This exercise uses one of your measurements and a reference database you have
configured. No experimental pattern, certified mixture composition or expected
phase ranking is supplied with this tutorial. Its purpose is to make your
analysis traceable, not to test the accuracy of a method against an invented truth.

## Prepare a record

Before importing, record the sample identifier, raw-file location, radiation,
angular units and any processing already performed by the instrument software.
Also record the reference database's name and source. If you do not know the
processing history, resolve it before applying corrections that might duplicate
earlier work.

## Follow one analysis

1. Import the measurement and check that its plot agrees with the acquisition
   range. Save an initial QPF project.
2. Inspect the background. If you edit it, examine both strong and weak features
   and record why the edit was needed.
3. Choose CSM or PFSM, inspect that method's project options and run it. Use the
   Workflow panel's **Details** to inspect the parameters actually used.
4. Inspect the leading candidate and at least one plausible alternative. Compare
   individual reflections as well as the overall score.
5. Accept a justified phase. Inspect the updated candidates and the unexplained
   signal before accepting another. Avoid selecting a card just to increase the
   number of phases in the result.
6. Review the selected composition, including any unavailable-result reasons.
   For a PFSM RIR revision, record the source and applicability of the replacement
   value in **Edit RIR...**.
7. Save the completed project under a meaningful name. Reopen it and inspect the
   selected cards, parameters and graph before sharing your interpretation.

## Questions to answer in your notes

| Question | Evidence to retain |
| --- | --- |
| Why was a phase selected? | Matched reflections, competing cards and sample context |
| What remains unexplained? | Residual features, missing phases and measurement artifacts |
| What processing was applied? | Workflow details and the preserved raw file |
| What do percentages cover? | Selected crystalline phases and RIR assumptions |
| Can the work be revisited? | QPF file, database identity and external input locations |

If you later compare settings, save separate projects and describe the changed
parameter. A score from another method or PFSM round has a different basis and
must not be treated as a directly comparable probability.
