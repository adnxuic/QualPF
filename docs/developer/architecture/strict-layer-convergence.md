# Strict layer contracts

The dependency direction is GUI -> Application -> Core, with Infrastructure
implementing Application/Core ports. The executable composition root assembles
concrete providers. Core and Application remain Qt-free. See
[ownership and lifetimes](overview.md) for canonical state and checked transactions.

## Ownership

- Application owns database selection/version checks, search orchestration,
  file admission/save, settings policy, maintenance and update interpretation.
- Infrastructure owns SQL connections, codecs, preferences, process/network
  transports and file discovery. Ports exchange owned values; providers do not
  access GUI objects or live Sessions. SQL connections belong to their execution thread.
- GUI borrows immutable Application views and submits intents. It does not include
  Core/Infrastructure headers or perform Core computations. Domain value types
  may cross through Application headers.
- Composition constructs providers before Workspace/use cases and windows, and
  keeps them alive through worker drain and subscription cleanup.

GUI presentation settings use opaque bytes with `QDataStream::Qt_6_0` where required;
storage preserves keys and QVariant types. Display-coordinate expressions live
in a separate Core target with their approved floating-point compilation mode.
Snapshot transport is test-only and cannot enter production dependency closures.

## Enforced boundaries

Application exports headers from `src/application/include/qualpf/application`.
The broad source tree is not a public include root. Targets declare their Qt
modules explicitly. The default build runs source checks and a configured-target
gate that traverses links/interfaces, resolves header ownership and indexes Core
operations, including dependencies hidden behind facade targets.

```sh
python -B tools/architecture/check_boundaries.py --self-test
python -B tools/architecture/check_strict_boundaries.py --graph build/windows-gnu/strict-targets.json --self-test
```

Use the graph from the configuration actually being checked. Normal coverage
includes `qualpf.architecture.strict`; source observations do not replace runtime
transaction, cancellation and lifetime tests. See [building](../building.md) and
[coverage](../../../tests/coverage.json).

## Original acceptance

The [original convergence report](../../../history/architecture/strict-layer-convergence.md)
and [Linux closure](../../../history/architecture/strict-layer-linux-closure.md)
retain their original revisions, test counts, failures and PFSM scope. Those
historical outcomes do not certify later changes. This guide extracts the ongoing
contracts without reopening numerical or product acceptance.
