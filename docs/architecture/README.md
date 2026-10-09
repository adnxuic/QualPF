# QualPF architecture

QualPF is a pure C++20 application with Qt-free Core and Application layers.
Final independent Milestone A accepts `5a247152` as
**CPP_ARCHITECTURE_REFACTOR_COMPLETE** after the bounded A-01 correction and
three-platform independent reacceptance. All 22 mandatory criteria pass.
PFSM has not started.

The separately authorized strict layer refinement is closed as
**STRICT_LAYER_CONVERGENCE_3_PLATFORM_PASS** at implementation `895e4138`.
Windows GCC/MSVC evidence is reused with identical inputs; independent Ubuntu
26.04 / GCC 15.2 / Qt 6.10.2 validation passes all 86 normal tests, Qt-free clients,
public headers, standalone snapshot consumers and installed DEB/runtime checks.
Historical Milestone A/F acceptance scopes and failures remain unchanged. STOP.

- [Strict layer convergence and current validation](strict-layer-convergence.md)
- [Linux independent validation and three-platform closure](strict-layer-linux-closure.md)
- [Ownership and dependencies](overview.md)
- [Independent Milestone A reacceptance and result](milestone-a/a01/independent-review.md)
- [Original blocked review (unchanged)](milestone-a/independent-review.md)
- [Architecture decisions](decisions.md)
- [Retained product ADRs](adr/README.md)
- [Recorded project baseline](../development/project-baseline.json)
- [Retained compatibility responsibilities](compatibility-allowlist.json)
- [Numerical policy and independent references](../numerical/policy.md)
- [Build and test instructions](../development/building.md)
- [History and restoration](../migration-history.md)
