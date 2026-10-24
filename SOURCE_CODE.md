# Obtaining QualPF source code

Obtain the corresponding source from the same distribution directory or download
page as the installer, at no additional charge. For QualPF 1.0.0 Windows candidates
the files are:

- **qualpf-1.0.0-Source.tar.gz**: the matching application source, build scripts,
  scientific-table modification inputs, generators and notices.
- **qualpf-1.0.0-Windows-Dependencies.tar.gz**: all collected Windows dependency
  source archives, original licenses, actual SDK header sources and build inputs.
- **SHA256SUMS** and **candidate-manifest.json**: the exact archive checksums,
  application revision and scope of the local package acceptance.

The slim Windows installer keeps the program, runtime libraries and original
legal notices installed. The complete dependency sources are supplied as a
separate archive beside the installer. Retain or download that archive separately
when you want to examine, modify or rebuild those components.

The installed `share/qualpf/distribution-build.json` identifies this application's
version and source revision. For separate delivery,
`share/qualpf/DEPENDENCIES/dependency-delivery.json` records the dependency archive's
exact filename, size and SHA-256. Use the matching archives identified by those
records and SHA256SUMS; a moving source branch alone does not identify them.

Extract the application archive into a new directory and follow its
`docs/developer/building.md`. It builds without Git metadata when matching SDKs
are provided. The dependency archive contains its own file index and the supplier
procedure in `BUILD-INPUTS/README.md`. Original third-party file terms remain in
force. Separate delivery changes the packaging layout, not the recorded IFW
vendor-build/CRT or scientific-data qualifications in NOTICE.md and the inventory.

Before any public binary distribution, both source archives and their checksums
must be available from the binary's download location with clear, free access.
Local candidate directories already carry these files; this notice does not
announce a public upload. If a source archive is missing where you obtained the
binary, report it through [QualPF Issues](https://github.com/adnxuic/QualPF/issues)
or **uncercalculate@163.com**.
