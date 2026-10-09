# Infrastructure

`database` contains Qt SQL persistence and the CIF writer. `io` contains CIF
syntax/import, powder-CIF and existing file-format implementations. `resources`,
`settings`, `platform` and `diagnostics` provide runtime resources, configuration,
platform entry points and diagnostics.

This module registers the SQL writer; Application registers card, product and
database-build services. Class responsibilities, database schema, formats and
link relationships remain unchanged.

See the [repository layout](../../docs/development/repository-layout.md) for
current responsibilities and [migration status](../../.agent/MIGRATION_STATUS.md)
for existing numerical and product authorities.
