# Database setup

Search and match reads reference cards from a registered database. Obtain a
database appropriate to your experiment and retain its source and usage terms.
QualPF does not bundle a reference database or turn a database citation into
permission to redistribute its contents.

## Keep the database files together

A compiled database uses a `.sq` file with companion files. For example, a
database called `my_reference` has these members:

```text
my_reference.sq
my_reference.sq.info
my_reference.sq.infostat
my_reference.sq.search
```

Keep the complete set from the same database build in one directory. An archive
must be extracted before use. A CIF collection is source material for database
creation, not a replacement for the compiled file set.

## Register and select

The default discovery directory is `QualXDB` in your home folder. Subdirectories
are searched for `.sq` databases at startup. On Windows this is normally
`C:\Users\<username>\QualXDB`; on Linux it is `~/QualXDB`.

Open **S&M > Manage Databases** to inspect registrations, add a `.sq` file from
another location and select the database to use. If the initial missing-folder
prompt appears, **Choose folder...** selects a discovery directory; **Manage
Databases later** leaves configuration for a later session.

The manager's **Create** command opens database creation. For your own CIF
collection, choose **User database** and **Source from CIF files**. Check the
import outcome before relying on the new database. Removing a registration and
deleting its files are different actions: inspect the deletion dialog's
file-removal choice before accepting it.

## Troubleshooting and saved analyses

If registration or search fails, check that all four members exist, can be read
and belong together. Do not repair a database by mixing companion files from
different downloads. Record the reported error along with the database source.

A QPF project stores the cards and evidence already used in its analysis, so
viewing those results does not require the original database. Retrieving new
cards does require a compatible database identity. After changing the database,
restart the analysis before requesting new references.

Database providers and relevant research resources are listed in
[References](references.md). This manual does not promise availability or a
particular size for a provider's downloadable archives.
