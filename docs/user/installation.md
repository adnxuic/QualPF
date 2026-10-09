# Installation

## Windows

1. Build the Windows installer with CPack, or obtain the resulting `QualPF-<version>-win64.exe` from this project's local `releases` or build output. No public QualPF download endpoint is currently configured.
2. Run the installer and follow the on-screen instructions. The default installation folder is `C:\Program Files\QualPF`.
3. QualPF will be available from the Start Menu and the desktop shortcut.

> **Note:**
> If a previous version of QualPF is already installed, it must be uninstalled
> first before installing into the same folder.
>
---

## Linux

### Option A — AppImage (recommended)

The AppImage is a self-contained executable that bundles Qt6 and its other runtime dependencies. It runs on most recent x86_64 Linux
distributions without installing anything.

**1. Download**

Build `qualpf-<version>-x86_64.AppImage` with `scripts/package/build_appimage.sh`.
The resulting file is copied to `releases/<version>/linux-appimage/`.
The upstream QualX releases contain a different product.

**2. Make it executable and run it**

```bash
chmod +x qualpf-<version>-x86_64.AppImage
./qualpf-<version>-x86_64.AppImage
```

> **Note:**
> AppImages require FUSE. If you get a "FUSE not found" error, either
> install `libfuse2` (`sudo apt-get install libfuse2` on Debian/Ubuntu) or
> extract and run the AppImage without FUSE:
> ```bash
> ./qualpf-<version>-x86_64.AppImage --appimage-extract
> ./squashfs-root/AppRun
> ```
>
---

### Option B — Compilation from source (.tar.gz)

Use this method on Linux when building from source with a C++20 compiler.

**Requirements**

| Tool | Minimum version |
|------|-----------------|
| CMake | 3.20 |
| C++ compiler | C++20 support |
| Qt6 | 6.5 (Widgets, Sql, PrintSupport, Network) |
| Eigen | 3.4 |
| Boost | Multiprecision headers |

Install build dependencies on Debian/Ubuntu:

```bash
sudo apt-get install cmake build-essential pkg-config \
    qt6-base-dev libeigen3-dev libboost-dev
```

On Fedora/RHEL:

```bash
sudo dnf install cmake gcc gcc-c++ make pkgconfig \
    qt6-qtbase-devel eigen3-devel boost-devel
```

**Automated installation with `install.sh`**

```bash
tar xzf qualpf-<version>-Source.tar.gz
cd qualpf-<version>-Source
./install.sh
```

The script configures, compiles, and installs QualPF to `/opt/qualpf-<version>`.
To run it afterwards:

```bash
/opt/qualpf-<version>/bin/qualpf
# or add to PATH:
export PATH=/opt/qualpf-<version>/bin:$PATH
```

Pass an optional Qt6 prefix to `install.sh`:

```bash
./install.sh /home/user/Qt/6.5.0/gcc_64
```

A Fortran compiler is not required. Select a C++ compiler through `CXX` or CMake.

**Manual CMake build**

```bash
tar xzf qualpf-<version>-Source.tar.gz
cd qualpf-<version>-Source

cmake -DCMAKE_BUILD_TYPE=Release \
      -S . -B build
cmake --build build -j$(nproc)
sudo cmake --install build
```

---

## Databases

QualPF requires at least one crystallographic reference database.

| Database | Format | Notes |
|----------|--------|-------|
| COD (Crystallography Open Database) | COD/CIF | Free — download separately, not bundled with QualPF |
| PDF-2 | PDF-2 DAT | Commercial — ICDD license required |

Database files are managed through **Search → Manage Databases**.

For instructions on installing a database, see [Database Installation](database-installation.md).

---

## First Launch

On first launch QualPF will ask you to configure the path to the database files.
See [Getting Started](introduction/getting-started.md) for a step-by-step guide.

## Product name transition

The current command is `qualpf` (`qualpf.exe` on Windows); no old command alias is installed.
Install the renamed product into its new default directory rather than replacing an older installation.
Existing databases and the `QualXDB` folder stay in place.
At the first normal startup, QualPF imports application-specific settings from its previous identity
into the new user store. Existing new values win; the old store is retained. After a successful
import, deleted preferences are not imported again. `--settings-dir` confines the import to that
explicit directory, and help/version/invalid-command exits do not write settings.
