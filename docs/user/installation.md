# Installation

QualPF runs on Windows and Linux. Public QualPF downloads are not yet configured.
The locally verified 1.0.0 candidates are a Windows MinGW installer, a Debian 13
DEB and an Ubuntu 22.04 AppImage. Exact versions and acceptance scopes are in the
[distribution guide](https://github.com/adnxuic/QualPF/blob/main/docs/developer/releasing.md#current-native-package-acceptance).
There is currently no published Windows portable ZIP.

## Platform and resource requirements

| Item | Current scope |
| :--- | :--- |
| **Windows** | The installer was checked on Windows 11 x64 with the matching MinGW/Qt runtime. Windows 10 has not been separately verified for this release. |
| **Linux DEB** | The Debian 13 x64 candidate requires its recorded system dependencies, including glibc 2.38+ and libstdc++6 14+. It is not an Ubuntu 22.04 package. |
| **Linux AppImage** | The x64 candidate was built and checked on Ubuntu 22.04 with Qt 6.8.3. Extract-and-run was checked; FUSE mounting and full desktop integration were not exercised. |
| **CPU and memory** | Current candidates are 64-bit. PFSM can use multiple workers; memory and runtime depend on the observation grid, candidate pool, accepted phases and template cache. No universal minimum RAM or timing guarantee has been established. |
| **Storage** | Allow room for the application, separately supplied databases and saved projects. Keep matching application and dependency source archives with a distributed binary. |

The frozen candidates predate later maintenance fixes. Build and package the
intended release revision before publishing it.

## Installing on Windows

When a verified installer is supplied:

1. Keep its checksum and matching application/dependency source archives.
2. Run the supplied `QualPF-1.0.0-win64.exe` installer and choose a new QualPF
   installation directory.
3. Follow the destination and Start Menu pages, then launch the installed program.

Do not combine DLLs or plugins from different Qt/compiler kits. Building from source
requires the [supported tools](https://github.com/adnxuic/QualPF/blob/main/docs/developer/building.md).

## Building and installing on Linux

Supply CMake 3.20+, Ninja, a C++20 GNU compiler, matching Qt 6.7+ development files,
Eigen 3.4 and Boost headers. Python 3 is required by build checks; use Python 3.11+
for the complete validation suite. The Qt kit must include its SQLite SQL driver.
Ubuntu 22.04's original Qt 6.2 package is too old.

For a product build without optional development regressions:

~~~bash
cmake -S . -B build/linux-product -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux-product --parallel 2
./build/linux-product/src/qualpf
cmake --install build/linux-product --prefix "$HOME/.local/qualpf"
~~~

Set `CMAKE_PREFIX_PATH` or `Qt6_DIR` when Qt is outside the system search path.
Use the `linux-gnu` development preset after provisioning the independent references
and original fixtures in the [build guide](https://github.com/adnxuic/QualPF/blob/main/docs/developer/building.md).
Clang is not a verified release toolchain.

The root `./install.sh [qt_prefix]` is an alternative source-installation entry.
It checks dependencies and installs under `/opt/qualpf-1.0.0` by default, requesting
elevated privileges for installation.

## Verifying your installation

1. Open **Help > About** to check version, build identity and offline notices.
2. **Help > User Manual (HTML)** opens the current online guide. **Documentation
   (PDF)** opens an installed manual only when one was generated with matching
   source provenance; an installation need not contain a PDF.
3. Open **S&M > Manage Databases** and select a reference database before searching.

![QualPF About dialog showing version and licensing information](images/about-dialog.png)

## First run

Missing preferences are imported from the previous application settings on first
normal startup. Existing QualPF values and the old settings store are preserved.
Databases remain separate, including the compatible `QualXDB` directory.

See [Database Installation](database-installation.md) to register or create a database.
Optional defaults for future projects are under **Setting > Preprocessing**, **CSM**,
**PFSM** and **Peak Search**; see [Workflow Settings and Project History](settings.md).
