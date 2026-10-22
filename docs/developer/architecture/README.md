# QualPF architecture

QualPF is a pure C++20 application with Qt-free Core and Application layers.
Application owns canonical product state; Infrastructure implements its ports,
GUI borrows immutable views, and explicit composition connects their lifetimes.
PFSM provides fixed-IL expansion and NNLS/TRF fitting through Application workflows
used by the GUI and CLI; see the algorithm and settings contracts below.

- [Ownership and dependencies](overview.md)
- [Search-Match workflow and seven algorithm contracts](search-algorithms.md)
- [Strict layer contracts](strict-layer-convergence.md)
- [Architecture decisions](decisions.md)
- [Retained product ADRs](adr/README.md)
- [Retained compatibility responsibilities](compatibility-allowlist.json)
- [Numerical policy and independent references](../numerical/policy.md)
- [Build, test and contribution instructions](../building.md)
- [Changelog](../../../CHANGELOG.md)

Recorded refactoring acceptance lives in the [project baseline](../project-baseline.json).
Original reviews, validation receipts and closure reports are indexed in
[migration history](../../../history/README.md#retained-reports). Their Phase,
Milestone and STOP records describe completed historical scopes; ongoing
development follows scoped tasks and the changelog.
