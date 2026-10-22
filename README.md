# QualPF

QualPF is a modified project derived from [QualX3](https://github.com/ccorrado71/qualx3), originally developed by IC-CNR.

**Qualitative phase analysis software for X-ray powder diffraction patterns**, based on QualX3 originally developed at the [Institute of Crystallography (IC-CNR), Bari](https://www.ic.cnr.it/).

QualPF helps identify the crystalline phases present in a powder diffraction pattern by comparing it against large reference databases (PDF-2, COD) and crystallographic data imported from CIF files.

[![License: GPL v3 only](https://img.shields.io/badge/License-GPL--3.0--only-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C)
![Qt6](https://img.shields.io/badge/Qt-6-41CD52)

---

## Table of contents

- [About](#about)
- [Features](#features)
- [Reference databases](#reference-databases)
  - [Installing a reference database](#installing-a-reference-database)
- [Requirements](#requirements)
- [Installation](#installation)
  - [Linux (quick install)](#linux-quick-install)
  - [Manual build](#manual-build)
- [Usage](#usage)
  - [Graphical interface](#graphical-interface)
  - [Command-line database creation](#command-line-database-creation)
  - [Command-line search](#command-line-search)
- [Documentation](#documentation)
- [Repository structure](#repository-structure)
- [Citing QualX](#citing-qualx)
- [License](#license)
- [Authors and acknowledgments](#authors-and-acknowledgments)
- [Contributing](#contributing)
- [Support](#support)

## About

QualPF is a pure C++20/Qt 6 application for qualitative phase identification
from powder X-ray diffraction. Core and Application are Qt-free; one Application
Workspace owns canonical state. CLI/tests use the synchronous SearchEngine, and
GUI searches use that same engine in a dedicated background worker with cooperative
cancellation and atomic owner-thread publication.

Product and normal tests require no Fortran compiler/runtime or historical runner.
See [current architecture](docs/developer/architecture/README.md),
[building and testing](docs/developer/building.md) and
[migration history/recovery](history/README.md). Classical and PFSM
Search-Match workflows are available; PFSM identification and performance
qualifications remain explicit in the [PFSM guide](docs/developer/numerical/pfsm.md).
Projects use the incompatible QPF 1.0 format; older QXP projects are not accepted.

The application can work with:
- **PDF-2** (Powder Diffraction File) reference data,
- **COD** (Crystallography Open Database) entries,
- custom databases built from a folder of **CIF** files.

## Features

- Qualitative phase identification against large crystallographic databases
- Built-in database creation/import tools (from PDF-2 or CIF files)
- Search engine based on top d-value indexing for fast pattern matching
- Supported platforms: Windows and Linux
- GUI workflow plus a scriptable command-line interface for database creation
- Qt-independent C++ numerical kernels with qualified compatibility contracts

## Reference databases

QualX databases are composed of four files sharing a common base name:

| Extension       | Content                                   |
|------------------|--------------------------------------------|
| `.sq`            | Main data (id, chemical formula, subfiles) |
| `.sq.info`       | Crystallographic and bibliographic data    |
| `.sq.infostat`   | Statistical tables                         |
| `.sq.search`     | Search index (top d-values)                |

Reference databases are installed separately from the source tree. See the
installation instructions below for [COD](https://www.crystallography.net/cod/)
databases and their runtime location.

You can also build your own database from PDF-2 data or a folder of CIF files — see [Command-line database creation](#command-line-database-creation).

### Installing a reference database

> ⚠️ **Important: Install a reference database before using QualPF**
>
> QualPF requires a crystallographic reference database to perform search & match analysis. Without a database installed, the software will not be able to identify phases.
>
> Two COD-based databases are available for download:
> - one dedicated to inorganic phases
> - one general-purpose database
>
> 📥 Download them here: <https://www.ba.ic.cnr.it/content/old/qualx3/Databases/>

QualPF looks for databases in a dedicated `QualXDB` folder inside your home directory (`~/QualXDB` on Linux, `C:\Users\<username>\QualXDB` on Windows). At every launch, QualPF scans this folder recursively for `.sq` files and registers any database it finds automatically.

To install a database:

1. Create the `QualXDB` folder in your home directory (if it doesn't already exist).
2. Download one of the zip archives from the link above and extract it directly into `QualXDB`.
3. Launch QualPF — the database is detected and registered automatically.

You can also add a database stored elsewhere via **S&M → Manage Databases → Add existing**. See the [Database Installation](docs/user/database-installation.md) documentation page for full details.

## Requirements

- CMake ≥ 3.20
- A C++20 compiler (GCC, Clang, or MSVC)
- Qt6 (components: `Widgets`, `Sql`, `PrintSupport`, `Network`)
- Eigen 3.4 and Boost headers
- SQLite (used via Qt SQL driver)

Supported operating systems are Windows and Linux. The current release acceptance
matrix is:

| Release configuration | C++ toolchain | Qt |
| --- | --- | --- |
| Windows 11 x64, MinGW | GCC 13.1 | 6.8.3 |
| Windows 11 x64, MSVC | MSVC 19.40.33811 | 6.8.3 |
| Ubuntu 26.04 x64 | GCC 15.2 | 6.10.2 |

The compiler examples above describe the C++20 requirement. Clang has not been
validated for the current release. macOS is outside the supported platform scope.

## Installation

The product name is **QualPF** and its command is `qualpf` (`qualpf.exe` on Windows).
Use a new installation directory for the renamed product. The first normal startup copies
missing preferences from the previous application settings into the new user store,
retaining the old store and any existing new values. Databases and `QualXDB` do not move.
Public QualPF downloads are not configured; build locally using the instructions below.


### Linux (quick install)

An automatic installation script is provided for Debian-based and RedHat-based distributions:

```bash
./install.sh [qt_prefix]
```

Examples:

```bash
./install.sh                  # auto-detects Qt
./install.sh /home/user/Qt/6.5.0/gcc_64
```

The script checks dependencies, configures the project with CMake, builds it, and installs it to `/opt/qualpf-<version>` (requires `sudo` for the install step).

### Manual build

```bash
cmake -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.y/gcc_64 \
      -DCMAKE_BUILD_TYPE=Release \
      -S . -B build/linux-gnu

cmake --build build/linux-gnu -j$(nproc)
sudo cmake --install build/linux-gnu
```

Adjust `CMAKE_PREFIX_PATH` to point to your Qt6 installation. A Fortran compiler is no longer a build requirement. Platform-specific packaging resources (icons, installer scripts) are available under `deploy/linux` and `deploy/Windows`.

## Usage

### Graphical interface

Launch the installed executable (`qualpf`) to open the GUI, load a diffraction pattern, select a reference database, and run a qualitative phase search.

### Command-line database creation

QualPF can also build reference databases without launching the GUI:

```bash
# From a PDF-2 file:
qualpf --createdb --pdf2 /path/to/pdf2.dat --dbout /path/to/output

# From a folder of CIF files:
qualpf --createdb --cifdir /path/to/cifs [--recursive] --dbout /path/to/output
```

### Command-line search

A qualitative phase search can also be run from the command line, against the currently configured reference database(s):

```bash
qualpf --search /path/to/data_file
```

Add `--nogui` to run entirely without graphics (e.g. over SSH or in scripts):

```bash
qualpf --search /path/to/data_file --nogui
```

Optional composition filters can be combined with `--search`:

```bash
qualpf --search /path/to/data_file --nogui --composition "Al AND Si" [--exact | --contains-any]
```

## Documentation

A user manual is available under `docs/user/` (built with [MkDocs](https://www.mkdocs.org/)):

```bash
pip install -r docs/requirements.txt
mkdocs build --strict -f docs/mkdocs.yml
bash docs/make_pdf.sh   # optional: generates docs/qualpf_manual.pdf
```

Choose [user documentation](docs/user/index.md) for installation and operation,
or the [developer guide](docs/developer/README.md) for building, architecture,
numerical contracts and tests. [Documentation ownership](docs/README.md) explains
where each topic belongs. [History](history/README.md) indexes original evidence;
[AGENTS.md](AGENTS.md) is the entry for AI working rules.

## Repository structure

```text
apps/qualpf/            GUI/CLI entry points and executable assembly
src/core/               Qt-free numerical/domain kernels and memory parsers
src/application/        Qt-free canonical state, engine, transactions and ports
src/infrastructure/     SQL, file/codecs/settings and resource providers
src/gui/                Qt Widgets, views, intents and background scheduling
cmake/                  build policy, templates and Windows/Linux packaging
scripts/                platform build/install/package helpers
tests/                  unit, integration, product and numerical regression
tests/data/             unchanged independent numerical/CIF datasets
tests/regression/references/  algorithm-organized frozen resources/provenance
tests/reference/        current reference identity manifest
tests/support/snapshot/ test-only binary32 transport consumers
tools/                  current context, architecture, validation and table tools
third_party/            required third-party source
share/qualx/            consumed runtime data with original names/attribution
deploy/                 current Windows/Linux packaging resources
docs/user/              user manual with unchanged page URLs
docs/developer/         architecture, numerical contracts and developer guides
history/                necessary acceptance records and independent archive index
.agent/                 AI working rules, documentation navigation and task handoff
build/<configuration>/  ignored local caches, binaries and test logs
releases/               ignored local package outputs
```

[Directory responsibilities](docs/developer/repository-layout.md) and
[build/contribution instructions](docs/developer/building.md) describe shared
presets and machine-local SDK configuration.

## Citing QualX

If you use QualX in your research, please cite one of the following works:

1. Altomare, A., Corriero, N., Cuocci, C., Falcicchio, A., Moliterni, A. and Rizzi, R.,
   *'Main features of QUALX2.0 software for qualitative phase analysis'* (2017). *Powder Diffr.*
   [https://doi.org/10.1017/S0885715617000240](https://doi.org/10.1017/S0885715617000240)

2. Altomare, A., Corriero, N., Cuocci, C., Falcicchio, A., Moliterni, A. and Rizzi, R.,
   *'QUALX2.0: a qualitative phase analysis software using the freely available database POW_COD'* (2015). *J. Appl. Cryst.* **48**, 598–603.
   [https://doi.org/10.1107/S1600576715002319](https://doi.org/10.1107/S1600576715002319)

3. Altomare, A., Cuocci, C., Giacovazzo, C., Moliterni, A. and Rizzi, R.,
   *'QUALX: A computer program for qualitative analysis using powder diffraction data'* (2008). *J. Appl. Cryst.* **41**(4), 815–817.
   [https://doi.org/10.1107/S0021889808016956](https://doi.org/10.1107/S0021889808016956)

## License

QualPF's additions and combined application are offered under the **GNU General
Public License, version 3 only (GPL-3.0-only)**. See [LICENSE](LICENSE), the
[origin and modification notice](NOTICE.md) and
[third-party notices](THIRD_PARTY_NOTICES.md). Third-party components retain their
own licenses. Outstanding rights and package-specific source obligations must be
resolved before distributing a release; a local audit package is not a cleared release.

## Authors and acknowledgments

QualPF is independently maintained and published by **adnxuic**. It derives from
QualX3, originally developed at the **Institute of Crystallography, CNR (IC-CNR),
Bari**, Italy. Original authorship and scientific references are retained; no
institutional endorsement or official QualX distribution status is claimed.

## Contributing

Bug reports, feature requests, and merge requests are welcome. Please open an issue describing the problem or proposal before submitting larger changes.

See the [contribution workflow](docs/developer/building.md#contribution-workflow)
for checks and change records. Current documentation uses English; frozen evidence
retains its original language and bytes.

## Support

For QualPF questions and issues, use [QualPF Issues](https://github.com/adnxuic/QualPF/issues).
