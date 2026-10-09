# Core

`qualpf_core` is a Qt-free C++20 STATIC library with public headers in
`include/qualpf`. Responsibility directories contain domain models and validation,
crystallography, numerical foundations, classic search, preprocessing, peak
processing and pattern readers. Profile, measurement and export retain their
separate targets. Eigen and Boost are private implementation dependencies.
Versioned `.inc` numerical tables are source inputs.

Production and tests have completed Fortran decoupling. Compatibility algorithms,
strict floating-point options and accepted numerical contracts remain in effect.
Public interfaces expose no Qt, SQL or Eigen/oneMKL types.

See the [repository layout](../../docs/development/repository-layout.md) for
current responsibilities and [migration status](../../.agent/MIGRATION_STATUS.md)
for existing numerical and product authorities.
