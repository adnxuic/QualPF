# Installation

Use a QualPF build with its matching runtime files. The application executable is
`qualpf` on Linux and `qualpf.exe` on Windows. Databases and experimental patterns
are separate inputs; installing the program does not supply either.

## Obtain a build

The project has no configured public binary download endpoint. Local packages
marked `audit` are inspection artifacts, not a formal release. Consult the
repository's developer build and distribution guides when producing a package;
package generation alone does not establish release readiness.

On Windows, a prepared installer guides directory selection and shortcut creation.
Keep the executable with the Qt plugins and runtime libraries supplied by that
build. Copying only the executable can prevent startup. When replacing an
installation, follow the installer's instructions for the existing version.

For a prepared Linux AppImage, make the file executable and launch it from your
file manager or terminal. If its FUSE integration is unavailable, the AppImage
runtime also provides `--appimage-extract`; run `squashfs-root/AppRun` from the
extracted directory. Availability on a particular system depends on the build's
runtime requirements, not just the filename.

## Build from source

The supported build uses CMake 3.20 or later, a C++20 compiler, matching Qt 6
development libraries, Eigen 3.4 and Boost headers. PFSM execution also uses
compiler OpenMP support. A Fortran compiler is not needed. Dependencies must be
provided before configuring; CMake does not download them.

From a configured source checkout, the usual sequence is:

```sh
cmake -S . -B build/local -DCMAKE_BUILD_TYPE=Release
cmake --build build/local --parallel
cmake --install build/local --prefix /your/install/directory
```

Use a writable installation prefix or the platform's normal installation
permissions. Windows builds require a compiler-compatible Qt kit. Consult
`docs/developer/building.md` in the source distribution for presets and platform
setup; the commands above assume that CMake can already locate the dependencies.

## First session

Launch the installed executable and follow [Database setup](database-installation.md).
You may postpone database configuration to inspect a pattern or an existing QPF
project. A new database search requires an available reference database.

The compatibility folder name `QualXDB` is retained. Existing QualPF settings take
precedence when the first normal startup imports older application preferences;
the older settings store is kept. This import is not repeated after it succeeds.
Preferences and dock layouts are local to your installation, while project
analysis settings are stored in QPF files.

## Licenses and source code

Open **Help → About → License and notices** for the GPL, origin and third-party
notices. The **Origin and changes** page links to the offline source-retrieval
instructions. A slim Windows distribution supplies the application and dependency
source archives separately beside the installer, with their SHA256SUMS. Keep the
matching archives if you want to inspect or rebuild that version; see
[Obtaining QualPF source code](../../SOURCE_CODE.md).
