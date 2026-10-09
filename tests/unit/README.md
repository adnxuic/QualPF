# Unit tests

`core` contains domain, numerics, crystallography, preprocessing and classic-search
tests. `application` contains Session, presentation and CIF card-service tests.
`infrastructure/cif` contains syntax and structure-import tests. Writer/build
tests that actually create databases reside in `../integration/database`.

Original assertions, frozen inputs and numerical tolerances remain unchanged.
See the [test entry guide](../README.md) for configuration.
