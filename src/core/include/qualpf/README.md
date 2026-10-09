# Core public interfaces

`domain`, `numerics`, `crystallography`, `preprocessing`, `peaks`, `search`,
`profile`, `io` and `export` expose existing standard C++20 interfaces by domain.
`migration/backends.h` retains the abstract backend interface identity.
Public types expose no Qt, SQL, GUI, Eigen/oneMKL or Fortran ABI types.

See [migration status](../../../../.agent/MIGRATION_STATUS.md) for current owners
and authorities.
