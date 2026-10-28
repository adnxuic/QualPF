# Infrastructure

`database` contains execution-thread Qt SQL repositories, writers and database-build
providers. `io` supplies filesystem/binary32 boundaries, PDF-2 reading and atomic
export targets; Core owns the memory Pattern, CIF and powder-CIF parsers.
`serialization` implements QPF and settings codecs. `resources`, `settings`,
`platform`, `network` and `diagnostics` provide the remaining external-service
adapters.

Concrete providers implement Application ports and are assembled by the
`apps/qualpf` composition root. They consume owned inputs without reading live
Sessions or GUI objects. Existing database schemas and format contracts remain
unchanged.

See the [repository layout](../../docs/developer/repository-layout.md) for
current responsibilities and [developer contracts](../../docs/developer/README.md)
for existing numerical and product authorities.
