# Integration tests

`database` exercises actual Qt SQL writer and CIF database-build workflows.
`classic_workflow_test.cpp` exercises standalone Core/Session/snapshot classic
search integration, enabled by the optional GoogleTest configuration.

Product probes and dedicated helpers reside in `../product`.
See the [test entry guide](../README.md) for configuration.
